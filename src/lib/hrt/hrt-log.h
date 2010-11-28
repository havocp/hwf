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

#ifndef __HRT_LOG_H__
#define __HRT_LOG_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    HRT_LOG_FLAG_DEBUG = 1 << 0
} HrtLogFlags;

void                    hrt_log_init          (HrtLogFlags flags);
G_CONST_RETURN gboolean hrt_log_debug_enabled (void);

#define hrt_debug(...)                                  \
    do {                                                \
        if (G_UNLIKELY(hrt_log_debug_enabled())) {      \
            g_log(G_LOG_DOMAIN,                         \
                  G_LOG_LEVEL_DEBUG,                    \
                  __VA_ARGS__);                         \
        }                                               \
    } while (0)

#define hrt_message(...)                        \
    g_log(G_LOG_DOMAIN,                         \
          G_LOG_LEVEL_MESSAGE,                  \
          __VA_ARGS__)

G_END_DECLS

#endif  /* __HRT_LOG_H__ */
