/*
 * 网关任务与共享数据。建议在阅读 main.c 后阅读本文件。
 * UI 任务负责显示/触摸；sensor 任务采集；network 任务在 gateway_network.c 中；monitor 任务检查健康。
 * 任务函数通常采用 for(;;) 永久循环，并用 vTaskDelay 或阻塞等待主动让出 CPU。
 */
#include "gateway.h"
#include "gateway_board.h"
#include "gateway_protocol.h"
#include "lvgl.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include <stdio.h>
#include <string.h>
volatile gw_diag_t g_gateway_diag = {0x48373530u};
gw_sample_t g_gateway_sample;
QueueHandle_t g_sample_queue;
SemaphoreHandle_t g_storage_mutex;
gw_store_t g_log_store, g_cache_store;
/*
 * 统一记录日志：先输出到 UART1，存储可用时再追加到日志分区。
 * text 必须是有效字符串；当前接口不向调用者返回日志写入失败信息。
 */
void gateway_log(const char *text)
{
    /* 启动调度前也会写启动日志；那时尚无任务竞争，不使用阻塞式互斥锁。 */
    int lock = xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
    printf("[GW %lu] %s\r\n", (unsigned long)HAL_GetTick(), text);
    if (!g_gateway_diag.storage_ready)
        return;
    if (lock && xSemaphoreTake(g_storage_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return;
    gw_store_append(&g_log_store, text, strlen(text), HAL_GetTick());
    if (lock)
        xSemaphoreGive(g_storage_mutex);
}
/*
 * 记录致命错误码并关闭中断停机，便于调试器定位。
 * 若独立看门狗已启动，后续不再喂狗，最终会触发复位。
 */
void gateway_panic(uint32_t code)
{
    g_gateway_diag.fatal_code = code;
    __disable_irq();
    for (;;)
    {
        __NOP();
    }
}
/* FreeRTOS 堆分配失败钩子。通常应检查堆大小、任务栈和资源泄漏。 */
void vApplicationMallocFailedHook(void)
{
    gateway_panic(0xa002);
}
/* FreeRTOS 栈溢出检查钩子；触发时记录错误，不继续执行可能已损坏的任务。 */
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    gateway_panic(0xa003);
}
/*
 * 界面任务：初始化 LCD/触摸/LVGL，并循环处理显示、按钮、息屏和唤醒。
 * 本工程仅由这个任务调用 LVGL，避免多任务同时操作界面对象。
 */
void gateway_ui_task(void *arg)
{
    uint32_t update = 0, activity = HAL_GetTick(), last_touch = 0;
    int lit = 1;
    (void)arg;
    /* 先初始化面板与触摸，再检查实际逻辑分辨率，防止用错误尺寸写帧缓冲。 */
    g_gateway_diag.heartbeat_ui = HAL_GetTick();
    lcd_init();
    lcd_display_dir(1);
    tp_dev.init();
    g_gateway_diag.lcd_id = lcddev.id;
    g_gateway_diag.lcd_width = lcddev.width;
    g_gateway_diag.lcd_height = lcddev.height;
    if (lcddev.width != 800 || lcddev.height != 480)
        gateway_panic(0xd001);
    lv_init();
    lv_port_h750_init();
    gateway_ui_create();
    g_gateway_diag.stage = 5;
    gateway_log("RGB UI ready: 800x480; touch initialized");
    /* LVGL 定时处理会执行触摸读取、事件回调、动画和需要的局部刷新。 */
    for (;;)
    {
        g_gateway_diag.heartbeat_ui = HAL_GetTick();
        lv_timer_handler();
        /* 以有效触摸活动重置空闲计时；按住屏幕期间也会不断更新。 */
        if (g_gateway_diag.touch_events != last_touch)
        {
            activity = HAL_GetTick();
            last_touch = g_gateway_diag.touch_events;
            if (!lit)
            {
                gateway_backlight(1);
                lit = 1;
            }
        }
        /* 界面数值每 500ms 更新一次；任务本身每轮休眠 5ms。 */
        if (HAL_GetTick() - update >= 500)
        {
            gateway_ui_update();
            update = HAL_GetTick();
        }
        if (lit && HAL_GetTick() - activity >= GW_SCREEN_IDLE_MS)
        {
            gateway_backlight(0);
            lit = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
/*
 * 采集任务：依次询问两个 RS485 从站，转换数值，更新共享快照并缓存有效样本。
 * 当前板级读取接口固定使用 03 功能码、读取两个连续寄存器。
 */
void gateway_sensor_task(void *arg)
{
    gw_sample_t sample;
    uint16_t v[2];
    uint8_t old_temp = 0, old_light = 0;
    (void)arg;
    for (;;)
    {
        memset(&sample, 0, sizeof(sample));
        g_gateway_diag.heartbeat_sensor = HAL_GetTick();
        /*
         * 函数返回 0 表示收到合法报文；按配置选择温湿度寄存器、符号和倍率。
         * 例如原始温度值 253，乘 0.1 后为 25.3 摄氏度。
         */
        if (!gateway_modbus_read(GW_TEMP_ADDRESS, GW_TEMP_REGISTER, v))
        {
            sample.temperature =
                gw_sensor_register(v[GW_TEMP_INDEX], GW_TEMP_SWAP_BYTES, 1, GW_TEMP_SCALE);
            sample.humidity =
                gw_sensor_register(v[GW_HUMI_INDEX], GW_TEMP_SWAP_BYTES, 0, GW_HUMI_SCALE);
            sample.temperature_valid = 1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        /* 两个 16 位寄存器拼成 32 位光照值；高低字顺序和倍率由配置决定。 */
        if (!gateway_modbus_read(GW_LIGHT_ADDRESS, GW_LIGHT_REGISTER, v))
        {
            sample.light =
                gw_sensor_light(v, GW_LIGHT_LOW_WORD_FIRST, GW_LIGHT_SWAP_BYTES, GW_LIGHT_SCALE);
            sample.light_valid = 1;
        }
        /*
         * 短临界区内复制整个样本，防止 UI 读到只更新了一半的结构体。
         * 临界区内只做快速内存操作，不在里面等待串口或擦写 Flash。
         */
        sample.timestamp = HAL_GetTick();
        taskENTER_CRITICAL();
        g_gateway_sample = sample;
        taskEXIT_CRITICAL();
        g_gateway_diag.temperature_valid = sample.temperature_valid;
        g_gateway_diag.light_valid = sample.light_valid;
        g_gateway_diag.sensor_polls++;
        if (sample.temperature_valid != old_temp)
        {
            gateway_log(sample.temperature_valid ? "Temperature/humidity sensor connected"
                                                 : "Temperature/humidity sensor timeout");
            old_temp = sample.temperature_valid;
        }
        if (sample.light_valid != old_light)
        {
            gateway_log(sample.light_valid ? "Light sensor connected" : "Light sensor timeout");
            old_light = sample.light_valid;
        }
        /* 至少一种传感器读取成功才生成上报数据；未读取到的字段保留为 JSON null。 */
        if (sample.temperature_valid || sample.light_valid)
        {
            /* 先把样本格式化为 JSON。shidu 是为兼容原项目保留的湿度字段名。 */
            char temp[24] = "null", humi[24] = "null", light[24] = "null", json[160];
            int length, durable = 0;
            if (sample.temperature_valid)
            {
                snprintf(temp, sizeof(temp), "%.1f", sample.temperature);
                snprintf(humi, sizeof(humi), "%.1f", sample.humidity);
            }
            if (sample.light_valid)
                snprintf(light, sizeof(light), "%.0f", sample.light);
            length = snprintf(json, sizeof(json), "{\"temperature\":%s,\"shidu\":%s,\"light\":%s}",
                              temp, humi, light);
            /* 优先持久保存到缓存分区。durable=1 表示已通过记录存储的提交/读回步骤。 */
            if (g_gateway_diag.storage_ready && length > 0 && length < (int)sizeof(json) &&
                xSemaphoreTake(g_storage_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
            {
                if (gw_store_append(&g_cache_store, json, (size_t)length, sample.timestamp) < 0)
                    printf("[GW] cache full or write failed; using volatile live queue\r\n");
                else
                    durable = 1;
                g_gateway_diag.cache_pending = g_cache_store.pending;
                xSemaphoreGive(g_storage_mutex);
            }
            /*
             * 只有没存成功的样本进入内存降级队列，避免与 Flash 补传重复入队。
             * 此队列会覆盖旧等待样本，且断电不保存。
             */
            if (!durable)
                xQueueOverwrite(g_sample_queue, &sample);
        }
        g_gateway_diag.heartbeat_sensor = HAL_GetTick();
        vTaskDelay(pdMS_TO_TICKS(GW_SENSOR_PERIOD_MS));
    }
}
/*
 * 健康监控任务：更新运行时间和空闲堆，仅当三个业务任务最近均有活动才喂狗。
 * 心跳正常仅表明任务仍运行，不等于传感器有数据或云端已收到数据。
 */
void gateway_monitor_task(void *arg)
{
    uint32_t report = 0, now;
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1000));
    gateway_watchdog_start();
    for (;;)
    {
        now = HAL_GetTick();
        g_gateway_diag.uptime_ms = now;
        g_gateway_diag.free_heap = xPortGetFreeHeapSize();
        /* 三个业务任务的心跳都在 15 秒以内才喂狗；一个任务卡住就停止喂狗。 */
        if (now - g_gateway_diag.heartbeat_ui < 15000 &&
            now - g_gateway_diag.heartbeat_sensor < 15000 &&
            now - g_gateway_diag.heartbeat_network < 15000)
            gateway_watchdog_feed();
        /* 每 5 秒向调试串口输出一行诊断，传感器/网络缺失不会被当成致命错误。 */
        if (now - report >= 5000)
        {
            printf(
                "GW_DIAG stage=%lu uptime=%lu lcd=%04lX %lux%lu flash=%06lX test=%lu frames=%lu touch=%lu polls=%lu net=%lu heap=%lu fatal=%lu\r\n",
                (unsigned long)g_gateway_diag.stage, (unsigned long)now,
                (unsigned long)g_gateway_diag.lcd_id, (unsigned long)g_gateway_diag.lcd_width,
                (unsigned long)g_gateway_diag.lcd_height, (unsigned long)g_gateway_diag.flash_id,
                (unsigned long)g_gateway_diag.flash_selftest,
                (unsigned long)g_gateway_diag.ui_frames, (unsigned long)g_gateway_diag.touch_events,
                (unsigned long)g_gateway_diag.sensor_polls,
                (unsigned long)g_gateway_diag.network_state,
                (unsigned long)g_gateway_diag.free_heap, (unsigned long)g_gateway_diag.fatal_code);
            report = now;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
