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
#include <hio/hio-response-http.h>
#include <hio/hio-outgoing.h>
#include <hrt/hrt-log.h>

typedef struct {
    HrtBuffer *name;
    HrtBuffer *value;
} Header;

struct HioResponseHttp {
    HioOutgoing      parent_instance;

    HioOutputStream *header_stream;
    HioOutputStream *body_stream;

    GMutex *headers_lock;
    GSList *headers;
    gboolean headers_sent;
};

struct HioResponseHttpClass {
    HioOutgoingClass parent_class;
};


G_DEFINE_TYPE(HioResponseHttp, hio_response_http, HIO_TYPE_OUTGOING);

enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */

static Header*
header_new(HrtBuffer       *name,
           HrtBuffer       *value)
{
    Header *header;

    hrt_buffer_ref(name);
    hrt_buffer_ref(value);

    header = g_slice_new(Header);
    header->name = name;
    header->value = value;

    return header;
}

static void
header_free(Header *header)
{
    hrt_buffer_unref(header->name);
    hrt_buffer_unref(header->value);
    g_slice_free(Header, header);
}


HioResponseHttp*
hio_response_http_new(HioOutputStream *header_stream,
                      HioOutputStream *body_stream)
{
    HioResponseHttp *http;

    http = g_object_new(HIO_TYPE_RESPONSE_HTTP,
                        NULL);

    http->header_stream = g_object_ref(header_stream);
    http->body_stream = g_object_ref(body_stream);

    return http;
}

void
hio_response_http_set_header(HioResponseHttp *http,
                             HrtBuffer       *name,
                             HrtBuffer       *value)
{
    g_return_if_fail(hrt_buffer_is_locked(name));
    g_return_if_fail(hrt_buffer_is_locked(value));

    g_mutex_lock(http->headers_lock);

    if (http->headers_sent) {
        g_mutex_unlock(http->headers_lock);
        hrt_message("Attempt to set http header after we already sent the headers, ignoring");
        return;
    }

    /* FIXME overwrite a previous duplicate header if any */
    http->headers = g_slist_prepend(http->headers,
                                    header_new(name, value));
    g_mutex_unlock(http->headers_lock);
}

static void
write_to_header(HioResponseHttp *http,
                const char      *s)
{
    HrtBuffer *buffer;

    buffer = hrt_buffer_new_static_utf8_locked(s);
    hio_output_stream_write(http->header_stream,
                            buffer);
    hrt_buffer_unref(buffer);
}

void
hio_response_http_send_headers(HioResponseHttp *http)
{
    /* FIXME send actual headers */

    g_mutex_lock(http->headers_lock);

    /* Sending headers twice is allowed, for example request handlers
     * can do it early, but a container might do it automatically
     * after the request handler runs. Only first send does anything
     * of course and it's an error to try to set headers after we
     * already sent them.
     */
    if (http->headers_sent) {
        g_mutex_unlock(http->headers_lock);
        return;
    }
    http->headers_sent = TRUE;

    write_to_header(http, "HTTP/1.1 200 OK\r\n");
    write_to_header(http, "Date: Wed, 21 Jul 2010 02:24:36 GMT\r\n");
    write_to_header(http, "Server: hrt/" VERSION "\r\n");
    write_to_header(http, "Last-Modified: Tue, 01 Dec 2009 23:10:05 GMT\r\n");
    write_to_header(http, "Content-Type: text/html\r\n");
    write_to_header(http, "Connection: close\r\n");
    write_to_header(http, "\r\n");

    hio_output_stream_close(http->header_stream);

    g_mutex_unlock(http->headers_lock);
}

void
hio_response_http_write(HioResponseHttp *http,
                        HrtBuffer       *buffer)
{
    /* this has to be more complex for chunked encoding, which is why
     * we don't allow direct access to the body stream.
     */
    hio_output_stream_write(http->body_stream, buffer);
}

void
hio_response_http_close(HioResponseHttp *http)
{
    hio_output_stream_close(http->body_stream);
}

static void
hio_response_http_get_property(GObject                *object,
                               guint                   prop_id,
                               GValue                 *value,
                               GParamSpec             *pspec)
{
    HioResponseHttp *http;

    http = HIO_RESPONSE_HTTP(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_response_http_set_property(GObject                *object,
                               guint                   prop_id,
                               const GValue           *value,
                               GParamSpec             *pspec)
{
    HioResponseHttp *http;

    http = HIO_RESPONSE_HTTP(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_response_http_dispose(GObject *object)
{
    HioResponseHttp *http;

    http = HIO_RESPONSE_HTTP(object);

    if (http->header_stream) {
        g_object_unref(http->header_stream);
        http->header_stream = NULL;
    }

    if (http->body_stream) {
        g_object_unref(http->body_stream);
        http->body_stream = NULL;
    }

    while (http->headers != NULL) {
        header_free(http->headers->data);
        http->headers = g_slist_delete_link(http->headers,
                                            http->headers);
    }

    G_OBJECT_CLASS(hio_response_http_parent_class)->dispose(object);
}

static void
hio_response_http_finalize(GObject *object)
{
    HioResponseHttp *http;

    http = HIO_RESPONSE_HTTP(object);

    g_mutex_free(http->headers_lock);

    G_OBJECT_CLASS(hio_response_http_parent_class)->finalize(object);
}

static void
hio_response_http_init(HioResponseHttp *http)
{
    http->headers_lock = g_mutex_new();
}

static GObject*
hio_response_http_constructor(GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_params)
{
    GObject *object;
    HioResponseHttp *http;

    object = G_OBJECT_CLASS(hio_response_http_parent_class)->constructor(type,
                                                                         n_construct_properties,
                                                                         construct_params);

    http = HIO_RESPONSE_HTTP(object);


    return object;
}

static void
hio_response_http_class_init(HioResponseHttpClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->constructor = hio_response_http_constructor;
    object_class->get_property = hio_response_http_get_property;
    object_class->set_property = hio_response_http_set_property;

    object_class->dispose = hio_response_http_dispose;
    object_class->finalize = hio_response_http_finalize;
}
