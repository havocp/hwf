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
#include <hjs/hjs-runtime-spidermonkey.h>
#include <hjs/hjs-spidermonkey-private.h>
#include <hrt/hrt-log.h>

struct HjsRuntimeSpidermonkey {
    HjsRuntime parent_instance;

    JSRuntime *runtime;
    JSObject *global_proto;

    /* Context for the main thread the runtime normally runs in
     */
    ThreadContext *main_context;

    /* We have a JSContext in each thread.  We never free the
     * JSContext until dispose, just add them to this free list.
     */
    GSList *free_thread_contexts;
    guint   active_thread_context_count; /* also protected by same lock */
    GMutex *free_thread_contexts_lock;
};

struct HjsRuntimeSpidermonkeyClass {
    HjsRuntimeClass parent_class;
};

G_DEFINE_TYPE(HjsRuntimeSpidermonkey, hjs_runtime_spidermonkey, HJS_TYPE_RUNTIME);

static HjsScript*
hjs_runtime_spidermonkey_compile_script(HjsRuntime     *runtime,
                                        const char     *filename,
                                        GError        **error)
{
    HjsRuntimeSpidermonkey *runtime_spidermonkey;
    HjsScriptSpidermonkey *script_spidermonkey;
    char *contents;
    gsize len;

    runtime_spidermonkey = HJS_RUNTIME_SPIDERMONKEY(runtime);

    g_return_val_if_fail(runtime_spidermonkey->main_context != NULL, FALSE);

    if (!g_file_get_contents(filename, &contents, &len, error))
        return FALSE;

    script_spidermonkey = _hjs_script_spidermonkey_new(runtime_spidermonkey);

    if (!_hjs_script_spidermonkey_compile_script(script_spidermonkey,
                                                 runtime_spidermonkey->main_context,
                                                 filename,
                                                 contents, len,
                                                 error)) {
        g_object_unref(script_spidermonkey);
        script_spidermonkey = NULL;
    }

    g_free(contents);

    return HJS_SCRIPT(script_spidermonkey);
}

static void*
spidermonkey_malloc(gsize bytes,
                    void *allocator_data)
{
    void *mem;

    /* js_malloc is just malloc(), while JS_malloc() takes a context
     * and updates the GC and sets an exception on OOM. We don't want
     * the exception so use js_ not JS_ functions. When (and only
     * when) we hand the memory from HrtBuffer to a JS gc thing, we
     * should call JS_updateMallocCounter() to kick spidermonkey to
     * know about the allocated memory. At that point spidermonkey
     * will take ownership and free with JS_free() or equivalent.
     */
    mem = js_malloc(bytes);

    return mem;
}

static void
spidermonkey_free(void *mem,
                  void *allocator_data)
{
    if (mem == NULL)
        return;

    /* See comment in spidermonkey_malloc(). js_free() should be safe here
     * because we'll only be called if the buffer was not "stolen"
     * and given to a JSString or the like. So, we aren't expecting to
     * need to do anything special (JS_free() will defer the free
     * to the next GC, for example, here we don't do that).
     */
    js_free(mem);
}

static void*
spidermonkey_realloc(void *mem,
                     gsize bytes,
                     void *allocator_data)
{
    /* See comment in spidermonkey_malloc(). */
    return js_realloc(mem, bytes);
}

static const HrtBufferAllocator spidermonkey_allocator = {
    spidermonkey_malloc,
    spidermonkey_free,
    spidermonkey_realloc
};

static HrtBuffer*
hjs_runtime_spidermonkey_create_buffer(HjsRuntime   *runtime)
{
    return hrt_buffer_new(HRT_BUFFER_ENCODING_UTF16,
                          &spidermonkey_allocator,
                          /* we could pass in runtime here if we wanted
                           * to tell the GC about the buffer, but for
                           * now we don't want to do that until we wrap
                           * the buffer's data in a JS string object.
                           */
                          NULL, NULL);
}

static void
hjs_runtime_spidermonkey_get_property (GObject                *object,
                                       guint                   prop_id,
                                       GValue                 *value,
                                       GParamSpec             *pspec)
{
    HjsRuntimeSpidermonkey *runtime_spidermonkey;

    runtime_spidermonkey = HJS_RUNTIME_SPIDERMONKEY(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hjs_runtime_spidermonkey_set_property (GObject                *object,
                                       guint                   prop_id,
                                       const GValue           *value,
                                       GParamSpec             *pspec)
{
    HjsRuntimeSpidermonkey *runtime_spidermonkey;

    runtime_spidermonkey = HJS_RUNTIME_SPIDERMONKEY(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
thread_context_destroy(ThreadContext *thread_context)
{
    g_assert(thread_context->runtime == NULL);

    JS_SetContextThread(thread_context->context);

    JS_BeginRequest(thread_context->context);
    JS_RemoveObjectRoot(thread_context->context,
                        &thread_context->global_proto);
    JS_EndRequest(thread_context->context);

    JS_DestroyContext(thread_context->context);

    g_free(thread_context);
}

static void
hjs_runtime_spidermonkey_dispose(GObject *object)
{
    HjsRuntimeSpidermonkey *runtime_spidermonkey;
    ThreadContext *thread_context;

    runtime_spidermonkey = HJS_RUNTIME_SPIDERMONKEY(object);

    /* Detach our main thread context, which does
     * JS_ClearContextThread() as a side effect so our current thread
     * won't be using any context. Also moves the main context into
     * the free list.
     */
    if (runtime_spidermonkey->main_context) {
        /* Dispose is not allowed while threads are active. */
        g_assert(runtime_spidermonkey->active_thread_context_count == 1);

        _hjs_runtime_spidermonkey_context_detach(runtime_spidermonkey->main_context);
        runtime_spidermonkey->main_context = NULL;
    }

    /* Clean up any thread contexts that were used in exited threads
     * along with the main context we just moved to free list.
     */

    g_mutex_lock(runtime_spidermonkey->free_thread_contexts_lock);

    g_assert(runtime_spidermonkey->active_thread_context_count == 0);

    while (runtime_spidermonkey->free_thread_contexts) {
        thread_context =
            runtime_spidermonkey->free_thread_contexts->data;

        runtime_spidermonkey->free_thread_contexts =
            g_slist_delete_link(runtime_spidermonkey->free_thread_contexts,
                                runtime_spidermonkey->free_thread_contexts);

        thread_context_destroy(thread_context);
    }
    g_mutex_unlock(runtime_spidermonkey->free_thread_contexts_lock);

    g_assert(runtime_spidermonkey->free_thread_contexts == NULL);
    g_assert(runtime_spidermonkey->active_thread_context_count == 0);
    g_assert(runtime_spidermonkey->main_context == NULL);

    G_OBJECT_CLASS(hjs_runtime_spidermonkey_parent_class)->dispose(object);
}

static void
hjs_runtime_spidermonkey_finalize(GObject *object)
{
    HjsRuntimeSpidermonkey *runtime_spidermonkey;

    runtime_spidermonkey = HJS_RUNTIME_SPIDERMONKEY(object);

    g_assert(runtime_spidermonkey->free_thread_contexts == NULL);
    g_assert(runtime_spidermonkey->active_thread_context_count == 0);
    g_assert(runtime_spidermonkey->main_context == NULL);

    JS_DestroyRuntime(runtime_spidermonkey->runtime);

    g_mutex_free(runtime_spidermonkey->free_thread_contexts_lock);

    G_OBJECT_CLASS(hjs_runtime_spidermonkey_parent_class)->finalize(object);
}

static void
error_reporter(JSContext     *context,
               const char    *message,
               JSErrorReport *report)
{
    /* FIXME this needs to be better. Use gjs code. */
    g_debug("JS error: %s", message);
}

/* This is copyright litl, LLC under the same license as the rest of
 * this file. (from gjs.) FIXME just copy over most of jsapi-util
 * stuff from gjs wholesale and properly.
 */
static gboolean
try_string_to_utf8(JSContext  *context,
                   const jsval string_val,
                   char      **utf8_string_p)
{
    jschar *s;
    size_t s_length;
    char *utf8_string;
    long read_items;
    long utf8_length;
    GError *convert_error = NULL;

    JS_BeginRequest(context);

    if (!JSVAL_IS_STRING(string_val)) {
        /* FIXME throw */
        g_warning("not a string");
        JS_EndRequest(context);
        return FALSE;
    }

    s = JS_GetStringChars(JSVAL_TO_STRING(string_val));
    s_length = JS_GetStringLength(JSVAL_TO_STRING(string_val));

    utf8_string = g_utf16_to_utf8(s,
                                  (glong)s_length,
                                  &read_items, &utf8_length,
                                  &convert_error);

    /* ENDING REQUEST - no JSAPI after this point */
    JS_EndRequest(context);

    if (!utf8_string) {
        /* FIXME throw */
        g_warning("utf16-to-utf8 failed");
        g_error_free(convert_error);
        return FALSE;
    }

    if ((size_t)read_items != s_length) {
        /* FIXME throw */
        g_warning("wrong length");
        g_free(utf8_string);
        return FALSE;
    }

    /* Our assumption is that the string is being converted to UTF-8
     * in order to use with GLib-style APIs; Javascript has a looser
     * sense of validate-Unicode than GLib, so validate here to
     * prevent problems later on. Given the success of the above,
     * the only thing that could really be wrong here is including
     * non-characters like a byte-reversed BOM. If the validation
     * ever becomes a bottleneck, we could do an inline-special
     * case of all-ASCII.
     */
    if (!g_utf8_validate (utf8_string, utf8_length, NULL)) {
        /* FIXME throw */
        g_warning("invalid utf8");
        g_free(utf8_string);
        return FALSE;
    }

    *utf8_string_p = utf8_string;
    return TRUE;
}

static JSBool
string_to_utf8(JSContext  *context,
               const jsval string_val,
               char      **utf8_string_p)
{
    if (!try_string_to_utf8(context, string_val, utf8_string_p)) {
        /* FIXME throw */
        return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
debug_func(JSContext *context,
           JSObject  *obj,
           uintN      argc,
           jsval     *argv,
           jsval     *retval)
{
    char *s;

    if (argc == 0) {
        /* FIXME throw exception */
        g_warning("no args to debug()");
        return JS_FALSE;
    }

    s = NULL;
    JS_BeginRequest(context);
    string_to_utf8(context, argv[0], &s);
    JS_EndRequest(context);

    if (s == NULL) {
        /* FIXME exception */
        g_warning("bad string in debug()");
        return JS_FALSE;
    }
    hrt_debug("%s", s);
    g_free(s);
    return JS_TRUE;
}

/* Class of the prototype we set on globals */
static JSClass global_prototype_class = {
    "HjsGlobalPrototype",
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

static void
init_global_proto(JSContext  *context,
                  JSObject  **location_p)
{
    JSObject *global_proto;

    JS_BeginRequest(context);

    /* Create with a new compartment so each thread has its own,
     * for performance reasons (not for security reasons).
     */
    global_proto = JS_NewCompartmentAndGlobalObject(context, &global_prototype_class, NULL);
    if (global_proto == NULL)
        g_error("Failed to create global_proto object");

    /* We generally replace the global object when we run scripts, but
     * we need to set it so that context->compartment is set to the
     * new compartment we created along with global_proto. Then when
     * we create another global object later, it will get the
     * context's compartment as determined by global_proto. We have to
     * do this right away before we create other objects or add roots
     * or anything.
     */
    JS_SetGlobalObject(context, global_proto);

    /* We root the object which is mostly paranoia, but we do
     * sometimes JS_SetGlobalObject() to something else and so having
     * global_proto as global presumably is not enough.
     */
    *location_p = NULL;
    JS_AddNamedObjectRoot(context,
                          location_p,
                          G_STRLOC);
    *location_p = global_proto;

    if (!JS_InitStandardClasses(context, global_proto))
        g_error("Failed to add standard classes to global_proto");

    if (!JS_DefineFunction(context,
                           global_proto,
                           "debug",
                           debug_func,
                           1,
                           (JSPROP_READONLY | JSPROP_PERMANENT | JSPROP_ENUMERATE)))
        g_error("could not define debug() on global_proto object");

    if (!JS_SealObject(context,
                       global_proto,
                       /* deep seal */
                       JS_TRUE))
        g_error("Failed to seal global prototype");

    JS_EndRequest(context);
}

static JSContext*
create_js_context(HjsRuntimeSpidermonkey *runtime_spidermonkey)
{
    JSContext *context;
    unsigned int option_flags;

    context = JS_NewContext(runtime_spidermonkey->runtime,
                            8192 /* stack chunk size */);

    if (context == NULL)
        g_error("Failed to create javascript context");

    JS_BeginRequest(context);

    JS_SetContextPrivate(context, runtime_spidermonkey);

    /* same as firefox, see discussion at
     * https://bugzilla.mozilla.org/show_bug.cgi?id=420869
     */
    JS_SetScriptStackQuota(context, 100*1024*1024);

    /* JSOPTION_DONT_REPORT_UNCAUGHT: Don't send exceptions to our
     * error report handler; instead leave them set.  This allows us
     * to get at the exception object.
     *
     * JSOPTION_STRICT: Report warnings to error reporter function.
     */
    /* FIXME have DONT_REPORT_UNCAUGHT disabled until we code
     * some actual exception handling
     */
    option_flags = /* JSOPTION_DONT_REPORT_UNCAUGHT | */ JSOPTION_STRICT |
        JSOPTION_JIT | JSOPTION_VAROBJFIX;

    JS_SetOptions(context,
                  JS_GetOptions(context) | option_flags);

    /* FIXME */
    /* JS_SetLocaleCallbacks(context, &locale_callbacks); */

    JS_SetErrorReporter(context, error_reporter);

    /* set version to latest */
    JS_SetVersion(context, JS_VERSION);

    JS_EndRequest(context);

    return context;
}

ThreadContext*
_hjs_runtime_spidermonkey_context_new(HjsRuntimeSpidermonkey *runtime_spidermonkey)
{
    ThreadContext *thread_context;

    thread_context = NULL;

    /* Try getting an existing context from free list */
    g_mutex_lock(runtime_spidermonkey->free_thread_contexts_lock);
    if (runtime_spidermonkey->free_thread_contexts != NULL) {
        thread_context = runtime_spidermonkey->free_thread_contexts->data;
        runtime_spidermonkey->free_thread_contexts =
            g_slist_delete_link(runtime_spidermonkey->free_thread_contexts,
                                runtime_spidermonkey->free_thread_contexts);
    }

    /* Whether we get it from free list or create it, the active
     * number of contexts is going up.
     */
    runtime_spidermonkey->active_thread_context_count += 1;

    g_mutex_unlock(runtime_spidermonkey->free_thread_contexts_lock);

    if (thread_context != NULL) {
        /* Move the context to the current thread */
        JS_SetContextThread(thread_context->context);
    } else {
        /* Have to create a context from scratch */
        thread_context = g_new0(ThreadContext, 1);

        thread_context->context =
            create_js_context(runtime_spidermonkey);

        init_global_proto(thread_context->context,
                          &thread_context->global_proto);
    }

    /* Ref the runtime only while we are not in the free list */
    thread_context->runtime = g_object_ref(runtime_spidermonkey);

    return thread_context;
}

/* Runs in the thread being exited, NOT MAIN THREAD. This is NOT
 * called for main_context, so should only have things that are
 * specific to detaching from another thread.
 */
void
_hjs_runtime_spidermonkey_context_detach(void *data)
{
    ThreadContext *thread_context = data;
    HjsRuntimeSpidermonkey *runtime_spidermonkey;

    g_assert(HJS_IS_RUNTIME_SPIDERMONKEY(thread_context->runtime));

    runtime_spidermonkey = thread_context->runtime;
    thread_context->runtime = NULL;

    JS_ClearContextThread(thread_context->context);

    /* Recycle contexts; we free the free list in runtime dispose().
     */
    g_mutex_lock(runtime_spidermonkey->free_thread_contexts_lock);

    runtime_spidermonkey->free_thread_contexts =
        g_slist_prepend(runtime_spidermonkey->free_thread_contexts,
                        thread_context);

    runtime_spidermonkey->active_thread_context_count -= 1;
    g_mutex_unlock(runtime_spidermonkey->free_thread_contexts_lock);

    /* ThreadContext had a strong ref to the runtime while it was
     * associated with a thread. We drop that now.
     */
    g_object_unref(runtime_spidermonkey);
}

ThreadContext*
_hjs_runtime_spidermonkey_get_main_context(HjsRuntimeSpidermonkey  *runtime_spidermonkey)
{
    g_return_val_if_fail(runtime_spidermonkey->main_context != NULL, NULL);

    return runtime_spidermonkey->main_context;
}

static void
hjs_runtime_spidermonkey_init(HjsRuntimeSpidermonkey *runtime_spidermonkey)
{
    runtime_spidermonkey->free_thread_contexts_lock = g_mutex_new();

    runtime_spidermonkey->runtime = JS_NewRuntime(G_MAXUINT /* max bytes */);
    if (runtime_spidermonkey->runtime == NULL)
        g_error("Failed to create javascript runtime");

    JS_SetGCParameter(runtime_spidermonkey->runtime, JSGC_MAX_BYTES, 0xffffffff);

    runtime_spidermonkey->main_context =
        _hjs_runtime_spidermonkey_context_new(runtime_spidermonkey);
}

static void
hjs_runtime_spidermonkey_class_init(HjsRuntimeSpidermonkeyClass *klass)
{
    GObjectClass *object_class;
    HjsRuntimeClass *runtime_class;

    object_class = G_OBJECT_CLASS(klass);
    runtime_class = HJS_RUNTIME_CLASS(klass);

    object_class->get_property = hjs_runtime_spidermonkey_get_property;
    object_class->set_property = hjs_runtime_spidermonkey_set_property;

    object_class->dispose = hjs_runtime_spidermonkey_dispose;
    object_class->finalize = hjs_runtime_spidermonkey_finalize;

    runtime_class->compile_script = hjs_runtime_spidermonkey_compile_script;
    runtime_class->create_buffer = hjs_runtime_spidermonkey_create_buffer;
}
