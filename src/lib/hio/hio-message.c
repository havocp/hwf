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
#include <hio/hio-message.h>
#include <hrt/hrt-log.h>
#include <string.h>

G_DEFINE_TYPE(HioMessage, hio_message, G_TYPE_OBJECT);

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
hio_message_get_property (GObject                *object,
                          guint                   prop_id,
                          GValue                 *value,
                          GParamSpec             *pspec)
{
    HioMessage *hio_message;

    hio_message = HIO_MESSAGE(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_message_set_property (GObject                *object,
                          guint                   prop_id,
                          const GValue           *value,
                          GParamSpec             *pspec)
{
    HioMessage *hio_message;

    hio_message = HIO_MESSAGE(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_message_dispose(GObject *object)
{
    HioMessage *hio_message;

    hio_message = HIO_MESSAGE(object);

    G_OBJECT_CLASS(hio_message_parent_class)->dispose(object);
}

static void
hio_message_finalize(GObject *object)
{
    HioMessage *hio_message;

    hio_message = HIO_MESSAGE(object);


    G_OBJECT_CLASS(hio_message_parent_class)->finalize(object);
}

static void
hio_message_init(HioMessage *hio_message)
{

}

static void
hio_message_class_init(HioMessageClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = hio_message_get_property;
    object_class->set_property = hio_message_set_property;

    object_class->dispose = hio_message_dispose;
    object_class->finalize = hio_message_finalize;
}

HrtBuffer*
hio_message_create_buffer(HioMessage *message)
{
    HioMessageClass *klass = HIO_MESSAGE_GET_CLASS(message);

    g_assert(klass->create_buffer != NULL);

    return (* klass->create_buffer)(message);
}
