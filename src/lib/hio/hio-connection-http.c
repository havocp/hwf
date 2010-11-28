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
#include <hio/hio-connection-http.h>
#include <hio/hio-connection.h>
#include <hrt/hrt-log.h>
#include <hio/hio-output-chain.h>
#include <deps/http-parser/http_parser.h>

/* FIXME unit tests:
 * empty request
 * request with bad path
 * request with zero-length header value
 * request with crazy http versions (huge numbers, missing numbers)
 */

struct HioConnectionHttpPrivate {
    http_parser parser;

    /* we do path as a C string instead of HrtBuffer because we expect
     * to want it in C to route requests, since some requests
     * (e.g. static files) might not go through non-C at all.
     * query_string quite possibly should be a buffer but keeping
     * it simple at first.
     */
    GString *path;
    GString *query_string;

    HioRequestHttp *current_request;

    HrtBuffer *current_header_name;
    HrtBuffer *current_header_value;

    HioOutputChain *response_chain;

    /* do we have the method, path, url, query string, http version */
    guint request_line_completed : 1;

    /* have we seen a header value since the last name.
     * could just check length of value > 0 except maybe headers
     * can have empty string as value?
     */
    /* important unit test: empty-valued header */
    guint header_value_seen : 1;
};


G_DEFINE_TYPE(HioConnectionHttp, hio_connection_http, HIO_TYPE_CONNECTION);

enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */


/* called whenever we see something that could be just after the
 * request line, to create the current_request object
 *    Request-Line = Method SP Request-URI SP HTTP-Version CRLF
 */
static void
complete_request_line(HioConnectionHttp *http)
{
    if (!http->priv->request_line_completed) {
        http->priv->request_line_completed = TRUE;
        http->priv->current_request =
            HIO_CONNECTION_HTTP_GET_CLASS(http)->create_request(http,
                                                                http_method_str(http->priv->parser.method),
                                                                http->priv->parser.http_major,
                                                                http->priv->parser.http_minor,
                                                                http->priv->path->str,
                                                                http->priv->query_string->str);
    }
}

/* called when we see a new header name, or headers complete */
static void
complete_header(HioConnectionHttp *http)
{
    if (http->priv->current_header_name &&
        http->priv->header_value_seen) {
        g_assert(http->priv->current_header_value != NULL);

        hrt_buffer_lock(http->priv->current_header_name);
        hrt_buffer_lock(http->priv->current_header_value);

        hio_request_http_add_header(http->priv->current_request,
                                    http->priv->current_header_name,
                                    http->priv->current_header_value);

        hrt_buffer_unref(http->priv->current_header_name);
        http->priv->current_header_name = NULL;
        hrt_buffer_unref(http->priv->current_header_value);
        http->priv->current_header_value = NULL;
        http->priv->header_value_seen = FALSE;
    }
}

#define PARSER_GET_CONNECTION(parser) HIO_CONNECTION_HTTP((parser)->data)
#define PARSER_GET_PRIVATE(parser)    (PARSER_GET_CONNECTION(parser)->priv)

static int
on_message_begin(http_parser *parser)
{
    HioConnectionHttp *http;

    hrt_debug("http_parser %s", G_STRFUNC);

    http = PARSER_GET_CONNECTION(parser);

    g_assert(http->priv->path->len == 0);
    g_assert(http->priv->query_string->len == 0);
    g_assert(http->priv->current_request == NULL);
    g_assert(!http->priv->request_line_completed);

    return 0;
}

static int
on_path(http_parser *parser,
        const char  *at,
        size_t       length)
{
    HioConnectionHttpPrivate *priv = PARSER_GET_PRIVATE(parser);
    hrt_debug("http_parser %s", G_STRFUNC);

    g_string_append_len(priv->path, at, length);

    return 0;
}

static int
on_query_string(http_parser *parser,
                const char  *at,
                size_t       length)
{
    HioConnectionHttpPrivate *priv = PARSER_GET_PRIVATE(parser);
    hrt_debug("http_parser %s", G_STRFUNC);

    g_string_append_len(priv->query_string, at, length);

    return 0;
}

static int
on_url(http_parser *parser,
       const char  *at,
       size_t       length)
{
    /* on_url has been seen both before and after on_path, so its
     * relationship to the the breakdown (path, query string,
     * fragment) is apparently undefined.
     */

    hrt_debug("http_parser %s", G_STRFUNC);

    g_assert(PARSER_GET_PRIVATE(parser)->response_chain == NULL);

    return 0;
}

static int
on_fragment(http_parser *parser,
            const char  *at,
            size_t       length)
{
    hrt_debug("http_parser %s", G_STRFUNC);

    return 0;
}

static int
on_header_field(http_parser *parser,
                const char  *at,
                size_t       length)
{
    HioConnectionHttp *http;

    hrt_debug("http_parser %s", G_STRFUNC);

    http = PARSER_GET_CONNECTION(parser);

    /* first time we see a header, we must be past the request line */
    complete_request_line(http);
    /* once we have path, we have to create request */
    g_assert(http->priv->current_request != NULL);

    /* complete any previous header */
    complete_header(http);

    if (http->priv->current_header_name == NULL) {
        g_assert(http->priv->current_header_value == NULL);
        http->priv->current_header_name =
            hio_message_create_buffer(HIO_MESSAGE(http->priv->current_request));
        http->priv->current_header_value =
            hio_message_create_buffer(HIO_MESSAGE(http->priv->current_request));
    }

    hrt_buffer_append_ascii(http->priv->current_header_name,
                            at, length);

    return 0;
}

static int
on_header_value(http_parser *parser,
                const char  *at,
                size_t       length)
{
    HioConnectionHttp *http;

    hrt_debug("http_parser %s", G_STRFUNC);

    http = PARSER_GET_CONNECTION(parser);

    g_assert(http->priv->current_header_value != NULL);

    http->priv->header_value_seen = TRUE;

    hrt_buffer_append_ascii(http->priv->current_header_value,
                            at, length);

    return 0;
}

static void
on_response_chain_empty(HioOutputChain    *chain,
                        void              *data)
{
    HioConnectionHttp *http;
    HioConnection *connection;

    http = HIO_CONNECTION_HTTP(data);
    connection = HIO_CONNECTION(data);

    g_assert(chain == http->priv->response_chain);

    /* break the refcount cycle by removing this notify */
    hio_output_chain_set_empty_notify(chain, NULL, NULL, NULL);

    hio_output_chain_set_fd(chain, -1);

    /* FIXME this will be more complex when we support keep-alive */
    _hio_connection_close_fd(connection);
}

static int
on_headers_complete(http_parser *parser)
{
    HioConnectionHttp *http;
    HioConnection *connection;
    HioOutputStream *header_stream;
    HioOutputStream *body_stream;
    HioResponseHttp *response;

    hrt_debug("http_parser %s", G_STRFUNC);

    http = PARSER_GET_CONNECTION(parser);
    connection = HIO_CONNECTION(http);

    complete_request_line(http);
    /* once we have request line, we will have created the request */
    g_assert(http->priv->current_request != NULL);

    complete_header(http);

    g_assert(http->priv->response_chain == NULL);

    /* ensure we have a response chain (reused for multiple requests) */
    if (http->priv->response_chain == NULL) {
        /* we do output in the same task thread as input; neither
         * one blocks, so this should be fine. What we really
         * want in a different thread is the likely CPU-bound
         * or data-store-bound request handler.
         */
        http->priv->response_chain = hio_output_chain_new(connection->task);
        hio_output_chain_set_fd(http->priv->response_chain,
                                connection->fd);

        hio_output_chain_set_empty_notify(http->priv->response_chain,
                                          on_response_chain_empty,
                                          /* reference cycle we'll
                                           * break by unsetting the
                                           * chain when notified
                                           */
                                          g_object_ref(connection),
                                          g_object_unref);
    }

    /* create response object with stream for header and then body */
    header_stream = hio_output_stream_new(connection->task);
    body_stream = hio_output_stream_new(connection->task);

    /* output the header then the body */
    hio_output_chain_add_stream(http->priv->response_chain,
                                header_stream);
    hio_output_chain_add_stream(http->priv->response_chain,
                                body_stream);

    response = hio_response_http_new(header_stream, body_stream);

    hio_request_http_set_response(http->priv->current_request,
                                  response);

    /* everything now owned by the request and the response chain */
    g_object_unref(response);
    g_object_unref(header_stream);
    g_object_unref(body_stream);

    /* Hand off the request to be handled. NOTE: this means from now
     * on, current_request is touched concurrently from our thread and
     * possible other threads.
     */
    HIO_CONNECTION_GET_CLASS(http)->on_incoming_message(HIO_CONNECTION(http),
                                                        HIO_INCOMING(http->priv->current_request));

    return 0;
}

static int
on_body(http_parser *parser,
        const char  *at,
        size_t       length)
{
    hrt_debug("http_parser %s", G_STRFUNC);

    return 0;
}

static int
on_message_complete(http_parser *parser)
{
    HioConnectionHttp *http;

    hrt_debug("http_parser %s", G_STRFUNC);

    http = PARSER_GET_CONNECTION(parser);

    g_assert(http->priv->current_request != NULL);

    g_object_unref(http->priv->current_request);
    http->priv->current_request = NULL;

    g_string_set_size(http->priv->path, 0);
    g_string_set_size(http->priv->query_string, 0);
    http->priv->request_line_completed = FALSE;

    return 0;
}

static const http_parser_settings parser_settings = {
    on_message_begin,
    on_path,
    on_query_string,
    on_url,
    on_fragment,
    on_header_field,
    on_header_value,
    on_headers_complete,
    on_body,
    on_message_complete
};

static void
hio_connection_http_on_incoming_data(HioConnection *connection)
{
    HioConnectionHttp *http;
    char buf[512];
    gssize bytes_read;
    gssize bytes_parsed;

    http = HIO_CONNECTION_HTTP(connection);

    bytes_read = _hio_connection_read(connection, &buf[0], sizeof(buf));

    if (bytes_read < 0) {
        hrt_debug("error reading from %d", connection->fd);
    } else if (bytes_read >= 0) {
        /* hrt_debug("Read %ld bytes from %d", (long) bytes_read, connection->fd); */

        if (bytes_read == 0) {
            hrt_debug("EOF on %d", connection->fd);
            /* We are supposed to pass bytes_read==0 to the parser to mark EOF */
        }

        bytes_parsed = http_parser_execute(&http->priv->parser,
                                           &parser_settings,
                                           &buf[0], bytes_read);
        if (bytes_parsed != bytes_read) {
            /* FIXME - error, maybe bad http. have to handle. */
            g_warning("HTTP Parser didn't consume all the data");
        }
    }
}

static void
hio_connection_http_get_property (GObject                *object,
                                  guint                   prop_id,
                                  GValue                 *value,
                                  GParamSpec             *pspec)
{
    HioConnectionHttp *http;

    http = HIO_CONNECTION_HTTP(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_connection_http_set_property (GObject                *object,
                                  guint                   prop_id,
                                  const GValue           *value,
                                  GParamSpec             *pspec)
{
    HioConnectionHttp *http;

    http = HIO_CONNECTION_HTTP(object);

    switch (prop_id) {

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hio_connection_http_dispose(GObject *object)
{
    HioConnectionHttp *http;

    http = HIO_CONNECTION_HTTP(object);

    if (http->priv->response_chain != NULL) {
        g_object_unref(http->priv->response_chain);
        http->priv->response_chain = NULL;
    }

    G_OBJECT_CLASS(hio_connection_http_parent_class)->dispose(object);
}

static void
hio_connection_http_finalize(GObject *object)
{
    HioConnectionHttp *http;

    http = HIO_CONNECTION_HTTP(object);

    g_string_free(http->priv->path, TRUE);
    g_string_free(http->priv->query_string, TRUE);

    if (http->priv->current_header_name != NULL) {
        hrt_buffer_unref(http->priv->current_header_name);
        http->priv->current_header_name = NULL;
    }

    if (http->priv->current_header_value != NULL) {
        hrt_buffer_unref(http->priv->current_header_value);
        http->priv->current_header_value = NULL;
    }

    G_OBJECT_CLASS(hio_connection_http_parent_class)->finalize(object);
}

static void
hio_connection_http_init(HioConnectionHttp *http)
{
    http->priv =
        G_TYPE_INSTANCE_GET_PRIVATE(http,
                                    HIO_TYPE_CONNECTION_HTTP,
                                    HioConnectionHttpPrivate);

    http_parser_init(&http->priv->parser, HTTP_REQUEST);
    http->priv->parser.data = http;

    http->priv->path = g_string_new(NULL);
    http->priv->query_string = g_string_new(NULL);

    g_assert(http->priv->response_chain == NULL);
}

static GObject*
hio_connection_http_constructor(GType                  type,
                                guint                  n_construct_properties,
                                GObjectConstructParam *construct_params)
{
    GObject *object;
    HioConnectionHttp *http;

    object = G_OBJECT_CLASS(hio_connection_http_parent_class)->constructor(type,
                                                                           n_construct_properties,
                                                                           construct_params);

    http = HIO_CONNECTION_HTTP(object);

    g_assert(http->priv->response_chain == NULL);

    return object;
}

static void
hio_connection_http_class_init(HioConnectionHttpClass *klass)
{
    GObjectClass *object_class;
    HioConnectionClass *connection_class;

    object_class = G_OBJECT_CLASS(klass);
    connection_class = HIO_CONNECTION_CLASS(klass);

    object_class->constructor = hio_connection_http_constructor;
    object_class->get_property = hio_connection_http_get_property;
    object_class->set_property = hio_connection_http_set_property;

    object_class->dispose = hio_connection_http_dispose;
    object_class->finalize = hio_connection_http_finalize;

    connection_class->on_incoming_data = hio_connection_http_on_incoming_data;

    g_type_class_add_private(object_class, sizeof(HioConnectionHttpPrivate));
}
