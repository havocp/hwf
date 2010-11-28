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

#ifndef __HIO_CONNECTION_H__
#define __HIO_CONNECTION_H__

/* HioConnection - some sort of connection coming in to a server, base class.
 */

#include <glib-object.h>
#include <hrt/hrt-task-runner.h>
#include <hio/hio-incoming.h>
#include <hio/hio-outgoing.h>

G_BEGIN_DECLS

typedef struct HioConnection      HioConnection;
typedef struct HioConnectionClass HioConnectionClass;

#define HIO_TYPE_CONNECTION              (hio_connection_get_type ())
#define HIO_CONNECTION(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HIO_TYPE_CONNECTION, HioConnection))
#define HIO_CONNECTION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HIO_TYPE_CONNECTION, HioConnectionClass))
#define HIO_IS_CONNECTION(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HIO_TYPE_CONNECTION))
#define HIO_IS_CONNECTION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HIO_TYPE_CONNECTION))
#define HIO_CONNECTION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HIO_TYPE_CONNECTION, HioConnectionClass))

struct HioConnection {
    GObject      parent_instance;
    HrtTask     *task;
    int fd;

    HrtWatcher *read_watcher;
};

struct HioConnectionClass {
    GObjectClass parent_class;

    /* In this hook, the subclass must read data and
     * use it to create incoming messages. Invoked
     * from the read watcher thread.
     */
    void (* on_incoming_data)    (HioConnection *connection);

    void (* on_incoming_message) (HioConnection *connection,
                                  HioIncoming   *incoming);
};


GType           hio_connection_get_type                  (void) G_GNUC_CONST;

void            hio_connection_process_socket(GType          subtype,
                                              HrtTask       *task,
                                              int            fd);

/* This is invoked by subclasses in the read watcher thread from
 * on_incoming_data()
 */
gssize _hio_connection_read     (HioConnection *connection,
                                 void          *buf,
                                 gsize          bytes_to_read);
void   _hio_connection_close_fd (HioConnection *connection);


G_END_DECLS

#endif  /* __HIO_CONNECTION_H__ */
