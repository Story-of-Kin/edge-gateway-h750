/*
 * 学习入口：系统上电后，启动文件完成基础准备，再进入 main()。
 * main() 的职责：初始化硬件 -> 恢复存储 -> 创建任务 -> 启动 FreeRTOS 调度。
 * 实际循环工作在 App/gateway_tasks.c 和 App/gateway_network.c 中。
 * 阅读提示：HAL 是 STM32 的硬件驱动接口；BSP 是针对本开发板的驱动。
 */
/* Polaris H750 gateway port, derived from zgd-dev/-V2.0. */
#include "gateway.h"
#include "gateway_board.h"
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include <stdio.h>
/* 程序入口。调度器正常启动后，不再沿 main() 向下执行。 */
int main(void)
{
    int test;
    /*
     * 第一步：开启 CPU 缓存并初始化 HAL、480MHz 系统时钟和延时基准。
     * 这些函数必须先在片内 Flash 执行，建立 QSPI 映射后才能调用外部 Flash 中的函数。
     */
    sys_cache_enable();
    HAL_Init();
    sys_stm32_clock_init(192, 5, 2, 4);
    delay_init(480);
    /*
     * 第二步：初始化板上 SDRAM、调试串口、RS485、Wi-Fi 串口等资源。
     * stage 是启动阶段标记，便于通过调试器判断程序走到了哪里。
     */
    g_gateway_diag.stage = 1;
    gateway_board_init();
    printf("\r\nH750 EDGE GATEWAY V2 PORT boot\r\n");
    g_gateway_diag.stage = 2;
    /*
     * 第三步：识别独立的数据 Flash，检查专用扇区，再扫描恢复日志和待发送缓存。
     * 只有两个存储区均成功打开，才置 storage_ready=1；失败仍可继续启动界面。
     */
    if (!gateway_flash_init())
    {
        /* 自检返回值转换为诊断状态：1=通过，2=未知数据存在而跳过，0=失败。 */
        test = gateway_flash_selftest();
        g_gateway_diag.flash_selftest = test == 0 ? 1 : test == 1 ? 2 : 0;
        if (!gw_store_open(&g_log_store, &gateway_flash, GW_LOG_BASE, GW_LOG_SIZE, 0) &&
            !gw_store_open(&g_cache_store, &gateway_flash, GW_CACHE_BASE, GW_CACHE_SIZE, 1))
            g_gateway_diag.storage_ready = 1;
    }
    printf("FLASH id=%06lX selftest=%lu ready=%lu\r\n", (unsigned long)g_gateway_diag.flash_id,
           (unsigned long)g_gateway_diag.flash_selftest,
           (unsigned long)g_gateway_diag.storage_ready);
    /*
     * 第四步：创建互斥锁和长度为 1 的队列。
     * 互斥锁保证多个任务不会同时操作同一 SPI Flash；队列保存未持久化的最新样本。
     */
    g_storage_mutex = xSemaphoreCreateMutex();
    g_sample_queue = xQueueCreate(1, sizeof(gw_sample_t));
    if (!g_storage_mutex || !g_sample_queue)
        gateway_panic(0xa010);
    gateway_log("Gateway boot; no synthetic sensor data");
    g_gateway_diag.stage = 3;
    /*
     * 第五步：创建四个任务。xTaskCreate 的参数依次是：
     * 任务函数、任务名、栈深度、传入参数、任务优先级、任务句柄输出指针。
     * 此 M7 端口的栈单位为 32 位字，2048 个字约为 8KB；任务优先级数字越大越高。
     * 各任务会主动延时/等待，让其他任务获得 CPU。
     */
    if (xTaskCreate(gateway_ui_task, "ui", 2048, 0, 3, 0) != pdPASS ||
        xTaskCreate(gateway_sensor_task, "sensor", 1536, 0, 2, 0) != pdPASS ||
        xTaskCreate(gateway_network_task, "network", 2048, 0, 2, 0) != pdPASS ||
        xTaskCreate(gateway_monitor_task, "monitor", 1024, 0, 4, 0) != pdPASS)
        gateway_panic(0xa011);
    /* 第六步：启动调度器。若启动失败而返回，则进入致命错误处理。 */
    g_gateway_diag.stage = 4;
    vTaskStartScheduler();
    gateway_panic(0xa012);
    return 0;
}
