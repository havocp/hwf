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
#include <hrt/hrt-thread-pool.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    HrtThreadPool *pool;

    volatile int sum;

    GMutex *processed_lock;
    GSList *processed;
} TestFixture;

typedef struct {
    int value;
    GThread *ran_in_thread;
} WorkItem;

static void
sum_item(void *item_data,
         void *handler_data)
{
    TestFixture *fixture = handler_data;
    WorkItem *item = item_data;

    g_atomic_int_add(&fixture->sum, item->value);

    item->ran_in_thread = g_thread_self();

    g_mutex_lock(fixture->processed_lock);
    fixture->processed =
        g_slist_prepend(fixture->processed, item);
    g_mutex_unlock(fixture->processed_lock);
}

static void
setup_test_fixture(TestFixture *fixture,
                   const void  *data)
{
    fixture->processed_lock = g_mutex_new();
}

static void
teardown_test_fixture(TestFixture *fixture,
                      const void  *data)
{
    g_mutex_free(fixture->processed_lock);
}

static void
process_items(TestFixture *fixture,
              int          n_items)
{
    int i;
    int sum;
    int n_threads;

    fixture->pool =
        hrt_thread_pool_new_func(sum_item,
                                 fixture,
                                 NULL);

    sum = 0;
    for (i = 0; i < n_items; ++i) {
        WorkItem *item = g_slice_new(WorkItem);

        item->value = i;
        hrt_thread_pool_push(fixture->pool, item);

        sum += item->value;
    }

    hrt_thread_pool_shutdown(fixture->pool);

    g_assert_cmpint(sum, ==, fixture->sum);
    g_assert_cmpint(n_items, ==, g_slist_length(fixture->processed));

    /* this test is not reliable with a small number of items */
    if (n_items > 100000) {
        GHashTable *thread_stats;
        guint average_per_thread;
        GHashTableIter iter;
        gpointer key, value;

        thread_stats = g_hash_table_new(g_direct_hash, g_direct_equal);
        while (fixture->processed) {
            WorkItem *item = fixture->processed->data;
            unsigned int thread_count;

            fixture->processed =
                g_slist_delete_link(fixture->processed,
                                    fixture->processed);

            thread_count = GPOINTER_TO_UINT(g_hash_table_lookup(thread_stats,
                                                                item->ran_in_thread));
            thread_count += 1;
            g_hash_table_replace(thread_stats,
                                 item->ran_in_thread,
                                 GUINT_TO_POINTER(thread_count));

            g_slice_free(WorkItem, item);
        }

        n_threads = g_hash_table_size(thread_stats);

        /* This is not guaranteed; things could be scheduled such that we
         * run every item in the same thread according to letter of specs,
         * probably. But if it happens in real life, something is surely
         * busted.
         */
        g_assert_cmpint(n_threads, >=, 2);

        average_per_thread = n_items / n_threads;

#if 0
        g_hash_table_iter_init(&iter, thread_stats);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            unsigned int thread_count = GPOINTER_TO_UINT(value);
            g_debug("%u", thread_count);
        }
#endif

        g_hash_table_iter_init(&iter, thread_stats);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            unsigned int thread_count = GPOINTER_TO_UINT(value);

            /* This isn't guaranteed at all, but for large numbers of
             * items in the test it seems to be reliable enough.  If the
             * test falsely fails occasionally we'll live.
             */
            g_assert_cmpint(average_per_thread / 2, <, thread_count);
            g_assert_cmpint(average_per_thread * 2, >, thread_count);
        }

        g_hash_table_destroy(thread_stats);
    } else {
        while (fixture->processed) {
            WorkItem *item = fixture->processed->data;

            fixture->processed =
                g_slist_delete_link(fixture->processed,
                                    fixture->processed);

            g_slice_free(WorkItem, item);
        }
    }

    fixture->sum = 0;
    g_object_unref(fixture->pool);
}

static void
test_pool_processes_items(TestFixture *fixture,
                          const void  *data)
{
    process_items(fixture, 300000);
}

static void
test_pool_shutdown(TestFixture *fixture,
                   const void  *data)
{
    int i;

    /* Shutdown a pool that never ran any tasks */
    fixture->pool =
        hrt_thread_pool_new_func(sum_item,
                                 fixture,
                                 NULL);
    hrt_thread_pool_shutdown(fixture->pool);
    g_object_unref(fixture->pool);

    /* Shutdown a pool multiple times */
    fixture->pool =
        hrt_thread_pool_new_func(sum_item,
                                 fixture,
                                 NULL);
    hrt_thread_pool_shutdown(fixture->pool);
    hrt_thread_pool_shutdown(fixture->pool); /* no-op */
    hrt_thread_pool_shutdown(fixture->pool); /* no-op */
    g_object_unref(fixture->pool);

    /* Do a bunch of run-stuff-then-shutdown iterations
     * to try and trigger shutdown races
     */
    for (i = 0; i < 1000; ++i) {
        process_items(fixture, 200);
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

    context = g_option_context_new("- Test Suite Thread Pool");
    g_option_context_add_main_entries(context, entries, "test-thread-pool");

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    if (option_version) {
        g_print("test-thread-pool %s\n",
                VERSION);
        exit(0);
    }

    hrt_log_init(option_debug ?
                 HRT_LOG_FLAG_DEBUG : 0);

    g_test_add("/thread_pool/processes_items",
               TestFixture,
               NULL,
               setup_test_fixture,
               test_pool_processes_items,
               teardown_test_fixture);

    g_test_add("/thread_pool/shutdown",
               TestFixture,
               NULL,
               setup_test_fixture,
               test_pool_shutdown,
               teardown_test_fixture);

    return g_test_run();
}
