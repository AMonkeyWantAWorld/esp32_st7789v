// Copyright 2021-2022 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "esp_http_server.h"
#include "url_handlers.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "wifi_sta_ap.h"
#include "web_wifi_config.h"

#if (ESP_IDF_VERSION_MAJOR >= 5)
#include "esp_mac.h"
#include "lwip/ip_addr.h"
#endif

/* The examples use WiFi configuration that you can set via 'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
// #define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
// #define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD

static const char *TAG = "simple_wifi_server";

static int s_retry_num = 0;
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group = NULL;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data)
{
    /* AP mode */
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
            MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
            MAC2STR(event->mac), event->aid);
    }
    /* Sta mode */
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
        if(s_retry_num == EXAMPLE_ESP_MAXIMUM_RETRY){
            ESP_LOGI(TAG, "reset wifi config and reboot esp32");
            wifi_config_t wifi_config;
            memset(&wifi_config, 0, sizeof(wifi_config_t));
            memcpy((char *)wifi_config.sta.ssid, "", 32);
            wifi_config_store(&wifi_config);
            esp_restart();
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }

    return;
}

static void wifi_init_softap(esp_netif_t *netif)
{
    if (strcmp(EXAMPLE_IP_ADDR, "192.168.4.1")) {
        int a, b, c, d;
        sscanf(EXAMPLE_IP_ADDR, "%d.%d.%d.%d", &a, &b, &c, &d);
        esp_netif_ip_info_t ip_info;
        IP4_ADDR(&ip_info.ip, a, b, c, d);
        IP4_ADDR(&ip_info.gw, a, b, c, d);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        ESP_ERROR_CHECK(esp_netif_dhcps_stop(netif));
        ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));
        ESP_ERROR_CHECK(esp_netif_dhcps_start(netif));
    }
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    snprintf((char *)wifi_config.ap.ssid, 32, "%s", EXAMPLE_ESP_WIFI_AP_SSID);
    wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);
    snprintf((char *)wifi_config.ap.password, 64, "%s", EXAMPLE_ESP_WIFI_AP_PASS);
    wifi_config.ap.max_connection = EXAMPLE_MAX_STA_CONN;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    if (strlen(EXAMPLE_ESP_WIFI_AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    if (strlen(EXAMPLE_ESP_WIFI_AP_CHANNEL)) {
        int channel;
        sscanf(EXAMPLE_ESP_WIFI_AP_CHANNEL, "%d", &channel);
        wifi_config.ap.channel = channel;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));

    ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s",
        EXAMPLE_ESP_WIFI_AP_SSID, EXAMPLE_ESP_WIFI_AP_PASS);
}

bool wifi_config_load_from_partition(const char* namespace, wifi_config_t *cfg)
{
    nvs_handle_t handle;
    size_t size;

    esp_err_t err_t = nvs_open(namespace, NVS_READONLY, &handle);

    if ( err_t != ESP_OK) {
        ESP_ERROR_CHECK(err_t);
        ESP_LOGE(TAG, "nvs open from partition fail");
        return false;
    }

    size = sizeof(cfg->sta.ssid);
    if (nvs_get_blob(handle, "w.sta_ssid", cfg->sta.ssid, &size) != ESP_OK) {
        ESP_LOGE(TAG, "load wifi sta ssid fail");
    }

    size = sizeof(cfg->sta.password);
    if (nvs_get_blob(handle, "w.sta_pwd", cfg->sta.password, &size) != ESP_OK) {
        ESP_LOGE(TAG, "load wifi sta pwd fail");
    }

    nvs_close(handle);

    return true;
}

bool wifi_config_store(wifi_config_t *cfg)
{
    nvs_handle_t handle;
    size_t size;
    if (nvs_open(WIFI_CONFIG_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "nvs open fail in wifi config");
        return false;
    }

    size = sizeof(cfg->sta.ssid);
    if (nvs_set_blob(handle, "w.sta_ssid", cfg->sta.ssid, size) != ESP_OK) {
        ESP_LOGE(TAG, "save wifi sta ssid fail");
    }

    size = sizeof(cfg->sta.password);
    if (nvs_set_blob(handle, "w.sta_pwd", cfg->sta.password, size) != ESP_OK) {
        ESP_LOGE(TAG, "save wifi sta pwd fail");
    }

    if (nvs_commit(handle) != ESP_OK) {
        ESP_LOGE(TAG, "nvs commit fail");
        goto clean_up;
    }

    ESP_LOGI(TAG, "wifi config store successfully, restart to connect the wifi");
    nvs_close(handle);
    return true;
clean_up:
    nvs_close(handle);
    return false;
}

bool wifi_init_sta(wifi_config_t *cfg)
{
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, cfg));
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
        cfg->sta.ssid, cfg->sta.password);
    return true;
}

wifi_mode_t app_wifi_main(bool *station_connected)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_mode_t mode = WIFI_MODE_NULL;
    wifi_config_t wifi_config;
    EventBits_t bits = 0;
    *station_connected = false;
    memset(&wifi_config, 0, sizeof(wifi_config_t));

    if(!wifi_config_load_from_partition(WIFI_CONFIG_NAMESPACE, &wifi_config)) {
        ESP_LOGW(TAG, "wifi sta load fail.");
    }

    if (strlen(EXAMPLE_ESP_WIFI_AP_SSID) && strlen((char *)wifi_config.sta.ssid)) {
        mode = WIFI_MODE_STA;
        ESP_LOGI(TAG, "mode:WIFI_MODE_STA");
    } else if (strlen(EXAMPLE_ESP_WIFI_AP_SSID)) {
        ESP_LOGI(TAG, "mode:WIFI_MODE_AP");
        mode = WIFI_MODE_AP;
    } 

    if (mode == WIFI_MODE_NULL) {
        ESP_LOGW(TAG, "Neither AP or STA have been configured. WiFi will be off.");
        return WIFI_MODE_NULL;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));

    if (mode & WIFI_MODE_AP) {
        wifi_init_softAp();
        ESP_ERROR_CHECK(init_fs());
        start_webserver();
    }

    if (mode & WIFI_MODE_STA) {
        esp_netif_create_default_wifi_sta();
        wifi_init_sta(&wifi_config);
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_LOGI(TAG, "wifi init finished.");

    if (mode & WIFI_MODE_STA) {
        bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);
    }
    
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;
    if(bits & WIFI_CONNECTED_BIT) {
        *station_connected = true;
    }

    return mode;
}
