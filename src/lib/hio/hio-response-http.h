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

#ifndef __HIO_RESPONSE_HTTP_H__
#define __HIO_RESPONSE_HTTP_H__

#include <glib-object.h>
#include <hio/hio-output-stream.h>

G_BEGIN_DECLS

typedef struct HioResponseHttp      HioResponseHttp;
typedef struct HioResponseHttpClass HioResponseHttpClass;

#define HIO_TYPE_RESPONSE_HTTP              (hio_response_http_get_type ())
#define HIO_RESPONSE_HTTP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HIO_TYPE_RESPONSE_HTTP, HioResponseHttp))
#define HIO_RESPONSE_HTTP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HIO_TYPE_RESPONSE_HTTP, HioResponseHttpClass))
#define HIO_IS_RESPONSE_HTTP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HIO_TYPE_RESPONSE_HTTP))
#define HIO_IS_RESPONSE_HTTP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HIO_TYPE_RESPONSE_HTTP))
#define HIO_RESPONSE_HTTP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HIO_TYPE_RESPONSE_HTTP, HioResponseHttpClass))

GType           hio_response_http_get_type                  (void) G_GNUC_CONST;

HioResponseHttp* hio_response_http_new             (HioOutputStream *header_stream,
                                                    HioOutputStream *body_stream);
void             hio_response_http_set_header      (HioResponseHttp *http,
                                                    HrtBuffer       *name,
                                                    HrtBuffer       *value);
void             hio_response_http_send_headers    (HioResponseHttp *http);
void             hio_response_http_write           (HioResponseHttp *http,
                                                    HrtBuffer       *buffer);
void             hio_response_http_close           (HioResponseHttp *http);




G_END_DECLS

#endif  /* __HIO_RESPONSE_HTTP_H__ */
