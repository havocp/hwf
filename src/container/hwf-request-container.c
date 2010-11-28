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
#include <container/hwf-request-container.h>
#include <hio/hio-request-http.h>
#include <hrt/hrt-log.h>
#include <hrt/hrt-task.h>

struct HwfRequestContainer {
    HioRequestHttp parent_instance;
    HjsRuntime *runtime;

};

struct HwfRequestContainerClass {
    HioRequestHttpClass parent_class;
};


G_DEFINE_TYPE(HwfRequestContainer, hwf_request_container, HIO_TYPE_REQUEST_HTTP);

enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */

static void
hwf_request_container_add_header(HioRequestHttp *http,
                                 HrtBuffer      *name,
                                 HrtBuffer      *value)
{
    /* FIXME */
    hrt_debug("%s", G_STRFUNC);
}

static HrtBuffer*
hwf_request_container_create_buffer(HioMessage *message)
{
    HwfRequestContainer *request = HWF_REQUEST_CONTAINER(message);

    return hjs_runtime_create_buffer(request->runtime);
}

static void
hwf_request_container_get_property (GObject                *object,
                                    guint                   prop_id,
                                    GValue                 *value,
                                    GParamSpec             *pspec)
{
    HwfRequestContainer *container;

    container = HWF_REQUEST_CONTAINER(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hwf_request_container_set_property (GObject                *object,
                                    guint                   prop_id,
                                    const GValue           *value,
                                    GParamSpec             *pspec)
{
    HwfRequestContainer *container;

    container = HWF_REQUEST_CONTAINER(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hwf_request_container_dispose(GObject *object)
{
    HwfRequestContainer *container;

    container = HWF_REQUEST_CONTAINER(object);

    if (container->runtime != NULL) {
        g_object_unref(container->runtime);
        container->runtime = NULL;
    }

    G_OBJECT_CLASS(hwf_request_container_parent_class)->dispose(object);
}

static void
hwf_request_container_finalize(GObject *object)
{
    HwfRequestContainer *container;

    container = HWF_REQUEST_CONTAINER(object);

    G_OBJECT_CLASS(hwf_request_container_parent_class)->finalize(object);
}

static void
hwf_request_container_init(HwfRequestContainer *container)
{
}

static GObject*
hwf_request_container_constructor(GType                  type,
                                  guint                  n_construct_properties,
                                  GObjectConstructParam *construct_params)
{
    GObject *object;
    HwfRequestContainer *container;

    object = G_OBJECT_CLASS(hwf_request_container_parent_class)->constructor(type,
                                                                             n_construct_properties,
                                                                             construct_params);

    container = HWF_REQUEST_CONTAINER(object);


    return object;
}

static void
hwf_request_container_class_init(HwfRequestContainerClass *klass)
{
    GObjectClass *object_class;
    HioMessageClass *message_class;
    HioRequestHttpClass *http_class;

    object_class = G_OBJECT_CLASS(klass);
    message_class = HIO_MESSAGE_CLASS(klass);
    http_class = HIO_REQUEST_HTTP_CLASS(klass);

    message_class->create_buffer = hwf_request_container_create_buffer;

    http_class->add_header = hwf_request_container_add_header;

    object_class->constructor = hwf_request_container_constructor;
    object_class->get_property = hwf_request_container_get_property;
    object_class->set_property = hwf_request_container_set_property;

    object_class->dispose = hwf_request_container_dispose;
    object_class->finalize = hwf_request_container_finalize;
}

void
hwf_request_container_set_runtime (HwfRequestContainer *request,
                                   HjsRuntime          *runtime)
{
    g_return_if_fail(request->runtime == NULL);
    g_return_if_fail(runtime != NULL);

    g_object_ref(runtime);
    request->runtime = runtime;
}

/* request has been sufficiently parsed (usually, it has all headers
 * but not the body if any) to go ahead and start working.
 */
void
hwf_request_container_execute(HwfRequestContainer *request,
                              HrtTask             *request_task)
{
    HioResponseHttp *response;
    HrtBuffer *buffer;

    hrt_debug("Executing request %p in task %p", request, request_task);

    response = hio_request_http_get_response(HIO_REQUEST_HTTP(request));

    hio_response_http_send_headers(response);

    buffer = hrt_buffer_new_static_utf8_locked("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"><TITLE>HELLO WORLD</TITLE><P>This is a web page.</P>");

    hio_response_http_write(response, buffer);

    hrt_buffer_unref(buffer);

    hio_response_http_close(response);
}
