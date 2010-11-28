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

#define MAX_DEPTH 5
#define BRANCHES_PER_NODE 10
#define NUM_TASKS (1 + 10 + 10*10 + 10*10*10 + 10*10*10*10 + 10*10*10*10*10)

typedef struct {
    HrtTaskRunner *runner;
    volatile int tasks_started_count;
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

        if (fixture->tasks_completed_count >= NUM_TASKS) {
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
    g_object_unref(fixture->runner);
    g_main_loop_unref(fixture->loop);
}

static void
on_dnotify_bump_count(void *data)
{
    TestFixture *fixture = data;
    g_atomic_int_inc(&fixture->dnotify_count);
}

typedef struct {
    volatile int total_descendants;
    volatile int subtasks_completed;
} AllSubtasksData;

typedef struct {
    AllSubtasksData *asd;
    HrtTask *subtask;
} SubtaskData;

static gboolean
on_subtask_completed(HrtTask        *task,
                     HrtWatcherFlags flags,
                     void           *data)
{
    SubtaskData *sd = data;
    int depth;
    int child_depth;
    GValue v = { 0, };
    int descendants;

    g_value_init(&v, G_TYPE_INT);
    hrt_task_get_result(sd->subtask, &v, NULL);
    descendants = g_value_get_int(&v) + 1;
    g_atomic_int_add(&sd->asd->total_descendants, descendants);
    g_value_unset(&v);

    g_value_init(&v, G_TYPE_INT);
    if (!hrt_task_get_arg(task, "depth", &v, NULL))
        g_error("no depth arg");
    depth = g_value_get_int(&v);
    g_value_unset(&v);

    g_value_init(&v, G_TYPE_INT);
    if (!hrt_task_get_arg(sd->subtask, "depth", &v, NULL))
        g_error("no child depth arg");
    child_depth = g_value_get_int(&v);
    g_value_unset(&v);

    g_assert_cmpint(child_depth, ==, depth + 1);

    g_atomic_int_inc(&sd->asd->subtasks_completed);

    if (g_atomic_int_get(&sd->asd->subtasks_completed) >= BRANCHES_PER_NODE) {
        /* All done, set our result */
        int total_descendants;

        total_descendants = g_atomic_int_get(&sd->asd->total_descendants);

        g_value_init(&v, G_TYPE_INT);
        g_value_set_int(&v, total_descendants);
        hrt_task_set_result(task, &v);
        g_value_unset(&v);

        /* free the shared-across-subtasks data */
        g_free(sd->asd);
    }

    return FALSE;
}

static gboolean
on_task_invoked(HrtTask        *task,
                HrtWatcherFlags flags,
                void           *data)
{
    TestFixture *fixture = data;
    int i;
    GValue v = { 0, };
    int depth;
    AllSubtasksData *asd;

    g_value_init(&v, G_TYPE_INT);
    if (!hrt_task_get_arg(task, "depth", &v, NULL))
        g_error("no depth arg");
    depth = g_value_get_int(&v);
    g_value_unset(&v);

    /* At max depth, stop spawning subtasks, and report
     * a result of 0 descendants
     */
    if (depth >= MAX_DEPTH) {
        g_value_init(&v, G_TYPE_INT);
        g_value_set_int(&v, 0);
        hrt_task_set_result(task, &v);
        g_value_unset(&v);
        return FALSE;
    }

    /* Spawn subtasks and count up their children */
    asd = g_new0(AllSubtasksData, 1);

    for (i = 0; i < BRANCHES_PER_NODE; ++i) {
        HrtTask *subtask;
        SubtaskData *sd;

        sd = g_new0(SubtaskData, 1);
        sd->asd = asd;

        subtask = hrt_task_create_task(task);
        g_atomic_int_inc(&fixture->tasks_started_count);

        sd->subtask = subtask;

        g_value_init(&v, G_TYPE_INT);
        g_value_set_int(&v, depth + 1);
        hrt_task_add_arg(subtask, "depth", &v);
        g_value_unset(&v);

        /* notify on subtask completion */
        hrt_task_add_subtask(task,
                             subtask,
                             on_subtask_completed,
                             sd,
                             g_free);

        /* The subtask's work (recurse) */
        hrt_task_add_immediate(subtask,
                               on_task_invoked,
                               fixture,
                               on_dnotify_bump_count);
    }

    return FALSE;
}

static void
test_run_subtask_tree(TestFixture *fixture,
                      const void  *data)
{
    HrtTask *task;
    GValue v = { 0, };

    fixture->tasks_started_count = 0;

    task = hrt_task_runner_create_task(fixture->runner);
    g_atomic_int_inc(&fixture->tasks_started_count);

    g_value_init(&v, G_TYPE_INT);
    g_value_set_int(&v, 0);
    hrt_task_add_arg(task, "depth", &v);
    g_value_unset(&v);

    hrt_task_add_immediate(task,
                           on_task_invoked,
                           fixture,
                           on_dnotify_bump_count);

    g_main_loop_run(fixture->loop);

    g_assert_cmpint(fixture->tasks_started_count, ==, NUM_TASKS);
    g_assert_cmpint(fixture->tasks_completed_count, ==, NUM_TASKS);
    g_assert_cmpint(fixture->dnotify_count, ==, NUM_TASKS);

    /* see if we counted up descendant nodes properly */
    g_value_init(&v, G_TYPE_INT);
    hrt_task_get_result(task, &v, NULL);
    g_assert_cmpint((g_value_get_int(&v) + 1), ==, NUM_TASKS);
    g_value_unset(&v);
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

    context = g_option_context_new("- Test Suite Subtasks");
    g_option_context_add_main_entries(context, entries, "test-subtask");

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    if (option_version) {
        g_print("test-subtask %s\n",
                VERSION);
        exit(0);
    }

    hrt_log_init(option_debug ?
                 HRT_LOG_FLAG_DEBUG : 0);

    g_test_add("/subtask/run_subtask_tree_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_run_subtask_tree,
               teardown_test_fixture);

    g_test_add("/subtask/run_subtask_tree_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_run_subtask_tree,
               teardown_test_fixture);

    return g_test_run();
}
