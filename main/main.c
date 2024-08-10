#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_freertos_hooks.h"
#include "lvgl_helpers.h" 
#include "ui.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "lvgl/demos/lv_demos.h"
 
void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
 
static void guiTask(void *pvParameter)
{
    (void) pvParameter;
 
    lv_init();
    lvgl_driver_init();
 
    lv_color_t* buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);
    lv_color_t *buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2 != NULL);
 
    static lv_disp_draw_buf_t disp_buf;
    uint32_t size_in_px = DISP_BUF_SIZE;
 
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, size_in_px);
 
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
 
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);
 
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1 * 1000));
 
    ui_init();
 
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_task_handler();
    }
 
    free(buf1);
    free(buf2);
    vTaskDelete(NULL);
}
 
void app_main(void)
{
    xTaskCreatePinnedToCore(guiTask, "gui", 4096 * 4, NULL, 0, NULL, 1);
}
 
void lv_tick_task(void *arg)
{
    (void)arg;
    lv_tick_inc(portTICK_PERIOD_MS);
}