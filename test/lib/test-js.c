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
#include <hrt/hrt-task-runner.h>
#include <hjs/hjs-runtime-spidermonkey.h>
#include <hjs/hjs-script-spidermonkey.h>
#include <stdlib.h>

typedef struct {
    HjsRuntime *runtime;
    HrtTaskRunner *runner;
    int tasks_started_count;
    int tasks_completed_count;
    /* dnotify_count is accessed by multiple task threads so needs to be atomic */
    volatile int dnotify_count;
    GMainLoop *loop;

} TestFixture;

static void
on_tasks_completed(HrtTaskRunner *runner,
                   void          *data)
{
    TestFixture *fixture = data;
    HrtTask *task;

    while ((task = hrt_task_runner_pop_completed(fixture->runner)) != NULL) {
        g_object_unref(task);

        fixture->tasks_completed_count += 1;

        if (fixture->tasks_completed_count ==
            fixture->tasks_started_count) {
            g_main_loop_quit(fixture->loop);
        }
    }
}

static void
setup_test_fixture_generic(TestFixture     *fixture,
                           HrtEventLoopType loop_type)
{
    fixture->runtime =
        g_object_new(HJS_TYPE_RUNTIME_SPIDERMONKEY,
                     NULL);

    fixture->loop =
        g_main_loop_new(NULL, FALSE);

    fixture->runner =
        g_object_new(HRT_TYPE_TASK_RUNNER,
                     "event-loop-type", loop_type,
                     NULL);

    g_signal_connect(G_OBJECT(fixture->runner),
                     "tasks-completed",
                     G_CALLBACK(on_tasks_completed),
                     fixture);
}

static void
setup_test_fixture_glib(TestFixture *fixture,
                        const void  *data)
{
    setup_test_fixture_generic(fixture, HRT_EVENT_LOOP_GLIB);
}

static void
setup_test_fixture_libev(TestFixture *fixture,
                         const void  *data)
{
    setup_test_fixture_generic(fixture, HRT_EVENT_LOOP_EV);
}

static void
teardown_test_fixture(TestFixture *fixture,
                      const void  *data)
{
    /* shut down threads */
    g_object_run_dispose(G_OBJECT(fixture->runner));
    g_object_unref(fixture->runner);

    /* runtime should be OK to dispose once
     * no threads refer to it
     */
    g_object_run_dispose(G_OBJECT(fixture->runtime));
    g_object_unref(fixture->runtime);

    g_main_loop_unref(fixture->loop);
}

static void
on_dnotify_bump_count(void *data)
{
    TestFixture *fixture = data;
    g_atomic_int_inc(&fixture->dnotify_count);
}

static char*
srcdir_filename(const char *basename)
{
    const char *dirname;

    dirname = g_getenv("TOP_SRCDIR");
    if (dirname == NULL)
        g_error("TOP_SRCDIR is not set");

    return g_build_filename(dirname, basename, NULL);
}

static void
test_js_that_logs(TestFixture *fixture,
                  const void  *data)
{
    HjsScript *script;
    GError *error;
    char *filename;
    int i;

    filename = srcdir_filename("test/lib/logSomething.js");

    error = NULL;
    script =
        hjs_runtime_compile_script(fixture->runtime,
                                   filename,
                                   &error);

    if (error != NULL) {
        g_error("failed to get script %s: %s",
                filename, error->message);
        g_error_free(error);
    }
    g_assert(script != NULL);

    g_free(filename);

#define N_TASKS 10000
    fixture->tasks_started_count = N_TASKS;
    for (i = 0; i < N_TASKS; ++i) {
        HrtTask *task;

        task = hrt_task_runner_create_task(fixture->runner);

        hjs_script_run_in_task(script,
                               task);

        g_object_unref(task);
    }

    g_main_loop_run(fixture->loop);

    g_assert_cmpint(fixture->tasks_completed_count, ==, N_TASKS);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    /* g_assert_cmpint(fixture->dnotify_count, ==, 1); */

#undef N_TASKS
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

    context = g_option_context_new("- Test Suite JS");
    g_option_context_add_main_entries(context, entries, "test-js");

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    if (option_version) {
        g_print("test-js %s\n",
                VERSION);
        exit(0);
    }

    hrt_log_init(option_debug ?
                 HRT_LOG_FLAG_DEBUG : 0);

    g_test_add("/js/run_that_logs",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_js_that_logs,
               teardown_test_fixture);

    return g_test_run();
}
