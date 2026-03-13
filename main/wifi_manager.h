#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

// Initializes Wi-Fi (NVS read, try Station, fallback to SoftAP)
void wifi_manager_init(void);

// For the web server to invoke saving new credentials and reboot
esp_err_t wifi_manager_save_config_and_reboot(const char* ssid, const char* password);

// Optional getter to know if it's running in softap mode
bool wifi_manager_is_softap(void);

#endif // WIFI_MANAGER_H
