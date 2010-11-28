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

#ifndef __HJS_SCRIPT_H__
#define __HJS_SCRIPT_H__

#include <glib.h>

#include <hjs/hjs-runtime.h>
#include <hrt/hrt-task.h>

G_BEGIN_DECLS

/* HjsScript forward-declared in hjs-runtime.h header */

struct HjsScript {
    GObject parent_instance;
};

struct HjsScriptClass {
    GObjectClass parent_class;

    void     (* run_in_task)    (HjsScript     *script,
                                 HrtTask       *task,
                                 const char   **names,
                                 const GValue  *values);
};

#define HJS_TYPE_SCRIPT              (hjs_script_get_type ())
#define HJS_SCRIPT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HJS_TYPE_SCRIPT, HjsScript))
#define HJS_SCRIPT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HJS_TYPE_SCRIPT, HjsScriptClass))
#define HJS_IS_SCRIPT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HJS_TYPE_SCRIPT))
#define HJS_IS_SCRIPT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HJS_TYPE_SCRIPT))
#define HJS_SCRIPT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HJS_TYPE_SCRIPT, HjsScriptClass))

GType           hjs_script_get_type                  (void) G_GNUC_CONST;

void         hjs_script_run_in_task              (HjsScript     *script,
                                                  HrtTask       *task);
void         hjs_script_run_in_task_with_values  (HjsScript     *script,
                                                  HrtTask       *task,
                                                  const char   **names,
                                                  const GValue  *values);


G_END_DECLS

#endif  /* __HJS_SCRIPT_H__ */
