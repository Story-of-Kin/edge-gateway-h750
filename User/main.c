/* Polaris H750 gateway port, derived from zgd-dev/-V2.0. */
#include "gateway.h"
#include "gateway_board.h"
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include <stdio.h>
int main(void)
{
    int test;
    /* These must execute from internal Flash before any QSPI function. */
    sys_cache_enable();HAL_Init();sys_stm32_clock_init(192,5,2,4);delay_init(480);
    g_gateway_diag.stage=1;gateway_board_init();
    printf("\r\nH750 EDGE GATEWAY V2 PORT boot\r\n");
    g_gateway_diag.stage=2;
    if(!gateway_flash_init()){
        test=gateway_flash_selftest();g_gateway_diag.flash_selftest=test==0?1:test==1?2:0;
        if(!gw_store_open(&g_log_store,&gateway_flash,GW_LOG_BASE,GW_LOG_SIZE,0)&&!gw_store_open(&g_cache_store,&gateway_flash,GW_CACHE_BASE,GW_CACHE_SIZE,1))g_gateway_diag.storage_ready=1;
    }
    printf("FLASH id=%06lX selftest=%lu ready=%lu\r\n",(unsigned long)g_gateway_diag.flash_id,(unsigned long)g_gateway_diag.flash_selftest,(unsigned long)g_gateway_diag.storage_ready);
    g_storage_mutex=xSemaphoreCreateMutex();g_sample_queue=xQueueCreate(1,sizeof(gw_sample_t));
    if(!g_storage_mutex||!g_sample_queue)gateway_panic(0xa010);
    gateway_log("Gateway boot; no synthetic sensor data");g_gateway_diag.stage=3;
    if(xTaskCreate(gateway_ui_task,"ui",2048,0,3,0)!=pdPASS||xTaskCreate(gateway_sensor_task,"sensor",1536,0,2,0)!=pdPASS||xTaskCreate(gateway_network_task,"network",2048,0,2,0)!=pdPASS||xTaskCreate(gateway_monitor_task,"monitor",1024,0,4,0)!=pdPASS)gateway_panic(0xa011);
    g_gateway_diag.stage=4;vTaskStartScheduler();gateway_panic(0xa012);return 0;
}
