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

#ifndef __HRT_BUFFER_H__
#define __HRT_BUFFER_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
    void*  (* malloc)  (gsize  bytes,
                        void  *allocator_data);
    void   (* free)    (void  *mem,
                        void  *allocator_data);
    void*  (* realloc) (void  *mem,
                        gsize  bytes,
                        void  *allocator_data);
} HrtBufferAllocator;

typedef enum {
    HRT_BUFFER_ENCODING_INVALID,
    HRT_BUFFER_ENCODING_UTF8,
    HRT_BUFFER_ENCODING_UTF16,
    HRT_BUFFER_ENCODING_BINARY
} HrtBufferEncoding;

typedef struct HrtBuffer HrtBuffer;

HrtBuffer* hrt_buffer_new                       (HrtBufferEncoding          encoding,
                                                 const HrtBufferAllocator  *allocator,
                                                 void                      *allocator_data,
                                                 GDestroyNotify             dnotify);
HrtBuffer* hrt_buffer_new_static_utf8_locked    (const char                *str);
HrtBuffer* hrt_buffer_new_copy_utf8             (const char                *str);
void       hrt_buffer_ref                       (HrtBuffer                 *buffer);
void       hrt_buffer_unref                     (HrtBuffer                 *buffer);
void       hrt_buffer_lock                      (HrtBuffer                 *buffer);
gboolean   hrt_buffer_is_locked                 (HrtBuffer                 *buffer);
void       hrt_buffer_append_ascii              (HrtBuffer                 *unlocked_buffer,
                                                 const char                *bytes,
                                                 gsize                      len);
gsize      hrt_buffer_get_length                (HrtBuffer                 *buffer);

void       hrt_buffer_steal_utf16               (HrtBuffer                 *locked_buffer,
                                                 guint16                  **utf16_data_p,
                                                 gsize                     *len_p);
void       hrt_buffer_peek_utf16                (HrtBuffer                 *locked_buffer,
                                                 const guint16            **utf16_data_p,
                                                 gsize                     *len_p);
void       hrt_buffer_steal_utf8                (HrtBuffer                 *locked_buffer,
                                                 char                     **utf8_data_p,
                                                 gsize                     *len_p);
void       hrt_buffer_peek_utf8                 (HrtBuffer                 *locked_buffer,
                                                 const char               **utf8_data_p,
                                                 gsize                     *len_p);
gsize      hrt_buffer_get_write_size            (HrtBuffer                 *locked_buffer);
gboolean   hrt_buffer_write                     (HrtBuffer                 *locked_buffer,
                                                 int                        fd,
                                                 gsize                     *remaining_inout);


G_END_DECLS

#endif  /* __HRT_BUFFER_H__ */
