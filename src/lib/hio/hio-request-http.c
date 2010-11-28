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
#include <hio/hio-request-http.h>
#include <hio/hio-incoming.h>
#include <hrt/hrt-log.h>


G_DEFINE_TYPE(HioRequestHttp, hio_request_http, HIO_TYPE_INCOMING);

enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */

const char*
hio_request_http_get_method(HioRequestHttp *request)
{
    return request->method;
}

int
hio_request_http_get_major_version(HioRequestHttp *request)
{
    return request->major;
}

int
hio_request_http_get_minor_version(HioRequestHttp *request)
{
    return request->minor;
}

const char*
hio_request_http_get_path(HioRequestHttp *request)
{
    return request->path;
}

const char*
hio_request_http_get_query_string(HioRequestHttp *request)
{
    return request->query_string;
}

HioRequestHttp*
hio_request_http_new(GType         subtype,
                     const char   *method,
                     int           major,
                     int           minor,
                     const char   *path,
                     const char   *query_string)
{
    HioRequestHttp *request;

    /* http_parser doesn't allow these versions to be >999
     * and of course the non-evil max is 1
     */
    g_return_val_if_fail(major <= (int) G_MAXUINT16, NULL);
    g_return_val_if_fail(minor <= (int) G_MAXUINT16, NULL);

    request = g_object_new(subtype,
                           NULL);

    request->method = g_strdup(method);
    request->major = major;
    request->minor = minor;
    request->path = g_strdup(path);
    request->query_string = g_strdup(query_string);

    return request;
}

void
hio_request_http_add_header(HioRequestHttp *request,
                            HrtBuffer      *name,
                            HrtBuffer      *value)
{
    HioRequestHttpClass *klass;

    klass = HIO_REQUEST_HTTP_GET_CLASS(request);

    g_assert(klass->add_header != NULL);

    (* klass->add_header) (request, name, value);
}

void
hio_request_http_set_response(HioRequestHttp  *request,
                              HioResponseHttp *response)
{
    /* can only set this once, otherwise we'd have
     * all kinds of wacky issues.
     */
    g_return_if_fail(request->response == NULL);

    request->response = g_object_ref(response);
}

HioResponseHttp*
hio_request_http_get_response(HioRequestHttp  *request)
{
    return request->response;
}

static void
hio_request_http_get_property (GObject                *object,
                               guint                   prop_id,
                               GValue                 *value,
                               GParamSpec             *pspec)
{
    HioRequestHttp *http;

    http = HIO_REQUEST_HTTP(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_request_http_set_property (GObject                *object,
                               guint                   prop_id,
                               const GValue           *value,
                               GParamSpec             *pspec)
{
    HioRequestHttp *http;

    http = HIO_REQUEST_HTTP(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_request_http_dispose(GObject *object)
{
    HioRequestHttp *http;

    http = HIO_REQUEST_HTTP(object);

    if (http->response) {
        g_object_unref(http->response);
        http->response = NULL;
    }

    G_OBJECT_CLASS(hio_request_http_parent_class)->dispose(object);
}

static void
hio_request_http_finalize(GObject *object)
{
    HioRequestHttp *http;

    http = HIO_REQUEST_HTTP(object);

    g_free(http->method);
    http->method = NULL;

    g_free(http->path);
    http->path = NULL;

    g_free(http->query_string);
    http->query_string = NULL;

    G_OBJECT_CLASS(hio_request_http_parent_class)->finalize(object);
}

static void
hio_request_http_init(HioRequestHttp *http)
{
}

static GObject*
hio_request_http_constructor(GType                  type,
                             guint                  n_construct_properties,
                             GObjectConstructParam *construct_params)
{
    GObject *object;
    HioRequestHttp *http;

    object = G_OBJECT_CLASS(hio_request_http_parent_class)->constructor(type,
                                                                        n_construct_properties,
                                                                        construct_params);

    http = HIO_REQUEST_HTTP(object);


    return object;
}

static void
hio_request_http_class_init(HioRequestHttpClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->constructor = hio_request_http_constructor;
    object_class->get_property = hio_request_http_get_property;
    object_class->set_property = hio_request_http_set_property;

    object_class->dispose = hio_request_http_dispose;
    object_class->finalize = hio_request_http_finalize;
}
