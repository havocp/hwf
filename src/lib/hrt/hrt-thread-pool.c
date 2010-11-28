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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO THREAD SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <config.h>
#include <hrt/hrt-thread-pool.h>
#include <hrt/hrt-log.h>
#include <hrt/hrt-builtins.h>
#include <hrt/hrt-marshalers.h>

struct HrtThreadPool {
    GObject      parent_instance;

    const HrtThreadPoolVTable *vtable;
    void                      *vfunc_data;
    GDestroyNotify             vfunc_data_dnotify;

    GAsyncQueue *queue;

    GThread **threads;
    gsize n_threads;

    gboolean shutting_down;
};

struct HrtThreadPoolClass {
    GObjectClass parent_class;
};

G_DEFINE_TYPE(HrtThreadPool, hrt_thread_pool, G_TYPE_OBJECT);

enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */

static void
hrt_thread_pool_dispose(GObject *object)
{
    HrtThreadPool *pool;

    pool = HRT_THREAD_POOL(object);

    hrt_thread_pool_shutdown(pool);

    if (pool->vfunc_data_dnotify != NULL) {

        (* pool->vfunc_data_dnotify) (pool->vfunc_data);
    }

    pool->vtable = NULL;
    pool->vfunc_data = NULL;
    pool->vfunc_data_dnotify = NULL;

    G_OBJECT_CLASS(hrt_thread_pool_parent_class)->dispose(object);
}

static void
hrt_thread_pool_finalize(GObject *object)
{
    HrtThreadPool *pool;

    pool = HRT_THREAD_POOL(object);

    g_assert(pool->threads == NULL);
    g_assert(pool->n_threads == 0);
    g_assert(pool->vtable == NULL);

    g_async_queue_unref(pool->queue);

    G_OBJECT_CLASS(hrt_thread_pool_parent_class)->finalize(object);
}

static void
hrt_thread_pool_init(HrtThreadPool *pool)
{
    pool->queue = g_async_queue_new();
}

static void
hrt_thread_pool_class_init(HrtThreadPoolClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = hrt_thread_pool_dispose;
    object_class->finalize = hrt_thread_pool_finalize;
}

/* just an arbitrary unique valid pointer */
static void* shutting_down_item = (void*) &hrt_thread_pool_class_init;

static void*
hrt_thread_pool_thread(void *data)
{
    HrtThreadPool *pool;
    void *thread_data;

    pool = HRT_THREAD_POOL(data);

    thread_data = (* pool->vtable->thread_data_new) (pool->vfunc_data);

    while (TRUE) {
        void *item;

        item = g_async_queue_pop(pool->queue);
        g_assert(item != NULL);

        if (item == shutting_down_item) {
            /* time to quit */
            break;
        } else {
            (* pool->vtable->handle_item) (thread_data,
                                           item,
                                           pool->vfunc_data);
        }
    }

    (* pool->vtable->thread_data_free) (thread_data, pool->vfunc_data);

    g_object_unref(pool);
    return NULL;
}

static void
create_threads(HrtThreadPool *pool)
{
    gsize i;

    /* on my 2-core system 3 or 4 threads seems to be optimal, with
     * two or five clearly worse. We might end up wanting to look at
     * number of cores and decide.
     */
    pool->n_threads = 4;
    pool->threads = g_new0(GThread*, pool->n_threads);

    for (i = 0; i < pool->n_threads; ++i) {
        GError *error = NULL;
        pool->threads[i] =
            g_thread_create(hrt_thread_pool_thread,
                            g_object_ref(pool),
                            TRUE, /* joinable */
                            &error);
        if (error != NULL) {
            g_error("Failed to create thread: %s", error->message);
        }
    }
}

HrtThreadPool*
hrt_thread_pool_new(const HrtThreadPoolVTable *vtable,
                    void                      *vfunc_data,
                    GDestroyNotify             vfunc_data_dnotify)
{
    HrtThreadPool *pool;

    pool = g_object_new(HRT_TYPE_THREAD_POOL,
                        NULL);
    pool->vtable = vtable;
    pool->vfunc_data = vfunc_data;
    pool->vfunc_data_dnotify = vfunc_data_dnotify;

    create_threads(pool);

    return pool;
}

typedef struct {
    GFunc          handler;
    void          *handler_data;
    GDestroyNotify handler_data_dnotify;
} Handler;

static void
handler_free(void *data)
{
    Handler *handler = data;

    if (handler->handler_data_dnotify) {
        (* handler->handler_data_dnotify) (handler->handler_data);
    }

    g_slice_free(Handler, handler);
}

static Handler*
handler_new(GFunc                      handler_func,
            void                      *handler_data,
            GDestroyNotify             handler_data_dnotify)
{
    Handler *handler;

    handler = g_slice_new(Handler);

    handler->handler = handler_func;
    handler->handler_data = handler_data;
    handler->handler_data_dnotify = handler_data_dnotify;

    return handler;
}

static void*
handler_thread_data_new(void *vfunc_data)
{
    return NULL;
}

static void
handler_handle_item (void *thread_data,
                     void *item,
                     void *vfunc_data)
{
    Handler *handler;

    handler = vfunc_data;

    (* handler->handler) (item, handler->handler_data);
}

static void
handler_thread_data_free(void *thread_data,
                         void *vfunc_data)
{
    /* nothing */
}

static const HrtThreadPoolVTable handler_vtable = {
    handler_thread_data_new,
    handler_handle_item,
    handler_thread_data_free
};

HrtThreadPool*
hrt_thread_pool_new_func (GFunc                      handler_func,
                          void                      *handler_data,
                          GDestroyNotify             handler_data_dnotify)
{
    Handler *handler;

    handler = handler_new(handler_func, handler_data,
                          handler_data_dnotify);

    return hrt_thread_pool_new(&handler_vtable,
                               handler,
                               handler_free);
}

void
hrt_thread_pool_shutdown(HrtThreadPool *pool)
{
    gsize i;

    g_return_if_fail(HRT_IS_THREAD_POOL(pool));

    if (pool->n_threads == 0)
        return;

    /* Mark that threads should exit when nothing left in queue */
    pool->shutting_down = TRUE;

    /* push a special item to tell threads to exit.  Threads will not
     * pop anything else once they get this special item, so each
     * thread should pop just one of the special items and then quit.
     * These are the last items in the queue, so all real items have
     * to get processed before threads will quit.
     */
    for (i = 0; i < pool->n_threads; ++i) {
        g_async_queue_push(pool->queue, shutting_down_item);
    }

    /* now close down */
    for (i = 0; i < pool->n_threads; ++i) {
        g_thread_join(pool->threads[i]);
    }

    g_free(pool->threads);
    pool->threads = NULL;
    pool->n_threads = 0;
}

/* "item" must be a valid pointer... we rely on being able to use
 * magic valid pointers that won't conflict with application's
 * pointers.
 */
void
hrt_thread_pool_push(HrtThreadPool *pool,
                     void          *item)
{
    g_return_if_fail(HRT_IS_THREAD_POOL(pool));
    g_return_if_fail(item != NULL);
    g_return_if_fail(!pool->shutting_down);
    g_return_if_fail(pool->n_threads > 0);

    g_async_queue_push(pool->queue, item);
}
