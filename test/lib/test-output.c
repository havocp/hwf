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
#include <hio/hio-output-chain.h>
#include <hrt/hrt-task.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>

typedef struct {
    const char *seed;
    gsize seed_len;
    int state;
} StreamGenerator;

static void
stream_generator_init(StreamGenerator *generator,
                      const char      *seed)
{
    generator->seed = seed;
    generator->seed_len = strlen(seed);
    generator->state = 0;
}

static void
stream_generator_generate(StreamGenerator *generator,
                          char            *buf,
                          gsize            count)
{
    gsize i;

    for (i = 0; i < count; ++i) {
        buf[i] = generator->seed[generator->state % generator->seed_len];
        generator->state += 1;
    }
}

static gboolean
stream_generator_verify(StreamGenerator *generator,
                        const char      *buf,
                        gsize            count)
{
    gsize i;

    for (i = 0; i < count; ++i) {
        char expected = generator->seed[generator->state % generator->seed_len];
        if (buf[i] != expected) {
            g_debug("Got %c expected %c",
                    buf[i], expected);
            return FALSE;
        }
        generator->state += 1;
    }

    return TRUE;
}

typedef struct {
    const char *name;
    const char *seed;
    gsize length;
} StreamDesc;

/* each one-stream test (vs. chain test) is run with each of these streams. */
static const StreamDesc single_stream_tests[] = {
    { "1byte", "This stream will write 'T' one byte of data. ", 1 },
    { "0bytes", "This stream will write no data, just close immediately. ", 0 },
    { "100k", "This stream will write 100k of data. ", 1024 * 100 }
};

static const StreamDesc various_stream_descs[] = {
    /* the names don't matter for this, we just want a variety of
     * streams to put in a chain mostly to verify they're written
     * in order. Make the first one short so basic failures not
     * dependent on length will have a minimum of noise.
     */
    { "1", "This is a sentence that goes on and on and on. ", 100 },
    { "2", "abcdefghijklmnopqrstuvwxyz", 15000 },
    { "3", "123456789", 231 },
    { "4", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 35232 },
    { "5", "The quick brown fox jumped over the lazy dog. ", 1234 },
    { "6", "This is a zero-length stream. ", 0 }
};

static void*
buffer_g_malloc(gsize bytes,
                void *allocator_data)
{
    return g_try_malloc(bytes);
}

static void
buffer_g_free(void *mem,
              void *allocator_data)
{
    g_free(mem);
}

static void*
buffer_g_realloc(void *mem,
                 gsize bytes,
                 void *allocator_data)
{
    return g_try_realloc(mem, bytes);
}

static const HrtBufferAllocator allocator = {
    buffer_g_malloc,
    buffer_g_free,
    buffer_g_realloc
};

static void
write_stream(HioOutputStream  *stream,
             const StreamDesc *desc)
{
    StreamGenerator generator;
    gsize remaining;

    stream_generator_init(&generator, desc->seed);
    remaining = desc->length;

    while (remaining > 0) {
        char buf[48];
        gsize count;
        HrtBuffer *buffer;

        count = MIN(G_N_ELEMENTS(buf), remaining);
        stream_generator_generate(&generator,
                                  &buf[0],
                                  count);

        buffer = hrt_buffer_new(HRT_BUFFER_ENCODING_UTF8,
                                &allocator,
                                NULL, NULL);
        hrt_buffer_append_ascii(buffer, &buf[0], count);
        hrt_buffer_lock(buffer);

        hio_output_stream_write(stream, buffer);

        hrt_buffer_unref(buffer);

        g_assert(count <= remaining);
        remaining -= count;
    }
}

static void
read_and_verify_stream(int               fd,
                       const StreamDesc *desc)
{
    StreamGenerator generator;
    gsize remaining;

    stream_generator_init(&generator, desc->seed);
    remaining = desc->length;

    while (remaining > 0) {
        gssize bytes_read;
        char buf[123];
        gsize count;

        count = MIN(G_N_ELEMENTS(buf), remaining);

    again:
        bytes_read = read(fd, &buf[0], count);

        if (bytes_read < 0) {
            if (errno == EINTR ||
                errno == EAGAIN ||
                errno == EWOULDBLOCK) {
                goto again;
            } else {
                g_error("Failed to read %d: %s", fd, strerror(errno));
            }
        } else if (bytes_read == 0) {
            /* all done */
            g_error("Got EOF on fd but have not read the entire stream yet");
        } else {
            if (!stream_generator_verify(&generator,
                                         &buf[0],
                                         bytes_read)) {
                g_error("Did not read the expected stream of bytes");
            }

            remaining -= bytes_read;
        }
    }
}

static void
create_socketpair(int *read_end_p,
                  int *write_end_p)
{
    int sv[2];
    int flags;

    if (socketpair(AF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0,
                   &sv[0]) < 0) {
        g_error("socketpair failed: %s", strerror(errno));
    }
    *read_end_p = sv[0];
    *write_end_p = sv[1];

    /* nonblock only the end we'll write to */
    flags = fcntl(*write_end_p,
                  F_GETFL);
    if (fcntl(*write_end_p,
              F_SETFL,
              flags | O_NONBLOCK) < 0) {
        g_error("F_SETFL failed: %s", strerror(errno));
    }
}

typedef struct {
    HrtTaskRunner *runner;
    HioOutputChain *chain;
    GMainLoop *loop;

    const StreamDesc *stream_descs;
    int n_stream_descs;

    int read_fd;
    int write_fd;

    /* The possible tasks (each task can be in a separate thread) are:
     *
     * - the chain task used to run the HioOutputChain
     * - the stream task used to write to the fd for each stream in chain
     * - the write task used to hio_output_stream_write() feeding data
     *   into each stream
     *
     * All these tasks can be the same, or all different,
     * and things are supposed to work. We run the tests in
     * multiple scenarios:
     *
     * - all tasks the same - serializes everything
     * - a chain task, a task shared by all streams, and a task shared by all writers.
     *   serializes things somewhat.
     * - a separate task for chain, each stream, each writer. Total
     *   free-for-all.
     *
     */
#define N_STREAMS_IN_CHAIN 50
    HrtTask *chain_task;
    HrtTask *stream_tasks[N_STREAMS_IN_CHAIN];
    HrtTask *write_tasks[N_STREAMS_IN_CHAIN];

    int tasks_started_count;
    int tasks_completed_count;
} OutputTestFixture;

static void
on_tasks_completed(HrtTaskRunner *runner,
                   void          *data)
{
    OutputTestFixture *fixture = data;
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

typedef struct {
    HioOutputStream *stream;
    const StreamDesc *desc;
} WriteTaskData;

static WriteTaskData*
write_task_data_new(HioOutputStream  *stream,
                    const StreamDesc *desc)
{
    WriteTaskData *wtd;

    wtd = g_new0(WriteTaskData, 1);
    wtd->stream = g_object_ref(stream);
    wtd->desc = desc;

    return wtd;
}

static void
write_task_data_free(void *data)
{
    WriteTaskData *wtd = data;
    g_object_unref(wtd->stream);
    g_free(wtd);
}

static gboolean
on_write_stream_task(HrtTask        *task,
                     HrtWatcherFlags flags,
                     void           *data)
{
    WriteTaskData *wtd = data;

    write_stream(wtd->stream, wtd->desc);

    hio_output_stream_close(wtd->stream);

    return FALSE;
}

typedef enum {
    TASK_SCENARIO_ALL_ONE,
    TASK_SCENARIO_THREE,
    TASK_SCENARIO_ALL_DISTINCT
} TaskScenario;

static void
setup(OutputTestFixture *fixture,
      const void        *data,
      TaskScenario       task_scenario,
      gboolean           with_chain)
{
    if (with_chain) {
        fixture->stream_descs = &various_stream_descs[0];
        fixture->n_stream_descs = G_N_ELEMENTS(various_stream_descs);
    } else {
        fixture->stream_descs = data;
        fixture->n_stream_descs = 1;
    }

    fixture->loop =
        g_main_loop_new(NULL, FALSE);

    fixture->runner =
        g_object_new(HRT_TYPE_TASK_RUNNER,
                     "event-loop-type", HRT_EVENT_LOOP_EV,
                     NULL);

    g_signal_connect(G_OBJECT(fixture->runner),
                     "tasks-completed",
                     G_CALLBACK(on_tasks_completed),
                     fixture);

    switch (task_scenario) {
    case TASK_SCENARIO_ALL_ONE: {
        HrtTask *task;
        int i;

        fixture->tasks_started_count = 1;

        task = hrt_task_runner_create_task(fixture->runner);

        for (i = 0; i < N_STREAMS_IN_CHAIN; ++i) {
            fixture->write_tasks[i] = g_object_ref(task);
            fixture->stream_tasks[i] = g_object_ref(task);
        }

        if (with_chain) {
            fixture->chain_task = task;
        } else {
            g_object_unref(task);
        }
    }
        break;
    case TASK_SCENARIO_THREE: {
        HrtTask *task;
        int i;

        /* with no chain there's no chain task so really two ;-) */
        if (with_chain) {
            task = hrt_task_runner_create_task(fixture->runner);

            fixture->chain_task = task;

            fixture->tasks_started_count = 3;
        } else {
            fixture->tasks_started_count = 2;
        }

        task = hrt_task_runner_create_task(fixture->runner);

        for (i = 0; i < N_STREAMS_IN_CHAIN; ++i) {
            fixture->write_tasks[i] = g_object_ref(task);
        }

        g_object_unref(task);

        task = hrt_task_runner_create_task(fixture->runner);

        for (i = 0; i < N_STREAMS_IN_CHAIN; ++i) {
            fixture->stream_tasks[i] = g_object_ref(task);
        }

        g_object_unref(task);
    }
        break;
    case TASK_SCENARIO_ALL_DISTINCT: {
        int i;

        if (with_chain) {
            HrtTask *task;

            task = hrt_task_runner_create_task(fixture->runner);

            fixture->chain_task = task;

            fixture->tasks_started_count = 1 + N_STREAMS_IN_CHAIN * 2;
        } else {
            fixture->tasks_started_count = N_STREAMS_IN_CHAIN * 2;
        }

        for (i = 0; i < N_STREAMS_IN_CHAIN; ++i) {
            fixture->write_tasks[i] = hrt_task_runner_create_task(fixture->runner);
            fixture->stream_tasks[i] = hrt_task_runner_create_task(fixture->runner);
        }
    }
        break;
    }

    if (with_chain) {
        fixture->chain = hio_output_chain_new(fixture->chain_task);
    }

    create_socketpair(&fixture->read_fd,
                      &fixture->write_fd);
}

/* setup with the same task writing out to fd and generating stream
 * data
 */
static void
setup_one_task_no_chain(OutputTestFixture *fixture,
                        const void        *data)
{
    setup(fixture, data, TASK_SCENARIO_ALL_ONE, FALSE);
}

/* setup with the stream data coming from a separate task */
static void
setup_two_tasks_no_chain(OutputTestFixture *fixture,
                         const void        *data)
{
    /* with no chain there's no chain task so really two ;-) */
    setup(fixture, data, TASK_SCENARIO_THREE, FALSE);
}

/* with no chain, ALL_DISTINCT would be the same as THREE because there's
 * only one stream anyway. So no setup_all_distinct_tasks_no_chain().
 */

static void
setup_one_task_with_chain(OutputTestFixture *fixture,
                          const void        *data)
{
    setup(fixture, data, TASK_SCENARIO_ALL_ONE, TRUE);
}

/* setup with the stream data coming from a separate task, writing to
 * fd with its task, and chain has its own task
 */
static void
setup_three_tasks_with_chain(OutputTestFixture *fixture,
                             const void        *data)
{
    setup(fixture, data, TASK_SCENARIO_THREE, TRUE);
}

/* setup with every stream having two tasks (write to fd, generate data)
 * and chain has its own task
 */
static void
setup_all_distinct_tasks_with_chain(OutputTestFixture *fixture,
                                    const void        *data)
{
    setup(fixture, data, TASK_SCENARIO_ALL_DISTINCT, TRUE);
}


static void
teardown(OutputTestFixture *fixture,
         const void        *data)
{
    int i;

    if (fixture->read_fd >= 0)
        close(fixture->read_fd);

    if (fixture->write_fd >= 0)
        close(fixture->write_fd);

    if (fixture->chain) {
        g_object_unref(fixture->chain);
    }

    if (fixture->chain_task) {
        g_object_unref(fixture->chain_task);
    }

    for (i = 0; i < N_STREAMS_IN_CHAIN; ++i) {
        g_object_unref(fixture->write_tasks[i]);
        g_object_unref(fixture->stream_tasks[i]);
    }

    g_object_unref(fixture->runner);
    g_main_loop_unref(fixture->loop);
}

static void
test_stream(OutputTestFixture *fixture,
            const void        *data)
{
    HioOutputStream *stream;

    g_assert(fixture->chain_task == NULL);

    stream = hio_output_stream_new(fixture->stream_tasks[0]);
    hio_output_stream_set_fd(stream, fixture->write_fd);

    hrt_task_add_immediate(fixture->write_tasks[0],
                           on_write_stream_task,
                           write_task_data_new(stream, fixture->stream_descs),
                           write_task_data_free);

    read_and_verify_stream(fixture->read_fd, fixture->stream_descs);

    /* run main loop to collect the task */
    g_main_loop_run(fixture->loop);

    g_assert(!hio_output_stream_got_error(stream));
    g_assert(hio_output_stream_is_done(stream));
    g_assert(hio_output_stream_is_closed(stream));

    g_object_unref(stream);
}

static void
test_stream_with_error(OutputTestFixture *fixture,
                       const void        *data)
{
    HioOutputStream *stream;

    g_assert(fixture->chain_task == NULL);

    close(fixture->read_fd); /* should error the writes */

    stream = hio_output_stream_new(fixture->stream_tasks[0]);
    hio_output_stream_set_fd(stream, fixture->write_fd);

    hrt_task_add_immediate(fixture->write_tasks[0],
                           on_write_stream_task,
                           write_task_data_new(stream, fixture->stream_descs),
                           write_task_data_free);

    /* run main loop to collect the tasks */
    g_main_loop_run(fixture->loop);

    /* if the stream is zero-length we'll never try to write and never get an error */
    g_assert(fixture->stream_descs->length == 0 ||
             hio_output_stream_got_error(stream));
    g_assert(hio_output_stream_is_done(stream));
    g_assert(hio_output_stream_is_closed(stream));

    g_object_unref(stream);

    /* so we don't close it again in teardown */
    fixture->read_fd = -1;
}

static void
test_stream_with_initial_error(OutputTestFixture *fixture,
                               const void        *data)
{
    HioOutputStream *stream;

    g_assert(fixture->chain_task == NULL);

    /* error before we even set an fd on it */
    stream = hio_output_stream_new(fixture->stream_tasks[0]);
    hio_output_stream_error(stream);

    hrt_task_add_immediate(fixture->write_tasks[0],
                           on_write_stream_task,
                           write_task_data_new(stream, fixture->stream_descs),
                           write_task_data_free);

    /* run main loop to collect the tasks */
    g_main_loop_run(fixture->loop);

    g_assert(hio_output_stream_got_error(stream));
    g_assert(hio_output_stream_is_done(stream));
    g_assert(hio_output_stream_is_closed(stream));

    g_object_unref(stream);
}

static void
on_chain_empty(HioOutputChain    *chain,
               void              *data)
{
    /* break the refcount cycle by removing this notify */
    hio_output_chain_set_empty_notify(chain, NULL, NULL, NULL);

    /* not allowed to finalize a chain with fd still set. */
    hio_output_chain_set_fd(chain, -1);
}

static gboolean
on_start_streams_in_chain(HrtTask        *task,
                          HrtWatcherFlags flags,
                          void           *data)
{
    OutputTestFixture *fixture = data;
    int i;

    /* Fire up all streams */
    for (i = 0; i < N_STREAMS_IN_CHAIN; ++i) {
        HioOutputStream *stream;

        stream = hio_output_stream_new(fixture->stream_tasks[i]);

        hrt_task_add_immediate(fixture->write_tasks[i],
                               on_write_stream_task,
                               write_task_data_new(stream,
                                                   &fixture->stream_descs[i % fixture->n_stream_descs]),
                               write_task_data_free);

        hio_output_chain_add_stream(fixture->chain, stream);

        g_object_unref(stream);
    }

    hio_output_chain_set_empty_notify(fixture->chain,
                                      on_chain_empty,
                                      /* reference cycle we'll
                                       * break by unsetting the
                                       * chain when notified
                                       */
                                      g_object_ref(fixture->chain),
                                      g_object_unref);

    /* start the writing! */
    hio_output_chain_set_fd(fixture->chain, fixture->write_fd);

    return FALSE;
}

static void
test_chain(OutputTestFixture *fixture,
           const void        *data)
{
    int i;

    g_assert(fixture->chain_task != NULL);

    /* it isn't allowed to do stuff with chains except in the chain's
     * task. So setup all the streams in the chain task.
     */
    hrt_task_add_immediate(fixture->chain_task,
                           on_start_streams_in_chain,
                           fixture,
                           NULL);

    /* Read all streams in order */
    for (i = 0; i < N_STREAMS_IN_CHAIN; ++i) {
        read_and_verify_stream(fixture->read_fd,
                               &fixture->stream_descs[i % fixture->n_stream_descs]);
    }

    /* run main loop to collect the tasks */
    g_main_loop_run(fixture->loop);

    g_assert(!hio_output_chain_got_error(fixture->chain));
    g_assert(hio_output_chain_is_empty(fixture->chain));
}

static void
test_chain_with_error(OutputTestFixture *fixture,
                      const void        *data)
{
    g_assert(fixture->chain_task != NULL);

    /* make writing to write_fd error */
    close(fixture->read_fd);

    /* it isn't allowed to do stuff with chains except in the chain's
     * task. So setup all the streams in the chain task.
     */
    hrt_task_add_immediate(fixture->chain_task,
                           on_start_streams_in_chain,
                           fixture,
                           NULL);

    /* run main loop to collect the tasks */
    g_main_loop_run(fixture->loop);

    g_assert(hio_output_chain_got_error(fixture->chain));
    g_assert(hio_output_chain_is_empty(fixture->chain));

    /* so we don't close it again in teardown */
    fixture->read_fd = -1;
}

static void
add_one_stream_test(const char *base_name,
                    void (* func) (OutputTestFixture*, gconstpointer))
{
    gsize i;

    for (i = 0; i < G_N_ELEMENTS(single_stream_tests); ++i) {
        char *s;
        s = g_strdup_printf("/output/%s_one_task_%s",
                            base_name, single_stream_tests[i].name);
        g_test_add(s,
                   OutputTestFixture,
                   &single_stream_tests[i],
                   setup_one_task_no_chain,
                   func,
                   teardown);
        g_free(s);

        s = g_strdup_printf("/output/%s_two_tasks_%s",
                            base_name, single_stream_tests[i].name);
        g_test_add(s,
                   OutputTestFixture,
                   &single_stream_tests[i],
                   setup_two_tasks_no_chain,
                   func,
                   teardown);
        g_free(s);
    }
}

static void
add_chain_test(const char *base_name,
               void (* func) (OutputTestFixture*, gconstpointer))
{
    char *s;

    s = g_strdup_printf("/output/%s_one_task",
                        base_name);
    g_test_add(s,
               OutputTestFixture,
               NULL,
               setup_one_task_with_chain,
               func,
               teardown);
    g_free(s);

    s = g_strdup_printf("/output/%s_three_tasks",
                        base_name);
    g_test_add(s,
               OutputTestFixture,
               NULL,
               setup_three_tasks_with_chain,
               func,
               teardown);
    g_free(s);

    s = g_strdup_printf("/output/%s_all_distinct_tasks",
                        base_name);
    g_test_add(s,
               OutputTestFixture,
               NULL,
               setup_all_distinct_tasks_with_chain,
               func,
               teardown);
    g_free(s);
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

    context = g_option_context_new("- Test Suite Output");
    g_option_context_add_main_entries(context, entries, "test-output");

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    if (option_version) {
        g_print("test-output %s\n",
                VERSION);
        exit(0);
    }

    hrt_log_init(option_debug ?
                 HRT_LOG_FLAG_DEBUG : 0);

    add_one_stream_test("stream",
                        test_stream);

    add_one_stream_test("stream_with_error",
                        test_stream_with_error);

    add_one_stream_test("stream_with_initial_error",
                        test_stream_with_initial_error);

    add_chain_test("chain",
                   test_chain);

    add_chain_test("chain_with_error",
                   test_chain_with_error);

    return g_test_run();
}
