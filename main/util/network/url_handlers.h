#ifndef _URL_HANDLERS_H_
#define _URL_HANDLERS_H_

#include <stdlib.h>
#include <stdint.h>

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_vfs_semihost.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include <fcntl.h>

#include "esp_netif.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "web_wifi_config.h"
#include "sdkconfig.h"
#include "ui_display_handler.h"
#include "esp_http_client.h"
#include "esp_tls.h"

#include <esp_http_server.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_OUTPUTS  4
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

//宏定义条件判断，如果不满足则打印并跳转至goto_tag处
#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)

//rest接口上下文
typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

static const char web_base_point[] = "/www";

//校验文件后缀
#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

esp_err_t start_webserver(void);
esp_err_t init_fs(void);
void http_get_task(const char *url);

#ifdef __cplusplus
}
#endif

#endif