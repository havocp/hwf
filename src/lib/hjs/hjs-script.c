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
#include <hjs/hjs-script.h>

G_DEFINE_TYPE(HjsScript, hjs_script, G_TYPE_OBJECT);

enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */

static void
hjs_script_get_property (GObject                *object,
                         guint                   prop_id,
                         GValue                 *value,
                         GParamSpec             *pspec)
{
    HjsScript *script;

    script = HJS_SCRIPT(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hjs_script_set_property (GObject                *object,
                         guint                   prop_id,
                         const GValue           *value,
                         GParamSpec             *pspec)
{
    HjsScript *script;

    script = HJS_SCRIPT(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hjs_script_dispose(GObject *object)
{
    HjsScript *script;

    script = HJS_SCRIPT(object);


    G_OBJECT_CLASS(hjs_script_parent_class)->dispose(object);
}

static void
hjs_script_finalize(GObject *object)
{
    HjsScript *script;

    script = HJS_SCRIPT(object);


    G_OBJECT_CLASS(hjs_script_parent_class)->finalize(object);
}

static void
hjs_script_init(HjsScript *script)
{

}

static void
hjs_script_class_init(HjsScriptClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = hjs_script_get_property;
    object_class->set_property = hjs_script_set_property;

    object_class->dispose = hjs_script_dispose;
    object_class->finalize = hjs_script_finalize;
}

void
hjs_script_run_in_task(HjsScript     *script,
                       HrtTask       *task)
{
    HJS_SCRIPT_GET_CLASS(script)->run_in_task(script, task, NULL, NULL);
}

void
hjs_script_run_in_task_with_values(HjsScript     *script,
                                   HrtTask       *task,
                                   const char   **names,
                                   const GValue  *values)
{
    HJS_SCRIPT_GET_CLASS(script)->run_in_task(script, task, names, values);
}
