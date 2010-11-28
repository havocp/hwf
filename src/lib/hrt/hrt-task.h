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

#ifndef __HRT_TASK_H__
#define __HRT_TASK_H__

/*
 * A HrtTask is an execution context (like a suspended stack or
 * continuation) which has one or more watchers (watcher is libev term
 * for GSource).  The HrtTask is resumed whenever a watcher is
 * invoked. When there are no watchers, then the task is complete.
 *
 */

#include <glib-object.h>
#include <hrt/hrt-task-runner.h>
#include <hrt/hrt-watcher.h>

G_BEGIN_DECLS

#ifdef G_DISABLE_CHECKS
#define HRT_ASSERT_IN_TASK_THREAD(task)
#define HRT_ASSERT_IN_TASK_THREAD_OR_COMPLETED(task)
#else
#define HRT_ASSERT_IN_TASK_THREAD(task)                         \
    g_assert(hrt_task_check_in_task_thread(task))
#define HRT_ASSERT_IN_TASK_THREAD_OR_COMPLETED(task)            \
    g_assert(hrt_task_check_in_task_thread_or_completed(task))
#endif

/* struct HrtTask forward-declared in task runner */
typedef struct HrtTaskClass HrtTaskClass;

#define HRT_TYPE_TASK              (hrt_task_get_type ())
#define HRT_TASK(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HRT_TYPE_TASK, HrtTask))
#define HRT_TASK_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HRT_TYPE_TASK, HrtTaskClass))
#define HRT_IS_TASK(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HRT_TYPE_TASK))
#define HRT_IS_TASK_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HRT_TYPE_TASK))
#define HRT_TASK_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HRT_TYPE_TASK, HrtTaskClass))

GType           hrt_task_get_type                  (void) G_GNUC_CONST;

HrtTask*       hrt_task_create_task        (HrtTask              *parent);
void           hrt_task_add_arg            (HrtTask              *task,
                                            const char           *name,
                                            const GValue         *value);
gboolean       hrt_task_get_arg            (HrtTask              *task,
                                            const char           *name,
                                            GValue               *value,
                                            GError              **error);
void           hrt_task_get_args           (HrtTask              *task,
                                            char               ***names_p,
                                            GValue              **values_p);
void           hrt_task_set_result         (HrtTask              *task,
                                            const GValue         *value);
gboolean       hrt_task_get_result         (HrtTask              *task,
                                            GValue               *value,
                                            GError              **error);
void*          hrt_task_get_thread_local   (HrtTask              *task,
                                            void                 *key);
void           hrt_task_set_thread_local   (HrtTask              *task,
                                            void                 *key,
                                            void                 *value,
                                            GDestroyNotify        dnotify);
void           hrt_task_block_completion   (HrtTask              *task);
void           hrt_task_unblock_completion (HrtTask              *task);
HrtWatcher*    hrt_task_add_immediate      (HrtTask              *task,
                                            HrtWatcherCallback    callback,
                                            void                 *data,
                                            GDestroyNotify        dnotify);
HrtWatcher*    hrt_task_add_idle           (HrtTask              *task,
                                            HrtWatcherCallback    callback,
                                            void                 *data,
                                            GDestroyNotify        dnotify);
HrtWatcher*    hrt_task_add_io             (HrtTask              *task,
                                            int                   fd,
                                            HrtWatcherFlags       io_flags,
                                            HrtWatcherCallback    callback,
                                            void                 *data,
                                            GDestroyNotify        dnotify);
HrtWatcher*    hrt_task_add_subtask        (HrtTask              *task,
                                            HrtTask              *wait_for_completed,
                                            HrtWatcherCallback    callback,
                                            void                 *data,
                                            GDestroyNotify        dnotify);


/* Internal (but has to be exported from lib), used by assertions only */
gboolean hrt_task_check_in_task_thread              (HrtTask *task);
gboolean hrt_task_check_in_task_thread_or_completed (HrtTask *task);

G_END_DECLS

#endif  /* __HRT_TASK_H__ */
