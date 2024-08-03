#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_freertos_hooks.h"
#include "lvgl.h"
#include "lvgl_helpers.h" 

#include "lvgl/demos/lv_demos.h"
 
void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
void lvgl_test();
 
SemaphoreHandle_t xGuiSemaphore;
 
static void guiTask(void *pvParameter)
{
    (void) pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();
 
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
 
    esp_register_freertos_tick_hook((void *)lv_tick_task);
 
    /* Create the demo application */
    
    lvgl_test();
 
    while (1)
    {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));
 
        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }
 
    /* A task should NEVER return */
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

void lvgl_test(void)
{
    LV_FONT_DECLARE(siyuan_simple_20);

    lv_obj_t *label1 = lv_label_create(lv_scr_act());
    lv_label_set_long_mode(label1, LV_LABEL_LONG_WRAP); /*Break the long lines*/
    lv_label_set_recolor(label1, true);                 /*Enable re-coloring by commands in the text*/
    lv_obj_set_style_text_font(label1, &siyuan_simple_20,0);
    lv_label_set_text(label1, "#0000ff 恭喜# #ff00ff 樊振东# 赢得比赛！");
    lv_obj_set_width(label1, 120); /*Set smaller width to make the lines wrap*/
    lv_obj_set_style_text_align(label1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label1, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t *label2 = lv_label_create(lv_scr_act());
    lv_label_set_long_mode(label2, LV_LABEL_LONG_SCROLL_CIRCULAR); /*Circular scroll*/
    lv_obj_set_width(label2, 120);
    lv_obj_set_style_text_font(label2, &siyuan_simple_20,0);
    lv_label_set_text(label2, "恭喜中国队打败小日子！");
    lv_obj_align(label2, LV_ALIGN_CENTER, 0, 40);
}