/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2010 Havoc Pennington
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO OUTPUT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <config.h>
#include <hio/hio-output-stream.h>
#include <hrt/hrt-log.h>
#include <hrt/hrt-task-runner.h>
#include <hrt/hrt-task.h>

static void check_write_watcher (HioOutputStream *stream);
static void drop_all_buffers    (HioOutputStream *stream);

struct HioOutputStream {
    GObject      parent_instance;

    /* Task in which we add our handlers for outputting the data */
    HrtTask     *task;

    /* fd we're writing to. -1 if we're done writing or have
     * never gotten an fd.
     */
    volatile int          fd;

    /* touched both from writing thread(s) and our task thread */
    GMutex      *buffers_lock;
    GQueue       buffers;

    /* generally set by writing thread(s) and read by our task thread */
    volatile int closed;

    /* generally set by task thread and read by any thread,
     * but can also be set by output chain's task thread
     */
    volatile int errored;

    /* touched both from writing thread(s) and our task thread */
    GMutex     *write_watcher_lock;
    HrtWatcher *write_watcher;

    /* touched only from our task thread (important because we don't
     * want to hold a lock that other threads want, while in a
     * write). So other threads can see if we have stuff pending to
     * write, we leave this buffer in the queue until we complete it.
     */
    HrtBuffer *current_buffer;
    gsize current_buffer_remaining;

    GMutex *done_notify_lock;
    HioOutputStreamDoneNotify done_notify_func;
    void *done_notify_data;
    GDestroyNotify done_notify_dnotify;

    volatile int done_notified;
};

struct HioOutputStreamClass {
    GObjectClass parent_class;

};

G_DEFINE_TYPE(HioOutputStream, hio_output_stream, G_TYPE_OBJECT);

enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */

static void
hio_output_stream_get_property (GObject                *object,
                                guint                   prop_id,
                                GValue                 *value,
                                GParamSpec             *pspec)
{
    HioOutputStream *stream;

    stream = HIO_OUTPUT_STREAM(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_output_stream_set_property (GObject                *object,
                                guint                   prop_id,
                                const GValue           *value,
                                GParamSpec             *pspec)
{
    HioOutputStream *stream;

    stream = HIO_OUTPUT_STREAM(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/* IN OUR TASK THREAD */
static void
ensure_current_buffer(HioOutputStream *stream,
                      HrtBuffer       *completed)
{
    HRT_ASSERT_IN_TASK_THREAD(stream->task);

    if (stream->current_buffer == NULL || completed) {
        g_mutex_lock(stream->buffers_lock);

        if (completed != NULL) {
            HrtBuffer *old;

            g_assert(completed == stream->current_buffer);

            old = g_queue_pop_head(&stream->buffers);
            g_assert(old == completed);
            hrt_buffer_unref(completed);
            stream->current_buffer = NULL;
        }

        /* Just peek, to leave the current buffer in stream->buffers
         * so the write thread can see if there's anything to write,
         * since only our task thread can look at
         * stream->current_buffer
         */

        stream->current_buffer =
            g_queue_peek_head(&stream->buffers);
        if (stream->current_buffer != NULL) {
            stream->current_buffer_remaining =
                hrt_buffer_get_write_size(stream->current_buffer);
        }

        g_mutex_unlock(stream->buffers_lock);
    }

    /* Be sure to NULL current_buffer if an error has occurred so we
     * don't try to write it. An async drop_all_buffers() will have been
     * queued as well but we want to do it earlier if we get here
     * since someone has asked for current_buffer to be up-to-date.
     */
    if (g_atomic_int_get(&stream->errored) > 0) {
        drop_all_buffers(stream);
    }
}

/* IN OUR TASK THREAD */
static void
notify_if_done(HioOutputStream *stream)
{
    HRT_ASSERT_IN_TASK_THREAD(stream->task);

    if (g_atomic_int_get(&stream->done_notified) == 0 &&
        hio_output_stream_is_done(stream)) {
        g_atomic_int_inc(&stream->done_notified);

        /* allow task to complete, assuming it has no other watchers.
         *
         * Another side effect: this should mean the task can complete
         * even if no watcher was ever added, which happens if the
         * stream is closed with no bytes ever written.
         */
        hrt_task_unblock_completion(stream->task);

        g_mutex_lock(stream->done_notify_lock);
        if (stream->done_notify_func != NULL) {
            HioOutputStreamDoneNotify func = stream->done_notify_func;
            void *data = stream->done_notify_data;
            GDestroyNotify dnotify = stream->done_notify_dnotify;

            stream->done_notify_func = NULL;
            stream->done_notify_data = NULL;
            stream->done_notify_dnotify = NULL;

            g_mutex_unlock(stream->done_notify_lock);

            g_object_ref(stream);
            (* func) (stream, data);
            if (dnotify != NULL) {
                (* dnotify) (data);
            }
            g_object_unref(stream);
        } else {
            g_mutex_unlock(stream->done_notify_lock);
        }
    }
}

/* IN OUR TASK THREAD */
static gboolean
on_ready_to_write(HrtTask        *task,
                  HrtWatcherFlags flags,
                  void           *data)
{
    HioOutputStream *stream;

    stream = HIO_OUTPUT_STREAM(data);

    HRT_ASSERT_IN_TASK_THREAD(stream->task);

    /* ensure_current_buffer() also does drop_all_buffers()
     * if an error has occurred to be sure current_buffer
     * is NULL on error.
     */
    ensure_current_buffer(stream, NULL);

    if (stream->current_buffer != NULL) {
        if (hrt_buffer_write(stream->current_buffer,
                             g_atomic_int_get(&stream->fd),
                             &stream->current_buffer_remaining)) {
            if (stream->current_buffer_remaining == 0) {
                /* get a new current buffer, deleting this one */
                ensure_current_buffer(stream, stream->current_buffer);
            }
        } else {
            /* ERROR */
            g_atomic_int_inc(&stream->errored);
            hio_output_stream_close(stream);
            drop_all_buffers(stream);
        }
    }

    if (stream->current_buffer == NULL) {
        /* if no new buffer, we probably need removing. */
        check_write_watcher(stream);
    }

    notify_if_done(stream);

    /* stay installed until check_write_watcher() removes us. */
    return TRUE;
}

/* CALLED FROM EITHER OUR TASK THREAD OR WRITE THREAD(S) */
static void
check_write_watcher(HioOutputStream *stream)
{
    gboolean need_write_watcher;

    /* can't look at current_buffer in here since it may not be our
     * task thread
     */

    /* We want to avoid removing/adding write watcher while buffers
     * simultaneously appear/disappear.  So keep both locks at once
     * until we're completely sorted out.
     */
    g_mutex_lock(stream->write_watcher_lock);
    g_mutex_lock(stream->buffers_lock);

    need_write_watcher =
        g_queue_get_length(&stream->buffers) > 0 &&
        g_atomic_int_get(&stream->fd) >= 0 &&
        g_atomic_int_get(&stream->errored) == 0;

    if (stream->write_watcher == NULL && need_write_watcher) {
        stream->write_watcher =
            hrt_task_add_io(stream->task,
                            g_atomic_int_get(&stream->fd),
                            HRT_WATCHER_FLAG_WRITE,
                            on_ready_to_write,
                            g_object_ref(stream),
                            (GDestroyNotify) g_object_unref);
    } else if (stream->write_watcher != NULL && !need_write_watcher) {
        hrt_watcher_remove(stream->write_watcher);
        stream->write_watcher = NULL;
    }

    g_mutex_unlock(stream->write_watcher_lock);
    g_mutex_unlock(stream->buffers_lock);
}

/* this should always be done before dispose, because we'll always
 * have a write watcher holding a ref to the stream until we either
 * write stuff out completely or get an error. If we get an error, we
 * should drop all buffers.
 */
/* IN OUR TASK THREAD */
static void
drop_all_buffers(HioOutputStream *stream)
{
    HrtBuffer *buffer;

    HRT_ASSERT_IN_TASK_THREAD(stream->task);

    /* we're about to free current_buffer as the head of the queue
     */
    stream->current_buffer = NULL;

    g_mutex_lock(stream->buffers_lock);
    while ((buffer = g_queue_pop_head(&stream->buffers)) != NULL) {
        hrt_buffer_unref(buffer);
    }
    g_mutex_unlock(stream->buffers_lock);

    check_write_watcher(stream);

    notify_if_done(stream);
}

static void
hio_output_stream_dispose(GObject *object)
{
    HioOutputStream *stream;

    stream = HIO_OUTPUT_STREAM(object);

    g_assert(g_queue_get_length(&stream->buffers) == 0);
    g_assert(stream->write_watcher == NULL);
    g_assert(stream->done_notify_func == NULL);
    g_assert(g_atomic_int_get(&stream->done_notified) > 0);

    if (stream->task) {
        g_object_unref(stream->task);
        stream->task = NULL;
    }

    G_OBJECT_CLASS(hio_output_stream_parent_class)->dispose(object);
}

static void
hio_output_stream_finalize(GObject *object)
{
    HioOutputStream *stream;

    stream = HIO_OUTPUT_STREAM(object);

    g_mutex_free(stream->buffers_lock);
    g_mutex_free(stream->write_watcher_lock);
    g_mutex_free(stream->done_notify_lock);

    G_OBJECT_CLASS(hio_output_stream_parent_class)->finalize(object);
}

static void
hio_output_stream_init(HioOutputStream *stream)
{
    stream->fd = -1;

    stream->buffers_lock = g_mutex_new();
    g_queue_init(&stream->buffers);

    stream->write_watcher_lock = g_mutex_new();

    stream->done_notify_lock = g_mutex_new();
}

static void
hio_output_stream_class_init(HioOutputStreamClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = hio_output_stream_get_property;
    object_class->set_property = hio_output_stream_set_property;

    object_class->dispose = hio_output_stream_dispose;
    object_class->finalize = hio_output_stream_finalize;
}

HioOutputStream*
hio_output_stream_new(HrtTask *task)
{
    HioOutputStream *stream;

    stream = g_object_new(HIO_TYPE_OUTPUT_STREAM, NULL);
    stream->task = g_object_ref(task);

    /* we'll want to add and remove write watcher, and don't want our
     * task to get collected as completed until we're actually done
     * with output.
     */
    hrt_task_block_completion(stream->task);

    return stream;
}

/* TYPICALLY CALLED FROM ANOTHER THREAD */
void
hio_output_stream_write(HioOutputStream *stream,
                        HrtBuffer       *locked_buffer)
{
    g_return_if_fail(hrt_buffer_is_locked(locked_buffer));

    if (hrt_buffer_get_length(locked_buffer) == 0)
        return;

    if (hio_output_stream_is_closed(stream))
        return;

    g_mutex_lock(stream->buffers_lock);
    /* note, we still want to buffer stuff if fd == -1 since that just
     * means we aren't being asked to write yet. But on error, discard
     * anything that gets written.
     */
    if (g_atomic_int_get(&stream->errored) == 0) {
        /* we could set stream->errored right here after we just
         * checked we haven't. However in that case, the
         * drop_all_buffers() just after we set stream->errored will
         * have to block for buffers_lock and only then drop them, so
         * it should drop this one we're pushing.
         */
        hrt_buffer_ref(locked_buffer);
        g_queue_push_tail(&stream->buffers,
                          locked_buffer);
    }
    g_mutex_unlock(stream->buffers_lock);

    /* add write watcher if necessary. */
    check_write_watcher(stream);
}

/* IN OUR TASK THREAD */
static gboolean
on_notify_done_after_close(HrtTask        *task,
                           HrtWatcherFlags flags,
                           void           *data)
{
    HioOutputStream *stream;

    stream = HIO_OUTPUT_STREAM(data);
    g_assert(stream->task == task);

    HRT_ASSERT_IN_TASK_THREAD(stream->task);

    /* notify */
    notify_if_done(stream);

    return FALSE;
}

/* This does NOT close the fd. The stream never owns its fd.  It just
 * signals that the stream is at EOF and nothing else will be
 * buffered. It also means that we silently ignore any write attempts
 * (we may close when there's an error.  then the writer's stuff just
 * /dev/null's unless they check for error).
 */
/* TYPICALLY CALLED FROM ANOTHER THREAD */
void
hio_output_stream_close(HioOutputStream *stream)
{
    if (g_atomic_int_exchange_and_add(&stream->closed, 1) == 0) {
        /* If we just went from 0 (not closed) to 1 (closed) and that
         * makes us done, we need to notify done-ness in the task
         * thread.  If we still aren't done, we'll notify done-ness
         * once we flush so don't need to add a handler here.
         *
         * It's important not to add a watcher if we've already
         * notified done-ness because we'll have unblocked completion
         * and you can't add watchers to completed tasks.
         */
        if (hio_output_stream_is_done(stream)) {
            hrt_task_add_immediate(stream->task,
                                   on_notify_done_after_close,
                                   g_object_ref(stream),
                                   g_object_unref);
        }
    }
}

/* CALLED FROM ANY THREAD */
gboolean
hio_output_stream_is_closed(HioOutputStream *stream)
{
    return g_atomic_int_get(&stream->closed) != 0;
}

/* CALLED FROM ANY THREAD */
gboolean
hio_output_stream_got_error(HioOutputStream *stream)
{
    return g_atomic_int_get(&stream->errored) != 0;
}

/* A stream is done if it has nothing else it's going to write to an
 * fd. This means closed and fully flushed, or errored.
 * Done-ness hasn't necessarily been notified yet since that
 * may happen asynchronously.
 */
/* CALLED FROM ANY THREAD */
gboolean
hio_output_stream_is_done(HioOutputStream *stream)
{
    gboolean done;

    g_mutex_lock(stream->buffers_lock);
    done = hio_output_stream_is_closed(stream) &&
        (g_queue_get_length(&stream->buffers) == 0 ||
         hio_output_stream_got_error(stream));
    g_mutex_unlock(stream->buffers_lock);

    return done;
}

/* IN OUR TASK THREAD */
static gboolean
on_error_drop_all_buffers(HrtTask        *task,
                          HrtWatcherFlags flags,
                          void           *data)
{
    HioOutputStream *stream;

    stream = HIO_OUTPUT_STREAM(data);
    g_assert(stream->task == task);

    HRT_ASSERT_IN_TASK_THREAD(stream->task);

    drop_all_buffers(stream);

    return FALSE;
}

/* Invoked if we know the fd has an error, before we even
 * set the fd on the stream.
 */
/* IN ANY THREAD */
void
hio_output_stream_error(HioOutputStream *stream)
{
    /* incrementing error and closing will make
     * hio_output_stream_is_done() return TRUE.  At that point we need
     * to be sure done-notification happens if it hasn't.
     */

    g_atomic_int_inc(&stream->errored);

    /* close to ignore any further writes.  we want to
     * silently eat them, so errors don't have to be handled
     * by the writer. (Though writer could do so, if desired.)
     */
    hio_output_stream_close(stream);

    /* Delete any leftover buffers, in our task thread. This would not
     * work if we were already done-notified because completion would
     * be unblocked and we can't add watchers to a completed
     * task. There's no buffers if we're already done, anyhow.
     */
    if (g_atomic_int_get(&stream->done_notified) == 0) {
        hrt_task_add_immediate(stream->task,
                               on_error_drop_all_buffers,
                               g_object_ref(stream),
                               g_object_unref);
    }
}

/* CALLED FROM ANY THREAD */
void
hio_output_stream_set_fd(HioOutputStream *stream,
                         int              fd)
{
    /* This is only intended to work going to or from -1.  It isn't
     * supported to change from one valid fd to another valid fd
     * without going through -1.  The purpose of the function is
     * mostly to allow controlling which output stream is currently
     * writing to a given fd, by passing the fd amongst multiple
     * streams. We "pause" while our fd is -1, should be after
     * creation until we're told to go.
     *
     * It only makes sense for a single thread to "own" calling this
     * function, i.e. we aren't expecting to handle it being
     * called arbitrarily by everyone, just by the code that
     * knows what fd the stream should be writing to.
     */
    g_atomic_int_set(&stream->fd, fd);

    /* add the write watcher if we now have an fd and didn't before. */
    check_write_watcher(stream);
}

/* notify when the stream has written everything it's going to
 * write to the fd. This is intended to be set only once,
 * and the caller needs to check after setting it that
 * the stream wasn't done already (so no notification).
 */
/* CALLED FROM ANY THREAD */
void
hio_output_stream_set_done_notify(HioOutputStream           *stream,
                                  HioOutputStreamDoneNotify  func,
                                  void                      *data,
                                  GDestroyNotify             dnotify)
{
    g_mutex_lock(stream->done_notify_lock);

    if (stream->done_notify_dnotify != NULL) {
        /* not really expecting this case (expecting that done_notify
         * is set only once) but make halfhearted effort
         */
        stream->done_notify_func = NULL;
        stream->done_notify_data = NULL;
        stream->done_notify_dnotify = NULL;

        (* stream->done_notify_dnotify) (stream->done_notify_data);
    }

    if (g_atomic_int_get(&stream->done_notified) > 0) {
        /* we could never call the done notify anyhow so don't set
         * it. Callers will have to check is_done() after calling
         * set_done_notify().
         */
        g_mutex_unlock(stream->done_notify_lock);

        if (dnotify != NULL) {
            (* dnotify) (data);
        }
    } else {
        stream->done_notify_func = func;
        stream->done_notify_data = data;
        stream->done_notify_dnotify = dnotify;

        g_mutex_unlock(stream->done_notify_lock);
    }
}
