#include "motion_detector.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <math.h>
#include <stdio.h>

// Global state variables
static volatile int current_rssi = 0;
static volatile float baseline_rssi = 0.0f;
static volatile bool is_calibrating = true;
static volatile bool motion_detected = false;

// EMA Alpha (smoothing factor).
#define EMA_ALPHA 0.05f
#define MOTION_THRESHOLD_DB 6.0f // Drop in DB to trigger motion

// Getter implementations
int motion_detector_get_current_rssi(void) { return current_rssi; }
float motion_detector_get_baseline_rssi(void) { return baseline_rssi; }
bool motion_detector_is_motion_detected(void) { return motion_detected; }
bool motion_detector_is_calibrating(void) { return is_calibrating; }

static void motion_detector_task(void *param)
{
    int rssi_sum = 0;
    int rssi_count = 0;
    int calibration_duration_sec = 5;
    int measurements_per_sec = 5; // Loop runs every 200ms

    while (1)
    {
        wifi_ap_record_t ap;
        esp_err_t err = esp_wifi_sta_get_ap_info(&ap);

        if (err == ESP_OK)
        {
            current_rssi = ap.rssi;

            if (is_calibrating)
            {
                rssi_sum += current_rssi;
                rssi_count++;

                if (rssi_count >= (calibration_duration_sec * measurements_per_sec))
                {
                    baseline_rssi = (float)rssi_sum / (float)rssi_count;
                    is_calibrating = false;
                }
            }
            else
            {
                // Exponential Moving Average Update
                baseline_rssi = ((float)current_rssi * EMA_ALPHA) + (baseline_rssi * (1.0f - EMA_ALPHA));
                float drop = baseline_rssi - (float)current_rssi;

                if (drop > MOTION_THRESHOLD_DB)
                {
                    motion_detected = true;
                    gpio_set_level(GPIO_NUM_2, 1); // Turn ON built-in LED
                }
                else
                {
                    motion_detected = false;
                    gpio_set_level(GPIO_NUM_2, 0); // Turn OFF built-in LED
                }
            }

            // Output JSON strictly here. The Serial Dashboard will parse this.
            printf("{\"rssi\": %d, \"baseline\": %.2f, \"motion\": %s, \"calibrating\": %s}\n",
                   current_rssi, baseline_rssi, motion_detected ? "true" : "false", is_calibrating ? "true" : "false");

            if (motion_detected)
            {
                // Keep the alarm on for at least 2 seconds (10 ticks roughly since 1 tick is 200ms)
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        }
        else
        {
            // Just print empty/error state
            printf("{\"error\": \"Not Connected\"}\n");
        }

        vTaskDelay(pdMS_TO_TICKS(200)); // Check every 200ms for faster response
    }
}

void motion_detector_init(void)
{
    gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT); // Custom External Pin
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);  // Built-in LED Pin

    xTaskCreate(&motion_detector_task, "MotionDetector", 4096, NULL, 5, NULL);
}
