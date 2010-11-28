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
#include <hio/hio-output-chain.h>
#include <hrt/hrt-log.h>
#include <hrt/hrt-task.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/*
 * HioOutputChain is a list of output streams, where only the first one
 * has an fd set on it; the others are all buffering with nowhere to flush
 * data. The idea is that we are building a series of replies to write
 * down a socket, and those replies need to stay in order. But we'd like
 * all the repliers to be able to go ahead and buffer data. All streams
 * buffer in parallel, but we have to close and flush them sequentially.
 */

static void update_current_stream(HioOutputChain *chain);

struct HioOutputChain {
    GObject      parent_instance;

    /* HioOutputChain doesn't support usage from more than one thread
     * at a time. That thread has to be the task thread of the chain's
     * task. The intent is to set the chain's task to the same task
     * that's generating the streams.
     */
    HrtTask     *task;

    int          fd;
    GQueue       streams;
    HioOutputStream *current_stream;

    HioOutputChainEmptyNotify empty_notify;
    void *empty_notify_data;
    GDestroyNotify empty_notify_dnotify;

    guint errored : 1;
    guint blocking_completion : 1;
    guint have_had_a_stream : 1;
    guint have_empty_notified : 1;
};

struct HioOutputChainClass {
    GObjectClass parent_class;

};

G_DEFINE_TYPE(HioOutputChain, hio_output_chain, G_TYPE_OBJECT);

enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */

static void
hio_output_chain_get_property (GObject                *object,
                               guint                   prop_id,
                               GValue                 *value,
                               GParamSpec             *pspec)
{
    HioOutputChain *chain;

    chain = HIO_OUTPUT_CHAIN(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_output_chain_set_property (GObject                *object,
                               guint                   prop_id,
                               const GValue           *value,
                               GParamSpec             *pspec)
{
    HioOutputChain *chain;

    chain = HIO_OUTPUT_CHAIN(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_output_chain_dispose(GObject *object)
{
    HioOutputChain *chain;

    chain = HIO_OUTPUT_CHAIN(object);

    /* we can't dispose with stuff pending to write */
    g_assert(chain->current_stream == NULL);
    g_assert(g_queue_get_length(&chain->streams) == 0);
    g_assert(chain->fd == -1);

    if (chain->empty_notify_dnotify) {
        (* chain->empty_notify_dnotify) (chain->empty_notify_data);
        chain->empty_notify = NULL;
        chain->empty_notify_dnotify = NULL;
        chain->empty_notify_data = NULL;
    }

    if (chain->task) {
        g_object_unref(chain->task);
        chain->task = NULL;
    }

    G_OBJECT_CLASS(hio_output_chain_parent_class)->dispose(object);
}

static void
hio_output_chain_finalize(GObject *object)
{
    HioOutputChain *chain;

    chain = HIO_OUTPUT_CHAIN(object);

    G_OBJECT_CLASS(hio_output_chain_parent_class)->finalize(object);
}

static void
hio_output_chain_init(HioOutputChain *chain)
{
    chain->fd = -1;
    g_queue_init(&chain->streams);
}

static void
hio_output_chain_class_init(HioOutputChainClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = hio_output_chain_get_property;
    object_class->set_property = hio_output_chain_set_property;

    object_class->dispose = hio_output_chain_dispose;
    object_class->finalize = hio_output_chain_finalize;
}

/* IN OUR OWN TASK THREAD */
static gboolean
on_update_current_stream(HrtTask        *task,
                         HrtWatcherFlags flags,
                         void           *data)
{
    HioOutputChain *chain;

    chain = HIO_OUTPUT_CHAIN(data);

    HRT_ASSERT_IN_TASK_THREAD(chain->task);

    update_current_stream(chain);

    return FALSE;
}

/* INVOKED IN STREAM'S TASK THREAD NOT OUR OWN */
static void
on_stream_done(HioOutputStream *stream,
               void            *data)
{
    HioOutputChain *chain;

    chain = HIO_OUTPUT_CHAIN(data);

    /* Get back to our own task thread, then
     * update current stream.
     */
    hrt_task_add_immediate(chain->task,
                           on_update_current_stream,
                           g_object_ref(chain),
                           g_object_unref);
}

static void
update_current_stream(HioOutputChain *chain)
{
    HRT_ASSERT_IN_TASK_THREAD(chain->task);

    if (chain->current_stream != NULL) {
        if (hio_output_stream_is_done(chain->current_stream)) {
            HioOutputStream *head;

            hrt_debug("output chain fd %d finished writing a stream %p",
                      chain->fd, chain->current_stream);

            head = g_queue_pop_head(&chain->streams);
            g_assert(head != NULL);
            g_assert(head == chain->current_stream);

            if (hio_output_stream_got_error(chain->current_stream)) {
                chain->errored = TRUE;

                /* drop all streams, marking them errored. */
                while ((head = g_queue_pop_head(&chain->streams)) != NULL) {
                    hrt_debug("output chain fd %d dropping stream %p due to error state",
                              chain->fd, head);

                    hio_output_stream_error(head);
                    g_object_unref(head);
                }
            } else {
                /* just unref the current stream */
                g_object_unref(chain->current_stream);
            }

            chain->current_stream = NULL;
        }
    }

    if (chain->current_stream == NULL &&
        chain->fd >= 0) {
        HioOutputStream *head;

        head = g_queue_peek_head(&chain->streams);

        hrt_debug("output chain fd %d setting stream %p as current",
                  chain->fd, head);

        if (head != NULL) {
            chain->current_stream = head;

            hio_output_stream_set_done_notify(head,
                                              on_stream_done,
                                              g_object_ref(chain),
                                              g_object_unref);

            /* We could get the done notify from the
             * stream's task thread at any point starting now...
             */

            if (hio_output_stream_is_done(head)) {
                hrt_debug("new current stream %p already done with nothing to write",
                          chain->current_stream);

                /* we won't ever get the notify if the stream was done
                 * before we got to it. In this case, queue a new
                 * watcher to update the current stream again.
                 */
                hrt_task_add_immediate(chain->task,
                                       on_update_current_stream,
                                       g_object_ref(chain),
                                       g_object_unref);
            } else {
                /* Give this stream something to start writing to. */
                hio_output_stream_set_fd(head, chain->fd);
            }
        }
    }

    if (hio_output_chain_is_empty(chain)) {
        g_assert(chain->current_stream == NULL);

        hrt_debug("output chain fd %d is now empty",
                  chain->fd);

        if (chain->empty_notify && !chain->have_empty_notified) {
            chain->have_empty_notified = TRUE;
            (* chain->empty_notify) (chain, chain->empty_notify_data);
        }

        if (chain->have_had_a_stream) {
            /* the first time we had some streams then no longer do, we
             * allow the task to complete if nothing else is going on in
             * the task. This means the caller will have to keep the task
             * alive if the caller wants to add streams, flush, then add
             * more streams.
             */
            if (chain->blocking_completion) {
                chain->blocking_completion = FALSE;
                hrt_task_unblock_completion(chain->task);
            }
        }
    }
}

HioOutputChain*
hio_output_chain_new(HrtTask *task)
{
    HioOutputChain *chain;

    chain = g_object_new(HIO_TYPE_OUTPUT_CHAIN,
                         NULL);

    chain->task = g_object_ref(task);

    /* task can't end while output chain is not done, or we couldn't
     * use the task.
     */
    chain->blocking_completion = TRUE;
    hrt_task_block_completion(chain->task);

    return chain;
}

void
hio_output_chain_set_fd(HioOutputChain  *chain,
                        int              fd)
{
    g_return_if_fail(fd == -1 || fd >= 0);

    HRT_ASSERT_IN_TASK_THREAD(chain->task);

    if (chain->fd == fd)
        return;

    chain->fd = fd;

    if (chain->current_stream != NULL) {
        /* we can start (or stop) the first stream writing */
        hio_output_stream_set_fd(chain->current_stream, fd);
    }

    update_current_stream(chain);
}

void
hio_output_chain_set_empty_notify(HioOutputChain            *chain,
                                  HioOutputChainEmptyNotify  func,
                                  void                      *data,
                                  GDestroyNotify             dnotify)
{
    if (chain->empty_notify_dnotify) {
        (* chain->empty_notify_dnotify) (chain->empty_notify_data);
    }

    chain->empty_notify = func;
    chain->empty_notify_data = data;
    chain->empty_notify_dnotify = dnotify;
}

/* we are considered errored if any of the streams are.  the error
 * flag really goes with the fd, so once it happens it happens to
 * everything that would be using that fd.
 */
gboolean
hio_output_chain_got_error(HioOutputChain  *chain)
{
    HRT_ASSERT_IN_TASK_THREAD_OR_COMPLETED(chain->task);

    return chain->errored;
}

/* "empty-ness" for the chain can toggle back to not-empty if a stream is
 * added. Just means "empty right now"
 */
gboolean
hio_output_chain_is_empty(HioOutputChain  *chain)
{
    HRT_ASSERT_IN_TASK_THREAD_OR_COMPLETED(chain->task);

    /* current_stream remains in the streams queue, so we just have to
     * check if there's anything in the queue, don't need to check
     * chain->current_stream also.
     */
    return g_queue_get_length(&chain->streams) == 0;
}

/* It only makes sense to add a stream to one chain, one time.
 * Because the stream can only write to one fd, and can only
 * have one error flag and closed flag, etc. Hopefully obvious.
 */
void
hio_output_chain_add_stream(HioOutputChain  *chain,
                            HioOutputStream *stream)
{
    HRT_ASSERT_IN_TASK_THREAD(chain->task);

    hrt_debug("output stream fd %d adding stream %p errored=%d",
              chain->fd, stream, chain->errored);

    chain->have_had_a_stream = TRUE;

    if (chain->errored) {
        /* Maintain invariant that if we got an error on the fd,
         * all streams are not in our list of streams, and have
         * errored flag set.
         */
        hrt_debug("output chain fd %d not adding stream %p after all due to error state",
                  chain->fd, stream);

        hio_output_stream_error(stream);
        return;
    }

    /* anytime we add a stream, we leave empty state so have to
     * re-notify when we re-enter empty state.
     */
    chain->have_empty_notified = FALSE;

    g_object_ref(stream);

    g_queue_push_tail(&chain->streams, stream);

    update_current_stream(chain);
}
