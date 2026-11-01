#include "gateway.h"
#include "gateway_board.h"
#include "gateway_protocol.h"
#include "lvgl.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include <stdio.h>
#include <string.h>
volatile gw_diag_t g_gateway_diag={0x48373530u};
gw_sample_t g_gateway_sample;
QueueHandle_t g_sample_queue;
SemaphoreHandle_t g_storage_mutex;
gw_store_t g_log_store,g_cache_store;
void gateway_log(const char*text)
{
    int lock=xTaskGetSchedulerState()==taskSCHEDULER_RUNNING;
    printf("[GW %lu] %s\r\n",(unsigned long)HAL_GetTick(),text);
    if(!g_gateway_diag.storage_ready)return;
    if(lock&&xSemaphoreTake(g_storage_mutex,pdMS_TO_TICKS(1000))!=pdTRUE)return;
    gw_store_append(&g_log_store,text,strlen(text),HAL_GetTick());
    if(lock)xSemaphoreGive(g_storage_mutex);
}
void gateway_panic(uint32_t code){g_gateway_diag.fatal_code=code;__disable_irq();for(;;){__NOP();}}
void vApplicationMallocFailedHook(void){gateway_panic(0xa002);}
void vApplicationStackOverflowHook(TaskHandle_t task,char*name){(void)task;(void)name;gateway_panic(0xa003);}
void gateway_ui_task(void*arg)
{
    uint32_t update=0,activity=HAL_GetTick(),last_touch=0;int lit=1;(void)arg;
    g_gateway_diag.heartbeat_ui=HAL_GetTick();lcd_init();lcd_display_dir(1);tp_dev.init();
    g_gateway_diag.lcd_id=lcddev.id;g_gateway_diag.lcd_width=lcddev.width;g_gateway_diag.lcd_height=lcddev.height;
    if(lcddev.width!=800||lcddev.height!=480)gateway_panic(0xd001);
    lv_init();lv_port_h750_init();gateway_ui_create();g_gateway_diag.stage=5;gateway_log("RGB UI ready: 800x480; touch initialized");
    for(;;){g_gateway_diag.heartbeat_ui=HAL_GetTick();lv_timer_handler();
        if(g_gateway_diag.touch_events!=last_touch){activity=HAL_GetTick();last_touch=g_gateway_diag.touch_events;if(!lit){gateway_backlight(1);lit=1;}}
        if(HAL_GetTick()-update>=500){gateway_ui_update();update=HAL_GetTick();}
        if(lit&&HAL_GetTick()-activity>=GW_SCREEN_IDLE_MS){gateway_backlight(0);lit=0;}
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
void gateway_sensor_task(void*arg)
{
    gw_sample_t sample;uint16_t v[2];uint8_t old_temp=0,old_light=0;(void)arg;
    for(;;){memset(&sample,0,sizeof(sample));g_gateway_diag.heartbeat_sensor=HAL_GetTick();
        if(!gateway_modbus_read(GW_TEMP_ADDRESS,GW_TEMP_REGISTER,v)){sample.temperature=gw_sensor_register(v[GW_TEMP_INDEX],GW_TEMP_SWAP_BYTES,1,GW_TEMP_SCALE);sample.humidity=gw_sensor_register(v[GW_HUMI_INDEX],GW_TEMP_SWAP_BYTES,0,GW_HUMI_SCALE);sample.temperature_valid=1;}
        vTaskDelay(pdMS_TO_TICKS(10));
        if(!gateway_modbus_read(GW_LIGHT_ADDRESS,GW_LIGHT_REGISTER,v)){sample.light=gw_sensor_light(v,GW_LIGHT_LOW_WORD_FIRST,GW_LIGHT_SWAP_BYTES,GW_LIGHT_SCALE);sample.light_valid=1;}
        sample.timestamp=HAL_GetTick();taskENTER_CRITICAL();g_gateway_sample=sample;taskEXIT_CRITICAL();
        g_gateway_diag.temperature_valid=sample.temperature_valid;g_gateway_diag.light_valid=sample.light_valid;g_gateway_diag.sensor_polls++;
        if(sample.temperature_valid!=old_temp){gateway_log(sample.temperature_valid?"Temperature/humidity sensor connected":"Temperature/humidity sensor timeout");old_temp=sample.temperature_valid;}
        if(sample.light_valid!=old_light){gateway_log(sample.light_valid?"Light sensor connected":"Light sensor timeout");old_light=sample.light_valid;}
        if(sample.temperature_valid||sample.light_valid){
            char temp[24]="null",humi[24]="null",light[24]="null",json[160];int length,durable=0;
            if(sample.temperature_valid){snprintf(temp,sizeof(temp),"%.1f",sample.temperature);snprintf(humi,sizeof(humi),"%.1f",sample.humidity);}
            if(sample.light_valid)snprintf(light,sizeof(light),"%.0f",sample.light);
            length=snprintf(json,sizeof(json),"{\"temperature\":%s,\"shidu\":%s,\"light\":%s}",temp,humi,light);
            if(g_gateway_diag.storage_ready&&length>0&&length<(int)sizeof(json)&&xSemaphoreTake(g_storage_mutex,pdMS_TO_TICKS(1000))==pdTRUE){
                if(gw_store_append(&g_cache_store,json,(size_t)length,sample.timestamp)<0)printf("[GW] cache full or write failed; using volatile live queue\r\n");else durable=1;
                g_gateway_diag.cache_pending=g_cache_store.pending;xSemaphoreGive(g_storage_mutex);
            }
            if(!durable)xQueueOverwrite(g_sample_queue,&sample);
        }
        g_gateway_diag.heartbeat_sensor=HAL_GetTick();vTaskDelay(pdMS_TO_TICKS(GW_SENSOR_PERIOD_MS));
    }
}
void gateway_monitor_task(void*arg)
{
    uint32_t report=0,now;(void)arg;vTaskDelay(pdMS_TO_TICKS(1000));gateway_watchdog_start();
    for(;;){now=HAL_GetTick();g_gateway_diag.uptime_ms=now;g_gateway_diag.free_heap=xPortGetFreeHeapSize();
        if(now-g_gateway_diag.heartbeat_ui<15000&&now-g_gateway_diag.heartbeat_sensor<15000&&now-g_gateway_diag.heartbeat_network<15000)gateway_watchdog_feed();
        if(now-report>=5000){printf("GW_DIAG stage=%lu uptime=%lu lcd=%04lX %lux%lu flash=%06lX test=%lu frames=%lu touch=%lu polls=%lu net=%lu heap=%lu fatal=%lu\r\n",(unsigned long)g_gateway_diag.stage,(unsigned long)now,(unsigned long)g_gateway_diag.lcd_id,(unsigned long)g_gateway_diag.lcd_width,(unsigned long)g_gateway_diag.lcd_height,(unsigned long)g_gateway_diag.flash_id,(unsigned long)g_gateway_diag.flash_selftest,(unsigned long)g_gateway_diag.ui_frames,(unsigned long)g_gateway_diag.touch_events,(unsigned long)g_gateway_diag.sensor_polls,(unsigned long)g_gateway_diag.network_state,(unsigned long)g_gateway_diag.free_heap,(unsigned long)g_gateway_diag.fatal_code);report=now;}
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
