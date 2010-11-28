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
#include <hrt/hrt-task.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    HrtEventLoopType loop_type;
    HrtTaskRunner *runner;
    int shutdown_count;
    int tasks_started_count;
    int tasks_completed_count;
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
    fixture->loop_type = loop_type;

    fixture->loop =
        g_main_loop_new(NULL, FALSE);
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
    g_main_loop_unref(fixture->loop);
}

static void
on_dnotify_bump_count(void *data)
{
    TestFixture *fixture = data;
    g_atomic_int_inc(&fixture->dnotify_count);
}

static gboolean
on_idle_callback(HrtTask        *task,
                 HrtWatcherFlags flags,
                 void           *data)
{
    /* TestFixture *fixture = data; */

    return FALSE;
}

static void
one_iteration_of_runners_with_idles(TestFixture *fixture)
{
    int i, j;
    int n_tasks;
    int idles_per_task;
    HrtTask **tasks;
    HrtTask *no_watcher_task;

    g_assert(fixture->runner == NULL);

    fixture->runner =
        g_object_new(HRT_TYPE_TASK_RUNNER,
                     "event-loop-type", fixture->loop_type,
                     NULL);

    g_signal_connect(G_OBJECT(fixture->runner),
                     "tasks-completed",
                     G_CALLBACK(on_tasks_completed),
                     fixture);

    n_tasks = 10;
    /* note that we serialize all idles for a task so making this
     * a large number will take forever if the idles do anything
     */
    idles_per_task = 10;

    /* assign before we run anything or it'd be a race */
    fixture->tasks_started_count = n_tasks;

    /* reset these */
    fixture->tasks_completed_count = 0;
    fixture->dnotify_count = 0;

    tasks = g_newa(HrtTask*,n_tasks);

    for (i = 0; i < n_tasks; ++i) {
        tasks[i] = hrt_task_runner_create_task(fixture->runner);
    }

    /* add idles "evenly" across tasks so first task doesn't finish first */
    for (i = 0; i < idles_per_task; ++i) {
        for (j = 0; j < n_tasks; ++j) {
            hrt_task_add_idle(tasks[j],
                              on_idle_callback,
                              fixture,
                              on_dnotify_bump_count);
        }
    }

    /* unref the tasks, their idle callbacks should keep them alive */
    for (i = 0; i < n_tasks; ++i) {
        g_object_unref(tasks[i]);
    }

    /* Have one task that never has anything running in it, this is to
     * see if a never-runs task breaks shutdown. Do this every other
     * time so we also test without it.
     */
    if ((fixture->shutdown_count % 2) == 0) {
        no_watcher_task = hrt_task_runner_create_task(fixture->runner);
    } else {
        no_watcher_task = NULL;
    }

    /* Run until all idles complete */
    g_main_loop_run(fixture->loop);

    if (no_watcher_task)
        g_object_unref(no_watcher_task);

    /* And the point of this test is mostly to see if this thing
     * hangs.
     */
    g_object_run_dispose(G_OBJECT(fixture->runner));
    g_object_unref(fixture->runner);

    /* check basic sanity that we really ran stuff */
    g_assert_cmpint(fixture->tasks_completed_count, ==, n_tasks);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, n_tasks * idles_per_task);

    fixture->runner = NULL;

    fixture->shutdown_count += 1;
}

static void
test_run_tasks_and_shutdown_runner(TestFixture *fixture,
                                   const void  *data)
{
    int i;

    for (i = 0; i < 300; ++i) {
        one_iteration_of_runners_with_idles(fixture);
    }
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

    context = g_option_context_new("- Test Suite Runner Shutdown");
    g_option_context_add_main_entries(context, entries, "test-runner-shutdown");

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    if (option_version) {
        g_print("test-runner-shutdown %s\n",
                VERSION);
        exit(0);
    }

    hrt_log_init(option_debug ?
                 HRT_LOG_FLAG_DEBUG : 0);

    g_test_add("/runner_shutdown/run_tasks_and_shutdown_runner_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_run_tasks_and_shutdown_runner,
               teardown_test_fixture);

    g_test_add("/runner_shutdown/run_tasks_and_shutdown_runner_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_run_tasks_and_shutdown_runner,
               teardown_test_fixture);

    return g_test_run();
}
