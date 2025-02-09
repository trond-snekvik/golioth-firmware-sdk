/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <golioth/stream.h>
#include <golioth/golioth_sys.h>
#include "coap_client.h"
#include "golioth_util.h"

#if defined(CONFIG_GOLIOTH_STREAM)

#define GOLIOTH_STREAM_PATH_PREFIX ".s/"

enum golioth_status golioth_stream_set_int_async(struct golioth_client *client,
                                                 const char *path,
                                                 int32_t value,
                                                 golioth_set_cb_fn callback,
                                                 void *callback_arg)
{
    char buf[16] = {};
    snprintf(buf, sizeof(buf), "%" PRId32, value);
    return golioth_coap_client_set(client,
                                   GOLIOTH_STREAM_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) buf,
                                   strlen(buf),
                                   callback,
                                   callback_arg,
                                   false,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_stream_set_bool_async(struct golioth_client *client,
                                                  const char *path,
                                                  bool value,
                                                  golioth_set_cb_fn callback,
                                                  void *callback_arg)
{
    const char *valuestr = (value ? "true" : "false");
    return golioth_coap_client_set(client,
                                   GOLIOTH_STREAM_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) valuestr,
                                   strlen(valuestr),
                                   callback,
                                   callback_arg,
                                   false,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_stream_set_float_async(struct golioth_client *client,
                                                   const char *path,
                                                   float value,
                                                   golioth_set_cb_fn callback,
                                                   void *callback_arg)
{
    char buf[32] = {};
    snprintf(buf, sizeof(buf), "%f", (double) value);
    return golioth_coap_client_set(client,
                                   GOLIOTH_STREAM_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) buf,
                                   strlen(buf),
                                   callback,
                                   callback_arg,
                                   false,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_stream_set_string_async(struct golioth_client *client,
                                                    const char *path,
                                                    const char *str,
                                                    size_t str_len,
                                                    golioth_set_cb_fn callback,
                                                    void *callback_arg)
{
    // Server requires that non-JSON-formatted strings
    // be surrounded with literal ".
    //
    // It's inefficient, but we're going to copy the string
    // so we can surround it in quotes.
    //
    // TODO - is there a better way to handle this?
    size_t bufsize = str_len + 3;  // two " and a NULL
    char *buf = golioth_sys_malloc(bufsize);
    if (!buf)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }
    memset(buf, 0, bufsize);
    snprintf(buf, bufsize, "\"%s\"", str);

    enum golioth_status status = golioth_coap_client_set(client,
                                                         GOLIOTH_STREAM_PATH_PREFIX,
                                                         path,
                                                         GOLIOTH_CONTENT_TYPE_JSON,
                                                         (const uint8_t *) buf,
                                                         bufsize - 1,  // excluding NULL
                                                         callback,
                                                         callback_arg,
                                                         false,
                                                         GOLIOTH_SYS_WAIT_FOREVER);

    golioth_sys_free(buf);
    return status;
}

enum golioth_status golioth_stream_set_async(struct golioth_client *client,
                                             const char *path,
                                             enum golioth_content_type content_type,
                                             const uint8_t *buf,
                                             size_t buf_len,
                                             golioth_set_cb_fn callback,
                                             void *callback_arg)
{
    return golioth_coap_client_set(client,
                                   GOLIOTH_STREAM_PATH_PREFIX,
                                   path,
                                   content_type,
                                   buf,
                                   buf_len,
                                   callback,
                                   callback_arg,
                                   false,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_stream_set_int_sync(struct golioth_client *client,
                                                const char *path,
                                                int32_t value,
                                                int32_t timeout_s)
{
    char buf[16] = {};
    snprintf(buf, sizeof(buf), "%" PRId32, value);
    return golioth_coap_client_set(client,
                                   GOLIOTH_STREAM_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) buf,
                                   strlen(buf),
                                   NULL,
                                   NULL,
                                   true,
                                   timeout_s);
}

enum golioth_status golioth_stream_set_bool_sync(struct golioth_client *client,
                                                 const char *path,
                                                 bool value,
                                                 int32_t timeout_s)
{
    const char *valuestr = (value ? "true" : "false");
    return golioth_coap_client_set(client,
                                   GOLIOTH_STREAM_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) valuestr,
                                   strlen(valuestr),
                                   NULL,
                                   NULL,
                                   true,
                                   timeout_s);
}

enum golioth_status golioth_stream_set_float_sync(struct golioth_client *client,
                                                  const char *path,
                                                  float value,
                                                  int32_t timeout_s)
{
    char buf[32] = {};
    snprintf(buf, sizeof(buf), "%f", (double) value);
    return golioth_coap_client_set(client,
                                   GOLIOTH_STREAM_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) buf,
                                   strlen(buf),
                                   NULL,
                                   NULL,
                                   true,
                                   timeout_s);
}

enum golioth_status golioth_stream_set_string_sync(struct golioth_client *client,
                                                   const char *path,
                                                   const char *str,
                                                   size_t str_len,
                                                   int32_t timeout_s)
{
    // Server requires that non-JSON-formatted strings
    // be surrounded with literal ".
    size_t bufsize = str_len + 3;  // two " and a NULL
    char *buf = golioth_sys_malloc(bufsize);
    if (!buf)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }
    memset(buf, 0, bufsize);
    snprintf(buf, bufsize, "\"%s\"", str);

    enum golioth_status status = golioth_coap_client_set(client,
                                                         GOLIOTH_STREAM_PATH_PREFIX,
                                                         path,
                                                         GOLIOTH_CONTENT_TYPE_JSON,
                                                         (const uint8_t *) buf,
                                                         bufsize - 1,  // excluding NULL
                                                         NULL,
                                                         NULL,
                                                         true,
                                                         timeout_s);

    golioth_sys_free(buf);
    return status;
}

enum golioth_status golioth_stream_set_sync(struct golioth_client *client,
                                            const char *path,
                                            enum golioth_content_type content_type,
                                            const uint8_t *buf,
                                            size_t buf_len,
                                            int32_t timeout_s)
{
    return golioth_coap_client_set(client,
                                   GOLIOTH_STREAM_PATH_PREFIX,
                                   path,
                                   content_type,
                                   buf,
                                   buf_len,
                                   NULL,
                                   NULL,
                                   true,
                                   timeout_s);
}

#endif  // CONFIG_GOLIOTH_STREAM
