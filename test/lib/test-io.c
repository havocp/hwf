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

/* test-io-scheduling.c has some further tests of IO handlers. */

#define _GNU_SOURCE 1
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
#include <signal.h>
#include <fcntl.h>

/* Redefine g_debug to see debug stuff */
#undef g_debug
#define g_debug(format...)

static const char sample_text[] =
    "Everybodys building the big ships and the boats, "
    "Some are building monuments, "
    "Others, jotting down notes, "
    "Everybodys in despair, "
    "Every girl and boy ";

typedef struct {
    HrtTask *task;
    int fd;
    int chunk;
    GString *buf;
} ReadTask;

typedef struct {
    HrtTask *task;
    int fd;
    int chunk;
    int bytes_so_far;
} WriteTask;

typedef struct {
    HrtTask *task;
    int fd;
    int chunk;
    int bytes_so_far;
    GString *buf;
    gboolean done_reading;
    gboolean done_writing;
} ReadWriteTask;

typedef struct {
    HrtTaskRunner *runner;
    int tasks_started_count;
    int tasks_completed_count;
    /* dnotify_count is accessed by multiple task threads so needs to be atomic */
    volatile int dnotify_count;
    GMainLoop *loop;
    int n_tasks;
    ReadTask *read_tasks;
    WriteTask *write_tasks;
    ReadWriteTask *read_write_tasks;
} TestFixture;


#define SOME_FDS 40
#define MANY_FDS 10000

static void
increase_ulimit_whine(void)
{
    const char whine[] =
        "\nTOO MANY OPEN FILES\n"
        "  You need to add to /etc/security/limits.conf:\n"
        "     * soft nofile 60000\n"
        "     * hard nofile 60000\n"
        "  and to /etc/pam.d/common-session:\n"
        "     session required pam_limits.so\n"
        "  and then log out and back in.\n"
        "INCREASE YOUR MAX OPEN FILES LIMIT\n";
    g_printerr("%s\n", whine);
    g_test_message("%s", whine);
}

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
                           HrtEventLoopType loop_type,
                           int              n_tasks)
{
    int i;
    int n_sockets;

    fixture->n_tasks = n_tasks;

    if (fixture->n_tasks == MANY_FDS &&
        !g_test_perf())
        return;

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

    fixture->read_tasks = g_new0(ReadTask, n_tasks);
    fixture->write_tasks = g_new0(WriteTask, n_tasks);
    fixture->read_write_tasks = g_new0(ReadWriteTask, n_tasks);

    g_assert(n_tasks % 2 == 0);
    n_sockets = n_tasks / 2;

    /* Test READ and WRITE pure tasks */
    for (i = 0; i < n_tasks; ++i) {
        int sv[2];
        if (pipe2(&sv[0], O_CLOEXEC | O_NONBLOCK) < 0) {
            if (errno == EMFILE)
                increase_ulimit_whine();
            g_error("pipe() failed: %s", strerror(errno));
        }
        fixture->read_tasks[i].fd = sv[0];
        fixture->write_tasks[i].fd = sv[1];
        g_debug("read and write %d %d", sv[0], sv[1]);
    }

    /* Test READ and WRITE in the same task */
    for (i = 0; i < n_sockets; ++i) {
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
                       0, &sv[0]) < 0) {
            if (errno == EMFILE)
                increase_ulimit_whine();
            g_error("socketpair() failed: %s", strerror(errno));
        }
        fixture->read_write_tasks[i].fd = sv[0];
        fixture->read_write_tasks[n_sockets + i].fd = sv[1];

        g_debug("readwrite %d %d", sv[0], sv[1]);
    }

    /* allocate bufs to read to */
    for (i = 0; i < n_tasks; ++i) {
        fixture->read_tasks[i].buf = g_string_new(NULL);
        fixture->read_write_tasks[i].buf = g_string_new(NULL);
    }

    /* vary the chunk sizes (mismatching reads and writes) */
    for (i = 0; i < n_tasks; ++i) {
        fixture->read_tasks[i].chunk = i % (sizeof(sample_text));
        if (fixture->read_tasks[i].chunk == 0)
            fixture->read_tasks[i].chunk = 1;
        g_debug("read chunk %d", fixture->read_tasks[i].chunk);
        fixture->write_tasks[i].chunk = sizeof(sample_text) - (i % (sizeof(sample_text)));
        if (fixture->write_tasks[i].chunk == 0)
            fixture->write_tasks[i].chunk = 1;
        g_debug("write chunk %d", fixture->write_tasks[i].chunk);
        /* here we use same size of read and write */
        fixture->read_write_tasks[i].chunk = i % (sizeof(sample_text));
        if (fixture->read_write_tasks[i].chunk == 0)
            fixture->read_write_tasks[i].chunk = 1;
        g_debug("readwrite chunk %d", fixture->read_write_tasks[i].chunk);
    }
}

static void
setup_test_fixture_some_fds_glib(TestFixture *fixture,
                                 const void  *data)
{
    setup_test_fixture_generic(fixture, HRT_EVENT_LOOP_GLIB, SOME_FDS);
}

static void
setup_test_fixture_some_fds_libev(TestFixture *fixture,
                                  const void  *data)
{
    setup_test_fixture_generic(fixture, HRT_EVENT_LOOP_EV, SOME_FDS);
}

static void
setup_test_fixture_many_fds_glib(TestFixture *fixture,
                                 const void  *data)
{
    setup_test_fixture_generic(fixture, HRT_EVENT_LOOP_GLIB, MANY_FDS);
}

static void
setup_test_fixture_many_fds_libev(TestFixture *fixture,
                                  const void  *data)
{
    setup_test_fixture_generic(fixture, HRT_EVENT_LOOP_EV, MANY_FDS);
}

static void
teardown_test_fixture(TestFixture *fixture,
                      const void  *data)
{
    int i;

    if (fixture->n_tasks == MANY_FDS &&
        !g_test_perf())
        return;

    for (i = 0; i < fixture->n_tasks; ++i) {
        close(fixture->read_tasks[i].fd);
        close(fixture->write_tasks[i].fd);
        close(fixture->read_write_tasks[i].fd);
        g_string_free(fixture->read_tasks[i].buf, TRUE);
        g_string_free(fixture->read_write_tasks[i].buf, TRUE);
    }
    g_free(fixture->read_tasks);
    g_free(fixture->write_tasks);
    g_free(fixture->read_write_tasks);

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
do_writing(int  fd,
           int *bytes_so_far_p,
           int  chunk)
{
    int bytes_written;
    int remaining;

    remaining = sizeof(sample_text) - *bytes_so_far_p;
    if (remaining == 0)
        return FALSE; /* all done */

    chunk = MIN(chunk, remaining);

    g_debug("writing %d to %d", chunk, fd);

    bytes_written =
        write(fd, &sample_text[*bytes_so_far_p],
              chunk);
    if (bytes_written < 0) {
        if (errno == EINTR ||
            errno == EAGAIN ||
            errno == EWOULDBLOCK)
            return TRUE; /* not done */
        else
            g_error("Failed to write to %d: %s", fd, strerror(errno));
    }
    *bytes_so_far_p += bytes_written;

    return TRUE; /* not done */
}

static gboolean
do_reading(int      fd,
           GString *buf,
           int      chunk)
{
    char *b;
    int bytes_read;

    b = g_alloca(chunk);

    g_debug("reading %d from %d", chunk, fd);

    bytes_read = read(fd, b, chunk);
    if (bytes_read < 0) {
        if (errno == EINTR ||
            errno == EAGAIN ||
            errno == EWOULDBLOCK) {
            g_debug("read: %s", strerror(errno));
            return TRUE; /* not done */
        } else {
            g_error("Failed to read %d: %s", fd, strerror(errno));
        }
    } else if (bytes_read == 0) {
        g_debug("read EOF from %d", fd);
        g_assert_cmpstr(sample_text, ==, buf->str);
        return FALSE; /* all done */
    } else {
        g_debug("read %d from %d", bytes_read, fd);
        g_string_append_len(buf, b, bytes_read);
        return TRUE; /* not done */
    }
}

static gboolean
on_do_read_task(HrtTask        *task,
                HrtWatcherFlags flags,
                void           *data)
{
    /* TestFixture *fixture = data; */
    ReadTask *rt = g_object_get_data(G_OBJECT(task), "read-task");

    g_debug("read callback %d", rt->fd);

    g_assert(flags == HRT_WATCHER_FLAG_READ);

    if (!do_reading(rt->fd, rt->buf, rt->chunk)) {
        g_debug("read %d completed", rt->fd);
        return FALSE;
    } else {
        return TRUE;
    }
}

static gboolean
on_do_write_task(HrtTask        *task,
                 HrtWatcherFlags flags,
                 void           *data)
{
    /* TestFixture *fixture = data; */
    WriteTask *wt = g_object_get_data(G_OBJECT(task), "write-task");

    g_debug("write callback %d", wt->fd);

    g_assert(flags == HRT_WATCHER_FLAG_WRITE);

    if (!do_writing(wt->fd, &wt->bytes_so_far, wt->chunk)) {
        /* we have to close to create EOF so the reader stops */
        g_debug("== closing %d", wt->fd);
        close(wt->fd);
        wt->fd = -1;
        g_debug("write completed");
        return FALSE;
    } else {
        return TRUE;
    }
}

static gboolean
on_do_read_write_task(HrtTask        *task,
                      HrtWatcherFlags flags,
                      void           *data)
{
    /* TestFixture *fixture = data; */
    ReadWriteTask *rwt = g_object_get_data(G_OBJECT(task), "read-write-task");

    g_debug("readwrite callback %d", rwt->fd);

    g_assert(flags != HRT_WATCHER_FLAG_NONE);

    if (flags & HRT_WATCHER_FLAG_WRITE) {
        /* if we're done writing there's an unfortunate
         * busy loop here but too hard to make it go away.
         */
        if (!rwt->done_writing) {
            rwt->done_writing = !do_writing(rwt->fd, &rwt->bytes_so_far, rwt->chunk);
        }
    }

    if (flags & HRT_WATCHER_FLAG_READ) {
        if (!rwt->done_reading) {
            rwt->done_reading = !do_reading(rwt->fd, rwt->buf, rwt->chunk);

            /* with the readwrite fd, we can't rely on getting EOF
             * because we can't be closing fds we may need to read from
             * just because we're done writing. So instead we're
             * done reading when we get the full text.
             */
            if (!rwt->done_reading &&
                rwt->buf->len == sizeof(sample_text)) {
                rwt->done_reading = TRUE;
            }
        }
    }

    if (rwt->done_writing && rwt->done_reading) {
        g_debug("readwrite completed");
        return FALSE;
    } else {
        return TRUE;
    }
}

static void
test_io_n_fds(TestFixture *fixture,
              const void  *data)
{
    int i;

    if (fixture->n_tasks == MANY_FDS &&
        !g_test_perf())
        return;

    /* this has to be set up front or there's a race in using it to
     * decide to quit mainloop, because task runner starts running
     * tasks right away, doesn't wait for our local mainloop
     */
    fixture->tasks_started_count = fixture->n_tasks * 3;

    /* start here, to include task creation. Also, watchers can start
     * running right away, before we block in main loop.
     */
    g_test_timer_start();

    /* add all reads first, since they'll just block until
     * we add the writers
     */
    for (i = 0; i < fixture->n_tasks; ++i) {
        HrtTask *task;

        task = hrt_task_runner_create_task(fixture->runner);
        fixture->read_tasks[i].task = task;

        g_object_set_data(G_OBJECT(task), "read-task",
                          &fixture->read_tasks[i]);

        g_debug("adding read %d",
                fixture->read_tasks[i].fd);
        hrt_task_add_io(task,
                        fixture->read_tasks[i].fd,
                        HRT_WATCHER_FLAG_READ,
                        on_do_read_task,
                        fixture,
                        on_dnotify_bump_count);
    }

    for (i = 0; i < fixture->n_tasks; ++i) {
        HrtTask *task;

        task = hrt_task_runner_create_task(fixture->runner);
        fixture->write_tasks[i].task = task;

        g_object_set_data(G_OBJECT(task), "write-task",
                          &fixture->write_tasks[i]);

        g_debug("adding write %d",
                fixture->write_tasks[i].fd);
        hrt_task_add_io(task,
                        fixture->write_tasks[i].fd,
                        HRT_WATCHER_FLAG_WRITE,
                        on_do_write_task,
                        fixture,
                        on_dnotify_bump_count);
    }

    for (i = 0; i < fixture->n_tasks; ++i) {
        HrtTask *task;

        task = hrt_task_runner_create_task(fixture->runner);
        fixture->read_write_tasks[i].task = task;

        g_object_set_data(G_OBJECT(task), "read-write-task",
                          &fixture->read_write_tasks[i]);

        g_debug("adding readwrite %d",
                fixture->read_write_tasks[i].fd);
        hrt_task_add_io(task,
                        fixture->read_write_tasks[i].fd,
                        HRT_WATCHER_FLAG_READ |
                        HRT_WATCHER_FLAG_WRITE,
                        on_do_read_write_task,
                        fixture,
                        on_dnotify_bump_count);
    }

    g_main_loop_run(fixture->loop);

    g_test_minimized_result(g_test_timer_elapsed(),
                            "Run %d each of read, write, and readwrite tasks",
                            fixture->n_tasks);

    g_assert_cmpint(fixture->tasks_completed_count, ==, fixture->n_tasks * 3);
    g_assert_cmpint(fixture->tasks_completed_count, ==,
                    fixture->tasks_started_count);
    g_assert_cmpint(fixture->dnotify_count, ==, fixture->n_tasks * 3);
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

    signal(EPIPE, SIG_IGN);

    context = g_option_context_new("- Test Suite Io Events");
    g_option_context_add_main_entries(context, entries, "test-io");

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

    g_test_add("/io/some_fds_glib",
               TestFixture,
               NULL,
               setup_test_fixture_some_fds_glib,
               test_io_n_fds,
               teardown_test_fixture);

    g_test_add("/io/performance_many_fds_glib",
               TestFixture,
               NULL,
               setup_test_fixture_many_fds_glib,
               test_io_n_fds,
               teardown_test_fixture);

    g_test_add("/io/some_fds_libev",
               TestFixture,
               NULL,
               setup_test_fixture_some_fds_libev,
               test_io_n_fds,
               teardown_test_fixture);

    g_test_add("/io/performance_many_fds_libev",
               TestFixture,
               NULL,
               setup_test_fixture_many_fds_libev,
               test_io_n_fds,
               teardown_test_fixture);

    return g_test_run();
}
