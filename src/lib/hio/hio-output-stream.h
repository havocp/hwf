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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO OUTPUT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __HIO_OUTPUT_STREAM_H__
#define __HIO_OUTPUT_STREAM_H__

#include <glib-object.h>
#include <hrt/hrt-buffer.h>
#include <hrt/hrt-task-runner.h>

G_BEGIN_DECLS

typedef struct HioOutputStream      HioOutputStream;
typedef struct HioOutputStreamClass HioOutputStreamClass;

typedef void (* HioOutputStreamDoneNotify) (HioOutputStream *stream,
                                            void            *data);

#define HIO_TYPE_OUTPUT_STREAM              (hio_output_stream_get_type ())
#define HIO_OUTPUT_STREAM(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HIO_TYPE_OUTPUT_STREAM, HioOutputStream))
#define HIO_OUTPUT_STREAM_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HIO_TYPE_OUTPUT_STREAM, HioOutputStreamClass))
#define HIO_IS_OUTPUT_STREAM(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HIO_TYPE_OUTPUT_STREAM))
#define HIO_IS_OUTPUT_STREAM_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HIO_TYPE_OUTPUT_STREAM))
#define HIO_OUTPUT_STREAM_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HIO_TYPE_OUTPUT_STREAM, HioOutputStreamClass))

GType           hio_output_stream_get_type (void) G_GNUC_CONST;

HioOutputStream* hio_output_stream_new             (HrtTask                   *task);
void             hio_output_stream_write           (HioOutputStream           *stream,
                                                    HrtBuffer                 *locked_buffer);
void             hio_output_stream_close           (HioOutputStream           *stream);
gboolean         hio_output_stream_is_closed       (HioOutputStream           *stream);
gboolean         hio_output_stream_got_error       (HioOutputStream           *stream);
gboolean         hio_output_stream_is_done         (HioOutputStream           *stream);
void             hio_output_stream_error           (HioOutputStream           *stream);
void             hio_output_stream_set_fd          (HioOutputStream           *stream,
                                                    int                        fd);
void             hio_output_stream_set_done_notify (HioOutputStream           *stream,
                                                    HioOutputStreamDoneNotify  func,
                                                    void                      *data,
                                                    GDestroyNotify             dnotify);



G_END_DECLS

#endif  /* __HIO_OUTPUT_STREAM_H__ */
