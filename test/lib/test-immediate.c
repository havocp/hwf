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

#define NUM_TASKS 100

typedef struct {
    HrtTaskRunner *runner;
    int tasks_started_count;
    int tasks_completed_count;
    /* dnotify_count is accessed by multiple task threads so needs to be atomic */
    volatile int dnotify_count;
    GMainLoop *loop;
    int times_run;
    struct {
        HrtTask *task;
        HrtWatcher *watcher;
        int immediates_run_count;
        int saw_another_immediate_in_an_immediate_count;
        /* this field is read by multiple task threads so needs to be atomic */
        volatile int in_an_immediate;
    } tasks[NUM_TASKS];

    /* used by test completion case */
    gboolean completion_should_be_blocked;
    gboolean completion_check_timeout_ran;
    guint completion_check_timeout_id;
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

static gboolean
on_immediate_sleep_manual_remove(HrtTask        *task,
                                 HrtWatcherFlags flags,
                                 void           *data)
{
    void *sleeping;
    TestFixture *fixture = data;

    g_assert(flags == HRT_WATCHER_FLAG_NONE);

    /* Use a flag to verify that multiple immediates don't run at once. */
    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping == NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", GINT_TO_POINTER(1));

    g_usleep(G_USEC_PER_SEC / 20);

    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping != NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", NULL);

    hrt_watcher_remove(fixture->tasks[0].watcher);
    fixture->tasks[0].watcher = NULL;

    fixture->tasks[0].immediates_run_count += 1;

    return TRUE;
}

static void
test_immediate_that_sleeps_manual_remove(TestFixture *fixture,
                                         const void  *data)
{
    HrtTask *task;

    task = hrt_task_runner_create_task(fixture->runner);
    fixture->tasks_started_count += 1;

    fixture->tasks[0].watcher =
        hrt_task_add_immediate(task,
                               on_immediate_sleep_manual_remove,
                               fixture,
                               on_dnotify_bump_count);

    g_main_loop_run(fixture->loop);

    g_assert_cmpint(fixture->tasks_completed_count, ==, 1);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, 1);
    g_assert_cmpint(fixture->tasks[0].immediates_run_count, ==, 1);
}

static gboolean
on_immediate_sleep_return_false(HrtTask        *task,
                                HrtWatcherFlags flags,
                                void           *data)
{
    void *sleeping;
    TestFixture *fixture = data;

    g_assert(flags == HRT_WATCHER_FLAG_NONE);

    /* Use a flag to verify that multiple immediates don't run at once. */
    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping == NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", GINT_TO_POINTER(1));

    g_usleep(G_USEC_PER_SEC / 20);

    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping != NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", NULL);

    fixture->tasks[0].immediates_run_count += 1;

    return FALSE;
}

static void
test_immediate_that_sleeps_return_false(TestFixture *fixture,
                                        const void  *data)
{
    HrtTask *task;

    task =
        hrt_task_runner_create_task(fixture->runner);
    fixture->tasks_started_count += 1;

    hrt_task_add_immediate(task,
                           on_immediate_sleep_return_false,
                           fixture,
                           on_dnotify_bump_count);

    g_main_loop_run(fixture->loop);

    g_assert_cmpint(fixture->tasks_completed_count, ==, 1);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, 1);
    g_assert_cmpint(fixture->tasks[0].immediates_run_count, ==, 1);
}


#define SEVERAL_TIMES 50
static gboolean
on_immediate_runs_several_times(HrtTask        *task,
                                HrtWatcherFlags flags,
                                void           *data)
{
    TestFixture *fixture = data;

    g_assert(flags == HRT_WATCHER_FLAG_NONE);

    fixture->times_run += 1;
    if (fixture->times_run == SEVERAL_TIMES)
        return FALSE;
    else
        return TRUE;
}

static void
test_immediate_runs_several_times(TestFixture *fixture,
                                  const void  *data)
{
    HrtTask *task;

    task =
        hrt_task_runner_create_task(fixture->runner);
    fixture->tasks_started_count += 1;

    hrt_task_add_immediate(task,
                           on_immediate_runs_several_times,
                           fixture,
                           on_dnotify_bump_count);

    g_main_loop_run(fixture->loop);

    g_assert_cmpint(fixture->tasks_completed_count, ==, 1);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, 1);
    g_assert_cmpint(fixture->times_run, ==, SEVERAL_TIMES);
}

static gboolean
on_immediate_many_for_one_task(HrtTask        *task,
                               HrtWatcherFlags flags,
                               void           *data)
{
    void *sleeping;
    TestFixture *fixture = data;

    /* Use a flag to verify that multiple immediates don't run at once. */
    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping == NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", GINT_TO_POINTER(1));

    g_usleep(G_USEC_PER_SEC / 20);

    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping != NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", NULL);

    fixture->tasks[0].immediates_run_count += 1;

    return FALSE;
}

/* the main point of this test is that the immediates run in serial
 * and each runs exactly once
 */
static void
test_one_task_many_immediates(TestFixture *fixture,
                              const void  *data)
{
    HrtTask *task;
    int i;

    task =
        hrt_task_runner_create_task(fixture->runner);
    fixture->tasks_started_count += 1;

    /* note that we serialize all immediates for a task so making this
     * a large number will take forever
     */
#define NUM_IMMEDIATES 7
    for (i = 0; i < NUM_IMMEDIATES; ++i) {
        hrt_task_add_immediate(task,
                               on_immediate_many_for_one_task,
                               fixture,
                               on_dnotify_bump_count);
    }

    g_main_loop_run(fixture->loop);

    g_assert_cmpint(fixture->tasks_completed_count, ==, 1);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, NUM_IMMEDIATES);
    /* we should have run each immediate exactly once */
    g_assert_cmpint(fixture->tasks[0].immediates_run_count, ==, NUM_IMMEDIATES);
}

static gboolean
on_immediate_for_many_tasks(HrtTask        *task,
                            HrtWatcherFlags flags,
                            void           *data)
{
    void *sleeping;
    TestFixture *fixture = data;
    int i, j;

    g_assert(flags == HRT_WATCHER_FLAG_NONE);

    for (i = 0; i < NUM_TASKS; ++i) {
        if (fixture->tasks[i].task == task)
            break;
    }
    g_assert(i < NUM_TASKS);

    g_assert(g_atomic_int_get(&fixture->tasks[i].in_an_immediate) == 0);

    for (j = 0; j < NUM_TASKS; ++j) {
        if (g_atomic_int_get(&fixture->tasks[j].in_an_immediate)) {
            g_assert(i != j);
            fixture->tasks[i].saw_another_immediate_in_an_immediate_count += 1;
            break;
        }
    }

    g_atomic_int_set(&fixture->tasks[i].in_an_immediate, 1);

    /* Use a flag to verify that multiple immediates don't run at once
     * on same task.
     */
    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping == NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", GINT_TO_POINTER(1));

    g_usleep(G_USEC_PER_SEC / 20);

    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping != NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", NULL);

    g_atomic_int_set(&fixture->tasks[i].in_an_immediate, 0);

    fixture->tasks[i].immediates_run_count += 1;

    return FALSE;
}

static void
test_many_tasks_many_immediates(TestFixture *fixture,
                                const void  *data)
{
    int i, j;
    gboolean some_overlap;
    GTimer *timer;
    GString *overlap_report;

    /* this has to be set up front of there's a race in using it to
     * decide to quit mainloop, because task runner starts running
     * tasks right away, doesn't wait for our local mainloop
     */
    fixture->tasks_started_count = NUM_TASKS;

    for (i = 0; i < NUM_TASKS; ++i) {
        HrtTask *task;

        task = hrt_task_runner_create_task(fixture->runner);
        fixture->tasks[i].task = task;

        /* note that we serialize all immediates for a task so making this
         * a large number will take forever
         */
#define NUM_IMMEDIATES 7
        for (j = 0; j < NUM_IMMEDIATES; ++j) {
            hrt_task_add_immediate(task,
                                   on_immediate_for_many_tasks,
                                   fixture,
                                   on_dnotify_bump_count);
        }
    }

    timer = g_timer_new();

    g_main_loop_run(fixture->loop);

    /* we don't want an assertion based on timing, will fail too often,
     * but print it for manual checking sometimes.
     */
    g_test_message("%g seconds elapsed to run lots of tasks that should have each taken 0.7 seconds\n",
                   g_timer_elapsed(timer, NULL));
    g_timer_destroy(timer);

    g_assert_cmpint(fixture->tasks_completed_count, ==, NUM_TASKS);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, NUM_IMMEDIATES * NUM_TASKS);
    /* we should have run each immediate exactly once */
    for (i = 0; i < NUM_TASKS; ++i) {
        g_assert(fixture->tasks[i].immediates_run_count == NUM_IMMEDIATES);
    }

    /* unfortunately this isn't strictly guaranteed, but it should be
     * nearly certain. If it keeps failing, increase number of immediates
     * and number of tasks.
     */
    some_overlap = FALSE;
    for (i = 0; i < NUM_TASKS; ++i) {
        if (fixture->tasks[i].saw_another_immediate_in_an_immediate_count > 0)
            some_overlap = TRUE;
    }
    g_assert(some_overlap);

    overlap_report = g_string_new(NULL);
    for (i = 0; i < NUM_TASKS; ++i) {
        g_string_append_printf(overlap_report,
                               " %d",
                               fixture->tasks[i].saw_another_immediate_in_an_immediate_count);
    }
    g_test_message("# of immediates of %d run during at least one other task's immediate:\n  %s\n",
                   NUM_IMMEDIATES,
                   overlap_report->str);
    g_string_free(overlap_report, TRUE);
#undef NUM_IMMEDIATES
}

static gboolean
on_immediate_for_performance_many_watchers(HrtTask        *task,
                                           HrtWatcherFlags flags,
                                           void           *data)
{
    /* TestFixture *fixture = data; */

    return FALSE;
}

static void
test_immediate_performance_n_watchers(TestFixture *fixture,
                                      const void  *data,
                                      int          n_watchers)
{
    int i, j;

    if (!g_test_perf())
        return;

    /* this has to be set up front of there's a race in using it to
     * decide to quit mainloop, because task runner starts running
     * tasks right away, doesn't wait for our local mainloop
     */
    fixture->tasks_started_count = NUM_TASKS;

    /* start here, to include task creation. Also, immediates can start
     * running right away, before we block in main loop.
     */
    g_test_timer_start();

    for (i = 0; i < NUM_TASKS; ++i) {
        HrtTask *task;

        task =
            hrt_task_runner_create_task(fixture->runner);

        fixture->tasks[i].task = task;
    }

    /* If we added n_watchers immediates to task 0, then task 1, then 2,
     * etc.  then we'd never use any parallelism because we'd just
     * have one task active at a time using only one thread.  By doing
     * the loop this way we get some use of multiple threads in
     * theory. Also this is more "real world" in that most likely
     * tasks do some work, add an event loop source, do some work,
     * etc. instead of just adding a pile of sources from the
     * same task all at once. This more "real world" scenario is
     * less efficient and slows down the benchmark.
     */
    for (j = 0; j < n_watchers; ++j) {
        for (i = 0; i < NUM_TASKS; ++i) {
            HrtTask *task = fixture->tasks[i].task;
            hrt_task_add_immediate(task,
                                   on_immediate_for_performance_many_watchers,
                                   fixture,
                                   on_dnotify_bump_count);
        }
    }

    g_main_loop_run(fixture->loop);

    g_test_minimized_result(g_test_timer_elapsed(),
                            "Run %d tasks with %d immediates each",
                            NUM_TASKS, n_watchers);

    g_assert_cmpint(fixture->tasks_completed_count, ==, NUM_TASKS);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, n_watchers * NUM_TASKS);
}

static void
test_immediate_performance_many_watchers(TestFixture *fixture,
                                         const void  *data)
{
    test_immediate_performance_n_watchers(fixture, data, 100000);
}

static gboolean
on_immediate_for_performance_many_tasks(HrtTask        *task,
                                        HrtWatcherFlags flags,
                                        void           *data)
{
    /* TestFixture *fixture = data; */

    return FALSE;
}

static void
test_immediate_performance_n_tasks(TestFixture *fixture,
                                   const void  *data,
                                   int          n_tasks)
{
    int i, j;

    if (!g_test_perf())
        return;

    /* this has to be set up front of there's a race in using it to
     * decide to quit mainloop, because task runner starts running
     * tasks right away, doesn't wait for our local mainloop
     */
    fixture->tasks_started_count = n_tasks;

    /* start here, to include task creation. Also, immediates can start
     * running right away, before we block in main loop.
     */
    g_test_timer_start();

    for (i = 0; i < n_tasks; ++i) {
        HrtTask *task;

        task =
            hrt_task_runner_create_task(fixture->runner);

#define NUM_IMMEDIATES 4
        for (j = 0; j < NUM_IMMEDIATES; ++j) {
            hrt_task_add_immediate(task,
                                   on_immediate_for_performance_many_tasks,
                                   fixture,
                                   on_dnotify_bump_count);
        }
    }

    g_main_loop_run(fixture->loop);

    g_test_minimized_result(g_test_timer_elapsed(),
                            "Run %d tasks with %d immediates each",
                            n_tasks, NUM_IMMEDIATES);

    g_assert_cmpint(fixture->tasks_completed_count, ==, n_tasks);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, NUM_IMMEDIATES * n_tasks);

#undef NUM_IMMEDIATES
}

static void
test_immediate_performance_many_tasks(TestFixture *fixture,
                                      const void  *data)
{
    test_immediate_performance_n_tasks(fixture, data, 1000000);
}

static void
setup_test_fixture_block_completion_glib(TestFixture *fixture,
                                         const void  *data)
{
    setup_test_fixture_glib(fixture, data);
    fixture->completion_should_be_blocked = TRUE;
}

static void
setup_test_fixture_block_completion_libev(TestFixture *fixture,
                                          const void  *data)
{
    setup_test_fixture_libev(fixture, data);
    fixture->completion_should_be_blocked = TRUE;
}

/* this is a timeout that runs in the main thread */
static gboolean
on_timeout_completion_check(void *data)
{
    TestFixture *fixture = data;

    fixture->completion_check_timeout_ran = TRUE;

    if (fixture->completion_should_be_blocked) {
        if (fixture->tasks_completed_count != 0) {
            g_error("The task was completed even though we had it blocked count=%d",
                    fixture->tasks_completed_count);
        }

        /* Now unblock, so we quit the main loop */
        hrt_task_unblock_completion(fixture->tasks[0].task);
    } else {
        /* In this case we should have quit the main loop without
         * running the timeout, so this should never be reached.
         */
        if (fixture->tasks_completed_count != 1) {
            g_error("The task was not completed even though we didn't block it count=%d",
                    fixture->tasks_completed_count);
        }
    }

    return FALSE;
}

/* this is an immediate watcher that runs in task thread */
static gboolean
on_immediate_for_block_completion_test(HrtTask        *task,
                                       HrtWatcherFlags flags,
                                       void           *data)
{
    TestFixture *fixture = data;

    fixture->tasks[0].immediates_run_count += 1;

    /* add a glib main loop timeout that should then run in main thread and
     * check we did not complete yet.
     */
    fixture->completion_check_timeout_id =
        g_timeout_add(100, on_timeout_completion_check,
                      fixture);

    return FALSE;
}

static void
test_immediate_block_completion(TestFixture *fixture,
                                const void  *data)
{
    HrtTask *task;

    task =
        hrt_task_runner_create_task(fixture->runner);

    fixture->tasks[0].task = task;

    fixture->tasks_started_count += 1;

    if (fixture->completion_should_be_blocked) {
        hrt_task_block_completion(task);
    }

    fixture->tasks[0].watcher =
        hrt_task_add_immediate(task,
                               on_immediate_for_block_completion_test,
                               fixture,
                               on_dnotify_bump_count);

    /* When the task is completed, the main loop quits.  From the
     * task, we add a timeout to the main loop. If the main loop quits
     * before the timeout runs, then we completed without waiting to
     * unblock.
     */
    g_main_loop_run(fixture->loop);

    g_source_remove(fixture->completion_check_timeout_id);

    g_assert_cmpint(fixture->tasks_completed_count, ==, 1);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, 1);
    g_assert_cmpint(fixture->tasks[0].immediates_run_count, ==, 1);

    if (fixture->completion_should_be_blocked) {
        g_assert(fixture->completion_check_timeout_ran);
    } else {
        g_assert(!fixture->completion_check_timeout_ran);
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

    context = g_option_context_new("- Test Suite Immediate Events");
    g_option_context_add_main_entries(context, entries, "test-immediate");

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    if (option_version) {
        g_print("test-immediate %s\n",
                VERSION);
        exit(0);
    }

    hrt_log_init(option_debug ?
                 HRT_LOG_FLAG_DEBUG : 0);

    g_test_add("/immediate/immediate_that_sleeps_manual_remove_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_immediate_that_sleeps_manual_remove,
               teardown_test_fixture);

    g_test_add("/immediate/immediate_that_sleeps_return_false_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_immediate_that_sleeps_return_false,
               teardown_test_fixture);

    g_test_add("/immediate/immediate_runs_several_times_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_immediate_runs_several_times,
               teardown_test_fixture);

    g_test_add("/immediate/one_task_many_immediates_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_one_task_many_immediates,
               teardown_test_fixture);

    g_test_add("/immediate/many_tasks_many_immediates_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_many_tasks_many_immediates,
               teardown_test_fixture);

    g_test_add("/immediate/performance_many_watchers_few_tasks_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_immediate_performance_many_watchers,
               teardown_test_fixture);

    g_test_add("/immediate/performance_many_tasks_few_watchers_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_immediate_performance_many_tasks,
               teardown_test_fixture);

    g_test_add("/immediate/immediate_that_sleeps_manual_remove_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_immediate_that_sleeps_manual_remove,
               teardown_test_fixture);

    g_test_add("/immediate/immediate_that_sleeps_return_false_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_immediate_that_sleeps_return_false,
               teardown_test_fixture);

    g_test_add("/immediate/immediate_runs_several_times_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_immediate_runs_several_times,
               teardown_test_fixture);

    g_test_add("/immediate/one_task_many_immediates_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_one_task_many_immediates,
               teardown_test_fixture);

    g_test_add("/immediate/many_tasks_many_immediates_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_many_tasks_many_immediates,
               teardown_test_fixture);

    g_test_add("/immediate/performance_many_watchers_few_tasks_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_immediate_performance_many_watchers,
               teardown_test_fixture);

    g_test_add("/immediate/performance_many_tasks_few_watchers_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_immediate_performance_many_tasks,
               teardown_test_fixture);

    /* Check that our code runs one way WITHOUT blocking completion */
    g_test_add("/immediate/no_block_completion_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_immediate_block_completion,
               teardown_test_fixture);

    g_test_add("/immediate/no_block_completion_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_immediate_block_completion,
               teardown_test_fixture);

    /* Check that it runs another way WITH block completion */
    g_test_add("/immediate/block_completion_libev",
               TestFixture,
               NULL,
               setup_test_fixture_block_completion_libev,
               test_immediate_block_completion,
               teardown_test_fixture);

    g_test_add("/immediate/block_completion_glib",
               TestFixture,
               NULL,
               setup_test_fixture_block_completion_glib,
               test_immediate_block_completion,
               teardown_test_fixture);

    return g_test_run();
}
