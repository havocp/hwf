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

#ifndef __HJS_RUNTIME_H__
#define __HJS_RUNTIME_H__

#include <glib-object.h>
#include <hrt/hrt-buffer.h>

G_BEGIN_DECLS

typedef struct HjsScript      HjsScript;
typedef struct HjsScriptClass HjsScriptClass;


typedef struct HjsRuntime      HjsRuntime;
typedef struct HjsRuntimeClass HjsRuntimeClass;

struct HjsRuntime {
    GObject parent_instance;
};

struct HjsRuntimeClass {
    GObjectClass parent_class;

    HjsScript* (* compile_script) (HjsRuntime    *runtime,
                                   const char    *filename,
                                   GError       **error);
    HrtBuffer*   (* create_buffer)  (HjsRuntime    *runtime);
};

#define HJS_TYPE_RUNTIME              (hjs_runtime_get_type ())
#define HJS_RUNTIME(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HJS_TYPE_RUNTIME, HjsRuntime))
#define HJS_RUNTIME_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HJS_TYPE_RUNTIME, HjsRuntimeClass))
#define HJS_IS_RUNTIME(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HJS_TYPE_RUNTIME))
#define HJS_IS_RUNTIME_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HJS_TYPE_RUNTIME))
#define HJS_RUNTIME_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HJS_TYPE_RUNTIME, HjsRuntimeClass))

GType           hjs_runtime_get_type                  (void) G_GNUC_CONST;

HjsScript* hjs_runtime_compile_script               (HjsRuntime    *runtime,
                                                     const char    *filename,
                                                     GError       **error);
HrtBuffer*   hjs_runtime_create_buffer                (HjsRuntime    *runtime);

G_END_DECLS

#endif  /* __HJS_RUNTIME_H__ */
