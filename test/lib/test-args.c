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
#include <hrt/hrt-task.h>
#include <stdlib.h>

/* g_assert_cmpfloat() is totally safe in this file. */
#pragma GCC diagnostic ignored "-Wfloat-equal"

typedef struct {
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

/*
  static void
  setup_test_fixture_glib(TestFixture *fixture,
  const void  *data)
  {
  setup_test_fixture_generic(fixture, HRT_EVENT_LOOP_GLIB);
  }
*/

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
    g_object_unref(fixture->runner);
    g_main_loop_unref(fixture->loop);
}

static void
on_dnotify_bump_count(void *data)
{
    TestFixture *fixture = data;
    g_atomic_int_inc(&fixture->dnotify_count);
}

#define STRING_VALUE "abcdefg"
#define INT_VALUE 42
#define DOUBLE_VALUE 3.14159


static gboolean
on_task_with_args_invoked(HrtTask        *task,
                          HrtWatcherFlags flags,
                          void           *data)
{
    /* TestFixture *fixture = data; */
    GError *error;
    GValue value = { 0, };
    GValue *values;
    char **names;
    int i;
    double result;

    error = NULL;

    /* Try getting all args individually */

    g_value_init(&value, G_TYPE_STRING);
    error = NULL;
    if (!hrt_task_get_arg(task, "a-string", &value, &error))
        g_error("%s", error->message);
    g_assert(error == NULL);

    g_assert(G_VALUE_TYPE(&value) == G_TYPE_STRING);
    g_assert_cmpstr(STRING_VALUE, ==, g_value_get_string(&value));

    g_value_unset(&value);

    g_value_init(&value, G_TYPE_INT);
    error = NULL;
    if (!hrt_task_get_arg(task, "an-int", &value, &error))
        g_error("%s", error->message);
    g_assert(error == NULL);

    g_assert(G_VALUE_TYPE(&value) == G_TYPE_INT);
    g_assert_cmpint(INT_VALUE, ==, g_value_get_int(&value));

    result = g_value_get_int(&value);

    g_value_unset(&value);

    g_value_init(&value, G_TYPE_DOUBLE);
    error = NULL;
    if (!hrt_task_get_arg(task, "a-double", &value, &error))
        g_error("%s", error->message);

    g_assert(error == NULL);

    g_assert(G_VALUE_TYPE(&value) == G_TYPE_DOUBLE);
    g_assert_cmpfloat(DOUBLE_VALUE, ==, g_value_get_double(&value));

    result += g_value_get_double(&value);

    g_value_unset(&value);

    /* Now set result */

    g_value_init(&value, G_TYPE_DOUBLE);
    g_value_set_double(&value, result);
    hrt_task_set_result(task, &value);
    g_value_unset(&value);

    /* Get result back */
    g_value_init(&value, G_TYPE_DOUBLE);
    error = NULL;
    if (!hrt_task_get_result(task, &value, &error))
        g_error("%s", error->message);
    g_assert(error == NULL);
    g_assert_cmpfloat(result, ==, g_value_get_double(&value));
    g_value_unset(&value);

    /* Now try getting all args at once */
    hrt_task_get_args(task, &names, &values);

    /* this test relies on the not-in-API-contract reverse
     * ordering of the args in the array. Can make the test
     * more complex to allow another order if the implementation
     * ever changes.
     */
    g_assert_cmpstr(names[0], ==, "a-double");
    g_assert_cmpstr(names[1], ==, "an-int");
    g_assert_cmpstr(names[2], ==, "a-string");
    g_assert(names[3] == NULL);

    g_assert_cmpfloat(g_value_get_double(&values[0]), ==, DOUBLE_VALUE);
    g_assert_cmpint(g_value_get_int(&values[1]), ==, INT_VALUE);
    g_assert_cmpstr(g_value_get_string(&values[2]), ==, STRING_VALUE);

    for (i = 0; names[i] != NULL; ++i) {
        g_value_unset(&values[i]);
    }
    g_assert(i == 3);
    g_free(values);
    g_strfreev(names);

    /* Only run one time */
    return FALSE;
}

static void
test_args_and_result(TestFixture *fixture,
                     const void  *data)
{
    HrtTask *task;
    GValue value = { 0, };
    GError *error;

    task = hrt_task_runner_create_task(fixture->runner);

    g_value_init(&value, G_TYPE_STRING);
    g_value_set_string(&value, STRING_VALUE);
    hrt_task_add_arg(task, "a-string", &value);
    g_value_unset(&value);

    g_value_init(&value, G_TYPE_INT);
    g_value_set_int(&value, INT_VALUE);
    hrt_task_add_arg(task, "an-int", &value);
    g_value_unset(&value);

    g_value_init(&value, G_TYPE_DOUBLE);
    g_value_set_double(&value, DOUBLE_VALUE);
    hrt_task_add_arg(task, "a-double", &value);
    g_value_unset(&value);

    fixture->tasks_started_count = 1;
    hrt_task_add_immediate(task,
                           on_task_with_args_invoked,
                           fixture,
                           on_dnotify_bump_count);

    g_main_loop_run(fixture->loop);

    /* See if the result was set properly */
    g_value_init(&value, G_TYPE_DOUBLE);
    error = NULL;
    if (!hrt_task_get_result(task, &value, &error))
        g_error("%s", error->message);
    g_assert(error == NULL);
    g_assert_cmpfloat((INT_VALUE + DOUBLE_VALUE), ==, g_value_get_double(&value));
    g_value_unset(&value);

    g_object_unref(task);

    g_assert_cmpint(fixture->tasks_completed_count, ==, 1);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, 1);
}

static gboolean
on_task_with_args_invoked_perf(HrtTask        *task,
                               HrtWatcherFlags flags,
                               void           *data)
{
    /* In this callback we want to avoid any "bogus" CPU time
     * and try to just include what would be inherent or typical
     * arg-reading and result-setting
     */
    /* TestFixture *fixture = data; */
    GError *error;
    GValue value = { 0, };

    error = NULL;

    g_value_init(&value, G_TYPE_STRING);
    error = NULL;
    if (!hrt_task_get_arg(task, "a-string", &value, &error))
        g_error("%s", error->message);
    g_assert(error == NULL);

    g_assert(G_VALUE_TYPE(&value) == G_TYPE_STRING);
    g_assert_cmpstr(STRING_VALUE, ==, g_value_get_string(&value));

    g_value_unset(&value);

    g_value_init(&value, G_TYPE_INT);
    error = NULL;
    if (!hrt_task_get_arg(task, "an-int", &value, &error))
        g_error("%s", error->message);
    g_assert(error == NULL);

    g_assert(G_VALUE_TYPE(&value) == G_TYPE_INT);
    g_assert_cmpint(INT_VALUE, ==, g_value_get_int(&value));

    g_value_unset(&value);

    g_value_init(&value, G_TYPE_DOUBLE);
    error = NULL;
    if (!hrt_task_get_arg(task, "a-double", &value, &error))
        g_error("%s", error->message);

    g_assert(error == NULL);

    g_assert(G_VALUE_TYPE(&value) == G_TYPE_DOUBLE);
    g_assert_cmpfloat(DOUBLE_VALUE, ==, g_value_get_double(&value));

    g_value_unset(&value);

    /* Now set result */

    g_value_init(&value, G_TYPE_DOUBLE);
    g_value_set_double(&value, DOUBLE_VALUE);
    hrt_task_set_result(task, &value);
    g_value_unset(&value);

    /* Only run one time */
    return FALSE;
}

static void
test_performance_args_and_result(TestFixture *fixture,
                                 const void  *data)
{
#define N_TASKS 700000
    int i;

    if (!g_test_perf())
        return;

    /* start here, to include task creation. Also, immediates can start
     * running right away, before we block in main loop.
     */
    g_test_timer_start();

    fixture->tasks_started_count = N_TASKS;
    for (i = 0; i < N_TASKS; ++i) {
        HrtTask *task;
        GValue value = { 0, };

        task = hrt_task_runner_create_task(fixture->runner);

        g_value_init(&value, G_TYPE_STRING);
        g_value_set_string(&value, STRING_VALUE);
        hrt_task_add_arg(task, "a-string", &value);
        g_value_unset(&value);

        g_value_init(&value, G_TYPE_INT);
        g_value_set_int(&value, INT_VALUE);
        hrt_task_add_arg(task, "an-int", &value);
        g_value_unset(&value);

        g_value_init(&value, G_TYPE_DOUBLE);
        g_value_set_double(&value, DOUBLE_VALUE);
        hrt_task_add_arg(task, "a-double", &value);
        g_value_unset(&value);

        hrt_task_add_immediate(task,
                               on_task_with_args_invoked_perf,
                               fixture,
                               on_dnotify_bump_count);

        g_object_unref(task);
    }

    g_main_loop_run(fixture->loop);

    g_test_minimized_result(g_test_timer_elapsed(),
                            "Run %d tasks with args and results",
                            N_TASKS);

    g_assert_cmpint(fixture->tasks_completed_count, ==, N_TASKS);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, N_TASKS);
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

    context = g_option_context_new("- Test Suite Args");
    g_option_context_add_main_entries(context, entries, "test-args");

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    if (option_version) {
        g_print("test-args %s\n",
                VERSION);
        exit(0);
    }

    hrt_log_init(option_debug ?
                 HRT_LOG_FLAG_DEBUG : 0);

    g_test_add("/args/args_and_result",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_args_and_result,
               teardown_test_fixture);

    g_test_add("/args/performance_args_and_result",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_performance_args_and_result,
               teardown_test_fixture);

    return g_test_run();
}
