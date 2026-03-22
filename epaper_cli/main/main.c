#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lvgl.h"
#include "epaper_lvgl.h"
#include "epaper.h"
#include "esp_netif_sntp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "cJSON.h"



// System
static const char *TAG = "HELLO";

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

// WeAct ESP32-C6 mini pins
#define PIN_SCL   GPIO_NUM_18
#define PIN_SDA   GPIO_NUM_19
#define PIN_CS    GPIO_NUM_20
#define PIN_DC    GPIO_NUM_21
#define PIN_RST   GPIO_NUM_22
#define PIN_BUSY  GPIO_NUM_23

static void lv_tick_cb(void *arg) {
    (void)arg;
    lv_tick_inc(10);
}

void init_lvgl_tick(void) {
    const esp_timer_create_args_t args = {
        .callback = &lv_tick_cb,
        .name = "lv_tick"
    };
    esp_timer_handle_t t; 
	ESP_ERROR_CHECK(esp_timer_create(&args, &t));
	ESP_ERROR_CHECK(esp_timer_start_periodic(t, 10 * 1000)); // 10ms в микросекундах
}

// Время
#define WAKE_PERIOD_US      (50ULL * 1000000ULL)        // 60 секунд

RTC_DATA_ATTR static time_t rtc_epoch_at_sleep = 0;     // epoch перед сном
RTC_DATA_ATTR static int rtc_last_yday = -1;            // день года последней синхры
RTC_DATA_ATTR static bool rtc_time_valid = false;       // было ли время хоть раз синхронизировано

static time_t approx_now_after_wake(uint64_t slept_us) {
    if (!rtc_time_valid) return 0;
    return rtc_epoch_at_sleep + (time_t)(slept_us / 1000000ULL);
}

// static bool need_daily_sync(time_t now_est) {
//     if (!rtc_time_valid) return true;                   // первый раз всегда синхронизируем

//     struct tm tm;
//     localtime_r(&now_est, &tm);
//     if (rtc_last_yday == -1) return true;

//     return (tm.tm_yday != rtc_last_yday);               // если день сменился — синхронизируем
// }

static bool need_test_sync(time_t now_est) {
    if (!rtc_time_valid) return true;

    struct tm tm;
    localtime_r(&now_est, &tm);
    if (rtc_last_yday == -1) return true;

    return (tm.tm_min%5 == 0);
}


static esp_err_t obtain_time_once(void) {
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);

    time_t now = 0;
    struct tm ti = {0};

    for (int i = 0; i < 20; i++) {
        time(&now);
        localtime_r(&now, &ti);
        if (ti.tm_year > (2016 - 1900)) {
            esp_netif_sntp_deinit();
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    esp_netif_sntp_deinit();
    return ESP_FAIL;
}

//wifi
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASS
#define API_URL "http://192.168.1.115:8080/school/schedule/1"

static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static const int WIFI_FAIL_BIT      = BIT1;
static int s_retry_num = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
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

static esp_err_t wifi_connect_sta(void) {
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

static void wifi_disconnect_sta(void) {
    esp_wifi_stop();
    esp_wifi_deinit();
}


//json
#define HTTP_RX_MAX 2048

static esp_err_t http_get_to_buffer(const char *url, char *out, size_t out_sz) {
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

    return (total > 0) ? ESP_OK : ESP_FAIL;
}

static bool json_value_by_key(const char *json, const char *key, char *out_value, size_t out_sz) {
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


static bool json_lessons_to_text(const char *json, const char *key, char *out, size_t out_sz) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsArray(arr)) {
        cJSON_Delete(root);
        return false;
    }

    out[0] = '\0';
    size_t used = 0;

    int n = cJSON_GetArraySize(arr);
    for (int i = 0; i < n; i++) {
        cJSON *it = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsString(it) || !it->valuestring) continue;

        // "1) Matematika\n"
        int written = snprintf(out + used, out_sz - used, "%d. %s\n", i + 1, it->valuestring);
        if (written < 0 || (size_t)written >= out_sz - used) break;
        used += (size_t)written;
    }

    cJSON_Delete(root);
    return (used > 0);
}

// *** *** *** *** ***
void app_main(void) {

	vTaskDelay(pdMS_TO_TICKS(1000));			// Окно на включение монитора
	esp_rom_printf("\nROM: boottt\n");
	esp_reset_reason_t rr = esp_reset_reason();
	esp_sleep_wakeup_cause_t wc = esp_sleep_get_wakeup_cause();
	printf("Reset reason: %s (%d)\n", reset_reason_str(rr), (int)rr);
	printf("Wakeup cause: %d\n", (int)wc);
	fflush(stdout);
	vTaskDelay(pdMS_TO_TICKS(1000));			// Окно, чтобы вы успели увидеть вывод

	lv_init();

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

    epd_handle_t epd = NULL;
    ESP_ERROR_CHECK(epd_init(&cfg, &epd));


    // Initialize LVGL display with partial refresh and dithering
    epd_lvgl_config_t lvgl_cfg = EPD_LVGL_CONFIG_DEFAULT();
    lvgl_cfg.epd = epd;
    lvgl_cfg.update_mode = EPD_UPDATE_PARTIAL;
    lvgl_cfg.use_partial_refresh = true;
    lvgl_cfg.partial_threshold = 2000;  // Force full refresh every N partial updates
    //lvgl_cfg.dither_mode = EPD_DITHER_FLOYD_STEINBERG;  // Enable grayscale dithering

    lv_display_t *disp = epd_lvgl_init(&lvgl_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "Failed to init LVGL display");
        epd_deinit(epd);
        return;
    }

    // Get panel info
    epd_panel_info_t panel_info;
    epd_get_info(epd, &panel_info);
    ESP_LOGI(TAG, "Panel: %dx%d, buffer: %lu bytes", 
             panel_info.width, panel_info.height, panel_info.buffer_size);

    init_lvgl_tick();
    lv_obj_t *scr = lv_screen_active();
    
    static lv_obj_t *json_label = NULL;
    json_label = lv_label_create(scr);
    lv_label_set_long_mode(json_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(json_label, 296);
    lv_obj_align(json_label, LV_ALIGN_TOP_LEFT, 0, 5);
    
    static lv_obj_t *sleep_label = NULL;
    sleep_label = lv_label_create(scr);
    lv_obj_align(sleep_label, LV_ALIGN_BOTTOM_LEFT, 0, -5);
    lv_label_set_text(sleep_label, "");
    //lv_area_t area = {0,256,128,296};

    char http_buf[HTTP_RX_MAX];
    char day[32];
    char lessons_txt[256];
    bool got_day = false;
    bool got_lessons = false;
	
    setenv("TZ", "UTC-3", 1);
    tzset();
    uint64_t slept_us = WAKE_PERIOD_US;         // если фиксированно
    time_t now_est = approx_now_after_wake(slept_us);
    struct tm tm;

    if (need_test_sync(now_est)) {
        if (wifi_connect_sta() == ESP_OK) {
            printf("wifi connectedd + 1.0sec pause\n");
            fflush(stdout);

            if (obtain_time_once() == ESP_OK) {
                time_t now;
                time(&now);
                localtime_r(&now, &tm);

                rtc_last_yday = tm.tm_yday;
                rtc_time_valid = true;
            }
            if (http_get_to_buffer(API_URL, http_buf, sizeof(http_buf)) == ESP_OK) {
                ESP_LOGI(TAG, "api read");
                vTaskDelay(pdMS_TO_TICKS(500));
                got_day = json_value_by_key(http_buf, "dayName", day, sizeof(day));
                got_lessons = json_lessons_to_text(http_buf, "lessons", lessons_txt, sizeof(lessons_txt));
            }
            wifi_disconnect_sta();
        } else {
            ESP_LOGW(TAG, "Wi-Fi connect failed");
        }

        // Рисуем
        if (got_day) {
            if (got_lessons) {
                static char text[400];
                localtime_r(&now_est, &tm);
                snprintf(text, sizeof(text), "%s\n%s\n%d", day, lessons_txt, tm.tm_min);
                lv_label_set_text(json_label, text);
            } else {
                lv_label_set_text_fmt(json_label, "%s\n(no lessons)", day);
            }
        } else {
            lv_label_set_text(json_label, "API error");
        }
        //epd_lvgl_force_full_refresh(disp);          // Force full refresh
        //ESP_LOGI(TAG, "EPD full refresh");
    }
	
    lv_label_set_text(sleep_label, "Going sleep...");
    lv_refr_now(disp);
    epd_lvgl_refresh(disp);

	
    printf("Going to deep sleep. Next pos\n");
	fflush(stdout);
	vTaskDelay(pdMS_TO_TICKS(100));
	
	// Deep sleep
    epd_sleep(epd);                     // Перевести дисплей в сон (полезно для low-power)
	vTaskDelay(pdMS_TO_TICKS(20));
    time_t before_sleep;
    time(&before_sleep);
    rtc_epoch_at_sleep = before_sleep;
    esp_sleep_enable_timer_wakeup(WAKE_PERIOD_US);
    esp_deep_sleep_start();
}