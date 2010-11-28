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

#ifndef __HRT_TASK_PRIVATE_H__
#define __HRT_TASK_PRIVATE_H__

#include <glib-object.h>
#include <hrt/hrt-task.h>
#include <hrt/hrt-task-runner.h>
#include <hrt/hrt-task-thread-local.h>
#include <hrt/hrt-watcher.h>

G_BEGIN_DECLS

/* Internal HrtTask API (used only by the task runner and watcher machinery) */
void           _hrt_task_set_runner                   (HrtTask            *task,
                                                       HrtTaskRunner      *runner);
HrtTaskRunner* _hrt_task_get_runner                   (HrtTask            *task);
void           _hrt_task_lock_invoker                 (HrtTask            *task);
void           _hrt_task_unlock_invoker               (HrtTask            *task);
HrtInvoker*    _hrt_task_get_invoker                  (HrtTask            *task);
void           _hrt_task_set_invoker                  (HrtTask            *task,
                                                       HrtInvoker         *invoker);
void           _hrt_task_enter_invoke                 (HrtTask            *task,
                                                       HrtTaskThreadLocal *thread_local);
void           _hrt_task_leave_invoke                 (HrtTask            *task);
void           _hrt_task_watchers_inc                 (HrtTask            *task);
void           _hrt_task_watchers_dec                 (HrtTask            *task);
gboolean       _hrt_task_has_watchers                 (HrtTask            *task);
void           _hrt_task_mark_completed               (HrtTask            *task);
gboolean       _hrt_task_is_completed                 (HrtTask            *task);
void           _hrt_task_add_completed_notify         (HrtTask            *task,
                                                       HrtWatcher         *subtask_watcher);
void           _hrt_task_remove_completed_notify      (HrtTask            *task,
                                                       HrtWatcher         *subtask_watcher);
gboolean       _hrt_task_is_running_in_current_thread (HrtTask            *task);


/* Internal HrtTaskRunner API */
void          _hrt_task_runner_watcher_pending      (HrtTaskRunner      *runner,
                                                     HrtWatcher         *watcher);
HrtEventLoop* _hrt_task_runner_get_event_loop       (HrtTaskRunner      *runner);
void          _hrt_task_runner_queue_completed_task (HrtTaskRunner      *runner,
                                                     HrtTask            *task);
HrtWatcher*   _hrt_task_runner_add_immediate        (HrtTaskRunner      *runner,
                                                     HrtTask            *task,
                                                     HrtWatcherCallback  callback,
                                                     void               *data,
                                                     GDestroyNotify      dnotify);
HrtWatcher*   _hrt_task_runner_add_idle             (HrtTaskRunner      *runner,
                                                     HrtTask            *task,
                                                     HrtWatcherCallback  callback,
                                                     void               *data,
                                                     GDestroyNotify      dnotify);
HrtWatcher*   _hrt_task_runner_add_io               (HrtTaskRunner      *runner,
                                                     HrtTask            *task,
                                                     int                 fd,
                                                     HrtWatcherFlags     io_flags,
                                                     HrtWatcherCallback  callback,
                                                     void               *data,
                                                     GDestroyNotify      dnotify);
HrtWatcher*   _hrt_task_runner_add_subtask          (HrtTaskRunner      *runner,
                                                     HrtTask            *task,
                                                     HrtTask            *wait_for_completed,
                                                     HrtWatcherCallback  callback,
                                                     void               *data,
                                                     GDestroyNotify      dnotify);


/* Internal HrtWatcher API */
typedef void (* HrtWatcherStartFunc)    (HrtWatcher *watcher);
typedef void (* HrtWatcherStopFunc)     (HrtWatcher *watcher);
typedef void (* HrtWatcherFinalizeFunc) (HrtWatcher *watcher);

typedef struct {
    HrtWatcherStartFunc start;
    HrtWatcherStopFunc stop;
    HrtWatcherFinalizeFunc finalize;
} HrtWatcherVTable;

struct HrtWatcher {
    volatile int refcount;
    volatile int removed;
    HrtWatcherFlags flags;
    HrtTask *task;
    HrtWatcherCallback func;
    void *data;
    GDestroyNotify dnotify;

    const HrtWatcherVTable *vtable;
};

void           _hrt_watcher_base_init        (HrtWatcher             *watcher,
                                              const HrtWatcherVTable *vtable,
                                              HrtTask                *task,
                                              HrtWatcherCallback      func,
                                              void                   *data,
                                              GDestroyNotify          dnotify);
void           _hrt_watcher_ref              (HrtWatcher             *watcher);
void           _hrt_watcher_unref            (HrtWatcher             *watcher);
void           _hrt_watcher_dnotify_callback (HrtWatcher             *watcher);
void           _hrt_watcher_detach           (HrtWatcher             *watcher);
void           _hrt_watcher_start            (HrtWatcher             *watcher);
void           _hrt_watcher_stop             (HrtWatcher             *watcher);
void           _hrt_watcher_queue_invoke     (HrtWatcher             *watcher,
                                              HrtWatcherFlags         flags);
HrtWatcher*    _hrt_watcher_new_removed      (HrtWatcher             *was_removed);
HrtWatcher*    _hrt_watcher_new_immediate    (HrtTask                *task,
                                              HrtWatcherCallback      callback,
                                              void                   *data,
                                              GDestroyNotify          dnotify);
HrtWatcher*    _hrt_watcher_new_subtask      (HrtTask                *task,
                                              HrtTask                *wait_for_completed,
                                              HrtWatcherCallback      callback,
                                              void                   *data,
                                              GDestroyNotify          dnotify);
void           _hrt_watcher_subtask_notify   (HrtWatcher             *watcher_subtask);
HrtEventLoop*  _hrt_watcher_get_event_loop   (HrtWatcher             *watcher);
HrtTaskRunner* _hrt_watcher_get_task_runner  (HrtWatcher             *watcher);


G_END_DECLS

#endif  /* __HRT_TASK_PRIVATE_H__ */
