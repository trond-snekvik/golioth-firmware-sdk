/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(stream, LOG_LEVEL_WRN);

#include <golioth/client.h>
#include <golioth/stream.h>
#include <golioth/ai.h>
#include <samples/common/sample_credentials.h>
#include <samples/common/net_connect.h>
#include <stdlib.h>
#include <string.h>
#include <golioth/rpc.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

struct golioth_client *client;
static K_SEM_DEFINE(connected, 0, 1);
static bool holding_button2 = false;

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

#if DT_NODE_HAS_STATUS(DT_ALIAS(temp0), okay)

static int get_temperature(struct sensor_value *val)
{
    const struct device *dev = DEVICE_DT_GET(DT_ALIAS(temp0));
    static const enum sensor_channel temp_channels[] = {
        SENSOR_CHAN_AMBIENT_TEMP,
        SENSOR_CHAN_DIE_TEMP,
    };
    int i;
    int err;

    err = sensor_sample_fetch(dev);
    if (err)
    {
        LOG_ERR("Failed to fetch temperature sensor: %d", err);
        return err;
    }

    for (i = 0; i < ARRAY_SIZE(temp_channels); i++)
    {
        err = sensor_channel_get(dev, temp_channels[i], val);
        if (err == -ENOTSUP)
        {
            /* try next channel */
            continue;
        }
        else if (err)
        {
            LOG_ERR("Failed to get temperature: %d", err);
            return err;
        }
    }

    return err;
}

#else

static int get_temperature(struct sensor_value *val)
{
    static int counter = 0;

    /* generate a temperature from 20 deg to 30 deg, with 0.5 deg step */

    val->val1 = 20 + counter / 2;
    val->val2 = counter % 2 == 1 ? 500000 : 0;

    // Every 5th temperature measurement is way too high:
    if (holding_button2)
    {
        val->val1 += 20;
        val->val2 = 0;
    }

    counter = (counter + 1) % 20;

    return 0;
}

#endif

static int panic;


static void blink_leds(struct k_work *work)
{
    static bool led_state;
    led_state = !led_state;
    dk_set_led(DK_LED1, led_state);
    dk_set_led(DK_LED2, !led_state);

    // only decrement every second
    if (led_state) {
        panic--;
    }

    if (panic) {
        k_work_reschedule(k_work_delayable_from_work(work), K_MSEC(250));
    } else {
        dk_set_led(DK_LED1, false);
        dk_set_led(DK_LED2, false);
    }

}

K_WORK_DELAYABLE_DEFINE(led_blinking_work, blink_leds);

static enum golioth_rpc_status on_panic(zcbor_state_t *request_params_array,
                                           zcbor_state_t *response_detail_map,
                                           void *callback_arg)
{
    double value;
    char message[256];
    struct zcbor_string cbor_message;
    bool ok;

    ok = zcbor_float_decode(request_params_array, &value)
        && zcbor_tstr_decode(request_params_array, &cbor_message);
    if (!ok)
    {
        LOG_ERR("Failed to decode array value");
        return GOLIOTH_RPC_INVALID_ARGUMENT;
    }

    cbor_message.len = MIN(cbor_message.len, sizeof(message));
    strncpy(message, cbor_message.value, cbor_message.len);
    message[cbor_message.len] = '\0';

    LOG_ERR("***********************************");
    LOG_ERR("%s", message);
    LOG_ERR("***********************************");
    panic = 10;
    k_work_schedule(&led_blinking_work, K_NO_WAIT);
    return GOLIOTH_RPC_OK;
}

static void temperature_push_handler(struct golioth_client *client,
                                     const struct golioth_response *response,
                                     const char *path,
                                     void *arg)
{
    if (response->status != GOLIOTH_OK)
    {
        LOG_WRN("Failed to push temperature: %d", response->status);
        return;
    }

    LOG_DBG("Temperature successfully pushed");

    return;
}

static void temperature_push_async(const struct sensor_value *temp)
{
    char sbuf[sizeof("{\"temp\":-4294967295.123456}")];
    int err;

    snprintk(sbuf, sizeof(sbuf), "{\"temp\":%d.%06d}", temp->val1, abs(temp->val2));

    err = golioth_stream_set_async(client,
                                   "sensor",
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   sbuf,
                                   strlen(sbuf),
                                   temperature_push_handler,
                                   NULL);
    if (err)
    {
        LOG_WRN("Failed to push temperature: %d", err);
        return;
    }
}


static char rsp_buf[1024];

static void response_handler(const char *rsp, void *arg)
{
    LOG_WRN("The Giant says: %s", rsp);
}

// Send a question to the Giant when a button is pressed
static void button_handler(uint32_t button_state, uint32_t has_changed)
{
    if (button_state & has_changed & DK_BTN1_MSK) {
        LOG_ERR("Asking the giant");
        int err = golioth_ask_giant_async(client,
                                          "What is the highest temperature in the last 10 minutes?",
                                          "sensor.temp",
                                          rsp_buf,
                                          sizeof(rsp_buf),
                                          response_handler,
                                          NULL);
        if (err)
        {
            LOG_ERR("Failed to ask the Giant: %d", err);
        }
    }
    if (has_changed & DK_BTN2_MSK) {
        holding_button2 = !!(button_state & DK_BTN2_MSK);
    }
}


int main(void)
{
    struct sensor_value temp;
    int err;

    LOG_DBG("Start LightDB Stream sample");

    IF_ENABLED(CONFIG_GOLIOTH_SAMPLE_COMMON, (net_connect();))

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    const struct golioth_client_config *client_config = golioth_sample_credentials_get();

    err = dk_leds_init();
    if (err) {
        LOG_ERR("Failed to initialize LEDs (err: %d)", err);
    }

    err = dk_buttons_init(button_handler);
    if (err) {
        LOG_ERR("Failed to initialize buttons (err: %d)", err);
    }

    client = golioth_client_create(client_config);
    golioth_client_register_event_callback(client, on_client_event, NULL);
    struct golioth_rpc *rpc = golioth_rpc_init(client);

    k_sem_take(&connected, K_FOREVER);

    err = golioth_ai_init(rpc);
    if (err) {
        LOG_ERR("Failed to initialize AI: %d", err);
    }


    err = golioth_rpc_register(rpc, "panic", on_panic, NULL);

    if (err)
    {
        LOG_ERR("Failed to register RPC: %d", err);
    }


    while (true)
    {
        /* Callback-based using JSON object */
        err = get_temperature(&temp);
        if (err)
        {
            k_sleep(K_SECONDS(1));
            continue;
        }

        LOG_DBG("Sending temperature %d.%06d (as object sync)", temp.val1, abs(temp.val2));

        temperature_push_async(&temp);
        k_sleep(K_SECONDS(10));
    }

    return 0;
}
