#include "gateway_at.h"
#include <string.h>
void gw_at_reset(gw_at_parser_t*p){memset(p,0,sizeof(*p));}
static void mqtt_byte(gw_at_parser_t*p,uint8_t b)
{
    size_t i,body=0,multiplier=1,total;
    if(p->mqtt_length>=sizeof(p->mqtt)){p->events|=GW_AT_FAIL;p->mqtt_length=0;return;}
    p->mqtt[p->mqtt_length++]=b;if(p->mqtt_length<2)return;
    for(i=1;i<p->mqtt_length&&i<5;i++){
        body+=(p->mqtt[i]&127)*multiplier;
        if(!(p->mqtt[i]&128))break;
        multiplier*=128;
    }
    if(i==p->mqtt_length)return;
    if(i==5){p->events|=GW_AT_FAIL;p->mqtt_length=0;return;}
    total=1+i+body;if(total>sizeof(p->mqtt)){p->events|=GW_AT_FAIL;p->mqtt_length=0;return;}
    if(p->mqtt_length<total)return;
    if(p->mqtt[0]==0x20&&total==4){if(p->mqtt[2]==0&&p->mqtt[3]==0)p->events|=GW_AT_CONNACK;else p->events|=GW_AT_FAIL;}
    if(p->mqtt[0]==0xd0&&total==2)p->events|=GW_AT_PONG;
    p->mqtt_length=0;
}
void gw_at_feed(gw_at_parser_t*p,const uint8_t*d,size_t n)
{
    size_t i,j;char b;uint32_t length;
    if(!p||(!d&&n))return;
    for(i=0;i<n;i++){
        if(p->ipd_remaining){p->ipd_remaining--;mqtt_byte(p,d[i]);continue;}
        b=(char)d[i];if(b=='>'){p->events|=GW_AT_PROMPT;continue;}
        if(p->line_length>=sizeof(p->line)-1){p->line_length=0;p->events|=GW_AT_FAIL;}
        if(b=='\r')continue;
        if(b=='\n'){
            p->line[p->line_length]=0;
            if(strstr(p->line,"SEND OK"))p->events|=GW_AT_SEND_OK;
            if(!strcmp(p->line,"OK"))p->events|=GW_AT_OK;
            if(strstr(p->line,"ERROR")||strstr(p->line,"FAIL"))p->events|=GW_AT_FAIL;
            if(strstr(p->line,"CLOSED")||strstr(p->line,"WIFI DISCONNECT"))p->events|=GW_AT_CLOSED;
            p->line_length=0;continue;
        }
        p->line[p->line_length++]=b;p->line[p->line_length]=0;
        if(b==':'&&p->line_length>6&&!strncmp(p->line,"+IPD,",5)){
            length=0;
            for(j=5;j<p->line_length-1;j++){
                if(p->line[j]<'0'||p->line[j]>'9'||length>6553){p->events|=GW_AT_FAIL;length=0;break;}
                length=length*10+(unsigned)(p->line[j]-'0');
            }
            if(length>65535){p->events|=GW_AT_FAIL;length=0;}
            p->ipd_remaining=length;p->line_length=0;
        }
    }
}
