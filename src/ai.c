/*
 * Copyright (c) 2022-2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <zcbor_encode.h>
#include "coap_client.h"
#include <golioth/ai.h>
#include <golioth/rpc.h>
#include <golioth/stream.h>
#include <golioth/golioth_debug.h>
#include <golioth/zcbor_utils.h>

LOG_TAG_DEFINE(golioth_ai);

#define CBOR_AI_MAX_LEN 1024
#define TIMEOUT_S 10

struct request
{
    bool pending;
    char *buf;
    size_t buf_len;
    golioth_giant_response_cb_fn callback;
    void *callback_arg;
};

static struct request request;

enum golioth_status golioth_ask_giant_async(struct golioth_client *client,
                                           const char *question,
                                           const char *data_path,
                                           char *rsp_buf,
                                           size_t rsp_buf_len,
                                           golioth_giant_response_cb_fn callback,
                                           void *callback_arg)
{
    if (request.pending) {
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    uint8_t *cbor_buf = malloc(CBOR_AI_MAX_LEN);
    enum golioth_status status = GOLIOTH_ERR_SERIALIZE;
    bool ok;


    if (!cbor_buf)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    request.buf = rsp_buf;
    request.buf_len = rsp_buf_len;
    request.callback = callback;
    request.callback_arg = callback_arg;
    request.pending = true;

    ZCBOR_STATE_E(zse, 1, cbor_buf, CBOR_AI_MAX_LEN, 1);

    ok = zcbor_map_start_encode(zse, 3);
    if (!ok)
    {
        status = GOLIOTH_ERR_SERIALIZE;
        goto cleanup;
    }

    ok = zcbor_tstr_put_lit(zse, "path")
        && (data_path ? zcbor_tstr_put_term_compat(zse, data_path, SIZE_MAX)
                      : zcbor_nil_put(zse, NULL))
        && zcbor_tstr_put_lit(zse, "q") && zcbor_tstr_put_term_compat(zse, question, SIZE_MAX);
    if (!ok)
    {
        status = 100 + GOLIOTH_ERR_SERIALIZE;
        goto cleanup;
    }

    ok = zcbor_map_end_encode(zse, 3);
    if (!ok)
    {
        status = GOLIOTH_ERR_SERIALIZE;
        goto cleanup;
    }

    status = golioth_stream_set_sync(client,
                                      "giant_ai",
                                      GOLIOTH_CONTENT_TYPE_CBOR,
                                      cbor_buf,
                                      zse->payload - cbor_buf,
                                      TIMEOUT_S);
    if (status != GOLIOTH_OK) {
        request.pending = false;
    }
cleanup:
    free(cbor_buf);
    return status;
}


static enum golioth_rpc_status on_response(zcbor_state_t *request_params_array,
                                           zcbor_state_t *response_detail_map,
                                           void *callback_arg)
{
    struct zcbor_string cbor_rsp;
    bool ok;

    ok =zcbor_tstr_decode(request_params_array, &cbor_rsp);
    if (!ok)
    {
        LOG_ERR("Failed to decode response %s", request_params_array->payload);
        return GOLIOTH_RPC_INVALID_ARGUMENT;
    }
    if (!request.pending) {
        LOG_ERR("Response received but no request pending");
        return GOLIOTH_RPC_OK;
    }

    cbor_rsp.len = MIN(cbor_rsp.len, request.buf_len - 1);
    strncpy(request.buf, cbor_rsp.value, cbor_rsp.len);
    request.buf[cbor_rsp.len] = '\0';
    request.callback(request.buf, request.callback_arg);

    request.pending =  false;

    return GOLIOTH_RPC_OK;
}

enum golioth_status golioth_ai_init(struct golioth_rpc *rpc)
{
    return golioth_rpc_register(rpc, "giant_ai_rsp", on_response, NULL);
}
