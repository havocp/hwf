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

#ifndef __HRT_EVENT_LOOP_H__
#define __HRT_EVENT_LOOP_H__

#include <glib-object.h>
#include <hrt/hrt-task-runner.h>

G_BEGIN_DECLS

typedef struct HrtEventLoopClass HrtEventLoopClass;

#define HRT_TYPE_EVENT_LOOP              (hrt_event_loop_get_type ())
#define HRT_EVENT_LOOP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HRT_TYPE_EVENT_LOOP, HrtEventLoop))
#define HRT_EVENT_LOOP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HRT_TYPE_EVENT_LOOP, HrtEventLoopClass))
#define HRT_IS_EVENT_LOOP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HRT_TYPE_EVENT_LOOP))
#define HRT_IS_EVENT_LOOP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HRT_TYPE_EVENT_LOOP))
#define HRT_EVENT_LOOP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HRT_TYPE_EVENT_LOOP, HrtEventLoopClass))

struct HrtEventLoop {
    GObject      parent_instance;

    /* private */
    GMutex *running_lock;
    GCond *running_cond;
    gboolean is_running;
};

struct HrtEventLoopClass {
    GObjectClass parent_class;

    void        (* run)          (HrtEventLoop *loop);
    void        (* quit)         (HrtEventLoop *loop);

    HrtWatcher* (* create_idle) (HrtEventLoop      *loop,
                                 HrtTask           *task,
                                 HrtWatcherCallback func,
                                 void              *data,
                                 GDestroyNotify     dnotify);
    HrtWatcher* (* create_io)   (HrtEventLoop      *loop,
                                 HrtTask           *task,
                                 int                fd,
                                 HrtWatcherFlags    io_flags,
                                 HrtWatcherCallback func,
                                 void              *data,
                                 GDestroyNotify     dnotify);
};

GType           hrt_event_loop_get_type (void) G_GNUC_CONST;

HrtEventLoop* _hrt_event_loop_new          (HrtEventLoopType    type);
void          _hrt_event_loop_run          (HrtEventLoop       *loop);
void          _hrt_event_loop_quit         (HrtEventLoop       *loop);
HrtWatcher*   _hrt_event_loop_create_idle  (HrtEventLoop       *loop,
                                            HrtTask            *task,
                                            HrtWatcherCallback  func,
                                            void               *data,
                                            GDestroyNotify      dnotify);
HrtWatcher*   _hrt_event_loop_create_io    (HrtEventLoop       *loop,
                                            HrtTask            *task,
                                            int                 fd,
                                            HrtWatcherFlags     io_flags,
                                            HrtWatcherCallback  func,
                                            void               *data,
                                            GDestroyNotify      dnotify);
void          _hrt_event_loop_wait_running (HrtEventLoop       *loop,
                                            gboolean            is_running);
void          _hrt_event_loop_set_running  (HrtEventLoop       *loop,
                                            gboolean            is_running);

G_END_DECLS

#endif  /* __HRT_EVENT_LOOP_H__ */
