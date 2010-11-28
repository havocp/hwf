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
#include <hrt/hrt-event-loop-glib.h>
#include <hrt/hrt-task-private.h>
#include <hrt/hrt-log.h>
#include <hrt/hrt-builtins.h>
#include <hrt/hrt-marshalers.h>

typedef struct {
    HrtWatcher base;

    GSource *source;
} HrtWatcherGLib;

typedef struct {
    HrtWatcherGLib base;
} HrtWatcherIdle;

/* We always wake up watchers on errors, but rely on the app to try to
 * read or write to see that an error occurred.  (Also, to get EOF we
 * need G_IO_ERR or G_IO_HUP not sure which.)
 */
#define HRT_GIO_READ_MASK (G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL)
#define HRT_GIO_WRITE_MASK (G_IO_OUT | G_IO_ERR | G_IO_HUP | G_IO_NVAL)
typedef struct {
    HrtWatcherGLib base;
    GIOChannel *channel;
    GIOCondition condition;
} HrtWatcherIo;

struct HrtEventLoopGLib {
    HrtEventLoop parent_instance;

    GMainContext *context;
    GMainLoop *loop;
};

struct HrtEventLoopGLibClass {
    HrtEventLoopClass parent_class;
};


G_DEFINE_TYPE(HrtEventLoopGLib, hrt_event_loop_glib, HRT_TYPE_EVENT_LOOP);

enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */

static GMainContext*
hrt_watcher_get_g_main_context(HrtWatcher *watcher)
{
    HrtEventLoop *loop;
    loop = _hrt_watcher_get_event_loop(watcher);
    return HRT_EVENT_LOOP_GLIB(loop)->context;
}

static void
hrt_watcher_glib_base_init(HrtWatcherGLib         *gwatcher,
                           const HrtWatcherVTable *vtable,
                           HrtTask                *task,
                           HrtWatcherCallback      func,
                           void                   *data,
                           GDestroyNotify          dnotify)
{
    _hrt_watcher_base_init(&gwatcher->base, vtable, task, func, data, dnotify);
    gwatcher->source = NULL;
}

/* IN EVENT THREAD */
static gboolean
run_watcher_source_func(void *data)
{
    HrtWatcher *watcher = data;

    /* remove from main loop, may be re-added after
     * we invoke (asynchronously)
     */
    _hrt_watcher_stop(watcher);

    /* pass off to invoke threads to run */
    _hrt_watcher_queue_invoke(watcher, HRT_WATCHER_FLAG_NONE);

    /* for now this doesn't really matter because we always re-start
     * the idle.
     */
    return FALSE;
}

/* IN EVENT OR INVOKE THREAD */
static void
hrt_watcher_glib_stop(HrtWatcher *watcher)
{
    HrtWatcherGLib *gwatcher = (HrtWatcherGLib*) watcher;

    if (gwatcher->source != NULL) {
        GSource *source;
        source = gwatcher->source;
        gwatcher->source = NULL;
        g_source_destroy(source);
        g_source_unref(source);
    }
}

static void
watcher_dnotify(void *data)
{
    HrtWatcher *watcher = data;
    _hrt_watcher_unref(watcher);
}

/* IN EVENT OR INVOKE THREAD */
static void
hrt_watcher_idle_start(HrtWatcher *watcher)
{
    HrtWatcherGLib *gwatcher = (HrtWatcherGLib*) watcher;

    if (gwatcher->source == NULL) {
        GSource *source;

        source = g_idle_source_new();

        _hrt_watcher_ref(watcher);
        g_source_set_callback(source, run_watcher_source_func,
                              watcher, watcher_dnotify);
        gwatcher->source = source; /* takes the ref */

        /* When we attach the source, we're addding it to the event
         * thread, and it can IMMEDIATELY RUN so from here, the
         * watcher is subject to re-entrancy even on initial
         * construct.
         */
        g_source_attach(source,
                        hrt_watcher_get_g_main_context(watcher));
    }
}

static void
hrt_watcher_idle_finalize(HrtWatcher *watcher)
{
    HrtWatcherGLib *gwatcher = (HrtWatcherGLib*) watcher;
    g_assert(gwatcher->source == NULL);
    g_slice_free(HrtWatcherIdle, (HrtWatcherIdle*) watcher);
}

static const HrtWatcherVTable idle_vtable = {
    hrt_watcher_idle_start,
    hrt_watcher_glib_stop,
    hrt_watcher_idle_finalize
};

static HrtWatcher*
hrt_event_loop_glib_create_idle(HrtEventLoop      *loop,
                                HrtTask           *task,
                                HrtWatcherCallback func,
                                void              *data,
                                GDestroyNotify     dnotify)
{
    HrtWatcherIdle *idle;

    idle = g_slice_new(HrtWatcherIdle);
    hrt_watcher_glib_base_init(&idle->base,
                               &idle_vtable,
                               task, func, data, dnotify);

    return (HrtWatcher*) idle;
}

static gboolean
run_watcher_io_func(GIOChannel   *channel,
                    GIOCondition  condition,
                    void         *data)
{
    HrtWatcher *watcher = data;
    HrtWatcherIo *iwatcher = (HrtWatcherIo*) watcher;
    HrtWatcherFlags flags;

    /* remove from main loop, may be re-added after
     * we invoke (asynchronously)
     */
    _hrt_watcher_stop(watcher);

    flags = HRT_WATCHER_FLAG_NONE;
    /* careful here not to set write on a read-only watcher
     * or vice versa just because an error flag is set
     */
    if ((condition & HRT_GIO_READ_MASK) &&
        (iwatcher->condition & G_IO_IN))
        flags |= HRT_WATCHER_FLAG_READ;
    if ((condition & HRT_GIO_WRITE_MASK) &&
        (iwatcher->condition & G_IO_OUT))
        flags |= HRT_WATCHER_FLAG_WRITE;

    /* pass off to invoke threads to run */
    _hrt_watcher_queue_invoke(watcher, flags);

    /* for now this doesn't really matter because we always re-add
     * the IO watch.
     */
    return FALSE;
}

/* IN EVENT OR INVOKE THREAD */
static void
hrt_watcher_io_start(HrtWatcher *watcher)
{
    HrtWatcherGLib *gwatcher = (HrtWatcherGLib*) watcher;
    HrtWatcherIo *io_watcher = (HrtWatcherIo*) watcher;

    if (gwatcher->source == NULL) {
        GSource *source;

        source = g_io_create_watch(io_watcher->channel,
                                   io_watcher->condition);

        _hrt_watcher_ref(watcher);
        g_source_set_callback(source, (GSourceFunc) run_watcher_io_func,
                              watcher, watcher_dnotify);
        gwatcher->source = source; /* takes the ref */

        /* When we attach the source, we're addding it to the event
         * thread, and it can IMMEDIATELY RUN so from here, the
         * watcher is subject to re-entrancy even on initial
         * construct.
         */
        g_source_attach(source,
                        hrt_watcher_get_g_main_context(watcher));
    }
}

static void
hrt_watcher_io_finalize(HrtWatcher *watcher)
{
    HrtWatcherGLib *gwatcher = (HrtWatcherGLib*) watcher;
    HrtWatcherIo *io_watcher = (HrtWatcherIo*) watcher;
    g_assert(gwatcher->source == NULL);
    g_io_channel_unref(io_watcher->channel);
    g_slice_free(HrtWatcherIo, (HrtWatcherIo*) watcher);
}

static const HrtWatcherVTable io_vtable = {
    hrt_watcher_io_start,
    hrt_watcher_glib_stop,
    hrt_watcher_io_finalize
};

static HrtWatcher*
hrt_event_loop_glib_create_io(HrtEventLoop      *loop,
                              HrtTask           *task,
                              int                fd,
                              HrtWatcherFlags    flags,
                              HrtWatcherCallback func,
                              void              *data,
                              GDestroyNotify     dnotify)
{
    HrtWatcherIo *io_watcher;

    io_watcher = g_slice_new(HrtWatcherIo);
    io_watcher->channel = g_io_channel_unix_new(fd);
    io_watcher->condition = 0;
    if (flags & HRT_WATCHER_FLAG_READ)
        io_watcher->condition |= HRT_GIO_READ_MASK;
    if (flags & HRT_WATCHER_FLAG_WRITE)
        io_watcher->condition |= HRT_GIO_WRITE_MASK;

    hrt_watcher_glib_base_init(&io_watcher->base,
                               &io_vtable,
                               task, func, data, dnotify);

    return (HrtWatcher*) io_watcher;
}

static gboolean
mark_running(void *data)
{
    HrtEventLoopGLib *gloop = HRT_EVENT_LOOP_GLIB(data);

    _hrt_event_loop_set_running(HRT_EVENT_LOOP(gloop),
                                g_main_loop_is_running(gloop->loop));

    return FALSE;
}

static void
hrt_event_loop_glib_run(HrtEventLoop *loop)
{
    HrtEventLoopGLib *gloop = HRT_EVENT_LOOP_GLIB(loop);
    GSource *source;
    GMainContext *context;

    /* Add a callback at high priority (first thing to run)
     * which marks the loop as running, so people can
     * block in another thread waiting for our loop to be
     * underway.
     */
    context = g_main_loop_get_context(gloop->loop);
    source = g_idle_source_new();
    g_source_set_priority(source, G_PRIORITY_HIGH);
    g_source_set_callback(source, mark_running,
                          g_object_ref(gloop),
                          (GDestroyNotify)g_object_unref);
    g_source_attach(source, context);
    g_source_unref(source);

    /* Now actually run the loop */
    g_main_loop_run(gloop->loop);
}

static gboolean
do_nothing_return_false(void *data)
{
    return FALSE;
}

static void
hrt_event_loop_glib_quit(HrtEventLoop *loop)
{
    HrtEventLoopGLib *gloop = HRT_EVENT_LOOP_GLIB(loop);
    GSource *source;
    GMainContext *context;

    /* g_main_loop_quit() doesn't reliably work from another thread so
     * we install a one-shot idle to be sure it doesn't get wedged in
     * poll(). https://bugzilla.gnome.org/show_bug.cgi?id=632301
     */

    context = g_main_loop_get_context(gloop->loop);
    g_main_context_ref(context);

    g_main_loop_quit(gloop->loop);
    _hrt_event_loop_set_running(HRT_EVENT_LOOP(gloop), FALSE);

    source = g_idle_source_new();
    g_source_set_callback(source, do_nothing_return_false,
                          NULL, NULL);
    g_source_attach(source, context);
    g_source_unref(source);

    g_main_context_unref(context);
}

static void
hrt_event_loop_glib_get_property (GObject                *object,
                                  guint                   prop_id,
                                  GValue                 *value,
                                  GParamSpec             *pspec)
{
    HrtEventLoopGLib *loop;

    loop = HRT_EVENT_LOOP_GLIB(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hrt_event_loop_glib_set_property (GObject                *object,
                                  guint                   prop_id,
                                  const GValue           *value,
                                  GParamSpec             *pspec)
{
    HrtEventLoopGLib *loop;

    loop = HRT_EVENT_LOOP_GLIB(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hrt_event_loop_glib_dispose(GObject *object)
{
    HrtEventLoopGLib *loop;

    loop = HRT_EVENT_LOOP_GLIB(object);

    if (loop->loop) {
        g_main_loop_unref(loop->loop);
        loop->loop = NULL;
    }

    if (loop->context) {
        g_main_context_unref(loop->context);
        loop->context = NULL;
    }

    G_OBJECT_CLASS(hrt_event_loop_glib_parent_class)->dispose(object);
}

static void
hrt_event_loop_glib_finalize(GObject *object)
{
    HrtEventLoopGLib *loop;

    loop = HRT_EVENT_LOOP_GLIB(object);

    G_OBJECT_CLASS(hrt_event_loop_glib_parent_class)->finalize(object);
}

static void
hrt_event_loop_glib_init(HrtEventLoopGLib *loop)
{
    loop->context = g_main_context_new();

    loop->loop = g_main_loop_new(loop->context, FALSE);
}

static void
hrt_event_loop_glib_class_init(HrtEventLoopGLibClass *klass)
{
    GObjectClass *object_class;
    HrtEventLoopClass *event_class;

    object_class = G_OBJECT_CLASS(klass);
    event_class = HRT_EVENT_LOOP_CLASS(klass);

    object_class->get_property = hrt_event_loop_glib_get_property;
    object_class->set_property = hrt_event_loop_glib_set_property;

    object_class->dispose = hrt_event_loop_glib_dispose;
    object_class->finalize = hrt_event_loop_glib_finalize;

    event_class->run = hrt_event_loop_glib_run;
    event_class->quit = hrt_event_loop_glib_quit;
    event_class->create_idle = hrt_event_loop_glib_create_idle;
    event_class->create_io = hrt_event_loop_glib_create_io;
}
