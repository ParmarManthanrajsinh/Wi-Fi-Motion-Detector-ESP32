#include "wifi_manager.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"

static const char *TAG = "wifi_manager";

#define NVS_NAMESPACE "wifi_cfg"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASS "password"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
#define MAXIMUM_RETRY 5

static void start_station(const char *ssid, const char *pass);

static void serial_listener_task(void *pvParameters)
{
    char line[256];
    while (1)
    {
        // Read line from USB Serial / UART0
        if (fgets(line, sizeof(line), stdin) != NULL)
        {
            cJSON *root = cJSON_Parse(line);
            if (root)
            {
                cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
                cJSON *password = cJSON_GetObjectItem(root, "password");

                if (ssid && password && ssid->valuestring && password->valuestring)
                {
                    ESP_LOGI(TAG, "Received new Wi-Fi credentials via Serial. Saving and restarting...");
                    wifi_manager_save_config_and_reboot(ssid->valuestring, password->valuestring);
                }
                cJSON_Delete(root);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_manager_init(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Start serial listener so laptop can configure it over USB anytime
    xTaskCreate(serial_listener_task, "serial_listener", 4096, NULL, 5, NULL);

    // NVS Read Check
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);

    char ssid[32] = {0};
    char pass[64] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(pass);

    bool has_creds = false;

    if (err == ESP_OK)
    {
        err = nvs_get_str(my_handle, NVS_KEY_SSID, ssid, &ssid_len);
        if (err == ESP_OK)
        {
            err = nvs_get_str(my_handle, NVS_KEY_PASS, pass, &pass_len);
            if (err == ESP_OK)
            {
                has_creds = true;
                ESP_LOGI(TAG, "Found stored Wi-Fi credentials for SSID: %s", ssid);
            }
        }
        nvs_close(my_handle);
    }
    else
    {
        ESP_LOGI(TAG, "No NVS config found. Waiting for configuring via Serial Dashboard...");
    }

    if (has_creds)
    {
        start_station(ssid, pass);

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                               pdFALSE,
                                               pdFALSE,
                                               portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT)
        {
            ESP_LOGI(TAG, "connected to ap SSID:%s", ssid);
        }
        else if (bits & WIFI_FAIL_BIT)
        {
            ESP_LOGI(TAG, "Failed to connect to SSID:%s. Waiting for configuring via Serial Dashboard...", ssid);
            while (1)
            {
                printf("{\"error\": \"Not Connected\"}\\n");
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        else
        {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }
    }
    else
    {
        // No creds, wait for configuring
        while (1)
        {
            printf("{\"error\": \"Not Connected\"}\\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

static void start_station(const char *ssid, const char *pass)
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
        },
    };

    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

esp_err_t wifi_manager_save_config_and_reboot(const char *ssid, const char *password)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    err = nvs_set_str(my_handle, NVS_KEY_SSID, ssid);
    if (err != ESP_OK)
        return err;

    err = nvs_set_str(my_handle, NVS_KEY_PASS, password);
    if (err != ESP_OK)
        return err;

    err = nvs_commit(my_handle);
    if (err != ESP_OK)
        return err;

    nvs_close(my_handle);

    ESP_LOGI(TAG, "Wi-Fi Config saved. Rebooting in 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}
