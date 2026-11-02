/*
 * 网络降级测试：模拟UART和队列，让真实网络任务运行一轮，确认Flash不可用时仍发送样本。
 */
/* Run the production network task for one loop with an actual AT parser. */
#define GATEWAY_H
#define GATEWAY_BOARD_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "gateway_store.h"
typedef void* SemaphoreHandle_t;
typedef struct{float temperature,humidity,light;uint32_t timestamp;uint8_t temperature_valid,light_valid;}gw_sample_t;
typedef struct{uint32_t heartbeat_network,network_state,storage_ready,cache_pending;}test_diag_t;
static test_diag_t g_gateway_diag;
static gw_store_t g_cache_store;
static void*g_storage_mutex,*g_sample_queue;
#define GW_WIFI_SSID ""
#define GW_WIFI_PASSWORD ""
#define GW_MQTT_CLIENT_ID ""
#define GW_MQTT_HOST "bemfa.com"
#define GW_MQTT_PORT 9501
#define GW_MQTT_TOPIC "stm32"
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
static jmp_buf end_loop;static uint8_t incoming[64];static size_t available;static int sent,queued;
static uint32_t HAL_GetTick(void){return 100;}
static void vTaskDelay(uint32_t n){if(n==100)longjmp(end_loop,1);}
static int xSemaphoreTake(void*s,uint32_t n){(void)s;(void)n;return 1;}
static void xSemaphoreGive(void*s){(void)s;}
static int xQueueReceive(void*q,void*out,uint32_t n){gw_sample_t sample={21.5f,60.0f,1000.0f,100,1,1};(void)q;(void)n;if(!queued)return 0;memcpy(out,&sample,sizeof(sample));queued=0;return 1;}
static void gateway_log(const char*s){(void)s;}
static int gateway_wifi_send(const void*p,size_t n){const uint8_t*b=p;if(n>=10&&!memcmp(p,"AT+CIPSEND",10)){incoming[0]='>';available=1;}else{if(b[0]==0x30)sent++;memcpy(incoming,"\r\nSEND OK\r\n",11);available=11;}return 0;}
static int gateway_wifi_read(uint8_t*p,size_t n){if(n>available)n=available;memcpy(p,incoming,n);available-=n;return (int)n;}
static void gateway_wifi_discard(void){available=0;}
#include "../App/gateway_network.c"
int main(void){(void)xQueueReceive;(void)g_sample_queue;queued=1;g_gateway_diag.network_state=3;g_gateway_diag.storage_ready=0;if(!setjmp(end_loop))gateway_network_task(0);if(sent!=1||queued){printf("fallback sent=%d queued=%d\n",sent,queued);return 1;}puts("network Flash failure live fallback: passed");return 0;}
