/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <golioth/golioth_status.h>
#include <golioth/rpc.h>
#include <golioth/client.h>

typedef void (*golioth_giant_response_cb_fn)(const char *response, void *callback_arg);

enum golioth_status golioth_ask_giant_async(struct golioth_client *client,
                                            const char *question,
                                            const char *data_path,
                                            char *rsp_buf,
                                            size_t rsp_buf_len,
                                            golioth_giant_response_cb_fn callback,
                                            void *callback_arg);
enum golioth_status golioth_ai_init(struct golioth_rpc *rpc);
