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
#include <glib-object.h>
#include <gio/gio.h>
#include <hio/hio-server.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define N_CLIENT_CONNECTS 100

typedef struct {
    /* This stuff touched only by server thread */
    HioServer *server;
    GMainContext *server_context;
    GMainLoop *server_loop;
    GThreadPool *server_threads;

    /* This stuff from main or client threads */
    GThread *server_thread;
    int port; /* copied from server for use outside server thread */
    gboolean results[N_CLIENT_CONNECTS];
} ServerTestFixture;

static void
on_echo_server_closed(HioServer *server,
                      void      *data)
{
    ServerTestFixture *fixture = data;

    g_main_loop_quit(fixture->server_loop);
}

static gboolean
on_echo_socket_accepted(HioServer *server,
                        int        fd,
                        void      *data)
{
    GError *error;
    ServerTestFixture *fixture = data;

    error = NULL;
    g_thread_pool_push(fixture->server_threads,
                       GINT_TO_POINTER(fd),
                       &error);
    if (error != NULL) {
        g_error("%s", error->message);
    }

    return TRUE;
}

static void
echo_server_pool_thread(void *task,
                        void *pool_data)
{
    int fd = GPOINTER_TO_INT(task);
    int bytes_read;
    char buf[256];
    int flags;

    /* We want to block */
    flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    while (TRUE) {
        bytes_read = read(fd, buf, sizeof(buf));
        if (bytes_read < 0) {
            if (errno == EINTR)
                continue;
            else
                g_error("read from client: %s", strerror(errno));
        } else if (bytes_read == 0) {
            /* EOF */
            break;
        } else {
            int bytes_written;

        write_more:
            bytes_written = write(fd, buf, bytes_read);
            if (bytes_written < 0) {
                if (errno == EINTR)
                    goto write_more;
                else
                    g_error("write to client: %s", strerror(errno));
            } else {
                bytes_read -= bytes_written;
                if (bytes_read == 0)
                    continue;
                else
                    goto write_more;
            }
        }
    }
}

static void*
echo_server_thread(void *data)
{
    ServerTestFixture *fixture = data;

    g_signal_connect(fixture->server, "closed",
                     G_CALLBACK(on_echo_server_closed),
                     fixture);

    g_signal_connect(fixture->server, "socket-accepted",
                     G_CALLBACK(on_echo_socket_accepted),
                     fixture);

    g_main_context_push_thread_default(fixture->server_context);
    fixture->server_loop = g_main_loop_new(fixture->server_context,
                                           FALSE);

    g_main_loop_run(fixture->server_loop);

    return NULL;
}

static void
setup_echo_server(ServerTestFixture *fixture,
                  const void        *data)
{
    GError *error;

    g_test_message("Setting up echo server");

    fixture->server_context = g_main_context_new();

    fixture->server = g_object_new(HIO_TYPE_SERVER,
                                   "main-context", fixture->server_context,
                                   NULL);

    error = NULL;
    if (!hio_server_listen_tcp(fixture->server,
                               "localhost", 0,
                               &error)) {
        g_error("%s", error->message);
    }

    g_object_get(G_OBJECT(fixture->server),
                 "port", &fixture->port,
                 NULL);

    fixture->server_threads =
        g_thread_pool_new(echo_server_pool_thread,
                          fixture,
                          -1, FALSE,
                          &error);
    if (error != NULL)
        g_error("%s", error->message);

    error = NULL;
    fixture->server_thread =
        g_thread_create(echo_server_thread, fixture, TRUE, &error);
    if (error != NULL)
        g_error("%s", error->message);

    g_test_message("echo server set up OK");
}

static void
echo_client_pool_thread(void *task,
                        void *pool_data)
{
    ServerTestFixture *fixture = pool_data;
    GSocketClient *client;
    GSocketConnection *connection;
    GOutputStream *ostream;
    GInputStream *istream;
    GError *error;
    const char message[] = "Hello!";
    char buf[128];
    gsize bytes_read;
    int count;

    g_test_message("In a client thread %d", GPOINTER_TO_INT(task));

    client = g_object_new(G_TYPE_SOCKET_CLIENT,
                          "family", G_SOCKET_FAMILY_IPV4,
                          "type", G_SOCKET_TYPE_STREAM,
                          "protocol", G_SOCKET_PROTOCOL_TCP,
                          NULL);

    error = NULL;
    connection = g_socket_client_connect_to_host(client,
                                                 "localhost",
                                                 fixture->port,
                                                 NULL,
                                                 &error);
    if (error != NULL) {
        g_error("client socket connect: %s", error->message);
    }

    for (count = 0; count < 128; ++count) {
        ostream = g_io_stream_get_output_stream(G_IO_STREAM(connection));

        error = NULL;
        g_output_stream_write_all(ostream, message,
                                  strlen(message) + 1, /* include nul */
                                  NULL, NULL,
                                  &error);
        if (error != NULL) {
            g_error("client socket write: %s", error->message);
        }

        istream = g_io_stream_get_input_stream(G_IO_STREAM(connection));

        error = NULL;
        g_input_stream_read_all(istream, &buf[0], strlen(message) + 1,
                                &bytes_read, NULL, &error);
        if (error != NULL) {
            g_error("client socket read: %s", error->message);
        }
        if (bytes_read != strlen(message) + 1) {
            g_error("Read %d bytes, wrong amount", bytes_read);
        }
        if (buf[strlen(message)] != '\0' ||
            strcmp(buf, message) != 0) {
            g_error("Sent '%s' got '%s'", buf, message);
        }
    }

    error = NULL;
    g_io_stream_close(G_IO_STREAM(connection), NULL, &error);
    if (error != NULL) {
        g_error("client socket close: %s", error->message);
    }

    g_object_unref(connection);
    g_object_unref(client);

    fixture->results[GPOINTER_TO_INT(task) - 1] = TRUE;
}

static void
test_connect_and_echo(ServerTestFixture *fixture,
                      const void        *data)
{
    GError *error;
    GThreadPool *threads;
    int i;

    g_test_message("Testing echo server");

    error = NULL;
    threads = g_thread_pool_new(echo_client_pool_thread,
                                fixture,
                                20, FALSE,
                                &error);
    if (error != NULL) {
        g_error("client thread pool create: %s", error->message);
    }

    for (i = 0; i < N_CLIENT_CONNECTS; ++i) {
        fixture->results[i] = FALSE;
        error = NULL;
        /* we have to number threads from 1 because you can't push NULL */
        g_thread_pool_push(threads, GINT_TO_POINTER(i + 1), &error);
        if (error != NULL) {
            g_error("push to client pool: %s", error->message);
        }
    }

    g_thread_pool_free(threads,
                       FALSE, /* !immediate */
                       TRUE); /* wait */

    for (i = 0; i < N_CLIENT_CONNECTS; ++i) {
        if (!fixture->results[i]) {
            g_error("client %d did not complete properly", i);
        }
    }

    g_test_message("%d clients completed OK", N_CLIENT_CONNECTS);
}

static void
teardown(ServerTestFixture *fixture,
         const void        *data)
{
    hio_server_close(fixture->server);

    g_object_unref(fixture->server);

    g_thread_join(fixture->server_thread);

    g_test_message("succesfully tore down echo server");
}

static gboolean option_version = FALSE;

static GOptionEntry entries[] = {
    { "version", 0, 0, G_OPTION_ARG_NONE, &option_version, "Show version info and exit", NULL },
    { NULL }
};

int
main(int    argc,
     char **argv)
{
    GError *error = NULL;
    GOptionContext *context;

    g_thread_init(NULL);
    g_type_init();

    g_test_init(&argc, &argv, NULL);

    context = g_option_context_new("- Test Suite HioServer");
    g_option_context_add_main_entries(context, entries, "test-server");

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    if (option_version) {
        g_print("test-server %s\n",
                VERSION);
        exit(0);
    }

    g_test_add("/server/connect_and_echo",
               ServerTestFixture,
               NULL,
               setup_echo_server,
               test_connect_and_echo,
               teardown);

    return g_test_run();
}
