/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "nvs.h"
#include "shell.h"
#include "wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <golioth/client.h>
#include <golioth/lightdb_state.h>
#include <string.h>

#define TAG "lightdb"

struct golioth_client *client;
static SemaphoreHandle_t _connected_sem = NULL;

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        xSemaphoreGive(_connected_sem);
    }
    GLTH_LOGI(TAG, "Golioth client %s", is_connected ? "connected" : "disconnected");
}

static void counter_observe_handler(struct golioth_client *client,
                                    const struct golioth_response *response,
                                    const char *path,
                                    const uint8_t *payload,
                                    size_t payload_size,
                                    void *arg)
{
    if (response->status != GOLIOTH_OK)
    {
        GLTH_LOGW(TAG, "Failed to receive observed payload: %d", response->status);
        return;
    }

    GLTH_LOG_BUFFER_HEXDUMP(TAG, payload, payload_size, GOLIOTH_DEBUG_LOG_LEVEL_INFO);

    return;
}

void app_main(void)
{
    GLTH_LOGI(TAG, "Start LightDB observe sample");

    // Initialize NVS first. For this example, it is assumed that WiFi and Golioth
    // PSK credentials are stored in NVS.
    nvs_init();

    // Create a background shell/CLI task (type "help" to see a list of supported commands)
    shell_start();

    // If the credentials haven't been set in NVS, we will wait here for the user
    // to input them via the shell.
    if (!nvs_credentials_are_set())
    {
        while (1)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            GLTH_LOGW(TAG, "WiFi and golioth credentials are not set");
            GLTH_LOGW(TAG, "Use the shell settings commands to set them, then restart");
            vTaskDelay(UINT32_MAX);
        }
    }

    // Initialize WiFi and wait for it to connect
    wifi_init(nvs_read_wifi_ssid(), nvs_read_wifi_password());
    wifi_wait_for_connected();

    // Now we are ready to connect to the Golioth cloud.
    const char *psk_id = nvs_read_golioth_psk_id();
    const char *psk = nvs_read_golioth_psk();

    struct golioth_client_config config = {
        .credentials =
            {
                .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
                .psk =
                    {
                        .psk_id = psk_id,
                        .psk_id_len = strlen(psk_id),
                        .psk = psk,
                        .psk_len = strlen(psk),
                    },
            },
    };
    struct golioth_client *client = golioth_client_create(&config);
    assert(client);

    _connected_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(_connected_sem);

    golioth_client_register_event_callback(client, on_client_event, NULL);

    GLTH_LOGW(TAG, "Waiting for connection to Golioth...");
    xSemaphoreTake(_connected_sem, portMAX_DELAY);

    /* Observe LightDB State Path */
    golioth_lightdb_observe_async(client, "counter", counter_observe_handler, NULL);

    while (true)
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
