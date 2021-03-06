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

#ifndef __HIO_CONNECTION_HTTP_H__
#define __HIO_CONNECTION_HTTP_H__

#include <glib-object.h>
#include <hio/hio-connection.h>
#include <hio/hio-request-http.h>

G_BEGIN_DECLS

typedef struct HioConnectionHttp        HioConnectionHttp;
typedef struct HioConnectionHttpClass   HioConnectionHttpClass;
typedef struct HioConnectionHttpPrivate HioConnectionHttpPrivate;

#define HIO_TYPE_CONNECTION_HTTP              (hio_connection_http_get_type ())
#define HIO_CONNECTION_HTTP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HIO_TYPE_CONNECTION_HTTP, HioConnectionHttp))
#define HIO_CONNECTION_HTTP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HIO_TYPE_CONNECTION_HTTP, HioConnectionHttpClass))
#define HIO_IS_CONNECTION_HTTP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HIO_TYPE_CONNECTION_HTTP))
#define HIO_IS_CONNECTION_HTTP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HIO_TYPE_CONNECTION_HTTP))
#define HIO_CONNECTION_HTTP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HIO_TYPE_CONNECTION_HTTP, HioConnectionHttpClass))

struct HioConnectionHttp {
    HioConnection      parent_instance;
    HioConnectionHttpPrivate *priv;
};

struct HioConnectionHttpClass {
    HioConnectionClass parent_class;

    HioRequestHttp* (* create_request)  (HioConnectionHttp *http,
                                         const char        *method,
                                         int                http_major,
                                         int                http_minor,
                                         const char        *path,
                                         const char        *query_string);
};

GType           hio_connection_http_get_type       (void) G_GNUC_CONST;


G_END_DECLS

#endif  /* __HIO_CONNECTION_HTTP_H__ */
