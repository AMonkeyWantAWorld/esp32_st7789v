#include "lv_time_handler.h"

static const char *TAG_DS = "DS1302";
DS1302_Dev date_dev;

void initialize_time(void)
{
	ESP_LOGI(TAG_DS, "Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	ESP_LOGI(TAG_DS, "Your NTP Server is %s", NTP_SERVER);
	sntp_setservername(8, NTP_SERVER);
	sntp_set_time_sync_notification_cb(set_clock_sntp);
	sntp_init();

	int retry = 0;
    const int retry_count = 3;
    while(sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count){
        ESP_LOGI(TAG_DS, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

	if(retry == retry_count){
		set_clock_rtc(NULL);
	}

}

void set_clock_rtc(void *paramerts){
	clock_init(NULL);

	DS1302_DateTime dt;
	get_clock(&dt);

	ESP_LOGI(pcTaskGetName(0), "Get time by ds1302 %d %02d-%02d-%d %d:%02d:%02d",
				dt.dayWeek, dt.dayMonth, dt.month, dt.year, dt.hour, dt.minute, dt.second);
	
	struct timeval tv;
	dtime_to_timeval(&dt, &tv);
	settimeofday(&tv, NULL);
	ESP_LOGI(pcTaskGetName(0), "Write rtc time to system.");
}

void dtime_to_timeval(DS1302_DateTime *ds1302_time, struct timeval *tv){
	struct tm tm;
	tm.tm_sec = ds1302_time->second;
    tm.tm_min = ds1302_time->minute;
    tm.tm_hour = ds1302_time->hour;
    tm.tm_mday = ds1302_time->dayMonth;
    tm.tm_mon = ds1302_time->month - 1; // tm_mon 从0开始计数
    tm.tm_year = ds1302_time->year + 100 - 1900; // 调整为自1900年起的年数
    tm.tm_isdst = -1; // 让系统自动判断夏令时

	// 计算 Unix 时间戳
    time_t t = mktime(&tm);

	tv->tv_sec = t;
	tv->tv_usec = 0;
}

void set_clock_sntp(struct timeval *tv)
{
	clock_init(NULL);
	// update 'now' variable with current time
	time_t now;
	struct tm timeinfo;
	char strftime_buf[64];
	time(&now);
	now = now + (CONFIG_TIMEZONE*60*60);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

	// Initialize RTC
	if (!DS1302_begin(&date_dev, CONFIG_CLK_GPIO, CONFIG_IO_GPIO, CONFIG_CE_GPIO)) {
		ESP_LOGE(pcTaskGetName(0), "Error: DS1302 begin");
		while (1) { vTaskDelay(1); }
	}
	ESP_LOGI(pcTaskGetName(0), "Set initial date time...");

	/*
	Member	  Type Meaning(Range)
	tm_sec	  int  seconds after the minute(0-60)
	tm_min	  int  minutes after the hour(0-59)
	tm_hour   int  hours since midnight(0-23)
	tm_mday   int  day of the month(1-31)
	tm_mon	  int  months since January(0-11)
	tm_year   int  years since 1900
	tm_wday   int  days since Sunday(0-6)
	tm_yday   int  days since January 1(0-365)
	tm_isdst  int  Daylight Saving Time flag	
	*/
	ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_sec=%d",timeinfo.tm_sec);
	ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_min=%d",timeinfo.tm_min);
	ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_hour=%d",timeinfo.tm_hour);
	ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_wday=%d",timeinfo.tm_wday);
	ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_mday=%d",timeinfo.tm_mday);
	ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_mon=%d",timeinfo.tm_mon);
	ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_year=%d",timeinfo.tm_year);

	// Set initial date and time
	DS1302_DateTime dt;
	dt.second = timeinfo.tm_sec;
	dt.minute = timeinfo.tm_min;
	dt.hour = timeinfo.tm_hour;
	dt.dayWeek = timeinfo.tm_wday; // 0= Sunday 1 = Monday
	dt.dayMonth = timeinfo.tm_mday;
	dt.month = (timeinfo.tm_mon + 1);
	dt.year = (timeinfo.tm_year + 1900);
	DS1302_setDateTime(&date_dev, &dt);

	// Check write protect state
	if (DS1302_isWriteProtected(&date_dev)) {
		ESP_LOGE(pcTaskGetName(0), "Error: DS1302 write protected");
		while (1) { vTaskDelay(1); }
	}

	// Check write protect state
	if (DS1302_isHalted(&date_dev)) {
		ESP_LOGE(pcTaskGetName(0), "Error: DS1302 halted");
		while (1) { vTaskDelay(1); }
	}
	ESP_LOGI(pcTaskGetName(0), "Set initial date time done");
}

void clock_init(void *pvParameters){
	// Initialize RTC
	ESP_LOGI(pcTaskGetName(0), "Start");
	if (!DS1302_begin(&date_dev, CONFIG_CLK_GPIO, CONFIG_IO_GPIO, CONFIG_CE_GPIO)) {
		ESP_LOGE(pcTaskGetName(0), "Error: DS1302 begin");
		while (!DS1302_begin(&date_dev, CONFIG_CLK_GPIO, CONFIG_IO_GPIO, CONFIG_CE_GPIO)) { vTaskDelay(1); }
	}
}

void get_clock(DS1302_DateTime *dt)
{
	// Get RTC date and time
	if (!DS1302_getDateTime(&date_dev, dt)) {
		ESP_LOGE(pcTaskGetName(0), "Error: DS1302 read failed");
	} 
}