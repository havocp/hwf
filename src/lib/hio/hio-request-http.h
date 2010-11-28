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

#ifndef __HIO_REQUEST_HTTP_H__
#define __HIO_REQUEST_HTTP_H__

#include <glib-object.h>
#include <hrt/hrt-buffer.h>
#include <hio/hio-incoming.h>
#include <hio/hio-response-http.h>

G_BEGIN_DECLS

typedef struct HioRequestHttp      HioRequestHttp;
typedef struct HioRequestHttpClass HioRequestHttpClass;

struct HioRequestHttp {
    HioIncoming      parent_instance;
    char *method;
    guint16 major;
    guint16 minor;
    char *path;
    char *query_string;
    HioResponseHttp *response;
};

struct HioRequestHttpClass {
    HioIncomingClass parent_class;

    void (* add_header) (HioRequestHttp *request,
                         HrtBuffer      *name,
                         HrtBuffer      *value);
};

#define HIO_TYPE_REQUEST_HTTP              (hio_request_http_get_type ())
#define HIO_REQUEST_HTTP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HIO_TYPE_REQUEST_HTTP, HioRequestHttp))
#define HIO_REQUEST_HTTP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HIO_TYPE_REQUEST_HTTP, HioRequestHttpClass))
#define HIO_IS_REQUEST_HTTP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HIO_TYPE_REQUEST_HTTP))
#define HIO_IS_REQUEST_HTTP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HIO_TYPE_REQUEST_HTTP))
#define HIO_REQUEST_HTTP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HIO_TYPE_REQUEST_HTTP, HioRequestHttpClass))

GType           hio_request_http_get_type                  (void) G_GNUC_CONST;

HioRequestHttp*  hio_request_http_new               (GType            subtype,
                                                     const char      *method,
                                                     int              major,
                                                     int              minor,
                                                     const char      *path,
                                                     const char      *query_string);
int              hio_request_http_get_major_version (HioRequestHttp  *request);
int              hio_request_http_get_minor_version (HioRequestHttp  *request);
const char*      hio_request_http_get_method        (HioRequestHttp  *request);
const char*      hio_request_http_get_path          (HioRequestHttp  *request);
const char*      hio_request_http_get_query_string  (HioRequestHttp  *request);
void             hio_request_http_add_header        (HioRequestHttp  *request,
                                                     HrtBuffer       *name,
                                                     HrtBuffer       *value);
void             hio_request_http_set_response      (HioRequestHttp  *request,
                                                     HioResponseHttp *response);
HioResponseHttp* hio_request_http_get_response      (HioRequestHttp  *request);

G_END_DECLS

#endif  /* __HIO_REQUEST_HTTP_H__ */
