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
#include <hrt/hrt-event-loop-ev.h>
#include <hrt/hrt-task-private.h>
#include <hrt/hrt-log.h>
#include <hrt/hrt-builtins.h>
#include <hrt/hrt-marshalers.h>

/* use a loop struct instead of global vars */
#define EV_MULTIPLICITY 1
/* source types we don't need */
#define EV_EMBED_ENABLE 0
#define EV_STAT_ENABLE 0
#define EV_CHECK_ENABLE 0
#define EV_FORK_ENABLE 0
#define EV_SIGNAL_ENABLE 0
#define EV_CHILD_ENABLE 0

/* Any fields we want in all watchers */
#define EV_COMMON unsigned int type_magic;

#define TYPE_MAGIC_ASYNC          2
#define TYPE_MAGIC_IDLE           4
#define TYPE_MAGIC_IO             8
#define TYPE_MAGIC_WATCH_KIND_MASK (2 | 4 | 8)
/* TYPE_MAGIC_NOTIFY_RUNNING watcher used to detect we're running now */
#define TYPE_MAGIC_NOTIFY_RUNNING 16
/* TYPE_MAGIC_HRT_WATCHER means it's embedded in a HrtWatcher */
#define TYPE_MAGIC_HRT_WATCHER    32

/* set libev to have no callbacks of its own */
struct ev_loop;
struct ev_watcher;
static void hrt_invoke_ev_watcher(struct ev_loop    *loop,
                                  struct ev_watcher *ewatcher,
                                  int                revents);
#define EV_CB_DECLARE(type)
#define EV_CB_INVOKE(ewatcher, revents) hrt_invoke_ev_watcher(loop, ewatcher, revents)
#define ev_set_cb(ewatcher, cb)

/* Unfortunately this file is a giant warning-fest.  These pragmas
 * can't strictly be used this way; some warnings are computed at the
 * end of the file or during optimization it seems, so ignoring then
 * re-enabling doesn't really quite work. But it does seem to mostly
 * work.
 */
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wnested-externs"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-result"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wimplicit-function-declaration"
/* not declared before use in ev.c */
void ev_feed_signal_event (struct ev_loop *loop, int signum);

#include <deps/libev/ev.c>

/* Use unused functions */
static void
call_unused_functions_from_libev(void)
{
    if (G_LIKELY((1 + 1) == 2))
        return;

    g_assert_not_reached();

    pendingcb(NULL, NULL, 0);
    ev_sighandler(0);
    sigfdcb(NULL, NULL, 0);
    once_cb_to(NULL, NULL, 0);
    once_cb_io(NULL, NULL, 0);
}

/* Try to re-enable warnings, with caveats noted above */
#pragma GCC diagnostic warning "-Wfloat-equal"
#pragma GCC diagnostic warning "-Wmissing-prototypes"
#pragma GCC diagnostic warning "-Wnested-externs"
#pragma GCC diagnostic warning "-Wparentheses"
#pragma GCC diagnostic warning "-Wsign-compare"
#pragma GCC diagnostic warning "-Wunused-function"
#pragma GCC diagnostic warning "-Wunused-result"
#pragma GCC diagnostic warning "-Wunused-value"
#pragma GCC diagnostic warning "-Wunused-variable"
#pragma GCC diagnostic warning "-Wimplicit-function-declaration"

typedef struct {
    HrtWatcher base;
    /* no generic fields for now.
     * the libev ev_watcher must always be
     * the first thing in derived watchers,
     * to support HRT_WATCHER_FROM_EV_WATCHER
     */
} HrtWatcherEv;

/* used to compute HRT_WATCHER_FROM_EV_WATCHER */
typedef struct {
    HrtWatcherEv base;
    ev_idle dummy;
} HrtWatcherDummy;

#define HRT_WATCHER_FROM_EV_WATCHER(ev_w) ((HrtWatcher*) (((char*)ev_w) - G_STRUCT_OFFSET(HrtWatcherDummy, dummy)))

typedef struct {
    HrtWatcherEv base;
    ev_idle idle;
} HrtWatcherIdle;

typedef struct {
    HrtWatcherEv base;
    ev_io io;
} HrtWatcherIo;

struct HrtEventLoopEv {
    HrtEventLoop parent_instance;

    GMutex *loop_lock;
    struct ev_loop *loop;
    ev_async loop_wakeup;
};

struct HrtEventLoopEvClass {
    HrtEventLoopClass parent_class;
};


G_DEFINE_TYPE(HrtEventLoopEv, hrt_event_loop_ev, HRT_TYPE_EVENT_LOOP);

enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */

static void
hrt_release_ev_loop(struct ev_loop *loop)
{
    g_mutex_unlock(ev_userdata(loop));
}

static void
hrt_acquire_ev_loop(struct ev_loop *loop)
{
    g_mutex_lock(ev_userdata(loop));
}

static void
hrt_event_loop_ev_wakeup(HrtEventLoopEv *event_loop)
{
    /* assumes we already have loop_lock ! */
    ev_async_send(event_loop->loop,
                  &event_loop->loop_wakeup);
}

static void
hrt_watcher_ev_base_init(HrtWatcherEv           *ewatcher,
                         const HrtWatcherVTable *vtable,
                         HrtTask                *task,
                         HrtWatcherCallback      func,
                         void                   *data,
                         GDestroyNotify          dnotify)
{
    _hrt_watcher_base_init(&ewatcher->base, vtable, task, func, data, dnotify);
}

/* IN EVENT OR INVOKE THREAD */
static void
hrt_watcher_idle_stop(HrtWatcher *watcher)
{
    HrtWatcherIdle *iwatcher = (HrtWatcherIdle*) watcher;
    HrtEventLoopEv *event_loop;

    event_loop = HRT_EVENT_LOOP_EV(_hrt_watcher_get_event_loop(watcher));

    g_mutex_lock(event_loop->loop_lock);
    if (ev_is_active(&iwatcher->idle)) {
        ev_idle_stop(event_loop->loop,
                     &iwatcher->idle);
        hrt_event_loop_ev_wakeup(event_loop);
    }
    g_mutex_unlock(event_loop->loop_lock);
}

/* IN EVENT OR INVOKE THREAD */
static void
hrt_watcher_idle_start(HrtWatcher *watcher)
{
    HrtWatcherIdle *iwatcher = (HrtWatcherIdle*) watcher;
    HrtEventLoopEv *event_loop;

    event_loop = HRT_EVENT_LOOP_EV(_hrt_watcher_get_event_loop(watcher));

    g_mutex_lock(event_loop->loop_lock);
    if (!ev_is_active(&iwatcher->idle)) {
        ev_idle_start(event_loop->loop,
                      &iwatcher->idle);
        hrt_event_loop_ev_wakeup(event_loop);
    }
    g_mutex_unlock(event_loop->loop_lock);
}

static void
hrt_watcher_idle_finalize(HrtWatcher *watcher)
{
    HrtWatcherIdle *iwatcher = (HrtWatcherIdle*) watcher;
    g_assert(!ev_is_active(&iwatcher->idle));
    g_slice_free(HrtWatcherIdle, (HrtWatcherIdle*) watcher);
}

static const HrtWatcherVTable idle_vtable = {
    hrt_watcher_idle_start,
    hrt_watcher_idle_stop,
    hrt_watcher_idle_finalize
};

static HrtWatcher*
hrt_event_loop_ev_create_idle(HrtEventLoop      *loop,
                              HrtTask           *task,
                              HrtWatcherCallback func,
                              void              *data,
                              GDestroyNotify     dnotify)
{
    HrtWatcherIdle *idle;

    idle = g_slice_new(HrtWatcherIdle);
    hrt_watcher_ev_base_init(&idle->base,
                             &idle_vtable,
                             task, func, data, dnotify);

    ev_idle_init(&idle->idle, NULL);
    idle->idle.type_magic = TYPE_MAGIC_IDLE | TYPE_MAGIC_HRT_WATCHER;

    return (HrtWatcher*) idle;
}

/* IN EVENT OR INVOKE THREAD */
static void
hrt_watcher_io_stop(HrtWatcher *watcher)
{
    HrtWatcherIo *iwatcher = (HrtWatcherIo*) watcher;
    HrtEventLoopEv *event_loop;

    event_loop = HRT_EVENT_LOOP_EV(_hrt_watcher_get_event_loop(watcher));

    g_mutex_lock(event_loop->loop_lock);
    if (ev_is_active(&iwatcher->io)) {
        ev_io_stop(event_loop->loop,
                   &iwatcher->io);
        hrt_event_loop_ev_wakeup(event_loop);
    }
    g_mutex_unlock(event_loop->loop_lock);
}

/* IN EVENT OR INVOKE THREAD */
static void
hrt_watcher_io_start(HrtWatcher *watcher)
{
    HrtWatcherIo *iwatcher = (HrtWatcherIo*) watcher;
    HrtEventLoopEv *event_loop;

    event_loop = HRT_EVENT_LOOP_EV(_hrt_watcher_get_event_loop(watcher));

    g_mutex_lock(event_loop->loop_lock);
    if (!ev_is_active(&iwatcher->io)) {
        ev_io_start(event_loop->loop,
                    &iwatcher->io);
        hrt_event_loop_ev_wakeup(event_loop);
    }
    g_mutex_unlock(event_loop->loop_lock);
}

static void
hrt_watcher_io_finalize(HrtWatcher *watcher)
{
    HrtWatcherIo *iwatcher = (HrtWatcherIo*) watcher;
    g_assert(!ev_is_active(&iwatcher->io));
    g_slice_free(HrtWatcherIo, (HrtWatcherIo*) watcher);
}

static const HrtWatcherVTable io_vtable = {
    hrt_watcher_io_start,
    hrt_watcher_io_stop,
    hrt_watcher_io_finalize
};

static HrtWatcher*
hrt_event_loop_ev_create_io(HrtEventLoop      *loop,
                            HrtTask           *task,
                            int                fd,
                            HrtWatcherFlags    flags,
                            HrtWatcherCallback func,
                            void              *data,
                            GDestroyNotify     dnotify)
{
    HrtWatcherIo *io;
    int ev_flags;

    io = g_slice_new(HrtWatcherIo);
    hrt_watcher_ev_base_init(&io->base,
                             &io_vtable,
                             task, func, data, dnotify);

    ev_flags = 0;
    if (flags & HRT_WATCHER_FLAG_READ)
        ev_flags |= EV_READ;
    if (flags & HRT_WATCHER_FLAG_WRITE)
        ev_flags |= EV_WRITE;
    ev_io_init(&io->io, NULL, fd, ev_flags);
    io->io.type_magic = TYPE_MAGIC_IO | TYPE_MAGIC_HRT_WATCHER;

    return (HrtWatcher*) io;
}

typedef struct {
    struct ev_prepare prepare;
    HrtEventLoopEv *eloop;
} NotifyRunning;

static void
handle_notify_running(NotifyRunning *notify)
{
    _hrt_event_loop_set_running(HRT_EVENT_LOOP(notify->eloop),
                                TRUE);

    ev_prepare_stop(notify->eloop->loop, &notify->prepare);

    g_object_unref(notify->eloop);

    g_slice_free(NotifyRunning, notify);
}

static void
hrt_event_loop_ev_run(HrtEventLoop *loop)
{
    HrtEventLoopEv *eloop = HRT_EVENT_LOOP_EV(loop);
    NotifyRunning *notify;

    hrt_acquire_ev_loop(eloop->loop);

    /* Set up notification once the loop is underway */
    notify = g_slice_new0(NotifyRunning);
    ev_prepare_init(&notify->prepare, 0);
    notify->prepare.type_magic = TYPE_MAGIC_NOTIFY_RUNNING;
    notify->eloop = g_object_ref(eloop);
    ev_prepare_start(eloop->loop, &notify->prepare);

    /* Now start loop */
    ev_loop(eloop->loop, 0);

    hrt_release_ev_loop(eloop->loop);
}

static void
hrt_event_loop_ev_quit(HrtEventLoop *loop)
{
    HrtEventLoopEv *eloop = HRT_EVENT_LOOP_EV(loop);

    hrt_acquire_ev_loop(eloop->loop);

    ev_unloop(eloop->loop, EVUNLOOP_ONE);

    _hrt_event_loop_set_running(HRT_EVENT_LOOP(eloop),
                                FALSE);

    hrt_event_loop_ev_wakeup(eloop); /* needed? think so */

    /* libev does not quit while there are still watchers, it seems we
     * have to remove the async watcher to get it to quit.
     */
    if (ev_is_active(&eloop->loop_wakeup)) {
        ev_async_stop(eloop->loop, &eloop->loop_wakeup);
    }
    hrt_release_ev_loop(eloop->loop);
}

/* IN EVENT THREAD */
static void
hrt_invoke_ev_watcher(struct ev_loop    *loop,
                      struct ev_watcher *ewatcher,
                      int                revents)
{
    HrtWatcher *watcher;
    HrtWatcherFlags flags;

    /* === This is called with the loop lock held ===
     * which is different from GMainLoop
     */

    if (!(ewatcher->type_magic & TYPE_MAGIC_HRT_WATCHER)) {
        if (ewatcher->type_magic == 0) {
            /* Relying on scary libev internals here.
             * libev creates one watcher implicitly,
             * which is an IO watch on a pipe that it
             * uses to implement async watchers.
             * This internal watcher should be the only
             * watcher with type_magic of 0 since
             * we didn't create it. And its callback is called
             * pipecb().
             */
            pipecb(loop, (ev_io*) ewatcher, revents);
        } else if (ewatcher->type_magic & TYPE_MAGIC_ASYNC) {
            /* this is probably the loop_wakeup async watcher */
        } else if (ewatcher->type_magic & TYPE_MAGIC_NOTIFY_RUNNING) {
            /* notification that loop is underway */
            handle_notify_running((NotifyRunning*) ewatcher);
        } else {
            /* WTF */
            g_assert_not_reached();
        }
        return;
    }

    watcher = HRT_WATCHER_FROM_EV_WATCHER(ewatcher);

    /* loop lock is already held... stop watcher
     * for now. We don't have to wakeup loop
     * since we know we aren't in a poll in this
     * event thread.
     */
    {
        /* FIXME use a vtable here */
        switch ((ewatcher->type_magic & TYPE_MAGIC_WATCH_KIND_MASK)) {
        case TYPE_MAGIC_IDLE: {
            HrtWatcherIdle *iwatcher = (HrtWatcherIdle*) watcher;
            if (ev_is_active(&iwatcher->idle)) {
                ev_idle_stop(loop,
                             &iwatcher->idle);
            }
        }
            break;
        case TYPE_MAGIC_IO: {
            HrtWatcherIo *iwatcher = (HrtWatcherIo*) watcher;
            if (ev_is_active(&iwatcher->io)) {
                ev_io_stop(loop,
                           &iwatcher->io);
            }
        }
            break;
        }
    }

    flags = HRT_WATCHER_FLAG_NONE;
    if (revents & EV_READ)
        flags |= HRT_WATCHER_FLAG_READ;
    if (revents & EV_WRITE)
        flags |= HRT_WATCHER_FLAG_WRITE;

    if (G_UNLIKELY(revents & EV_ERROR))
        g_warning("libev set EV_ERROR flag which is supposed to mean a bug in the program");

    /* pass off to invoke threads to run */
    _hrt_watcher_queue_invoke(watcher, flags);
}

static void
hrt_event_loop_ev_get_property (GObject                *object,
                                guint                   prop_id,
                                GValue                 *value,
                                GParamSpec             *pspec)
{
    HrtEventLoopEv *loop;

    loop = HRT_EVENT_LOOP_EV(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hrt_event_loop_ev_set_property (GObject                *object,
                                guint                   prop_id,
                                const GValue           *value,
                                GParamSpec             *pspec)
{
    HrtEventLoopEv *loop;

    loop = HRT_EVENT_LOOP_EV(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hrt_event_loop_ev_dispose(GObject *object)
{
    HrtEventLoopEv *loop;

    loop = HRT_EVENT_LOOP_EV(object);

    g_mutex_lock(loop->loop_lock);

    if (loop->loop) {
        if (ev_is_active(&loop->loop_wakeup)) {
            ev_async_stop(loop->loop, &loop->loop_wakeup);
        }
        ev_loop_destroy(loop->loop);
        loop->loop = NULL;
    }

    g_mutex_unlock(loop->loop_lock);

    G_OBJECT_CLASS(hrt_event_loop_ev_parent_class)->dispose(object);
}

static void
hrt_event_loop_ev_finalize(GObject *object)
{
    HrtEventLoopEv *loop;

    loop = HRT_EVENT_LOOP_EV(object);

    g_mutex_free(loop->loop_lock);

    G_OBJECT_CLASS(hrt_event_loop_ev_parent_class)->finalize(object);
}

static void
hrt_event_loop_ev_init(HrtEventLoopEv *loop)
{
    loop->loop_lock = g_mutex_new();
    loop->loop = ev_loop_new(EVFLAG_AUTO);

    ev_async_init(&loop->loop_wakeup, NULL);
    loop->loop_wakeup.type_magic = TYPE_MAGIC_ASYNC;
    ev_async_start(loop->loop, &loop->loop_wakeup);

    ev_set_userdata(loop->loop,
                    loop->loop_lock);
    ev_set_loop_release_cb(loop->loop,
                           hrt_release_ev_loop,
                           hrt_acquire_ev_loop);
}

static void
hrt_event_loop_ev_class_init(HrtEventLoopEvClass *klass)
{
    GObjectClass *object_class;
    HrtEventLoopClass *event_class;

    /* Hack to chill out libev warnings */
    call_unused_functions_from_libev();

    object_class = G_OBJECT_CLASS(klass);
    event_class = HRT_EVENT_LOOP_CLASS(klass);

    object_class->get_property = hrt_event_loop_ev_get_property;
    object_class->set_property = hrt_event_loop_ev_set_property;

    object_class->dispose = hrt_event_loop_ev_dispose;
    object_class->finalize = hrt_event_loop_ev_finalize;

    event_class->run = hrt_event_loop_ev_run;
    event_class->quit = hrt_event_loop_ev_quit;
    event_class->create_idle = hrt_event_loop_ev_create_idle;
    event_class->create_io = hrt_event_loop_ev_create_io;
}
