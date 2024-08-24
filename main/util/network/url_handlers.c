/*
 * @Author: Taburiss
 * @Date: 2024-08-03 16:36:51
 * @LastEditTime: 2024-08-24 12:26:06
 * @FilePath: /esp32_st7789v/main/util/network/url_handlers.c
 * @Description: esp32配网文件
 */
#include "url_handlers.h"

//网页参数
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* TAG = "simple_server";

extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");

//字符转int
int httpdHexVal(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    return 0;
}

//对http请求进行编码
int httpdUrlDecode(char* val, int valLen, char* ret, int retLen)
{
    int s = 0, d = 0;
    int esced = 0, escVal = 0;

    while (s < valLen && d < retLen) {
        if (esced == 1)  {
            escVal = httpdHexVal(val[s]) << 4;
            esced = 2;
        } else if (esced == 2) {
            escVal += httpdHexVal(val[s]);
            ret[d++] = escVal;
            esced = 0;
        } else if (val[s] == '%') {
            esced = 1;
        } else if (val[s] == '+') {
            ret[d++] = ' ';
        } else {
            ret[d++] = val[s];
        }

        s++;
    }

    if (d < retLen) {
        ret[d] = 0;
    }

    return d;
}

//获取http的请求参数
int httpd_find_arg(const char* line, const char* arg, char* buff, int buffLen)
{
    char* p, *e;
    // bool first_in = true;

    if (line == NULL) {
        return -1;
    }

    p = line;

    while (p != NULL && *p != '\n' && *p != '\r' && *p != 0) {
        // printf("findArg: %s\n", p);
        if (strncmp(p, arg, strlen(arg)) == 0 && p[strlen(arg)] == '=') {
            p += strlen(arg) + 1; //move p to start of value
            e = (char*)strstr(p, "&");

            if (e == NULL) {
                e = p + strlen(p);
            }

            // printf("findArg: val %s len %d\n", p, (e-p));
            return httpdUrlDecode(p, (e - p), buff, buffLen);
        }

        p = (char*)strstr(p, "&"); // Changed
        
        if (p != NULL) {
            p += 1;
        }
    }

    printf("Finding %s in %s: Not found\n", arg, line);
    return -1; //not found
}

//根据文件后缀设置不同的http文本类型
esp_err_t set_content_type_from_file(httpd_req_t* req, const char* filepath)
{
    const char* type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

//按块对文件传输
esp_err_t custom_send_file_chunk(httpd_req_t* req, const char *filepath)
{
    rest_server_context_t* rest_context = (rest_server_context_t*) req->user_ctx;
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char* chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

//返回请求的http请求
esp_err_t rest_common_get_handler(httpd_req_t* req)
{
    char filepath[FILE_PATH_MAX];
    rest_server_context_t* rest_context = (rest_server_context_t*) req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }

    char* p = strrchr(filepath, '?');
    if (p != NULL) {
        *p = '\0';
    }
    if(custom_send_file_chunk(req, filepath) != ESP_OK) {
        ESP_LOGE(TAG, "rest common send err");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

//接收post请求的数据
esp_err_t recv_post_data(httpd_req_t* req, char* buf)
{
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;

    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';//now ,the post is str format, like ssid=yuxin&pwd=TestPWD&chl=1&ecn=0&maxconn=1&ssidhidden=0
    ESP_LOGI(TAG, "Post data is : %s\n", buf);
    return ESP_OK;
}

//初始化spiffs文件系统
esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = web_base_point,
        .partition_label = NULL,
        .max_files = 5,//maybe the num can be set smaller
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

void timer_callback(TimerHandle_t timer)
{
    esp_restart();
}

//esp重启定时器
void create_a_restart_timer(void)
{
    TimerHandle_t oneshot = xTimerCreate("oneshot", 5000 / portTICK_PERIOD_MS, pdFALSE,
                                         NULL, timer_callback);
    xTimerStart(oneshot, 1);

    printf("Restarting in 5 seconds...\n");
    fflush(stdout);
}

//通过http获取到的网络ssid和password
esp_err_t wifi_config_post_handler(httpd_req_t* req)
{
    lv_display_spinner(-4, 0, 50, 50, LV_OBJ_FLAG_HIDDEN);
    // lv_display_text(2, ui_Screen1,-4, 0, LV_ALIGN_CENTER, "获取wifi帐号和密码。。。");
    ESP_LOGD(TAG, "in / post handler");
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    // wifi_config_store(&wifi_config);

    char filepath[FILE_PATH_MAX];
    rest_server_context_t* rest_context = (rest_server_context_t*) req->user_ctx;
    char* buf = ((rest_server_context_t*) (req->user_ctx))->scratch;
    int str_len = 0;
    char temp_str[64] = {0};
    if (recv_post_data(req, buf) != ESP_OK) {
        ESP_LOGE(TAG, "recv post data error");
        return ESP_FAIL;
    }

    str_len = httpd_find_arg(buf, PARAM_INPUT_1, temp_str, sizeof(temp_str));
    if ((str_len != -1) && (strlen((char *)temp_str) != 0)) {
        memcpy((char *)wifi_config.sta.ssid, temp_str, 32);
        ESP_LOGI(TAG, "ssid:%s", (char *)wifi_config.sta.ssid);
    }
    
    memset(temp_str, '\0', sizeof(temp_str));

    str_len = httpd_find_arg(buf, PARAM_INPUT_2, temp_str, sizeof(temp_str));
    if ((str_len != -1) && (strlen((char *)temp_str) != 0)) {
        memcpy((char *)wifi_config.sta.password, temp_str, 64);
        ESP_LOGI(TAG, "pwd:%s", (char *)wifi_config.sta.password);
    }
    if(!wifi_config_store(&wifi_config)) {
        return ESP_FAIL;
    }
    // lv_display_text(2, ui_Screen1,-4, 0, LV_ALIGN_CENTER, "3秒后即将重启连接wifi。。。");
    vTaskDelay(3000/portTICK_PERIOD_MS);
    esp_restart();

    return ESP_OK;
}

//wifi管理页面获取
esp_err_t wifi_manage_html_get_handler(httpd_req_t* req)
{
    char filepath[FILE_PATH_MAX];
    rest_server_context_t* rest_context = (rest_server_context_t*) req->user_ctx;
    
    // return index html file
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    strlcat(filepath, "/wifimanager.html", sizeof(filepath));
    if(custom_send_file_chunk(req, filepath) != ESP_OK) {
        ESP_LOGE(TAG, "rest common send err");
        return ESP_FAIL;
    }

    return ESP_OK;
}

//wifi配置页面获取
esp_err_t softap_wifi_html_get_handler(httpd_req_t* req)
{
    char filepath[FILE_PATH_MAX];
    rest_server_context_t* rest_context = (rest_server_context_t*) req->user_ctx;
    
    // return index html file
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    strlcat(filepath, "/wifimanager_softap.html", sizeof(filepath));
    if(custom_send_file_chunk(req, filepath) != ESP_OK) {
        ESP_LOGE(TAG, "rest common send err");
        return ESP_FAIL;
    }

    return ESP_OK;
}

//ota上传页面获取
esp_err_t ota_html_get_handler(httpd_req_t* req)
{
    char filepath[FILE_PATH_MAX];
    rest_server_context_t* rest_context = (rest_server_context_t*) req->user_ctx;
    
    // return index html file
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    strlcat(filepath, "/ota.html", sizeof(filepath));
    if(custom_send_file_chunk(req, filepath) != ESP_OK) {
        ESP_LOGE(TAG, "rest common send err");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * 方法描述：重启esp32的请求页面
 * 作者:Taburiss
 * 创建时间:2024/08/24 11:53
*/
esp_err_t reboot_html_get_handler(httpd_req_t* req)
{
    char filepath[FILE_PATH_MAX];
    rest_server_context_t* rest_context = (rest_server_context_t*) req->user_ctx;
    
    // return index html file
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    strlcat(filepath, "/reboot.html", sizeof(filepath));
    if(custom_send_file_chunk(req, filepath) != ESP_OK) {
        ESP_LOGE(TAG, "rest common send err");
        return ESP_FAIL;
    }

    create_a_restart_timer();
    return ESP_OK;
}

static bool s_sta_connected = false;

/**
 * 方法描述：开启web服务
 * 作者:Taburiss
 * 创建时间:2024/08/24 11:52
*/
esp_err_t start_webserver(void)
{
    rest_server_context_t* rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, web_base_point, sizeof(rest_context->base_path));

    httpd_uri_t *uris = NULL;
    int uris_len = 0;
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 7;
    config.max_open_sockets = 7;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 5912;

    ESP_LOGI(TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);

    httpd_uri_t httpd_uri_array_softap_only[] = {
        {"/wifi_config", HTTP_POST, wifi_config_post_handler, rest_context},
        {"/*", HTTP_GET, softap_wifi_html_get_handler,rest_context}
    };

    httpd_uri_t httpd_uri_array[] = {
        {"/wifimanager", HTTP_GET, wifi_manage_html_get_handler, rest_context},
        {"/ota", HTTP_GET, ota_html_get_handler, rest_context},
        {"/wifi_config", HTTP_POST, wifi_config_post_handler, rest_context},
        // {"/update", HTTP_POST, OTA_update_post_handler, rest_context},
        // {"/status", HTTP_POST, OTA_update_status_handler, rest_context},
        {"/reboot", HTTP_GET, reboot_html_get_handler, rest_context},
        {"/*", HTTP_GET, rest_common_get_handler,rest_context}//Catch-all callback function for the filesystem, this must be set to the array last one
    };

    if (!s_sta_connected) {
        uris = httpd_uri_array_softap_only;
        uris_len = sizeof(httpd_uri_array_softap_only)/sizeof(httpd_uri_t);
    } else {
        uris = httpd_uri_array;
        uris_len = sizeof(httpd_uri_array)/sizeof(httpd_uri_t);
    }

    for(int i = 0; i < uris_len; i++){
        if (httpd_register_uri_handler(server, &uris[i]) != ESP_OK) {
            ESP_LOGE(TAG, "httpd register uri_array[%d] fail", i);
        }
    }

    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}

/**
 * 方法描述：http访问回调函数，处理各个事件的方法
 * 作者:Taburiss
 * 创建时间:2024/08/24 11:53
*/
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    /* response数据的缓冲区 */
    static char *output_buffer; 
    /* response数据的长度，即读取到bytes长度*/
    static int output_len;      
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // Clean the buffer in case of a new request
            if (output_len == 0 && evt->user_data) {
                // we are just starting to copy the output data into the use
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                int copy_len = 0;
                if (evt->user_data) {
                    // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                    ESP_LOGI(TAG, "Response is %s", output_buffer);
                }
                output_len += copy_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}

/**
 * 方法描述：http或https请求，已经关闭tls验证
 * 作者：Taburiss
 * 创建时间：2024/08/24 11:55
*/
void http_get_task(const char *url)
{
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        // .user_data = url,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    /* 发起get请求 */
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d",
                esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    /* 释放资源 */
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}
