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
#include <container/container.h>
#include <container/hwf-connection-container.h>
#include <hrt/hrt-log.h>
#include <string.h>
#include <hrt/hrt-task-runner.h>
#include <hrt/hrt-task.h>
#include <hio/hio-server.h>
#include <hjs/hjs-runtime-spidermonkey.h>
#include <hjs/hjs-script-spidermonkey.h>

typedef struct {
    HioServer *server;
    char *host;
    int port;
} Server;


static Server*
server_new(const char *host,
           int         port)
{
    Server *server;

    server = g_slice_new0(Server);

    server->host = g_strdup(host);
    server->port = port;

    return server;
}

static void
server_free(Server *server)
{
    if (server->server) {
        hio_server_close(server->server);
        g_object_unref(server->server);
        server->server = NULL;
    }
    g_free(server->host);
    g_slice_free(Server, server);
}

static gboolean
server_start(Server  *server,
             GError **error)
{
    /* Needs to be a no-op if done twice */
    if (server->server == NULL) {
        server->server = g_object_new(HIO_TYPE_SERVER, NULL);
        if (!hio_server_listen_tcp(server->server,
                                   server->host,
                                   server->port,
                                   error)) {
            g_object_unref(server->server);
            server->server = NULL;

            return FALSE;
        }
    }

    return TRUE;
}


struct HwfContainer {
    GObject parent_instance;

    HrtTaskRunner *runner;
    HjsRuntime *runtime;

    GSList *servers;
};

struct HwfContainerClass {
    GObjectClass parent_class;
};

G_DEFINE_TYPE(HwfContainer, hwf_container, G_TYPE_OBJECT);

enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */


static void
on_tasks_completed(HrtTaskRunner *runner,
                   void          *data)
{
    HwfContainer *container = HWF_CONTAINER(data);
    HrtTask *task;

    while ((task = hrt_task_runner_pop_completed(container->runner)) != NULL) {
        g_object_unref(task);
    }
}

static void
on_server_closed(HioServer *server,
                 void      *data)
{
    /* HwfContainer *container = HWF_CONTAINER(data); */

    /* FIXME */
}

static gboolean
on_server_socket_accepted(HioServer *server,
                          int        fd,
                          void      *data)
{
    HwfContainer *container = HWF_CONTAINER(data);
    HrtTask *task;
    GValue value = { 0, };

    hrt_debug("Creating connection for accepted socket %d", fd);

    task = hrt_task_runner_create_task(container->runner);

    g_value_init(&value, HJS_TYPE_RUNTIME_SPIDERMONKEY);
    g_value_set_object(&value, container->runtime);
    hrt_task_add_arg(task, "runtime", &value);
    g_value_unset(&value);

    hio_connection_process_socket(HWF_TYPE_CONNECTION_CONTAINER,
                                  task,
                                  fd);

    /* task will be ref'd by the watchers added by hwf_connection_process_socket() */
    g_object_unref(task);

    return TRUE;
}

static void
hwf_container_get_property (GObject                *object,
                            guint                   prop_id,
                            GValue                 *value,
                            GParamSpec             *pspec)
{
    HwfContainer *container;

    container = HWF_CONTAINER(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hwf_container_set_property (GObject                *object,
                            guint                   prop_id,
                            const GValue           *value,
                            GParamSpec             *pspec)
{
    HwfContainer *container;

    container = HWF_CONTAINER(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hwf_container_dispose(GObject *object)
{
    HwfContainer *container;

    container = HWF_CONTAINER(object);

    while (container->servers != NULL) {
        Server *server = container->servers->data;
        container->servers =
            g_slist_remove(container->servers,
                           container->servers->data);
        server_free(server);
    }

    if (container->runner) {
        g_object_unref(container->runner);
        container->runner = NULL;
    }

    if (container->runtime) {
        g_object_unref(container->runtime);
        container->runtime = NULL;
    }

    G_OBJECT_CLASS(hwf_container_parent_class)->dispose(object);
}

static void
hwf_container_finalize(GObject *object)
{
    HwfContainer *container;

    container = HWF_CONTAINER(object);


    G_OBJECT_CLASS(hwf_container_parent_class)->finalize(object);
}

static void
hwf_container_init(HwfContainer *container)
{
    container->runner = g_object_new(HRT_TYPE_TASK_RUNNER,
                                     "event-loop-type", HRT_EVENT_LOOP_EV,
                                     NULL);

    g_signal_connect_data(G_OBJECT(container->runner),
                          "tasks-completed",
                          G_CALLBACK(on_tasks_completed),
                          g_object_ref(container),
                          (GClosureNotify) g_object_unref,
                          G_CONNECT_AFTER);

    container->runtime = g_object_new(HJS_TYPE_RUNTIME_SPIDERMONKEY,
                                      NULL);
}

static void
hwf_container_class_init(HwfContainerClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = hwf_container_get_property;
    object_class->set_property = hwf_container_set_property;

    object_class->dispose = hwf_container_dispose;
    object_class->finalize = hwf_container_finalize;
}

void
hwf_container_add_address(HwfContainer  *container,
                          const char    *host,
                          int            port)
{
    Server *server;

    server = server_new(host, port);
    container->servers = g_slist_prepend(container->servers,
                                         server);
}

gboolean
hwf_container_start(HwfContainer  *container,
                    GError       **error)
{
    GSList *tmp;

    for (tmp = container->servers;
         tmp != NULL;
         tmp = tmp->next) {
        Server *server = tmp->data;

        /* If one fails, we leave some started;
         * the caller could try again (then restarting
         * some is a no-op) or the caller could dispose
         * the container (we would then stop all servers
         * on destroy)
         */
        if (!server_start(server, error))
            return FALSE;

        g_signal_connect_data(G_OBJECT(server->server),
                              "closed",
                              G_CALLBACK(on_server_closed),
                              g_object_ref(container),
                              (GClosureNotify) g_object_unref,
                              G_CONNECT_AFTER);
        g_signal_connect_data(G_OBJECT(server->server),
                              "socket-accepted",
                              G_CALLBACK(on_server_socket_accepted),
                              g_object_ref(container),
                              (GClosureNotify) g_object_unref,
                              G_CONNECT_AFTER);
    }

    return TRUE;
}
