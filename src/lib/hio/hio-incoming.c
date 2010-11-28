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
#include <hio/hio-incoming.h>
#include <hrt/hrt-log.h>
#include <string.h>

G_DEFINE_TYPE(HioIncoming, hio_incoming, HIO_TYPE_MESSAGE);

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
hio_incoming_get_property (GObject                *object,
                           guint                   prop_id,
                           GValue                 *value,
                           GParamSpec             *pspec)
{
    HioIncoming *hio_incoming;

    hio_incoming = HIO_INCOMING(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_incoming_set_property (GObject                *object,
                           guint                   prop_id,
                           const GValue           *value,
                           GParamSpec             *pspec)
{
    HioIncoming *hio_incoming;

    hio_incoming = HIO_INCOMING(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_incoming_dispose(GObject *object)
{
    HioIncoming *hio_incoming;

    hio_incoming = HIO_INCOMING(object);

    G_OBJECT_CLASS(hio_incoming_parent_class)->dispose(object);
}

static void
hio_incoming_finalize(GObject *object)
{
    HioIncoming *hio_incoming;

    hio_incoming = HIO_INCOMING(object);


    G_OBJECT_CLASS(hio_incoming_parent_class)->finalize(object);
}

static void
hio_incoming_init(HioIncoming *hio_incoming)
{

}

static void
hio_incoming_class_init(HioIncomingClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = hio_incoming_get_property;
    object_class->set_property = hio_incoming_set_property;

    object_class->dispose = hio_incoming_dispose;
    object_class->finalize = hio_incoming_finalize;
}
