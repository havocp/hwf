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

#ifndef __HJS_SCRIPT_SPIDERMONKEY_H__
#define __HJS_SCRIPT_SPIDERMONKEY_H__

#include <hjs/hjs-script.h>

G_BEGIN_DECLS

typedef struct HjsScriptSpidermonkey      HjsScriptSpidermonkey;
typedef struct HjsScriptSpidermonkeyClass HjsScriptSpidermonkeyClass;

#define HJS_TYPE_SCRIPT_SPIDERMONKEY              (hjs_script_spidermonkey_get_type ())
#define HJS_SCRIPT_SPIDERMONKEY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HJS_TYPE_SCRIPT_SPIDERMONKEY, HjsScriptSpidermonkey))
#define HJS_SCRIPT_SPIDERMONKEY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HJS_TYPE_SCRIPT_SPIDERMONKEY, HjsScriptSpidermonkeyClass))
#define HJS_IS_SCRIPT_SPIDERMONKEY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HJS_TYPE_SCRIPT_SPIDERMONKEY))
#define HJS_IS_SCRIPT_SPIDERMONKEY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HJS_TYPE_SCRIPT_SPIDERMONKEY))
#define HJS_SCRIPT_SPIDERMONKEY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HJS_TYPE_SCRIPT_SPIDERMONKEY, HjsScriptSpidermonkeyClass))

GType           hjs_script_spidermonkey_get_type                  (void) G_GNUC_CONST;


G_END_DECLS

#endif  /* __HJS_SCRIPT_SPIDERMONKEY_H__ */
