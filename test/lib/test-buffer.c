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
#include <hrt/hrt-buffer.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    HrtBuffer *buffer;
    int allocator_dnotify_count;
    gboolean used_our_allocator;
} BufferTestFixture;

static void*
buffer_g_malloc(gsize bytes,
                void *allocator_data)
{
    /* break if allocator isn't used */
    void *mem;
    mem = g_try_malloc(bytes + 4);
    return mem ? ((char*)mem) + 4 : NULL;
}

static void
buffer_g_free(void *mem,
              void *allocator_data)
{
    g_free((((char*)mem) - 4));
}

static void*
buffer_g_realloc(void *mem,
                 gsize bytes,
                 void *allocator_data)
{
    void *new_mem;

    new_mem = g_try_realloc((((char*)mem) - 4), bytes + 4);

    return new_mem ? ((char*)new_mem) + 4 : NULL;
}

static const HrtBufferAllocator allocator = {
    buffer_g_malloc,
    buffer_g_free,
    buffer_g_realloc
};


static const char *ascii_alphabet_chunks[] = {
    "",
    "a",
    "",
    "b",
    "",
    "c",
    "defghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
};

static const char *ascii_alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static void
allocator_dnotify(void *data)
{
    BufferTestFixture *fixture = data;
    fixture->allocator_dnotify_count += 1;
}

static void
setup_utf8(BufferTestFixture *fixture,
           const void        *data)
{
    fixture->buffer =
        hrt_buffer_new(HRT_BUFFER_ENCODING_UTF8,
                       &allocator, fixture, allocator_dnotify);
    fixture->used_our_allocator = TRUE;
}

static void
setup_utf8_copy(BufferTestFixture *fixture,
                const void        *data)
{
    fixture->buffer =
        hrt_buffer_new_copy_utf8("");
}

static void
setup_utf16(BufferTestFixture *fixture,
            const void        *data)
{
    fixture->buffer =
        hrt_buffer_new(HRT_BUFFER_ENCODING_UTF16,
                       &allocator, fixture, allocator_dnotify);
    fixture->used_our_allocator = TRUE;
}

static void
setup_utf8_static(BufferTestFixture *fixture,
                  const void        *data)
{
    fixture->buffer =
        hrt_buffer_new_static_utf8_locked(ascii_alphabet);
}

static void
teardown(BufferTestFixture *fixture,
         const void        *data)
{
    int old_count;

    old_count = fixture->allocator_dnotify_count;

    hrt_buffer_unref(fixture->buffer);

    if (fixture->used_our_allocator) {
        g_assert_cmpint(old_count + 1, ==, fixture->allocator_dnotify_count);
    } else {
        g_assert_cmpint(old_count, ==, fixture->allocator_dnotify_count);
    }
}

static void
append_ascii_alphabet_in_chunks(HrtBuffer *buffer)
{
    unsigned int i;

    for (i = 0; i < G_N_ELEMENTS(ascii_alphabet_chunks); ++i) {
        hrt_buffer_append_ascii(buffer, ascii_alphabet_chunks[i], strlen(ascii_alphabet_chunks[i]));
    }
}

static void
test_utf16_append_ascii(BufferTestFixture *fixture,
                        const void        *data)
{
    gsize i;
    const guint16 *utf16;
    gsize len;
    gsize ascii_len;

    append_ascii_alphabet_in_chunks(fixture->buffer);
    hrt_buffer_lock(fixture->buffer);

    hrt_buffer_peek_utf16(fixture->buffer, &utf16, &len);

    ascii_len = strlen(ascii_alphabet);

    g_assert_cmpint(ascii_len, ==, len);
    g_assert(utf16[len] == 0); /* should be nul-terminated */

    for (i = 0; i < len; ++i) {
        g_assert_cmpint((int) ascii_alphabet[i], ==, (int) utf16[i]);
    }
}

static void
test_utf8_append_ascii(BufferTestFixture *fixture,
                       const void        *data)
{
    const char *utf8;
    gsize len;
    append_ascii_alphabet_in_chunks(fixture->buffer);
    hrt_buffer_lock(fixture->buffer);
    hrt_buffer_peek_utf8(fixture->buffer, &utf8, &len);
    g_assert(utf8[len] == '\0'); /* should be nul-terminated */
    g_assert_cmpstr(ascii_alphabet, ==, utf8);
}

static void
test_utf16_steal(BufferTestFixture *fixture,
                 const void        *data)
{
    gsize i;
    guint16 *utf16;
    gsize len;
    gsize ascii_len;

    /* this test also tests appending the alphabet in one big chunk */
    ascii_len = strlen(ascii_alphabet);
    hrt_buffer_append_ascii(fixture->buffer,
                            ascii_alphabet,
                            ascii_len);
    hrt_buffer_lock(fixture->buffer);

    hrt_buffer_steal_utf16(fixture->buffer, &utf16, &len);

    g_assert_cmpint(ascii_len, ==, len);
    g_assert(utf16[len] == 0); /* should be nul-terminated */

    for (i = 0; i < len; ++i) {
        g_assert_cmpint((int) ascii_alphabet[i], ==, (int) utf16[i]);
    }

    allocator.free(utf16, fixture);
}

static void
test_utf8_steal(BufferTestFixture *fixture,
                const void        *data)
{
    char *utf8;
    gsize len;
    gsize ascii_len;

    /* this test also tests appending the alphabet in one big chunk */
    ascii_len = strlen(ascii_alphabet);
    hrt_buffer_append_ascii(fixture->buffer,
                            ascii_alphabet,
                            ascii_len);
    hrt_buffer_lock(fixture->buffer);

    hrt_buffer_steal_utf8(fixture->buffer, &utf8, &len);

    g_assert_cmpint(ascii_len, ==, len);
    g_assert(utf8[len] == '\0'); /* should be nul-terminated */
    g_assert_cmpstr(ascii_alphabet, ==, utf8);

    allocator.free(utf8, fixture);
}

static void
test_utf8_static(BufferTestFixture *fixture,
                 const void        *data)
{
    const char *utf8;
    gsize len;
    gsize ascii_len;

    g_assert(hrt_buffer_is_locked(fixture->buffer));

    ascii_len = strlen(ascii_alphabet);

    hrt_buffer_peek_utf8(fixture->buffer, &utf8, &len);

    g_assert_cmpint(ascii_len, ==, len);
    g_assert(utf8[len] == '\0'); /* should be nul-terminated */
    g_assert_cmpstr(ascii_alphabet, ==, utf8);
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

    context = g_option_context_new("- Test Suite Buffer");
    g_option_context_add_main_entries(context, entries, "test-buffer");

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    if (option_version) {
        g_print("test-buffer %s\n",
                VERSION);
        exit(0);
    }

    hrt_log_init(option_debug ?
                 HRT_LOG_FLAG_DEBUG : 0);

    g_test_add("/buffer/utf16_append_ascii",
               BufferTestFixture,
               NULL,
               setup_utf16,
               test_utf16_append_ascii,
               teardown);

    g_test_add("/buffer/utf8_append_ascii",
               BufferTestFixture,
               NULL,
               setup_utf8,
               test_utf8_append_ascii,
               teardown);

    g_test_add("/buffer/utf8_copy_append_ascii",
               BufferTestFixture,
               NULL,
               setup_utf8_copy,
               test_utf8_append_ascii,
               teardown);

    g_test_add("/buffer/utf16_steal",
               BufferTestFixture,
               NULL,
               setup_utf16,
               test_utf16_steal,
               teardown);

    g_test_add("/buffer/utf8_steal",
               BufferTestFixture,
               NULL,
               setup_utf8,
               test_utf8_steal,
               teardown);

    g_test_add("/buffer/utf8_static",
               BufferTestFixture,
               NULL,
               setup_utf8_static,
               test_utf8_static,
               teardown);

    return g_test_run();
}
