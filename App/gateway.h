#ifndef GATEWAY_H
#define GATEWAY_H
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "gateway_store.h"
#include "gateway_config.example.h"
typedef struct {
    float temperature,humidity,light;
    uint32_t timestamp;
    uint8_t temperature_valid,light_valid;
} gw_sample_t;
typedef struct {
    uint32_t magic,stage,uptime_ms,lcd_id,lcd_width,lcd_height;
    uint32_t flash_id,flash_selftest,storage_ready,ui_frames,touch_events;
    uint32_t sensor_polls,temperature_valid,light_valid,network_state,rx_overflows;
    uint32_t heartbeat_ui,heartbeat_sensor,heartbeat_network,free_heap,fatal_code;
    uint32_t touch_x,touch_y,cache_pending;
} gw_diag_t;
extern volatile gw_diag_t g_gateway_diag;
extern gw_sample_t g_gateway_sample;
extern QueueHandle_t g_sample_queue;
extern SemaphoreHandle_t g_storage_mutex;
extern gw_store_t g_log_store,g_cache_store;
void gateway_panic(uint32_t code);
void gateway_log(const char *text);
void gateway_ui_task(void *arg);
void gateway_sensor_task(void *arg);
void gateway_network_task(void *arg);
void gateway_monitor_task(void *arg);
void gateway_ui_create(void);
void gateway_ui_update(void);
void lv_port_h750_init(void);
#endif
