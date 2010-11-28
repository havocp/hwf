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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO OUTPUT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __HIO_OUTPUT_CHAIN_H__
#define __HIO_OUTPUT_CHAIN_H__

#include <glib-object.h>
#include <hio/hio-output-stream.h>

G_BEGIN_DECLS

typedef struct HioOutputChain      HioOutputChain;
typedef struct HioOutputChainClass HioOutputChainClass;

#define HIO_TYPE_OUTPUT_CHAIN              (hio_output_chain_get_type ())
#define HIO_OUTPUT_CHAIN(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HIO_TYPE_OUTPUT_CHAIN, HioOutputChain))
#define HIO_OUTPUT_CHAIN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HIO_TYPE_OUTPUT_CHAIN, HioOutputChainClass))
#define HIO_IS_OUTPUT_CHAIN(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HIO_TYPE_OUTPUT_CHAIN))
#define HIO_IS_OUTPUT_CHAIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HIO_TYPE_OUTPUT_CHAIN))
#define HIO_OUTPUT_CHAIN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HIO_TYPE_OUTPUT_CHAIN, HioOutputChainClass))

typedef void (* HioOutputChainEmptyNotify) (HioOutputChain  *chain,
                                            void            *data);


GType           hio_output_chain_get_type (void) G_GNUC_CONST;

HioOutputChain* hio_output_chain_new              (HrtTask                   *task);
void            hio_output_chain_set_fd           (HioOutputChain            *chain,
                                                   int                        fd);
gboolean        hio_output_chain_got_error        (HioOutputChain            *chain);
gboolean        hio_output_chain_is_empty         (HioOutputChain            *chain);
void            hio_output_chain_add_stream       (HioOutputChain            *chain,
                                                   HioOutputStream           *stream);
void            hio_output_chain_set_empty_notify (HioOutputChain            *chain,
                                                   HioOutputChainEmptyNotify  func,
                                                   void                      *data,
                                                   GDestroyNotify             dnotify);



G_END_DECLS

#endif  /* __HIO_OUTPUT_CHAIN_H__ */
