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
#include <hrt/hrt-event-loop.h>
#include <hrt/hrt-log.h>
#include <hrt/hrt-event-loop-glib.h>
#include <hrt/hrt-event-loop-ev.h>
#include <hrt/hrt-builtins.h>
#include <hrt/hrt-marshalers.h>

G_DEFINE_TYPE(HrtEventLoop, hrt_event_loop, G_TYPE_OBJECT);

enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */

static void
hrt_event_loop_get_property (GObject                *object,
                             guint                   prop_id,
                             GValue                 *value,
                             GParamSpec             *pspec)
{
    HrtEventLoop *loop;

    loop = HRT_EVENT_LOOP(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hrt_event_loop_set_property (GObject                *object,
                             guint                   prop_id,
                             const GValue           *value,
                             GParamSpec             *pspec)
{
    HrtEventLoop *loop;

    loop = HRT_EVENT_LOOP(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hrt_event_loop_dispose(GObject *object)
{
    HrtEventLoop *loop;

    loop = HRT_EVENT_LOOP(object);


    G_OBJECT_CLASS(hrt_event_loop_parent_class)->dispose(object);
}

static void
hrt_event_loop_finalize(GObject *object)
{
    HrtEventLoop *loop;

    loop = HRT_EVENT_LOOP(object);

    g_mutex_free(loop->running_lock);
    g_cond_free(loop->running_cond);

    G_OBJECT_CLASS(hrt_event_loop_parent_class)->finalize(object);
}

static void
hrt_event_loop_init(HrtEventLoop *loop)
{
    loop->running_lock = g_mutex_new();
    loop->running_cond = g_cond_new();
}

static void
hrt_event_loop_class_init(HrtEventLoopClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = hrt_event_loop_get_property;
    object_class->set_property = hrt_event_loop_set_property;

    object_class->dispose = hrt_event_loop_dispose;
    object_class->finalize = hrt_event_loop_finalize;
}

HrtEventLoop*
_hrt_event_loop_new(HrtEventLoopType type)
{
    GType gtype = G_TYPE_NONE;

    switch (type) {
    case HRT_EVENT_LOOP_GLIB:
        gtype = HRT_TYPE_EVENT_LOOP_GLIB;
        break;
    case HRT_EVENT_LOOP_EV:
        gtype = HRT_TYPE_EVENT_LOOP_EV;
        break;
    }

    return g_object_new(gtype, NULL);
}

void
_hrt_event_loop_run(HrtEventLoop *loop)
{
    HRT_EVENT_LOOP_GET_CLASS(loop)->run(loop);
}

void
_hrt_event_loop_quit(HrtEventLoop *loop)
{
    HRT_EVENT_LOOP_GET_CLASS(loop)->quit(loop);
}

HrtWatcher*
_hrt_event_loop_create_idle(HrtEventLoop      *loop,
                            HrtTask           *task,
                            HrtWatcherCallback func,
                            void              *data,
                            GDestroyNotify     dnotify)
{
    return HRT_EVENT_LOOP_GET_CLASS(loop)->create_idle(loop, task, func, data, dnotify);
}

HrtWatcher*
_hrt_event_loop_create_io(HrtEventLoop       *loop,
                          HrtTask            *task,
                          int                 fd,
                          HrtWatcherFlags     io_flags,
                          HrtWatcherCallback  func,
                          void               *data,
                          GDestroyNotify      dnotify)
{
    return HRT_EVENT_LOOP_GET_CLASS(loop)->create_io(loop, task, fd, io_flags,
                                                     func, data, dnotify);
}

void
_hrt_event_loop_wait_running(HrtEventLoop *loop,
                             gboolean      is_running)
{
    g_mutex_lock(loop->running_lock);
    while (loop->is_running != (is_running != FALSE)) {
        g_cond_wait(loop->running_cond, loop->running_lock);
    }
    g_mutex_unlock(loop->running_lock);
}

void
_hrt_event_loop_set_running (HrtEventLoop       *loop,
                             gboolean            is_running)
{
    g_mutex_lock(loop->running_lock);
    loop->is_running = is_running;
    g_cond_signal(loop->running_cond);
    g_mutex_unlock(loop->running_lock);
}
