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

#ifndef __HIO_MESSAGE_H__
#define __HIO_MESSAGE_H__

/* HioMessage - a request or response in a protocol. "Data thing which can be queued"
 */

#include <glib-object.h>
#include <hrt/hrt-buffer.h>

G_BEGIN_DECLS

typedef struct HioMessage      HioMessage;
typedef struct HioMessageClass HioMessageClass;

#define HIO_TYPE_MESSAGE              (hio_message_get_type ())
#define HIO_MESSAGE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HIO_TYPE_MESSAGE, HioMessage))
#define HIO_MESSAGE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HIO_TYPE_MESSAGE, HioMessageClass))
#define HIO_IS_MESSAGE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HIO_TYPE_MESSAGE))
#define HIO_IS_MESSAGE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HIO_TYPE_MESSAGE))
#define HIO_MESSAGE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HIO_TYPE_MESSAGE, HioMessageClass))

struct HioMessage {
    GObject      parent_instance;

};

struct HioMessageClass {
    GObjectClass parent_class;

    HrtBuffer* (* create_buffer) (HioMessage *message);
};


GType           hio_message_get_type                  (void) G_GNUC_CONST;

HrtBuffer* hio_message_create_buffer(HioMessage *message);

G_END_DECLS

#endif  /* __HIO_MESSAGE_H__ */
