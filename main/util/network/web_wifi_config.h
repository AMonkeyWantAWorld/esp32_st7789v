/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */
#ifndef _APP_WIFI_H_
#define _APP_WIFI_H_

#include "esp_wifi.h"
#include "ui_display_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXAMPLE_ESP_MAXIMUM_RETRY  4
#define EXAMPLE_ESP_WIFI_AP_SSID   "fly_pig"
#define EXAMPLE_ESP_WIFI_AP_PASS   "123456qwe"
#define EXAMPLE_MAX_STA_CONN       4
#define EXAMPLE_IP_ADDR            "192.168.4.2"
#define EXAMPLE_ESP_WIFI_AP_CHANNEL 1
#define WIFI_CONFIG_NAMESPACE        "c_wifi"

wifi_mode_t app_wifi_main(bool *station_connected);
bool wifi_config_store(wifi_config_t *cfg);
bool wifi_config_load_from_partition(const char* namespace, wifi_config_t *cfg);
bool wifi_init_sta(wifi_config_t *cfg);
#ifdef __cplusplus
}
#endif

#endif
