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
#include <container/hwf-connection-container.h>
#include <container/hwf-request-container.h>

#include <hrt/hrt-log.h>
#include <hio/hio-request-http.h>
#include <hjs/hjs-runtime.h>
#include <hrt/hrt-task.h>

struct HwfConnectionContainer {
    HioConnectionHttp      parent_instance;
};

struct HwfConnectionContainerClass {
    HioConnectionHttpClass parent_class;
};


G_DEFINE_TYPE(HwfConnectionContainer, hwf_connection_container, HIO_TYPE_CONNECTION_HTTP);

enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */

static HioRequestHttp*
hwf_connection_container_create_request(HioConnectionHttp *http,
                                        const char        *method,
                                        int                major_version,
                                        int                minor_version,
                                        const char        *path,
                                        const char        *query_string)
{
    HioRequestHttp *request;
    GValue value = { 0, };

    request = hio_request_http_new(HWF_TYPE_REQUEST_CONTAINER,
                                   method,
                                   major_version, minor_version,
                                   path, query_string);

    g_value_init(&value, HJS_TYPE_RUNTIME);
    if (!hrt_task_get_arg(HIO_CONNECTION(http)->task,
                          "runtime",
                          &value,
                          NULL))
        g_error("Task didn't have a runtime set on it");

    hwf_request_container_set_runtime(HWF_REQUEST_CONTAINER(request),
                                      g_value_get_object(&value));

    g_value_unset(&value);

    hrt_debug("Created request %s %d.%d '%s' query '%s'",
              hio_request_http_get_method(request),
              hio_request_http_get_major_version(request),
              hio_request_http_get_minor_version(request),
              hio_request_http_get_path(request),
              hio_request_http_get_query_string(request));

    return request;
}

static gboolean
on_request_completed(HrtTask        *task,
                     HrtWatcherFlags flags,
                     void           *data)
{
    HwfRequestContainer *request;
    HioResponseHttp *response;

    request = HWF_REQUEST_CONTAINER(data);

    hrt_debug("Task %p request %p completed",
              task, request);

    /* Automatically send headers and close the body,
     * if it wasn't done already.
     */
    response = hio_request_http_get_response(HIO_REQUEST_HTTP(request));

    hio_response_http_send_headers(response);
    hio_response_http_close(response);

    return FALSE;
}

static gboolean
on_request_run(HrtTask        *task,
               HrtWatcherFlags flags,
               void           *data)
{
    HwfRequestContainer *request;

    request = HWF_REQUEST_CONTAINER(data);

    hwf_request_container_execute(request, task);

    return FALSE;
}

/* Called when headers are complete but body may not be.
 * Request is thus in use from another thread once headers
 * are complete.
 */
static void
hwf_connection_container_on_incoming_message(HioConnection   *connection,
                                             HioIncoming     *incoming)
{
    HrtTask *task;

    g_assert(HWF_IS_REQUEST_CONTAINER(incoming));

    task = hrt_task_create_task(connection->task);

    hrt_debug("Created task %p for incoming request %p",
              task, incoming);

    /* execute the request handler */
    hrt_task_add_immediate(task,
                           on_request_run,
                           g_object_ref(incoming),
                           (GDestroyNotify) g_object_unref);

    /* don't quit the connection task while request tasks are pending */
    hrt_task_add_subtask(connection->task,
                         task,
                         on_request_completed,
                         g_object_ref(incoming),
                         (GDestroyNotify) g_object_unref);

    g_object_unref(task);
}

static void
hwf_connection_container_get_property (GObject                *object,
                                       guint                   prop_id,
                                       GValue                 *value,
                                       GParamSpec             *pspec)
{
    HwfConnectionContainer *container;

    container = HWF_CONNECTION_CONTAINER(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hwf_connection_container_set_property (GObject                *object,
                                       guint                   prop_id,
                                       const GValue           *value,
                                       GParamSpec             *pspec)
{
    HwfConnectionContainer *container;

    container = HWF_CONNECTION_CONTAINER(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hwf_connection_container_dispose(GObject *object)
{
    HwfConnectionContainer *container;

    container = HWF_CONNECTION_CONTAINER(object);

    G_OBJECT_CLASS(hwf_connection_container_parent_class)->dispose(object);
}

static void
hwf_connection_container_finalize(GObject *object)
{
    HwfConnectionContainer *container;

    container = HWF_CONNECTION_CONTAINER(object);

    G_OBJECT_CLASS(hwf_connection_container_parent_class)->finalize(object);
}

static void
hwf_connection_container_init(HwfConnectionContainer *container)
{
}

static GObject*
hwf_connection_container_constructor(GType                  type,
                                     guint                  n_construct_properties,
                                     GObjectConstructParam *construct_params)
{
    GObject *object;
    HwfConnectionContainer *container;

    object = G_OBJECT_CLASS(hwf_connection_container_parent_class)->constructor(type,
                                                                                n_construct_properties,
                                                                                construct_params);

    container = HWF_CONNECTION_CONTAINER(object);


    return object;
}

static void
hwf_connection_container_class_init(HwfConnectionContainerClass *klass)
{
    GObjectClass *object_class;
    HioConnectionClass *connection_class;
    HioConnectionHttpClass *http_class;

    object_class = G_OBJECT_CLASS(klass);
    connection_class = HIO_CONNECTION_CLASS(klass);
    http_class = HIO_CONNECTION_HTTP_CLASS(klass);

    object_class->constructor = hwf_connection_container_constructor;
    object_class->get_property = hwf_connection_container_get_property;
    object_class->set_property = hwf_connection_container_set_property;

    object_class->dispose = hwf_connection_container_dispose;
    object_class->finalize = hwf_connection_container_finalize;

    connection_class->on_incoming_message = hwf_connection_container_on_incoming_message;

    http_class->create_request = hwf_connection_container_create_request;
}
