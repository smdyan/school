#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "lvgl.h"
#include "epaper.h"
#include "epaper_lvgl.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "cJSON.h"


static const char *TAG = "HELLO";

#define WAKE_PERIOD_US      (60ULL * 1000000ULL)   // 60 секунд
#define DEBUG_AWAKE_MS      2000                   // 2 секунды остаёмся бодрствовать для отладки

static const char* reset_reason_str(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_EXT:       return "EXT (EN/RST)";
        case ESP_RST_SW:        return "SW";
        case ESP_RST_PANIC:     return "PANIC/ABORT";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        default:                return "OTHER";
    }
}

// WeAct ESP32-C6 mini pins (как в вашем Arduino примере)
#define PIN_SCL   GPIO_NUM_18
#define PIN_SDA   GPIO_NUM_19
#define PIN_CS    GPIO_NUM_20
#define PIN_DC    GPIO_NUM_21
#define PIN_RST   GPIO_NUM_22
#define PIN_BUSY  GPIO_NUM_23

// LVGL tick (обязателен для LVGL)

static void lv_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(10);
}

void init_lvgl_tick(void)
{
    const esp_timer_create_args_t args = {
        .callback = &lv_tick_cb,
        .name = "lv_tick"
    };
    esp_timer_handle_t t; 
	ESP_ERROR_CHECK(esp_timer_create(&args, &t));
	ESP_ERROR_CHECK(esp_timer_start_periodic(t, 10 * 1000)); // 10ms в микросекундах
}

// Сохраняется между deep sleep
RTC_DATA_ATTR static uint8_t pos_toggle = 0;
RTC_DATA_ATTR static int32_t rtc_minutes = 0;  // минуты от 00:00 (0..1439)

// формат HH:MM
static void format_hhmm(char *out, size_t out_sz, int32_t minutes)
{
    minutes %= (24 * 60);
    if (minutes < 0) minutes += 24 * 60;
    int hh = minutes / 60;
    int mm = minutes % 60;
    snprintf(out, out_sz, "%02d:%02d", hh, mm);
}


#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASS

static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static const int WIFI_FAIL_BIT      = BIT1;
static int s_retry_num = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            s_retry_num++;
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_connect_sta(void)
{
    // NVS нужен Wi-Fi стеку
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = { 0 };
    snprintf((char*)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", WIFI_SSID);
    snprintf((char*)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", WIFI_PASS);

    // Для WPA2/WPA3 mixed обычно ок:
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdTRUE, pdFALSE,
        pdMS_TO_TICKS(15000)  // 15 сек таймаут
    );

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

static void wifi_disconnect_sta(void)
{
    // аккуратно гасим Wi-Fi перед sleep
    esp_wifi_stop();
    esp_wifi_deinit();
}


#define API_URL "http://192.168.1.115:8080/school/schedule/1"
#define HTTP_RX_MAX 2048

static esp_err_t http_get_to_buffer(const char *url, char *out, size_t out_sz)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 8000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_FAIL;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        printf("http open err=%s\n", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int64_t cl = esp_http_client_fetch_headers(client); // <- выполняет запрос/читает заголовки
    int status = esp_http_client_get_status_code(client);
    printf("HTTP status=%d, content_length=%lld\n", status, (long long)cl);

    // Читать тело можно даже при 404, если вы хотите распарсить {"detail":"Not Found"}
    int total = 0;
    while (1) {
        int r = esp_http_client_read(client, out + total, (int)out_sz - 1 - total);
        if (r < 0) {
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        if (r == 0) break;
        total += r;
        if (total >= (int)out_sz - 1) break;
    }
    out[total] = 0;

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    // Вернём OK если хоть что-то прочитали
    return (total > 0) ? ESP_OK : ESP_FAIL;
}

// Получить значение "первого поля" JSON-объекта как строку в out_value
static bool json_value_by_key(const char *json, const char *key,
                              char *out_value, size_t out_sz)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;

    // Ищем ключ в объекте верхнего уровня
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!item) {
        cJSON_Delete(root);
        return false;
    }

    if (cJSON_IsString(item) && item->valuestring) {
        snprintf(out_value, out_sz, "%s", item->valuestring);
    } else if (cJSON_IsNumber(item)) {
        // если нужно без дробной части:
        // snprintf(out_value, out_sz, "%.0f", item->valuedouble);
        snprintf(out_value, out_sz, "%g", item->valuedouble);
    } else if (cJSON_IsBool(item)) {
        snprintf(out_value, out_sz, "%s", cJSON_IsTrue(item) ? "true" : "false");
    } else {
        // объект/массив/null — печатаем компактно в JSON
        char *printed = cJSON_PrintUnformatted(item);
        if (!printed) {
            cJSON_Delete(root);
            return false;
        }
        snprintf(out_value, out_sz, "%s", printed);
        cJSON_free(printed);
    }

    cJSON_Delete(root);
    return true;
}


void app_main(void)
{
	vTaskDelay(pdMS_TO_TICKS(1000));			// Окно на включение монитора
	esp_rom_printf("\nROM: boottt\n");
	esp_reset_reason_t rr = esp_reset_reason();
	esp_sleep_wakeup_cause_t wc = esp_sleep_get_wakeup_cause();

	printf("Reset reason: %s (%d)\n", reset_reason_str(rr), (int)rr);
	printf("Wakeup cause: %d\n", (int)wc);
	fflush(stdout);
	vTaskDelay(pdMS_TO_TICKS(1000));			// Окно, чтобы вы успели увидеть вывод
		
	esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
	
	if (cause == ESP_SLEEP_WAKEUP_TIMER) {
	    rtc_minutes += (int32_t)(WAKE_PERIOD_US / 1000000ULL) / 60;
	} else {
	    rtc_minutes = 0;
	    pos_toggle = 0;
	}
	pos_toggle ^= 1;
	
	char http_buf[HTTP_RX_MAX];
	char value[128];
	bool got = false;
	
    if (wifi_connect_sta() == ESP_OK) {
        printf("wifi connectedd + 1.0sec pause\n");
        fflush(stdout);
        if (http_get_to_buffer(API_URL, http_buf, sizeof(http_buf)) == ESP_OK) {
            ESP_LOGI(TAG, "Wi-Fi connected");
            vTaskDelay(pdMS_TO_TICKS(1000));
            got = json_value_by_key(http_buf, "dayName", value, sizeof(value));
        }
    } else {
        ESP_LOGW(TAG, "Wi-Fi connect failed");
    }
    wifi_disconnect_sta();
	
	
	lv_init();
	init_lvgl_tick();

    epd_config_t cfg = {
        .pins = {
            .busy = PIN_BUSY,
            .rst  = PIN_RST,
            .dc   = PIN_DC,
            .cs   = PIN_CS,
            .sck  = PIN_SCL,
            .mosi = PIN_SDA,
        },
        .spi = {
            .host = SPI2_HOST,
            .speed_hz = 4 * 1000 * 1000,   // начните с 4 МГц (потом можно 8)
        },
        .panel = {
            // 2.9" 128x296 (SSD1680/SSD16xx family)
            .type = EPD_PANEL_SSD16XX_290,
            .width = 0,
            .height = 0,
        },
    };

    epd_handle_t epd;
    ESP_ERROR_CHECK(epd_init(&cfg, &epd));

    epd_lvgl_config_t lvgl_cfg = EPD_LVGL_CONFIG_DEFAULT();
    lvgl_cfg.epd = epd;
    lvgl_cfg.update_mode = EPD_UPDATE_FULL;

    lv_display_t *disp = epd_lvgl_init(&lvgl_cfg);

    // Рисуем
    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *json_label = lv_label_create(scr);
	if (got) {
	    lv_label_set_text(json_label, value);
	} else {
	    lv_label_set_text(json_label, "API error");
	}
    lv_obj_align(json_label, LV_ALIGN_TOP_LEFT, 0, +5);

	char time_str[6];
	format_hhmm(time_str, sizeof(time_str), rtc_minutes);
	
	int y_offset = pos_toggle ? -25 : +25;

	lv_obj_t *time_label = lv_label_create(scr);
	lv_label_set_text(time_label, time_str);
	lv_obj_align(time_label, LV_ALIGN_CENTER, 0, y_offset);
	
    // Отрисовать и выполнить физический refresh
    lv_refr_now(disp);
    epd_lvgl_refresh(disp);
    ESP_LOGI(TAG, "EPD refreshed");

    // Перевести дисплей в сон (полезно для low-power)
    epd_sleep(epd);
	vTaskDelay(pdMS_TO_TICKS(20));

	// Включаем пробуждение по таймеру
	esp_sleep_enable_timer_wakeup(WAKE_PERIOD_US);
	
	printf("Going to deep sleep. Next pos=%d\n", pos_toggle);
	fflush(stdout);
	vTaskDelay(pdMS_TO_TICKS(100)); // дать логу уйти в порт
	
	// Deep sleep (после пробуждения будет полный рестарт)
	esp_deep_sleep_start();
	// while(1){
	// 	vTaskDelay(pdMS_TO_TICKS(1000));
	}
