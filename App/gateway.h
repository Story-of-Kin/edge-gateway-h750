/*
 * 应用层公共接口：集中定义任务之间共享的数据、诊断信息和函数声明。
 * 本文件只声明接口；函数的具体执行过程在对应 .c 文件中。
 */
#ifndef GATEWAY_H
#define GATEWAY_H
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "gateway_store.h"
#include "gateway_config.example.h"
/*
 * 一份采集快照。temperature/humidity/light 分别是摄氏度、相对湿度百分比、Lux。
 * timestamp 是本次采集时的开机毫秒数，不是日期时间；valid=0 表示本次没有有效读数。
 */
typedef struct
{
    float temperature, humidity, light;
    uint32_t timestamp;
    uint8_t temperature_valid, light_valid;
} gw_sample_t;
/*
 * 运行诊断区：可通过 SWD 读取，帮助定位启动、显示、通信和任务问题。
 * magic=0x48373530 用于识别固件；stage=5 表示 UI 初始化完成。
 * flash_selftest：0失败/1通过/2跳过；network_state：0未配置/1连接或离线/2建TCP/3已联网。
 * ui_frames 是刷屏回调次数，touch_events 是有效按下采样次数（均不是完整帧数/点击次数）。
 * heartbeat_* 为任务最近活动的开机毫秒数；fatal_code 非零表示已进入致命错误处理。
 */
typedef struct
{
    uint32_t magic, stage, uptime_ms, lcd_id, lcd_width, lcd_height;
    uint32_t flash_id, flash_selftest, storage_ready, ui_frames, touch_events;
    uint32_t sensor_polls, temperature_valid, light_valid, network_state, rx_overflows;
    uint32_t heartbeat_ui, heartbeat_sensor, heartbeat_network, free_heap, fatal_code;
    uint32_t touch_x, touch_y, cache_pending;
} gw_diag_t;
/*
 * extern 表示变量在别的 .c 文件中定义；volatile 要求编译器按实际访问读取诊断数据。
 * volatile 本身不提供互斥或一致快照；样本共享另外使用临界区保护。
 */
extern volatile gw_diag_t g_gateway_diag;
extern gw_sample_t g_gateway_sample;
/* FreeRTOS 队列句柄：只用于 Flash 未成功保存的样本，容量为 1，写入覆盖旧等待样本。 */
extern QueueHandle_t g_sample_queue;
/* 存储互斥锁句柄：任务读写日志、缓存之前先申请，操作完毕必须释放。 */
extern SemaphoreHandle_t g_storage_mutex;
/* 同一套记录存储接口对应两个分区：循环日志和待发送缓存。 */
extern gw_store_t g_log_store, g_cache_store;
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
