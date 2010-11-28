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

#ifndef __HJS_SPIDERMONKEY_PRIVATE_H__
#define __HJS_SPIDERMONKEY_PRIVATE_H__

#include <hjs/hjs-runtime-spidermonkey.h>
#include <hjs/hjs-script-spidermonkey.h>
#include <jsapi.h>

G_BEGIN_DECLS

typedef struct {
    HjsRuntimeSpidermonkey *runtime;
    JSContext *context;
    JSObject *global_proto;
} ThreadContext;

HjsScriptSpidermonkey*   _hjs_script_spidermonkey_new                  (HjsRuntimeSpidermonkey  *runtime_spidermonkey);
gboolean                 _hjs_script_spidermonkey_compile_script       (HjsScriptSpidermonkey   *script_spidermonkey,
                                                                        ThreadContext             *thread_context,
                                                                        const char                *filename,
                                                                        const char                *contents,
                                                                        gsize                      len,
                                                                        GError                   **error);
ThreadContext*           _hjs_runtime_spidermonkey_get_main_context    (HjsRuntimeSpidermonkey  *runtime_spidermonkey);
ThreadContext*           _hjs_runtime_spidermonkey_context_new         (HjsRuntimeSpidermonkey  *runtime_spidermonkey);
void                     _hjs_runtime_spidermonkey_context_detach      (void                      *data);

G_END_DECLS

#endif  /* __HJS_SPIDERMONKEY_PRIVATE_H__ */
