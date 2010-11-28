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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <config.h>

#include <hrt/hrt-task-private.h>

#include <hrt/hrt-watcher.h>
#include <hrt/hrt-log.h>

void
_hrt_watcher_base_init(HrtWatcher             *watcher,
                       const HrtWatcherVTable *vtable,
                       HrtTask                *task,
                       HrtWatcherCallback      func,
                       void                   *data,
                       GDestroyNotify          dnotify)
{
    watcher->vtable = vtable;

    g_assert(_hrt_task_get_runner(task) != NULL);
    g_assert(!_hrt_task_is_completed(task));

    watcher->task = task;
    g_object_ref(task);

    watcher->func = func;
    watcher->data = data;
    watcher->dnotify = dnotify;

    _hrt_task_watchers_inc(watcher->task);

    watcher->refcount = 1;
    watcher->flags = HRT_WATCHER_FLAG_NONE;
    watcher->removed = 0;
}

void
_hrt_watcher_ref(HrtWatcher *watcher)
{
    g_atomic_int_inc(&watcher->refcount);
}

void
_hrt_watcher_unref(HrtWatcher *watcher)
{
    if (g_atomic_int_dec_and_test(&watcher->refcount)) {
        g_object_unref(watcher->task);

        /* the virtual finalize has to free because it knows the
         * actual size of the watcher struct
         */
        if (watcher->vtable->finalize) {
            (* watcher->vtable->finalize)(watcher);
        }
    }
}

/* CALLED FROM INVOKE THREAD WHEN WATCHER IS REMOVED */
void
_hrt_watcher_dnotify_callback (HrtWatcher *watcher)
{
    GDestroyNotify dnotify;
    void *data;

    dnotify = watcher->dnotify;
    data = watcher->data;

    watcher->func = NULL;
    watcher->data = NULL;
    watcher->dnotify = NULL;

    if (dnotify != NULL) {
        (* dnotify) (data);
    }
}

/* CALLED FROM INVOKE THREAD WHEN WATCHER IS REMOVED */
void
_hrt_watcher_detach(HrtWatcher *watcher)
{
    _hrt_watcher_dnotify_callback(watcher);

    g_assert(!_hrt_task_is_completed(watcher->task));
    _hrt_task_watchers_dec(watcher->task);

    /* Remove ref owned by runner */
    _hrt_watcher_unref(watcher);
}

void
_hrt_watcher_start(HrtWatcher *watcher)
{
    g_assert(g_atomic_int_get(&watcher->removed) == 0);

    if (watcher->vtable->start) {
        (* watcher->vtable->start)(watcher);
    }
}

void
_hrt_watcher_stop(HrtWatcher *watcher)
{
    if (watcher->vtable->stop) {
        (* watcher->vtable->stop)(watcher);
    }
}

void
_hrt_watcher_queue_invoke(HrtWatcher     *watcher,
                          HrtWatcherFlags flags)
{
    HrtTaskRunner *runner;

    watcher->flags |= flags;

    runner = _hrt_watcher_get_task_runner(watcher);

    /* pass watcher to runner to be invoked in invoke threads */
    _hrt_task_runner_watcher_pending(runner, watcher);
}

HrtEventLoop*
_hrt_watcher_get_event_loop(HrtWatcher *watcher)
{
    return _hrt_task_runner_get_event_loop(_hrt_watcher_get_task_runner(watcher));
}

HrtTaskRunner*
_hrt_watcher_get_task_runner(HrtWatcher *watcher)
{
    return _hrt_task_get_runner(watcher->task);
}


/* Most watchers are event-loop-specific, but the "removed" watcher
 * isn't a real event loop source, it's just a thing we stick in the
 * invoke thread queue because we already know it's pending without
 * having to ask the event loop whether it is. So it can be generic
 * across all event loops.
 */
typedef struct {
    HrtWatcher base;
} HrtWatcherRemoved;

/* IN AN INVOKE THREAD */
static gboolean
on_watcher_removed(HrtTask        *task,
                   HrtWatcherFlags flags,
                   void           *data)
{
    HrtWatcher *removed = data;

    g_assert(g_atomic_int_get(&removed->removed) == 1);

    _hrt_watcher_detach(removed);

    /* there won't be a dnotify so we do this here */
    _hrt_task_watchers_dec(task);

    _hrt_watcher_unref(removed);

    /* important not to actually remove_watcher() on the
     * remove-watcher watcher, so return TRUE, but we get cleaned up
     * anyway because neither the task runner nor main loop holds a
     * ref to it.
     */
    return TRUE;
}

static void
_hrt_watcher_removed_finalize(HrtWatcher *watcher)
{
    g_slice_free(HrtWatcherRemoved, (HrtWatcherRemoved*) watcher);
}

static const HrtWatcherVTable removed_vtable = {
    NULL, /* start */
    NULL, /* stop */
    _hrt_watcher_removed_finalize  /* finalize */
};

/* a "removed" watcher is only an event, never added to main loop.
 * It is an event indicating we need to detach (destroy notify)
 * another watcher. The point is to get the dnotify into an
 * invoke thread and serialized with other event invokes
 * on the same task.
 */
HrtWatcher*
_hrt_watcher_new_removed(HrtWatcher *was_removed)
{
    HrtWatcherRemoved *removed;

    _hrt_watcher_ref(was_removed); /* removed by on_watcher_removed */

    removed = g_slice_new(HrtWatcherRemoved);
    _hrt_watcher_base_init(&removed->base,
                           &removed_vtable,
                           was_removed->task,
                           on_watcher_removed,
                           was_removed,
                           /* NOTE: remove-watcher is never itself
                            * removed so dnotify would never get called
                            */
                           NULL);

    return (HrtWatcher*) removed;
}

/* This can be called from another thread, and while invoking
 * the watcher, or while the watcher is in the invocation queue.
 */
void
hrt_watcher_remove(HrtWatcher *watcher)
{
    HrtWatcher *remove_watcher;

    g_assert(g_atomic_int_get(&watcher->removed) == 0);

    /* immediately remove the actual event notification
     * (remove watcher from main loop)
     */
    _hrt_watcher_stop(watcher);

    /* flag watcher as removed so we don't invoke any
     * already-queued events.
     */
    g_atomic_int_inc(&watcher->removed);

    /* queue a task to invoke thread, to dnotify
     * the watcher. unlike a typical watcher,
     * the task runner does not hold a ref to it and
     * it is not really added to the main loop,
     * so after the first invocation it's just unref'd
     * and vanishes.
     */
    remove_watcher = _hrt_watcher_new_removed(watcher);
    _hrt_watcher_start(remove_watcher);
    _hrt_task_runner_watcher_pending(_hrt_task_get_runner(remove_watcher->task),
                                     remove_watcher);
    /* runner does not hold a ref */
    _hrt_watcher_unref(remove_watcher);
}

/* Most watchers are event-loop-specific, but the "immediate" watcher
 * isn't a real event loop source, it's just a thing we stick in the
 * invoke thread queue because we already know it's pending without
 * having to ask the event loop whether it is. So it can be generic
 * across all event loops.
 */
typedef struct {
    HrtWatcher base;
} HrtWatcherImmediate;

static void
_hrt_watcher_immediate_finalize(HrtWatcher *watcher)
{
    g_slice_free(HrtWatcherImmediate, (HrtWatcherImmediate*) watcher);
}

static void
_hrt_watcher_immediate_start(HrtWatcher *watcher)
{
    /* Just run the watcher right away, no main loop. */
    _hrt_watcher_queue_invoke(watcher, HRT_WATCHER_FLAG_NONE);
}

static const HrtWatcherVTable immediate_vtable = {
    _hrt_watcher_immediate_start, /* start */
    NULL, /* stop doesn't make sense since we run right away (stop would always race) */
    _hrt_watcher_immediate_finalize  /* finalize */
};

/* an "immediate" watcher is only an event, never added to main loop.
 * It just runs right away (as if it had been triggered already in the
 * last main loop iteration).  This is different from a
 * default-priority idle in GLib, which would wait for the next main
 * loop iteration.
 */
HrtWatcher*
_hrt_watcher_new_immediate(HrtTask            *task,
                           HrtWatcherCallback  callback,
                           void               *data,
                           GDestroyNotify      dnotify)
{
    HrtWatcherImmediate *immediate;

    immediate = g_slice_new(HrtWatcherImmediate);
    _hrt_watcher_base_init(&immediate->base,
                           &immediate_vtable,
                           task,
                           callback,
                           data,
                           dnotify);

    return (HrtWatcher*) immediate;
}

typedef struct {
    HrtWatcher base;
    HrtTask *wait_for_completed;
    gboolean started;
} HrtWatcherSubtask;

static void
_hrt_watcher_subtask_finalize(HrtWatcher *watcher)
{
    HrtWatcherSubtask *subtask = (HrtWatcherSubtask*) watcher;

    _hrt_task_remove_completed_notify(subtask->wait_for_completed,
                                      watcher);
    g_object_unref(subtask->wait_for_completed);
    g_slice_free(HrtWatcherSubtask, subtask);
}

static void
_hrt_watcher_subtask_stop(HrtWatcher *watcher)
{
    HrtWatcherSubtask *subtask = (HrtWatcherSubtask*) watcher;

    subtask->started = FALSE;
}

static void
_hrt_watcher_subtask_start(HrtWatcher *watcher)
{
    HrtWatcherSubtask *subtask = (HrtWatcherSubtask*) watcher;

    subtask->started = TRUE;
}

static const HrtWatcherVTable subtask_vtable = {
    _hrt_watcher_subtask_start, /* start */
    _hrt_watcher_subtask_stop,
    _hrt_watcher_subtask_finalize  /* finalize */
};

/* an "subtask" watcher runs when the specified task (other than the
 * one with the watcher) is completed. Tasks can't wait on themselves
 * since they'd never complete. A "task cycle" would also be a problem,
 * but don't do that then.
 */
HrtWatcher*
_hrt_watcher_new_subtask(HrtTask            *task,
                         HrtTask            *wait_for_completed,
                         HrtWatcherCallback  callback,
                         void               *data,
                         GDestroyNotify      dnotify)
{
    HrtWatcherSubtask *subtask;

    g_assert(task != wait_for_completed);

    subtask = g_slice_new(HrtWatcherSubtask);
    _hrt_watcher_base_init(&subtask->base,
                           &subtask_vtable,
                           task,
                           callback,
                           data,
                           dnotify);
    subtask->wait_for_completed = wait_for_completed;
    g_object_ref(subtask->wait_for_completed);

    _hrt_task_add_completed_notify(subtask->wait_for_completed,
                                   (HrtWatcher*) subtask);

    return (HrtWatcher*) subtask;
}

void
_hrt_watcher_subtask_notify(HrtWatcher *watcher)
{
    HrtWatcherSubtask *subtask = (HrtWatcherSubtask*) watcher;

    if (!subtask->started)
        return;

    _hrt_watcher_queue_invoke(watcher, HRT_WATCHER_FLAG_NONE);
}
