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
#include <glib-object.h>
#include <hrt/hrt-log.h>
#include <hrt/hrt-task-thread-local.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    int dummy;
} TestFixture;

static void
setup_test_fixture(TestFixture *fixture,
                   const void  *data)
{

}

static void
teardown_test_fixture(TestFixture *fixture,
                      const void  *data)
{

}

static void
dnotify_set_bool(void *data)
{
    *(gboolean*)data = TRUE;
}

static void
test_local_get_set(TestFixture *fixture,
                   const void  *data)
{
    HrtTaskThreadLocal *tl;
    gboolean a;
    gboolean b;
    gboolean c;

    tl = _hrt_task_thread_local_new();

    a = b = c = FALSE;

    g_assert(_hrt_task_thread_local_get(tl, &setup_test_fixture) == NULL);
    g_assert(_hrt_task_thread_local_get(tl, &teardown_test_fixture) == NULL);
    g_assert(_hrt_task_thread_local_get(tl, &dnotify_set_bool) == NULL);

    _hrt_task_thread_local_set(tl, &setup_test_fixture, &a, dnotify_set_bool);
    _hrt_task_thread_local_set(tl, &teardown_test_fixture, &b, dnotify_set_bool);
    _hrt_task_thread_local_set(tl, &dnotify_set_bool, &c, dnotify_set_bool);

    g_assert(_hrt_task_thread_local_get(tl, &setup_test_fixture) == &a);
    g_assert(_hrt_task_thread_local_get(tl, &teardown_test_fixture) == &b);
    g_assert(_hrt_task_thread_local_get(tl, &dnotify_set_bool) == &c);

    g_assert(!a);
    g_assert(!b);
    g_assert(!c);

    /* set to NULL should dnotify */
    _hrt_task_thread_local_set(tl, &setup_test_fixture, NULL, NULL);
    /* set to another value rather than NULL */
    _hrt_task_thread_local_set(tl, &teardown_test_fixture, &tl, NULL);
    /* the third one we don't set but check dnotify on destroy thread local */

    g_assert(a);
    g_assert(b);
    g_assert(!c);

    _hrt_task_thread_local_free(tl);

    g_assert(a);
    g_assert(b);
    g_assert(c);
}


static gboolean option_debug = FALSE;
static gboolean option_version = FALSE;

static GOptionEntry entries[] = {
    { "debug", 0, 0, G_OPTION_ARG_NONE, &option_debug, "Enable debug logging", NULL },
    { "version", 0, 0, G_OPTION_ARG_NONE, &option_version, "Show version info and exit", NULL },
    { NULL }
};

int
main(int    argc,
     char **argv)
{
    GError *error = NULL;
    GOptionContext *context;

    g_thread_init(NULL);
    g_type_init();

    g_test_init(&argc, &argv, NULL);

    context = g_option_context_new("- Test Suite Thread Local");
    g_option_context_add_main_entries(context, entries, "test-thread-local");

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    if (option_version) {
        g_print("test-thread-local %s\n",
                VERSION);
        exit(0);
    }

    hrt_log_init(option_debug ?
                 HRT_LOG_FLAG_DEBUG : 0);

    g_test_add("/thread_local/get_set",
               TestFixture,
               NULL,
               setup_test_fixture,
               test_local_get_set,
               teardown_test_fixture);

    return g_test_run();
}
