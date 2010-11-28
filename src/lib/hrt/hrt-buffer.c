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
#include <hrt/hrt-buffer.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>


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

static const HrtBufferAllocator g_allocator = {
    buffer_g_malloc,
    buffer_g_free,
    buffer_g_realloc
};

static void*
buffer_static_malloc(gsize bytes,
                     void *allocator_data)
{
    g_error("static buffer should not malloc");
    return NULL;
}

static void
buffer_static_free(void *mem,
                   void *allocator_data)
{
    /* no-op */
}

static void*
buffer_static_realloc(void *mem,
                      gsize bytes,
                      void *allocator_data)
{
    g_error("static buffer should not realloc");
    return NULL;
}

static const HrtBufferAllocator static_allocator = {
    buffer_static_malloc,
    buffer_static_free,
    buffer_static_realloc
};


typedef struct {
    HrtBufferEncoding encoding;
    void        (* finalize)       (HrtBuffer  *buffer);
    gsize       (* get_write_size) (HrtBuffer  *buffer);
    const void* (* get_write_data) (HrtBuffer  *buffer);
    void        (* append_ascii)   (HrtBuffer  *buffer,
                                    const char *bytes,
                                    gsize       len);
} HrtEncodingClass;

struct HrtBuffer {
    volatile int refcount;
    const HrtBufferAllocator  *allocator;
    void *allocator_data;
    GDestroyNotify allocator_data_dnotify;
    const HrtEncodingClass *encoding;
    unsigned int locked : 1;
    /* number of array elements, not bytes or encoded chars  */
    unsigned int length : 31;
    union {
        struct {
            guint16 *data;
            /* this is number of array elements, not bytes or encoded chars */
            gsize allocated;
        } buf_16;
        struct {
            char *data;
            gsize allocated;
        } buf_8;
    } d;
};

static void
buf8_finalize(HrtBuffer *buffer)
{
    if (buffer->d.buf_8.data != NULL) {
        buffer->allocator->free(buffer->d.buf_8.data,
                                buffer->allocator_data);
    }
}

static gsize
buf8_get_write_size(HrtBuffer *buffer)
{
    return buffer->length;
}

static const void*
buf8_get_write_data(HrtBuffer *buffer)
{
    return buffer->d.buf_8.data;
}

static void
buf16_finalize(HrtBuffer *buffer)
{
    if (buffer->d.buf_16.data != NULL) {
        buffer->allocator->free(buffer->d.buf_16.data,
                                buffer->allocator_data);
    }
}

static gsize
buf16_get_write_size(HrtBuffer *buffer)
{
    return buffer->length * 2;
}

static const void*
buf16_get_write_data(HrtBuffer *buffer)
{
    return buffer->d.buf_16.data;
}

static void
utf8_append_ascii(HrtBuffer  *buffer,
                  const char *bytes,
                  gsize       len)
{
    gsize new_needed;
    char *dest;

    new_needed = buffer->length + len + 1; /* 1 for nul, always auto-nul */
    if (new_needed > buffer->d.buf_8.allocated) {
        if (buffer->d.buf_8.allocated == 0) {
            /* The common case is we never realloc, so alloc exact length
             * the first time.
             */
            buffer->d.buf_8.data = buffer->allocator->malloc(new_needed,
                                                             buffer->allocator_data);
            if (buffer->d.buf_8.data == NULL)
                g_error("Failed to allocate %u bytes", new_needed);
            buffer->d.buf_8.allocated = new_needed;
        } else {
            /* overly cute way to approximately double each time */
            gsize new_allocated = new_needed + buffer->d.buf_8.allocated;
            buffer->d.buf_8.data =
                buffer->allocator->realloc(buffer->d.buf_8.data, new_allocated,
                                           buffer->allocator_data);
            if (buffer->d.buf_8.data == NULL)
                g_error("Failed to reallocate %u bytes", new_needed);
            buffer->d.buf_8.allocated = new_allocated;
        }
    }
    dest = buffer->d.buf_8.data + buffer->length;
    memcpy(dest, bytes, len);
    dest[len] = '\0';

    buffer->length = buffer->length + len;
}

static const HrtEncodingClass utf8_encoding = {
    HRT_BUFFER_ENCODING_UTF8,
    buf8_finalize,
    buf8_get_write_size,
    buf8_get_write_data,
    utf8_append_ascii
};

static void
utf16_append_ascii(HrtBuffer  *buffer,
                   const char *bytes,
                   gsize       len)
{
    gsize new_needed;
    guint16 *dest;
    const char *src;
    gsize i;

    new_needed = buffer->length + len + 1; /* 1 for nul, always auto-nul */
    if (new_needed > buffer->d.buf_16.allocated) {
        if (buffer->d.buf_16.allocated == 0) {
            /* The common case is we never realloc, so alloc exact length
             * the first time.
             */
            buffer->d.buf_16.data = buffer->allocator->malloc(new_needed * sizeof(guint16),
                                                              buffer->allocator_data);
            if (buffer->d.buf_16.data == NULL)
                g_error("Failed to allocate %u*2 bytes", new_needed);
            buffer->d.buf_16.allocated = new_needed;
        } else {
            /* overly cute way to approximately double each time */
            gsize new_allocated = new_needed + buffer->d.buf_16.allocated;
            buffer->d.buf_16.data =
                buffer->allocator->realloc(buffer->d.buf_16.data, new_allocated * sizeof(guint16),
                                           buffer->allocator_data);
            if (buffer->d.buf_16.data == NULL)
                g_error("Failed to allocate %u*2 bytes", new_allocated);
            buffer->d.buf_16.allocated = new_allocated;
        }
    }
    dest = buffer->d.buf_16.data + buffer->length;
    src = bytes;
    for (i = 0; i < len; ++i) {
        dest[i] = src[i];
    }
    dest[i] = 0;

    buffer->length = buffer->length + len;
}

static const HrtEncodingClass utf16_encoding = {
    HRT_BUFFER_ENCODING_UTF16,
    buf16_finalize,
    buf16_get_write_size,
    buf16_get_write_data,
    utf16_append_ascii
};

HrtBuffer*
hrt_buffer_new(HrtBufferEncoding         encoding,
               const HrtBufferAllocator *allocator,
               void                     *allocator_data,
               GDestroyNotify            dnotify)
{
    HrtBuffer *buffer;

    g_return_val_if_fail(encoding != HRT_BUFFER_ENCODING_INVALID, NULL);
    g_return_val_if_fail(allocator != NULL,
                         NULL);

    buffer = g_slice_new0(HrtBuffer);

    buffer->refcount = 1;
    switch (encoding) {
    case HRT_BUFFER_ENCODING_INVALID:
        g_assert_not_reached();
        break;
    case HRT_BUFFER_ENCODING_UTF8:
        buffer->encoding = &utf8_encoding;
        break;
    case HRT_BUFFER_ENCODING_UTF16:
        buffer->encoding = &utf16_encoding;
        break;
    case HRT_BUFFER_ENCODING_BINARY:
        g_error("binary buffers not implemented yet");
        break;
    }

    buffer->allocator = allocator;
    buffer->allocator_data = allocator_data;
    buffer->allocator_data_dnotify = dnotify;

    return buffer;
}

HrtBuffer*
hrt_buffer_new_static_utf8_locked(const char *str)
{
    HrtBuffer *buffer;

    buffer = hrt_buffer_new(HRT_BUFFER_ENCODING_UTF8,
                            &static_allocator,
                            NULL, NULL);

    buffer->length = strlen(str);
    buffer->d.buf_8.allocated = buffer->length;
    buffer->d.buf_8.data = (char*) str;

    /* may as well lock it since we can't add to this */
    hrt_buffer_lock(buffer);

    return buffer;
}

HrtBuffer*
hrt_buffer_new_copy_utf8(const char *str)
{
    HrtBuffer *buffer;

    buffer = hrt_buffer_new(HRT_BUFFER_ENCODING_UTF8,
                            &g_allocator,
                            NULL, NULL);

    /* FIXME append utf8 not ascii */
    hrt_buffer_append_ascii(buffer, str, strlen(str));

    return buffer;
}

void
hrt_buffer_ref(HrtBuffer *buffer)
{
    g_atomic_int_inc(&buffer->refcount);
}

void
hrt_buffer_unref(HrtBuffer *buffer)
{
    if (g_atomic_int_dec_and_test(&buffer->refcount)) {
        (* buffer->encoding->finalize) (buffer);

        if (buffer->allocator_data_dnotify) {
            (* buffer->allocator_data_dnotify) (buffer->allocator_data);
        }

        g_slice_free(HrtBuffer, buffer);
    }
}

/* Locking a buffer means it's now immutable (appending will assert)
 * so it can be used from more than one thread
 */
void
hrt_buffer_lock(HrtBuffer *buffer)
{
    buffer->locked = TRUE;
}

gboolean
hrt_buffer_is_locked(HrtBuffer *buffer)
{
    return buffer->locked != 0;
}

void
hrt_buffer_append_ascii(HrtBuffer     *unlocked_buffer,
                        const char    *bytes,
                        gsize          len)
{
    g_return_if_fail(!unlocked_buffer->locked);

    (* unlocked_buffer->encoding->append_ascii) (unlocked_buffer, bytes, len);
}

gsize
hrt_buffer_get_length(HrtBuffer *buffer)
{
    return buffer->length;
}

/* This mutates a locked buffer, so is only allowed when you know the
 * buffer is confined to a single thread.
 */
void
hrt_buffer_steal_utf16(HrtBuffer     *locked_buffer,
                       guint16      **utf16_data_p,
                       gsize         *len_p)
{
    g_return_if_fail(locked_buffer->encoding == &utf16_encoding);
    g_return_if_fail(locked_buffer->locked);

    *utf16_data_p = locked_buffer->d.buf_16.data;
    *len_p = locked_buffer->length;

    locked_buffer->d.buf_16.data = NULL;
    locked_buffer->length = 0;
}

void
hrt_buffer_peek_utf16(HrtBuffer      *locked_buffer,
                      const guint16 **utf16_data_p,
                      gsize          *len_p)
{
    g_return_if_fail(locked_buffer->encoding == &utf16_encoding);
    g_return_if_fail(locked_buffer->locked);

    *utf16_data_p = locked_buffer->d.buf_16.data;
    *len_p = locked_buffer->length;
}

void
hrt_buffer_steal_utf8(HrtBuffer     *locked_buffer,
                      char         **utf8_data_p,
                      gsize         *len_p)
{
    g_return_if_fail(locked_buffer->encoding == &utf8_encoding);
    g_return_if_fail(locked_buffer->locked);

    *utf8_data_p = locked_buffer->d.buf_8.data;
    *len_p = locked_buffer->length;

    locked_buffer->d.buf_8.data = NULL;
    locked_buffer->length = 0;
}

void
hrt_buffer_peek_utf8(HrtBuffer      *locked_buffer,
                     const char    **utf8_data_p,
                     gsize          *len_p)
{
    g_return_if_fail(locked_buffer->encoding == &utf8_encoding);
    g_return_if_fail(locked_buffer->locked);

    *utf8_data_p = locked_buffer->d.buf_8.data;
    *len_p = locked_buffer->length;
}

gsize
hrt_buffer_get_write_size(HrtBuffer *locked_buffer)
{
    g_return_val_if_fail(locked_buffer->locked, 0);

    return (* locked_buffer->encoding->get_write_size)(locked_buffer);
}

/* Returns FALSE only if there was a fatal error on the fd.  Otherwise
 * does a nonblocking write and returns TRUE with remaining
 * size in bytes. Write is complete when returned remaining size is 0.
 */
gboolean
hrt_buffer_write(HrtBuffer                 *locked_buffer,
                 int                        fd,
                 gsize                     *remaining_inout)
{
    gssize bytes_written;
    gsize total;
    const void *buf;

    g_return_val_if_fail(locked_buffer->locked, FALSE);

    total = (* locked_buffer->encoding->get_write_size) (locked_buffer);
    buf = (* locked_buffer->encoding->get_write_data) (locked_buffer);

    g_return_val_if_fail(*remaining_inout <= total, FALSE);

    bytes_written =
        send(fd, ((char*)buf) + (total - *remaining_inout), *remaining_inout,
             /* no SIGPIPE, no blocking, batch into packets */
             MSG_NOSIGNAL | MSG_DONTWAIT | MSG_MORE);

    if (bytes_written < 0) {
        if (errno == EINTR ||
            errno == EAGAIN ||
            errno == EWOULDBLOCK)
            return TRUE; /* not done, leave remaining_inout unchanged */
        else
            return FALSE; /* error case. */
    } else {
        *remaining_inout -= bytes_written;

        return TRUE;
    }
}
