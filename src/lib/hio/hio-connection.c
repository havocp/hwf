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
#include <hio/hio-connection.h>
#include <hrt/hrt-log.h>
#include <hrt/hrt-task.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

G_DEFINE_TYPE(HioConnection, hio_connection, G_TYPE_OBJECT);

/* remember that adding props makes GObject a lot slower to
 * construct
 */
enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */

static void
hio_connection_get_property (GObject                *object,
                             guint                   prop_id,
                             GValue                 *value,
                             GParamSpec             *pspec)
{
    HioConnection *hio_connection;

    hio_connection = HIO_CONNECTION(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_connection_set_property (GObject                *object,
                             guint                   prop_id,
                             const GValue           *value,
                             GParamSpec             *pspec)
{
    HioConnection *hio_connection;

    hio_connection = HIO_CONNECTION(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
quit_reading(HioConnection *connection)
{
    if (connection->read_watcher != NULL) {
        hrt_watcher_remove(connection->read_watcher);
        connection->read_watcher = NULL;
    }
}

static void
close_fd(HioConnection *connection)
{
    if (connection->fd >= 0) {
        /* Apparently shutdown() drops any kernel-buffered data,
         * which we don't want to do; just close() should send it.
         * Not sure if there are other considerations.
         */
        /* shutdown(hio_connection->fd, SHUT_RDWR); */
        close(connection->fd);
        connection->fd = -1;
    }
}

static void
hio_connection_dispose(GObject *object)
{
    HioConnection *hio_connection;

    hio_connection = HIO_CONNECTION(object);

    quit_reading(hio_connection);

    close_fd(hio_connection);

    if (hio_connection->task != NULL) {
        g_object_unref(hio_connection->task);
        hio_connection->task = NULL;
    }

    G_OBJECT_CLASS(hio_connection_parent_class)->dispose(object);
}

static void
hio_connection_finalize(GObject *object)
{
    HioConnection *hio_connection;

    hio_connection = HIO_CONNECTION(object);

    G_OBJECT_CLASS(hio_connection_parent_class)->finalize(object);
}

static void
hio_connection_init(HioConnection *hio_connection)
{

}

static void
hio_connection_class_init(HioConnectionClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = hio_connection_get_property;
    object_class->set_property = hio_connection_set_property;

    object_class->dispose = hio_connection_dispose;
    object_class->finalize = hio_connection_finalize;
}

static HioConnection*
hio_connection_new_from_socket(GType          subtype,
                               HrtTask       *task,
                               int            fd)
{
    HioConnection *connection;

    connection = g_object_new(subtype, NULL);
    connection->task = task;
    connection->fd = fd;

    g_object_ref(connection->task);


    return connection;
}

static gboolean
on_incoming_data(HrtTask        *task,
                 HrtWatcherFlags flags,
                 void           *data)
{
    HioConnection *connection = HIO_CONNECTION(data);
    HioConnectionClass *klass = HIO_CONNECTION_GET_CLASS(connection);

    if (klass->on_incoming_data != NULL) {
        (* klass->on_incoming_data)(connection);
    }

    return TRUE;
}

void
hio_connection_process_socket(GType          subtype,
                              HrtTask       *task,
                              int            fd)
{
    HioConnection *connection;

    connection = hio_connection_new_from_socket(subtype, task, fd);

    connection->read_watcher =
        hrt_task_add_io(task,
                        fd,
                        HRT_WATCHER_FLAG_READ,
                        on_incoming_data,
                        g_object_ref(connection),
                        (GDestroyNotify) g_object_unref);

    g_object_unref(connection);
}

gssize
_hio_connection_read(HioConnection *connection,
                     void          *buf,
                     gsize          bytes_to_read)
{
    gssize bytes_read;

 again:
    bytes_read = read(connection->fd, buf, bytes_to_read);
    if (bytes_read < 0) {
        if (errno == EINTR ||
            errno == EAGAIN ||
            errno == EWOULDBLOCK) {
            goto again;
        } else {
            quit_reading(connection);
            return -1;
        }
    } else if (bytes_read == 0) {
        quit_reading(connection);
        return 0;
    } else {
        return bytes_read;
    }
}

void
_hio_connection_close_fd(HioConnection *connection)
{
    quit_reading(connection);
    close_fd(connection);
}
