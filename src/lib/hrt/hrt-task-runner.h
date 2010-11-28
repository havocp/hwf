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

#ifndef __HRT_TASK_RUNNER_H__
#define __HRT_TASK_RUNNER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
    HRT_EVENT_LOOP_GLIB,
    HRT_EVENT_LOOP_EV
} HrtEventLoopType;

typedef struct HrtEventLoop       HrtEventLoop;
typedef struct HrtTask            HrtTask;
typedef struct HrtWatcher         HrtWatcher;
typedef struct HrtInvoker         HrtInvoker;

typedef enum {
    HRT_WATCHER_FLAG_NONE = 0,
    HRT_WATCHER_FLAG_READ = 1,
    HRT_WATCHER_FLAG_WRITE = 2
} HrtWatcherFlags;

typedef gboolean (* HrtWatcherCallback) (HrtTask        *task,
                                         HrtWatcherFlags flags,
                                         void           *data);

typedef struct HrtTaskRunner      HrtTaskRunner;
typedef struct HrtTaskRunnerClass HrtTaskRunnerClass;

#define HRT_TYPE_TASK_RUNNER              (hrt_task_runner_get_type ())
#define HRT_TASK_RUNNER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HRT_TYPE_TASK_RUNNER, HrtTaskRunner))
#define HRT_TASK_RUNNER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HRT_TYPE_TASK_RUNNER, HrtTaskRunnerClass))
#define HRT_IS_TASK_RUNNER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HRT_TYPE_TASK_RUNNER))
#define HRT_IS_TASK_RUNNER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HRT_TYPE_TASK_RUNNER))
#define HRT_TASK_RUNNER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HRT_TYPE_TASK_RUNNER, HrtTaskRunnerClass))

GType           hrt_task_runner_get_type                  (void) G_GNUC_CONST;

HrtTask*      hrt_task_runner_create_task     (HrtTaskRunner      *runner);
HrtTask*      hrt_task_runner_pop_completed   (HrtTaskRunner      *runner);

G_END_DECLS

#endif  /* __HRT_TASK_RUNNER_H__ */
