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
#include <stdlib.h>
#include <container/container.h>
#include <hrt/hrt-log.h>

static gboolean option_debug;
static gboolean option_version;

static GOptionEntry entries[] = {
    { "debug", 0, 0, G_OPTION_ARG_NONE, &option_debug, "Enable debug logging", NULL },
    { "version", 0, 0, G_OPTION_ARG_NONE, &option_version, "Show version info and exit", NULL },
    { NULL }
};

int
main(int    argc,
     char **argv)
{
    GError *error = NULL;
    GOptionContext *context;
    GMainLoop *loop;
    HwfContainer *container;

    g_thread_init(NULL);
    g_type_init();

    context = g_option_context_new("- HWF Servlet Container");
    g_option_context_add_main_entries(context, entries, "hwf-container");

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    if (option_version) {
        g_print("%s container %s (compiled %s)\n",
                PACKAGE, VERSION, CONFIGURE_TIME);
        exit(0);
    }

    hrt_log_init(option_debug ?
                 HRT_LOG_FLAG_DEBUG : 0);

    container = g_object_new(HWF_TYPE_CONTAINER,
                             NULL);

    hwf_container_add_address(container, "localhost", 8080);

    error = NULL;
    if (!hwf_container_start(container, &error)) {
        g_printerr("failed to start: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }

    loop = g_main_loop_new(NULL, FALSE);

    g_main_loop_run(loop);

    g_main_loop_unref(loop);

    g_object_unref(container);

    exit(0);
}
