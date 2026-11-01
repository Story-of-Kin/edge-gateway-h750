/* The original gateway's main/log workflow, adapted for the 800x480 RGB panel. */
#include "gateway.h"
#include "lvgl.h"
#include <stdio.h>
extern const lv_font_t font_chinese_9;
static lv_obj_t *home,*logs,*value[3],*bar[3],*connection,*hardware,*uptime,*details,*log_list;
static lv_obj_t *label(lv_obj_t*parent,const char*text,int x,int y,const lv_font_t*font,uint32_t color)
{lv_obj_t*o=lv_label_create(parent);lv_label_set_text(o,text);lv_obj_set_pos(o,x,y);lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);return o;}
static void back_event(lv_event_t*e){(void)e;lv_scr_load(home);}
static void log_event(lv_event_t*e)
{
    unsigned i;char text[240],line[280];uint32_t stamp;int n;(void)e;lv_obj_clean(log_list);
    if(!g_gateway_diag.storage_ready){lv_list_add_text(log_list,"Data Flash unavailable");lv_scr_load(logs);return;}
    if(xSemaphoreTake(g_storage_mutex,pdMS_TO_TICKS(100))==pdTRUE){
        for(i=0;i<10;i++){n=gw_store_recent(&g_log_store,i,text,sizeof(text),&stamp);if(n<=0)break;snprintf(line,sizeof(line),"%lus   %s",(unsigned long)(stamp/1000),text);lv_list_add_text(log_list,line);}
        xSemaphoreGive(g_storage_mutex);
    }else lv_list_add_text(log_list,"Storage busy; return and retry.");
    lv_scr_load(logs);
}
static lv_obj_t *button(lv_obj_t*parent,const char*text,int x,int y,lv_event_cb_t callback)
{lv_obj_t*b=lv_btn_create(parent),*t;lv_obj_set_size(b,140,48);lv_obj_set_pos(b,x,y);t=lv_label_create(b);lv_obj_set_style_text_font(t,&font_chinese_9,0);lv_label_set_text(t,text);lv_obj_center(t);lv_obj_add_event_cb(b,callback,LV_EVENT_CLICKED,0);return b;}
void gateway_ui_create(void)
{
    static const char *titles[]={"温度","湿度","光照"};unsigned i;lv_obj_t*card;
    home=lv_obj_create(0);lv_obj_clear_flag(home,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(home,lv_color_hex(0x101d31),0);
    label(home,"边缘网关监控",24,20,&font_chinese_9,0xffffff);
    label(home,"STM32H750  /  800 x 480",460,24,&lv_font_montserrat_20,0x87b8e6);
    connection=label(home,"等待连接",24,67,&font_chinese_9,0xf4b860);
    for(i=0;i<3;i++){
        card=lv_obj_create(home);lv_obj_set_size(card,240,215);lv_obj_set_pos(card,20+i*260,108);lv_obj_clear_flag(card,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(card,lv_color_hex(0x1c304a),0);lv_obj_set_style_border_width(card,0,0);
        label(card,titles[i],8,5,&font_chinese_9,0xbcd0e8);
        value[i]=label(card,"--.-",8,60,&lv_font_montserrat_32,0xffffff);
        bar[i]=lv_bar_create(card);lv_obj_set_size(bar[i],200,12);lv_obj_set_pos(bar[i],8,157);lv_bar_set_range(bar[i],0,100);
    }
    hardware=label(home,"Data Flash: checking...",24,342,&lv_font_montserrat_14,0x8fb2d8);
    details=label(home,"No external sensors or Wi-Fi module configured",24,372,&lv_font_montserrat_14,0x8fb2d8);
    uptime=label(home,"00:00:00",24,430,&lv_font_montserrat_20,0xffffff);button(home,"日志",635,412,log_event);
    logs=lv_obj_create(0);lv_obj_clear_flag(logs,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(logs,lv_color_hex(0x101d31),0);
    button(logs,"返回",18,12,back_event);label(logs,"系统运行日志",240,22,&font_chinese_9,0xffffff);
    log_list=lv_list_create(logs);lv_obj_set_size(log_list,764,390);lv_obj_set_pos(log_list,18,78);lv_obj_set_style_text_font(log_list,&lv_font_montserrat_14,0);
    lv_scr_load(home);gateway_ui_update();
}
void gateway_ui_update(void)
{
    char text[180];gw_sample_t sample;uint32_t s=g_gateway_diag.uptime_ms/1000;
    taskENTER_CRITICAL();sample=g_gateway_sample;taskEXIT_CRITICAL();
    if(sample.temperature_valid){snprintf(text,sizeof(text),"%.1f C",sample.temperature);lv_label_set_text(value[0],text);snprintf(text,sizeof(text),"%.1f %%",sample.humidity);lv_label_set_text(value[1],text);lv_bar_set_value(bar[0],(int)sample.temperature,LV_ANIM_OFF);lv_bar_set_value(bar[1],(int)sample.humidity,LV_ANIM_OFF);}
    else{lv_label_set_text(value[0],"--.- C");lv_label_set_text(value[1],"--.- %");lv_bar_set_value(bar[0],0,LV_ANIM_OFF);lv_bar_set_value(bar[1],0,LV_ANIM_OFF);}
    if(sample.light_valid){snprintf(text,sizeof(text),"%.0f Lux",sample.light);lv_label_set_text(value[2],text);lv_bar_set_value(bar[2],sample.light>10000?100:(int)(sample.light/100),LV_ANIM_OFF);}
    else{lv_label_set_text(value[2],"---- Lux");lv_bar_set_value(bar[2],0,LV_ANIM_OFF);}
    lv_label_set_text(connection,g_gateway_diag.network_state==3?"在线":"等待连接");
    snprintf(text,sizeof(text),"Data Flash: %06lX  |  self-test: %s  |  cached: %lu",(unsigned long)g_gateway_diag.flash_id,g_gateway_diag.flash_selftest==1?"PASS":g_gateway_diag.flash_selftest==2?"SKIPPED":"FAIL",(unsigned long)g_gateway_diag.cache_pending);lv_label_set_text(hardware,text);
    snprintf(text,sizeof(text),"RS485 temperature: %s  /  light: %s   |   Wi-Fi: %s",sample.temperature_valid?"OK":"not connected",sample.light_valid?"OK":"not connected",g_gateway_diag.network_state==0?"not configured":g_gateway_diag.network_state==3?"online":"offline / connecting");lv_label_set_text(details,text);
    snprintf(text,sizeof(text),"%02lu:%02lu:%02lu  |  Free heap: %lu",(unsigned long)(s/3600),(unsigned long)(s/60%60),(unsigned long)(s%60),(unsigned long)g_gateway_diag.free_heap);lv_label_set_text(uptime,text);
}
