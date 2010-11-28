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

#ifndef __HRT_THREAD_POOL_H__
#define __HRT_THREAD_POOL_H__

#include <glib-object.h>
#include <hrt/hrt-task-runner.h>

G_BEGIN_DECLS

typedef struct {
    void* (* thread_data_new)  (void *vfunc_data);
    void  (* handle_item)      (void *thread_data,
                                void *item,
                                void *vfunc_data);
    void  (* thread_data_free) (void *thread_data,
                                void *vfunc_data);
} HrtThreadPoolVTable;

typedef struct HrtThreadPool      HrtThreadPool;
typedef struct HrtThreadPoolClass HrtThreadPoolClass;

#define HRT_TYPE_THREAD_POOL              (hrt_thread_pool_get_type ())
#define HRT_THREAD_POOL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HRT_TYPE_THREAD_POOL, HrtThreadPool))
#define HRT_THREAD_POOL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HRT_TYPE_THREAD_POOL, HrtThreadPoolClass))
#define HRT_IS_THREAD_POOL(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HRT_TYPE_THREAD_POOL))
#define HRT_IS_THREAD_POOL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HRT_TYPE_THREAD_POOL))
#define HRT_THREAD_POOL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HRT_TYPE_THREAD_POOL, HrtThreadPoolClass))

GType           hrt_thread_pool_get_type (void) G_GNUC_CONST;

HrtThreadPool* hrt_thread_pool_new      (const HrtThreadPoolVTable *vtable,
                                         void                      *vfunc_data,
                                         GDestroyNotify             vfunc_data_dnotify);
HrtThreadPool* hrt_thread_pool_new_func (GFunc                      handler_func,
                                         void                      *handler_data,
                                         GDestroyNotify             handler_data_dnotify);
void           hrt_thread_pool_shutdown (HrtThreadPool             *pool);
void           hrt_thread_pool_push     (HrtThreadPool             *pool,
                                         void                      *item);

G_END_DECLS

#endif  /* __HRT_THREAD_POOL_H__ */
