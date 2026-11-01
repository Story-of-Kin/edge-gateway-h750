#include "gateway.h"
#include "gateway_board.h"
#include "gateway_protocol.h"
#include "gateway_at.h"
#include <stdio.h>
#include <string.h>
static gw_at_parser_t modem;
static void pump(void)
{uint8_t b[128];int n;do{n=gateway_wifi_read(b,sizeof(b));if(n>0)gw_at_feed(&modem,b,(size_t)n);}while(n==(int)sizeof(b));g_gateway_diag.heartbeat_network=HAL_GetTick();}
static int wait_for(uint32_t event,uint32_t timeout)
{uint32_t start=HAL_GetTick();do{pump();if(modem.events&(GW_AT_FAIL|GW_AT_CLOSED))return -1;if(modem.events&event){modem.events&=~event;return 0;}vTaskDelay(pdMS_TO_TICKS(5));}while(HAL_GetTick()-start<timeout);return -1;}
static int command(const char*text,uint32_t event,uint32_t timeout)
{modem.events&=~(event|GW_AT_FAIL|GW_AT_CLOSED);if(gateway_wifi_send(text,strlen(text)))return -1;return wait_for(event,timeout);}
static int send_packet(const uint8_t*p,size_t n)
{char cmd[48];snprintf(cmd,sizeof(cmd),"AT+CIPSEND=%u\r\n",(unsigned)n);if(command(cmd,GW_AT_PROMPT,1500))return -1;modem.events&=~GW_AT_SEND_OK;if(gateway_wifi_send(p,n))return -1;return wait_for(GW_AT_SEND_OK,4000);}
static int safe_at_string(const char*p)
{size_t n=0;while(p[n]){if(p[n]=='"'||p[n]=='\\'||(unsigned char)p[n]<32||n>=100)return 0;n++;}return 1;}
static int connect_modem(void)
{
    char cmd[256];uint8_t packet[180];int n;gw_at_reset(&modem);gateway_wifi_discard();g_gateway_diag.network_state=1;
    if(!safe_at_string(GW_WIFI_SSID)||!safe_at_string(GW_WIFI_PASSWORD)||!safe_at_string(GW_MQTT_HOST))return -1;
    if(command("AT\r\n",GW_AT_OK,1000)||command("ATE0\r\n",GW_AT_OK,1000)||command("AT+CWMODE=1\r\n",GW_AT_OK,1000))return -1;
    command("AT+CIPCLOSE\r\n",GW_AT_OK,500);
    if(command("AT+CIPMUX=0\r\n",GW_AT_OK,1000)||command("AT+CIPMODE=0\r\n",GW_AT_OK,1000))return -1;
    snprintf(cmd,sizeof(cmd),"AT+CWJAP=\"%s\",\"%s\"\r\n",GW_WIFI_SSID,GW_WIFI_PASSWORD);
    if(command(cmd,GW_AT_OK,15000))return -1;
    g_gateway_diag.network_state=2;snprintf(cmd,sizeof(cmd),"AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",GW_MQTT_HOST,GW_MQTT_PORT);
    if(command(cmd,GW_AT_OK,10000))return -1;
    n=gw_mqtt_connect(packet,sizeof(packet),GW_MQTT_CLIENT_ID,30);modem.events&=~GW_AT_CONNACK;
    if(n<0||send_packet(packet,(size_t)n)||wait_for(GW_AT_CONNACK,4000))return -1;
    g_gateway_diag.network_state=3;gateway_log("MQTT connected; QoS0 transport acknowledgements only");return 0;
}
static int flush_cache(void)
{
    char json[GW_RECORD_PAYLOAD+1];uint8_t packet[512];uint32_t sequence;int len,n;
    if(!g_gateway_diag.storage_ready)return 0;
    if(xSemaphoreTake(g_storage_mutex,pdMS_TO_TICKS(500))!=pdTRUE)return 0;
    len=gw_store_peek(&g_cache_store,json,sizeof(json),&sequence);xSemaphoreGive(g_storage_mutex);
    if(len<=0)return 0; /* Local store errors must not disable live transport. */
    n=gw_mqtt_publish(packet,sizeof(packet),GW_MQTT_TOPIC,(const uint8_t*)json,(size_t)len);
    if(n<0||send_packet(packet,(size_t)n))return -1;
    /* SEND OK confirms the ESP transport, not broker/cloud persistence. */
    if(xSemaphoreTake(g_storage_mutex,pdMS_TO_TICKS(1000))!=pdTRUE)return -1;
    n=gw_store_ack(&g_cache_store,sequence);g_gateway_diag.cache_pending=g_cache_store.pending;xSemaphoreGive(g_storage_mutex);return n<0?0:1;
}
static int flush_live(void)
{
    static gw_sample_t sample;static int pending;
    char temp[24]="null",humi[24]="null",light[24]="null",json[160];uint8_t packet[512];int n,length;
    if(!pending){if(xQueueReceive(g_sample_queue,&sample,0)!=pdTRUE)return 0;pending=1;}
    if(sample.temperature_valid){snprintf(temp,sizeof(temp),"%.1f",sample.temperature);snprintf(humi,sizeof(humi),"%.1f",sample.humidity);}
    if(sample.light_valid)snprintf(light,sizeof(light),"%.0f",sample.light);
    length=snprintf(json,sizeof(json),"{\"temperature\":%s,\"shidu\":%s,\"light\":%s}",temp,humi,light);
    if(length<=0||length>=(int)sizeof(json))return -1;
    n=gw_mqtt_publish(packet,sizeof(packet),GW_MQTT_TOPIC,(const uint8_t*)json,(size_t)length);
    if(n<0||send_packet(packet,(size_t)n))return -1;
    pending=0;return 1;
}
void gateway_network_task(void*arg)
{
    uint32_t retry=0,ping=0;uint8_t request[2]={0xc0,0};int configured=GW_WIFI_SSID[0]&&GW_MQTT_CLIENT_ID[0],i,r;(void)arg;
    gateway_log(configured?"Wi-Fi configured; probing ESP8266 on USART3":"Wi-Fi not configured; no external module required to run UI");
    for(;;){pump();
        if(configured&&g_gateway_diag.network_state!=3&&(retry==0||HAL_GetTick()-retry>=30000)){
            if(connect_modem()){g_gateway_diag.network_state=1;gateway_log("Wi-Fi/MQTT unavailable; records retained for retry");}else ping=HAL_GetTick();retry=HAL_GetTick();
        }
        if(g_gateway_diag.network_state==3){
            if(modem.events&(GW_AT_CLOSED|GW_AT_FAIL)){g_gateway_diag.network_state=1;retry=HAL_GetTick();gateway_log("Network disconnected; retaining unsent records");}
            else{
                for(i=0;i<5;i++){r=flush_cache();if(r<0){g_gateway_diag.network_state=1;retry=HAL_GetTick();break;}if(!r)break;pump();vTaskDelay(pdMS_TO_TICKS(20));}
                if(g_gateway_diag.network_state==3&&flush_live()<0){g_gateway_diag.network_state=1;retry=HAL_GetTick();}
                if(g_gateway_diag.network_state==3&&HAL_GetTick()-ping>=10000){modem.events&=~GW_AT_PONG;if(send_packet(request,2)||wait_for(GW_AT_PONG,2000)){g_gateway_diag.network_state=1;retry=HAL_GetTick();gateway_log("MQTT heartbeat timed out");}ping=HAL_GetTick();}
            }
        }
        g_gateway_diag.heartbeat_network=HAL_GetTick();vTaskDelay(pdMS_TO_TICKS(100));
    }
}
