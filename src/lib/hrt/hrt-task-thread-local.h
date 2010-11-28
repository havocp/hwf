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

#ifndef __HRT_TASK_THREAD_LOCAL_H__
#define __HRT_TASK_THREAD_LOCAL_H__

/*
 * A HrtTaskThreadLocal is an internal object shared among
 * HrtTaskRunner and HrtTask used to implement thread-local data
 * accessible from inside task invocation threads.
 */

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct HrtTaskThreadLocal HrtTaskThreadLocal;

HrtTaskThreadLocal* _hrt_task_thread_local_new  (void);
void                _hrt_task_thread_local_free (HrtTaskThreadLocal *thread_local);
void*               _hrt_task_thread_local_get  (HrtTaskThreadLocal *thread_local,
                                                 void               *key);
void                _hrt_task_thread_local_set  (HrtTaskThreadLocal *thread_local,
                                                 void               *key,
                                                 void               *value,
                                                 GDestroyNotify      dnotify);

G_END_DECLS

#endif  /* __HRT_TASK_THREAD_LOCAL_H__ */
