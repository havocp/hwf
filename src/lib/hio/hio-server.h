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

#ifndef __HIO_SERVER_H__
#define __HIO_SERVER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct HioServer      HioServer;
typedef struct HioServerClass HioServerClass;

#define HIO_TYPE_SERVER              (hio_server_get_type ())
#define HIO_SERVER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HIO_TYPE_SERVER, HioServer))
#define HIO_SERVER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HIO_TYPE_SERVER, HioServerClass))
#define HIO_IS_SERVER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HIO_TYPE_SERVER))
#define HIO_IS_SERVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HIO_TYPE_SERVER))
#define HIO_SERVER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HIO_TYPE_SERVER, HioServerClass))

struct HioServer {
    GObject      parent_instance;
    /* access fd with atomic just to make it easier to close()
     * from any thread
     */
    volatile int fd;
    unsigned int port;
    unsigned int on_new_connections_id;
    GMainContext *main_context;
};

struct HioServerClass {
    GObjectClass parent_class;

    gboolean (* socket_accepted) (HioServer *server,
                                  int        fd);
};

GType           hio_server_get_type                  (void) G_GNUC_CONST;

gboolean hio_server_listen_tcp (HioServer  *hio_server,
                                const char *host,
                                int         port,
                                GError    **error);
void     hio_server_close      (HioServer  *hio_server);

G_END_DECLS

#endif  /* __HIO_SERVER_H__ */
