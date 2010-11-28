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

#ifndef __HWF_CONTAINER_H__
#define __HWF_CONTAINER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct HwfContainer      HwfContainer;
typedef struct HwfContainerClass HwfContainerClass;

#define HWF_TYPE_CONTAINER              (hwf_container_get_type ())
#define HWF_CONTAINER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HWF_TYPE_CONTAINER, HwfContainer))
#define HWF_CONTAINER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HWF_TYPE_CONTAINER, HwfContainerClass))
#define HWF_IS_CONTAINER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HWF_TYPE_CONTAINER))
#define HWF_IS_CONTAINER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HWF_TYPE_CONTAINER))
#define HWF_CONTAINER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HWF_TYPE_CONTAINER, HwfContainerClass))


GType           hwf_container_get_type                  (void) G_GNUC_CONST;

/* GSocketAddress is really what's wanted here, but too complex for now. */
void     hwf_container_add_address (HwfContainer  *container,
                                    const char    *host,
                                    int            port);
gboolean hwf_container_start       (HwfContainer  *container,
                                    GError       **error);



G_END_DECLS

#endif  /* __HWF_CONTAINER_H__ */
