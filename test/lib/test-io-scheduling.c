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

/* This file is basically a copy of test-idle.c and is primarily just
 * making sure that IO tasks are properly scheduled and removed.
 * test-io.c is more a test of IO behavior proper, and has IO handler
 * performance tests.
 */

#include <config.h>
#include <glib-object.h>
#include <hrt/hrt-log.h>
#include <hrt/hrt-task-runner.h>
#include <hrt/hrt-task.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

#define NUM_TASKS 100

typedef struct {
    HrtTaskRunner *runner;
    int tasks_started_count;
    int tasks_completed_count;
    /* dnotify_count is accessed by multiple task threads so needs to be atomic */
    volatile int dnotify_count;
    GMainLoop *loop;
    /* this is a full-duplex pipe we have one byte in we never read,
     * and we never write to so it doesn't fill up and is always ready
     * to write.
     */
    int always_ready_fd;
    int always_ready_fd_other_end;
    struct {
        HrtTask *task;
        HrtWatcher *watcher;
        int ios_run_count;
        int saw_another_io_in_an_io_count;
        /* this field is read by multiple task threads so needs to be atomic */
        volatile int in_an_io;
    } tasks[NUM_TASKS];
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
    int sv[2];
    char buf[1];

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

    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
                   0, &sv[0]) < 0)
        g_error("socketpair() failed: %s", strerror(errno));

    fixture->always_ready_fd = sv[0];
    fixture->always_ready_fd_other_end = sv[1];

    buf[0] = 'a';
    if (write(fixture->always_ready_fd_other_end,
              buf, 1) != 1)
        g_error("write() failed: %s", strerror(errno));
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
    close(fixture->always_ready_fd);
    close(fixture->always_ready_fd_other_end);
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
on_io_sleep_manual_remove(HrtTask        *task,
                          HrtWatcherFlags flags,
                          void           *data)
{
    void *sleeping;
    TestFixture *fixture = data;

    g_assert(flags == HRT_WATCHER_FLAG_READ);

    /* Use a flag to verify that multiple ios don't run at once. */
    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping == NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", GINT_TO_POINTER(1));

    g_usleep(G_USEC_PER_SEC / 20);

    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping != NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", NULL);

    hrt_watcher_remove(fixture->tasks[0].watcher);
    fixture->tasks[0].watcher = NULL;

    fixture->tasks[0].ios_run_count += 1;

    return TRUE;
}

static void
test_io_that_sleeps_manual_remove(TestFixture *fixture,
                                  const void  *data)
{
    HrtTask *task;

    task = hrt_task_runner_create_task(fixture->runner);

    fixture->tasks_started_count += 1;

    fixture->tasks[0].watcher =
        hrt_task_add_io(task,
                        fixture->always_ready_fd,
                        HRT_WATCHER_FLAG_READ,
                        on_io_sleep_manual_remove,
                        fixture,
                        on_dnotify_bump_count);

    g_main_loop_run(fixture->loop);

    g_assert_cmpint(fixture->tasks_completed_count, ==, 1);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, 1);
    g_assert_cmpint(fixture->tasks[0].ios_run_count, ==, 1);
}

static gboolean
on_io_sleep_return_false(HrtTask        *task,
                         HrtWatcherFlags flags,
                         void           *data)
{
    void *sleeping;
    TestFixture *fixture = data;

    g_assert(flags == HRT_WATCHER_FLAG_READ);

    /* Use a flag to verify that multiple ios don't run at once. */
    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping == NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", GINT_TO_POINTER(1));

    g_usleep(G_USEC_PER_SEC / 20);

    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping != NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", NULL);

    fixture->tasks[0].ios_run_count += 1;

    return FALSE;
}

static void
test_io_that_sleeps_return_false(TestFixture *fixture,
                                 const void  *data)
{
    HrtTask *task;

    task = hrt_task_runner_create_task(fixture->runner);

    fixture->tasks_started_count += 1;

    hrt_task_add_io(task,
                    fixture->always_ready_fd,
                    HRT_WATCHER_FLAG_READ,
                    on_io_sleep_return_false,
                    fixture,
                    on_dnotify_bump_count);

    g_main_loop_run(fixture->loop);

    g_assert_cmpint(fixture->tasks_completed_count, ==, 1);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, 1);
    g_assert_cmpint(fixture->tasks[0].ios_run_count, ==, 1);
}

static gboolean
on_io_many_for_one_task(HrtTask        *task,
                        HrtWatcherFlags flags,
                        void           *data)
{
    void *sleeping;
    TestFixture *fixture = data;

    /* Use a flag to verify that multiple ios don't run at once. */
    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping == NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", GINT_TO_POINTER(1));

    g_usleep(G_USEC_PER_SEC / 20);

    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping != NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", NULL);

    fixture->tasks[0].ios_run_count += 1;

    return FALSE;
}

/* the main point of this test is that the ios run in serial
 * and each runs exactly once
 */
static void
test_one_task_many_ios(TestFixture *fixture,
                       const void  *data)
{
    HrtTask *task;
    int i;

    task = hrt_task_runner_create_task(fixture->runner);

    fixture->tasks_started_count += 1;

    /* note that we serialize all ios for a task so making this
     * a large number will take forever
     */
#define NUM_IOS 7
    for (i = 0; i < NUM_IOS; ++i) {
        hrt_task_add_io(task,
                        fixture->always_ready_fd,
                        HRT_WATCHER_FLAG_READ,
                        on_io_many_for_one_task,
                        fixture,
                        on_dnotify_bump_count);
    }

    g_main_loop_run(fixture->loop);

    g_assert_cmpint(fixture->tasks_completed_count, ==, 1);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, NUM_IOS);
    /* we should have run each io exactly once */
    g_assert_cmpint(fixture->tasks[0].ios_run_count, ==, NUM_IOS);
}

static gboolean
on_io_for_many_tasks(HrtTask        *task,
                     HrtWatcherFlags flags,
                     void           *data)
{
    void *sleeping;
    TestFixture *fixture = data;
    int i, j;

    g_assert(flags == HRT_WATCHER_FLAG_READ);

    for (i = 0; i < NUM_TASKS; ++i) {
        if (fixture->tasks[i].task == task)
            break;
    }
    g_assert(i < NUM_TASKS);

    g_assert(g_atomic_int_get(&fixture->tasks[i].in_an_io) == 0);

    for (j = 0; j < NUM_TASKS; ++j) {
        if (g_atomic_int_get(&fixture->tasks[j].in_an_io)) {
            g_assert(i != j);
            fixture->tasks[i].saw_another_io_in_an_io_count += 1;
            break;
        }
    }

    g_atomic_int_set(&fixture->tasks[i].in_an_io, 1);

    /* Use a flag to verify that multiple ios don't run at once
     * on same task.
     */
    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping == NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", GINT_TO_POINTER(1));

    g_usleep(G_USEC_PER_SEC / 20);

    sleeping = g_object_get_data(G_OBJECT(task), "sleeping");
    g_assert(sleeping != NULL);
    g_object_set_data(G_OBJECT(task), "sleeping", NULL);

    g_atomic_int_set(&fixture->tasks[i].in_an_io, 0);

    fixture->tasks[i].ios_run_count += 1;

    return FALSE;
}

static void
test_many_tasks_many_ios(TestFixture *fixture,
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

        /* note that we serialize all ios for a task so making this
         * a large number will take forever
         */
#define NUM_IOS 7
        for (j = 0; j < NUM_IOS; ++j) {
            hrt_task_add_io(task,
                            fixture->always_ready_fd,
                            HRT_WATCHER_FLAG_READ,
                            on_io_for_many_tasks,
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
    g_assert_cmpint(fixture->dnotify_count, ==, NUM_IOS * NUM_TASKS);
    /* we should have run each io exactly once */
    for (i = 0; i < NUM_TASKS; ++i) {
        g_assert(fixture->tasks[i].ios_run_count == NUM_IOS);
    }

    /* unfortunately this isn't strictly guaranteed, but it should be
     * nearly certain. If it keeps failing, increase number of ios
     * and number of tasks.
     */
    some_overlap = FALSE;
    for (i = 0; i < NUM_TASKS; ++i) {
        if (fixture->tasks[i].saw_another_io_in_an_io_count > 0)
            some_overlap = TRUE;
    }
    g_assert(some_overlap);

    overlap_report = g_string_new(NULL);
    for (i = 0; i < NUM_TASKS; ++i) {
        g_string_append_printf(overlap_report,
                               " %d",
                               fixture->tasks[i].saw_another_io_in_an_io_count);
    }
    g_test_message("# of ios of %d run during at least one other task's io:\n  %s\n",
                   NUM_IOS,
                   overlap_report->str);
    g_string_free(overlap_report, TRUE);
#undef NUM_IOS
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

    context = g_option_context_new("- Test Suite Io Event Scheduling");
    g_option_context_add_main_entries(context, entries, "test-io-scheduling");

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    if (option_version) {
        g_print("test-io %s\n",
                VERSION);
        exit(0);
    }

    hrt_log_init(option_debug ?
                 HRT_LOG_FLAG_DEBUG : 0);

    g_test_add("/io_scheduling/io_that_sleeps_manual_remove_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_io_that_sleeps_manual_remove,
               teardown_test_fixture);

    g_test_add("/io_scheduling/io_that_sleeps_return_false_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_io_that_sleeps_return_false,
               teardown_test_fixture);

    g_test_add("/io_scheduling/one_task_many_ios_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_one_task_many_ios,
               teardown_test_fixture);

    g_test_add("/io_scheduling/many_tasks_many_ios_glib",
               TestFixture,
               NULL,
               setup_test_fixture_glib,
               test_many_tasks_many_ios,
               teardown_test_fixture);

    g_test_add("/io_scheduling/io_that_sleeps_manual_remove_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_io_that_sleeps_manual_remove,
               teardown_test_fixture);

    g_test_add("/io_scheduling/io_that_sleeps_return_false_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_io_that_sleeps_return_false,
               teardown_test_fixture);

    g_test_add("/io_scheduling/one_task_many_ios_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_one_task_many_ios,
               teardown_test_fixture);

    g_test_add("/io_scheduling/many_tasks_many_ios_libev",
               TestFixture,
               NULL,
               setup_test_fixture_libev,
               test_many_tasks_many_ios,
               teardown_test_fixture);

    return g_test_run();
}
