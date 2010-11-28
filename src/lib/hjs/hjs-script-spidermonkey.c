/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2010 Havoc Pennington
 * Copyright (c) 2009 litl, LLC (code from open source gjs project)
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
#include <hjs/hjs-script-spidermonkey.h>
#include <hjs/hjs-spidermonkey-private.h>
#include <hrt/hrt-log.h>

struct HjsScriptSpidermonkey {
    HjsScript parent_instance;

    HjsRuntimeSpidermonkey *runtime;
    jsval script_obj;
};

struct HjsScriptSpidermonkeyClass {
    HjsScriptClass parent_class;
};

G_DEFINE_TYPE(HjsScriptSpidermonkey, hjs_script_spidermonkey, HJS_TYPE_SCRIPT);

static JSClass script_context_global_class = {
    "HjsScriptContextGlobal",
    JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static gboolean
on_invoke_script(HrtTask        *task,
                 HrtWatcherFlags flags,
                 void           *data)
{
    ThreadContext *thread_context;
    HjsScriptSpidermonkey *script_spidermonkey;
    jsval rval;
    JSScript *js_script;
    JSObject *global;

    script_spidermonkey = HJS_SCRIPT_SPIDERMONKEY(data);

    /* we store the thread's context under the runtime pointer */
    thread_context = hrt_task_get_thread_local(task,
                                               script_spidermonkey->runtime);
    if (thread_context == NULL) {
        /* create and store the thread context if none */
        thread_context = _hjs_runtime_spidermonkey_context_new(script_spidermonkey->runtime);
        hrt_task_set_thread_local(task, script_spidermonkey->runtime,
                                  thread_context,
                                  _hjs_runtime_spidermonkey_context_detach);
    }

    JS_BeginRequest(thread_context->context);

    /* Create global object for the script.
     * This global should inherit compartment from the global_proto
     */
    global = JS_NewGlobalObject(thread_context->context, &script_context_global_class);
    if (global == NULL)
        g_error("Failed to create global object to invoke script");
    JS_AddNamedObjectRoot(thread_context->context, &global, G_STRLOC);

    if (!JS_SetPrototype(thread_context->context,
                         global,
                         thread_context->global_proto))
        g_error("Failed to set prototype on global object");

    js_script = JS_GetPrivate(thread_context->context,
                              JSVAL_TO_OBJECT(script_spidermonkey->script_obj));

    g_assert(JS_GetGlobalObject(thread_context->context) == thread_context->global_proto);

    JS_SetGlobalObject(thread_context->context, global);

    /* This assertion checks that when we set global object, we didn't
     * already have a scope chain. If we already had a chain then
     * setting the global would not do anything.
     */
    g_assert(JS_GetScopeChain(thread_context->context) == global);

    JS_ExecuteScript(thread_context->context,
                     global,
                     js_script,
                     &rval);

    /* Put global_proto back as the global object for next time */
    JS_SetGlobalObject(thread_context->context, thread_context->global_proto);
    g_assert(JS_GetScopeChain(thread_context->context) == thread_context->global_proto);

    /* the global object may still stick around if the script created callbacks
     * that will reference it, but we don't need to keep it here.
     */
    JS_RemoveObjectRoot(thread_context->context, &global);

    JS_EndRequest(thread_context->context);

    return FALSE;
}

static void
hjs_script_spidermonkey_run_in_task(HjsScript     *script,
                                    HrtTask       *task,
                                    const char   **names,
                                    const GValue  *values)
{
    g_object_ref(script);
    hrt_task_add_immediate(task,
                           on_invoke_script,
                           script, (GDestroyNotify) g_object_unref);
}

static void
hjs_script_spidermonkey_get_property (GObject                *object,
                                      guint                   prop_id,
                                      GValue                 *value,
                                      GParamSpec             *pspec)
{
    HjsScriptSpidermonkey *script_spidermonkey;

    script_spidermonkey = HJS_SCRIPT_SPIDERMONKEY(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hjs_script_spidermonkey_set_property (GObject                *object,
                                      guint                   prop_id,
                                      const GValue           *value,
                                      GParamSpec             *pspec)
{
    HjsScriptSpidermonkey *script_spidermonkey;

    script_spidermonkey = HJS_SCRIPT_SPIDERMONKEY(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hjs_script_spidermonkey_dispose(GObject *object)
{
    HjsScriptSpidermonkey *script_spidermonkey;

    script_spidermonkey = HJS_SCRIPT_SPIDERMONKEY(object);


    G_OBJECT_CLASS(hjs_script_spidermonkey_parent_class)->dispose(object);
}

static void
hjs_script_spidermonkey_finalize(GObject *object)
{
    HjsScriptSpidermonkey *script_spidermonkey;

    script_spidermonkey = HJS_SCRIPT_SPIDERMONKEY(object);

    if (script_spidermonkey->runtime != NULL) {
        ThreadContext *thread_context;

        thread_context = _hjs_runtime_spidermonkey_get_main_context(script_spidermonkey->runtime);

        JS_BeginRequest(thread_context->context);
        JS_RemoveValueRoot(thread_context->context,
                           &script_spidermonkey->script_obj);
        JS_EndRequest(thread_context->context);

        g_object_unref(script_spidermonkey->runtime);
        script_spidermonkey->runtime = NULL;
    }

    G_OBJECT_CLASS(hjs_script_spidermonkey_parent_class)->finalize(object);
}

static void
hjs_script_spidermonkey_init(HjsScriptSpidermonkey *script_spidermonkey)
{
    script_spidermonkey->script_obj = JSVAL_VOID;
}

static void
hjs_script_spidermonkey_class_init(HjsScriptSpidermonkeyClass *klass)
{
    GObjectClass *object_class;
    HjsScriptClass *script_class;

    object_class = G_OBJECT_CLASS(klass);
    script_class = HJS_SCRIPT_CLASS(klass);

    object_class->get_property = hjs_script_spidermonkey_get_property;
    object_class->set_property = hjs_script_spidermonkey_set_property;

    object_class->dispose = hjs_script_spidermonkey_dispose;
    object_class->finalize = hjs_script_spidermonkey_finalize;

    script_class->run_in_task = hjs_script_spidermonkey_run_in_task;
}

HjsScriptSpidermonkey*
_hjs_script_spidermonkey_new(HjsRuntimeSpidermonkey *runtime_spidermonkey)
{
    HjsScriptSpidermonkey *script_spidermonkey;

    script_spidermonkey = g_object_new(HJS_TYPE_SCRIPT_SPIDERMONKEY, NULL);

    g_assert(script_spidermonkey->runtime == NULL);

    script_spidermonkey->runtime = runtime_spidermonkey;
    g_object_ref(script_spidermonkey->runtime);

    return script_spidermonkey;
}

gboolean
_hjs_script_spidermonkey_compile_script(HjsScriptSpidermonkey   *script_spidermonkey,
                                        ThreadContext           *thread_context,
                                        const char              *filename,
                                        const char              *contents,
                                        gsize                    len,
                                        GError                 **error)
{
    JSScript *js_script;
    gboolean success;

    g_return_val_if_fail (script_spidermonkey->script_obj == JSVAL_VOID, FALSE);

    success = FALSE;

    JS_BeginRequest(thread_context->context);

    JS_AddNamedValueRoot(thread_context->context,
                         &script_spidermonkey->script_obj,
                         G_STRLOC);

    js_script = JS_CompileScript(thread_context->context, NULL,
                                 contents, len, filename, 0);
    if (js_script == NULL)
        goto error;

    script_spidermonkey->script_obj = OBJECT_TO_JSVAL(JS_NewScriptObject(thread_context->context, js_script));
    if (!JSVAL_IS_OBJECT(script_spidermonkey->script_obj))
        goto error;

    success = TRUE;

 error:
    JS_EndRequest(thread_context->context);

    if (!success) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                    "Failed to compile script %s", filename);
    }

    return success;
}
