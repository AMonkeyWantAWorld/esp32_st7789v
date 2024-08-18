
#ifndef LV_TIME_HANDLER_H_
#define LV_TIME_HANDLER_H_

#include "esp_sntp.h"
#include "ds1302.h"
#include "esp_log.h"

#define sntp_setoperatingmode esp_sntp_setoperatingmode
#define sntp_setservername esp_sntp_setservername
#define sntp_init esp_sntp_init

#define CONFIG_CLK_GPIO 15
#define CONFIG_IO_GPIO 19
#define CONFIG_CE_GPIO 21
#define CONFIG_TIMEZONE 8

#define NTP_SERVER "pool.ntp.org"

void initialize_time();
void set_clock_sntp(struct timeval *tv);
void get_clock(DS1302_DateTime *dt);
void clock_init(void *pvParameters);
void dtime_to_timeval(DS1302_DateTime *ds1302_time, struct timeval *tv);
void set_clock_rtc(void *paramerts);

#endif