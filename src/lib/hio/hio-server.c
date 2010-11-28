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
#define _GNU_SOURCE 1

#include <config.h>
#include <hio/hio-server.h>
#include <hrt/hrt-log.h>
#include <hio/hio-marshalers.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


G_DEFINE_TYPE(HioServer, hio_server, G_TYPE_OBJECT);

enum {
    PROP_0,
    PROP_PORT,
    PROP_MAIN_CONTEXT
};

enum  {
    SOCKET_ACCEPTED,
    CLOSED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
hio_server_get_property (GObject                *object,
                         guint                   prop_id,
                         GValue                 *value,
                         GParamSpec             *pspec)
{
    HioServer *hio_server;

    hio_server = HIO_SERVER(object);

    switch (prop_id) {
    case PROP_PORT:
        g_value_set_uint(value, hio_server->port);
        break;
    case PROP_MAIN_CONTEXT:
        g_value_set_pointer(value, hio_server->main_context);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }

}

static void
hio_server_set_property (GObject                *object,
                         guint                   prop_id,
                         const GValue           *value,
                         GParamSpec             *pspec)
{
    HioServer *hio_server;

    hio_server = HIO_SERVER(object);

    switch (prop_id) {
    case PROP_MAIN_CONTEXT: {
        GMainContext *new_context;

        if (g_atomic_int_get(&hio_server->fd) >= 0) {
            g_warning("Cannot change main context on an already-listening server");
        }
        new_context = g_value_get_pointer(value);
        if (new_context)
            g_main_context_ref(new_context);

        if (hio_server->main_context)
            g_main_context_unref(hio_server->main_context);

        hio_server->main_context = new_context;
    }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_server_dispose(GObject *object)
{
    HioServer *hio_server;

    hio_server = HIO_SERVER(object);

    hio_server_close(hio_server);

    if (hio_server->main_context) {
        g_main_context_unref(hio_server->main_context);
        hio_server->main_context = NULL;
    }

    G_OBJECT_CLASS(hio_server_parent_class)->dispose(object);
}

static void
hio_server_finalize(GObject *object)
{
    HioServer *hio_server;

    hio_server = HIO_SERVER(object);

    G_OBJECT_CLASS(hio_server_parent_class)->finalize(object);
}

static gboolean
socket_taken_accumulator (GSignalInvocationHint *hint,
                          GValue                *accumulated,
                          const GValue          *handler_return,
                          gpointer               unused)
{
    gboolean socket_taken;

    socket_taken = g_value_get_boolean(handler_return);
    g_value_set_boolean(accumulated, socket_taken);

    return !socket_taken; /* whether to continue signal emission */
}

static gboolean
on_new_connections(GIOChannel   *source,
                   GIOCondition  condition,
                   gpointer      data)
{
    HioServer *hio_server = HIO_SERVER(data);
    struct sockaddr addr;
    socklen_t addrlen;
    int client_fd;
    gboolean accepted;

    if (!(condition & G_IO_IN))
        return TRUE;

    addrlen = sizeof(addr);

    /* accept as many things as we can in a loop, this reduces
     * main loop iterations
     */
    while (TRUE) {
        int listen_fd = g_atomic_int_get(&hio_server->fd);
        if (listen_fd < 0) {
            /* socket was closed */
            break;
        }

        client_fd = accept4(listen_fd,
                            &addr, &addrlen, SOCK_CLOEXEC | SOCK_NONBLOCK);

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
                /* nothing else to accept */
                break;
            } else if (errno == EBADF) {
                /* we closed the socket */
                hrt_debug("socket was invalid when we called accept4()");
                break;
            } else {
                hrt_message("accept4(): %s", strerror(errno));
                break;
            }
        }

        hrt_debug("accepted new socket %d", client_fd);

        accepted = FALSE;
        g_signal_emit(G_OBJECT(hio_server),
                      signals[SOCKET_ACCEPTED],
                      0, client_fd, &accepted);
        /* If nobody else has returned true, we just close the new connection. */
        if (!accepted) {
            shutdown(client_fd, SHUT_RDWR);
            close(client_fd);
            hrt_debug("nobody wanted new socket %d so we closed it", client_fd);
        }
    }

    return TRUE;
}

static guint
add_io_watch(GMainContext *main_context,
             GIOChannel   *channel,
             GIOCondition  condition,
             GIOFunc       func,
             gpointer      user_data)
{
    GSource *source;
    guint id;

    source = g_io_create_watch(channel, condition);

    g_source_set_callback(source, (GSourceFunc)func, user_data, NULL);

    id = g_source_attach(source, main_context);
    g_source_unref(source);

    return id;
}

gboolean
hio_server_listen_tcp(HioServer   *hio_server,
                      const char  *host,
                      int          port,
                      GError     **error)
{
    struct addrinfo hints;
    struct addrinfo *ai_iter;
    struct addrinfo *ai;
    socklen_t addr_len;
    struct sockaddr_storage addr;
    char buf[10];
    int result;
    GIOChannel *io_channel;
    int fd;

    if (g_atomic_int_get(&hio_server->fd) >= 0) {
        g_set_error(error,
                    G_FILE_ERROR,
                    G_FILE_ERROR_FAILED,
                    "Server open already");
        return FALSE;
    }

    memset(&hints, '\0', sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; /* AI_NUMERICHOST needed on numeric hosts... */

    snprintf(buf, sizeof(buf), "%d", port);
    buf[sizeof(buf)-1] = '\0';
    ai = NULL;
    result = getaddrinfo(host,
                         &buf[0],
                         &hints,
                         &ai);
    if (result != 0 || ai == NULL) {
        g_set_error(error,
                    G_FILE_ERROR,
                    G_FILE_ERROR_FAILED,
                    "getaddrinfo(): %s",
                    gai_strerror(result));
        return FALSE;
    }

    /* for now we don't listen on multiple, just use first */
    for (ai_iter = ai; ai_iter != NULL; ai_iter = ai_iter->ai_next) {
        fd = socket(ai_iter->ai_family,
                    SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                    0);
        if (fd < 0) {
            freeaddrinfo(ai);

            g_set_error(error,
                        G_FILE_ERROR,
                        G_FILE_ERROR_FAILED,
                        "socket(): %s",
                        strerror(errno));
            return FALSE;
        }

        if (bind(fd,
                 (struct sockaddr*) ai_iter->ai_addr,
                 ai_iter->ai_addrlen) < 0) {

            freeaddrinfo(ai);
            close(fd);

            g_set_error(error,
                        G_FILE_ERROR,
                        G_FILE_ERROR_FAILED,
                        "bind(): %s",
                        strerror(errno));
            return FALSE;
        }

        if (listen(fd, 75) < 0) {
            freeaddrinfo(ai);
            close(fd);

            g_set_error(error,
                        G_FILE_ERROR,
                        G_FILE_ERROR_FAILED,
                        "listen(): %s",
                        strerror(errno));
            return FALSE;
        }

        break;
    }

    freeaddrinfo(ai);

    addr_len = sizeof(addr);
    result = getsockname(fd,
                         (struct sockaddr*) &addr,
                         &addr_len);
    if (result < 0) {
        g_set_error(error,
                    G_FILE_ERROR,
                    G_FILE_ERROR_FAILED,
                    "getsockname(): %s",
                    strerror(errno));
        close(fd);
        return FALSE;
    }

    if (port == 0) {
        /* see what port we got */
        result = getnameinfo((struct sockaddr*)&addr, addr_len,
                             NULL, 0,
                             buf, sizeof(buf),
                             NI_NUMERICHOST);
        buf[sizeof(buf) - 1] = '\0';
        if (result < 0) {
            g_set_error(error,
                        G_FILE_ERROR,
                        G_FILE_ERROR_FAILED,
                        "getnameinfo(): %s",
                        strerror(errno));
            close(fd);
            return FALSE;
        }

        hio_server->port = atoi(buf);
        if (hio_server->port == 0) {
            g_set_error(error,
                        G_FILE_ERROR,
                        G_FILE_ERROR_FAILED,
                        "invalid port %s",
                        buf);
            close(fd);
            return FALSE;
        }
    } else {
        hio_server->port = port;
    }

    io_channel = g_io_channel_unix_new(fd);
    hio_server->on_new_connections_id = add_io_watch(hio_server->main_context,
                                                     io_channel,
                                                     G_IO_IN,
                                                     on_new_connections,
                                                     hio_server);
    g_io_channel_unref(io_channel);

    g_atomic_int_set(&hio_server->fd, fd);

    hrt_debug("Socket fd %d now open host '%s' port %u",
              fd, host, hio_server->port);

    return TRUE;
}

/* close() is allowed from any thread */
void
hio_server_close(HioServer  *hio_server)
{
    /* Be sure only one thread closes. Doing it this way instead of
     * with a lock means that when hio_server_close() returns
     * the server may not quite be closed yet but that is hopefully ok
     */
    int fd = g_atomic_int_get(&hio_server->fd);
    if (fd < 0)
        return;

    if (!g_atomic_int_compare_and_exchange(&hio_server->fd,
                                           fd, -1))
        return;

    /* the closing thread must also remove the watch */
    g_assert(hio_server->on_new_connections_id != 0);

    g_source_remove(hio_server->on_new_connections_id);
    hio_server->on_new_connections_id = 0;

    shutdown(fd, SHUT_RDWR);
    close(fd);

    g_signal_emit(G_OBJECT(hio_server),
                  signals[CLOSED],
                  0);
}

static void
hio_server_init(HioServer *hio_server)
{
    hio_server->fd = -1;
}

static void
hio_server_class_init(HioServerClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = hio_server_get_property;
    object_class->set_property = hio_server_set_property;

    object_class->dispose = hio_server_dispose;
    object_class->finalize = hio_server_finalize;

    signals[SOCKET_ACCEPTED] =
        g_signal_new("socket-accepted",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(HioServerClass, socket_accepted),
                     socket_taken_accumulator, NULL,
                     _hio_marshal_BOOLEAN__INT,
                     G_TYPE_BOOLEAN, 1, G_TYPE_INT);

    signals[CLOSED] =
        g_signal_new("closed",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    g_object_class_install_property(object_class,
                                    PROP_PORT,
                                    g_param_spec_uint("port",
                                                      "Port",
                                                      "Network port",
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READABLE));

    g_object_class_install_property(object_class,
                                    PROP_MAIN_CONTEXT,
                                    g_param_spec_pointer("main-context",
                                                         "Main context",
                                                         "Main context",
                                                         G_PARAM_READWRITE));
}
