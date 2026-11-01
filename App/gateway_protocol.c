/* Bounded serializers for the protocols used by zgd-dev/-V2.0. */
#include "gateway_protocol.h"
#include <string.h>
uint16_t gw_crc16(const uint8_t *p, size_t n)
{
    uint16_t crc=0xffff; unsigned bit;
    if(!p && n) return 0;
    while(n--) { crc^=*p++;for(bit=0;bit<8;bit++)crc=(uint16_t)((crc>>1)^((crc&1)?0xa001:0)); }
    return crc;
}
int gw_modbus_decode(const uint8_t *p,size_t n,uint8_t a,uint16_t c,uint16_t *v,size_t z)
{
    size_t i;uint16_t crc;
    if(!p||!v||c==0||c>125||z<c||n!=(size_t)c*2+5||p[0]!=a||p[1]!=3||p[2]!=c*2) return -1;
    crc=gw_crc16(p,n-2);
    if(p[n-2]!=(uint8_t)crc||p[n-1]!=(uint8_t)(crc>>8)) return -2;
    for(i=0;i<c;i++)v[i]=(uint16_t)((p[3+i*2]<<8)|p[4+i*2]);
    return 0;
}
static uint16_t sensor_word(uint16_t raw,int swap_bytes)
{return swap_bytes?(uint16_t)((raw<<8)|(raw>>8)):raw;}
float gw_sensor_register(uint16_t raw,int swap_bytes,int is_signed,float scale)
{uint16_t word=sensor_word(raw,swap_bytes);return (is_signed?(float)(int16_t)word:(float)word)*scale;}
float gw_sensor_light(const uint16_t values[2],int low_word_first,int swap_bytes,float scale)
{uint32_t hi=sensor_word(values[low_word_first?1:0],swap_bytes),lo=sensor_word(values[low_word_first?0:1],swap_bytes);return (float)((hi<<16)|lo)*scale;}
static size_t bounded_len(const char *p,size_t max)
{size_t n=0;if(!p)return max;while(n<max&&p[n])n++;return n;}
static size_t remaining(uint8_t *p,size_t n)
{size_t i=0;do{uint8_t b=(uint8_t)(n%128);n/=128;if(n)b|=128;p[i++]=b;}while(n);return i;}
int gw_mqtt_connect(uint8_t *o,size_t cap,const char *s,uint16_t keep)
{
    size_t len=bounded_len(s,129),body,h;uint8_t rl[4];
    if(!o||!len||len>128)return -1;
    body=12+len;h=remaining(rl,body);
    if(cap<1+h+body)return -1;
    o[0]=0x10;memcpy(o+1,rl,h);o+=1+h;
    o[0]=0;o[1]=4;memcpy(o+2,"MQTT",4);o[6]=4;o[7]=2;
    o[8]=(uint8_t)(keep>>8);o[9]=(uint8_t)keep;o[10]=(uint8_t)(len>>8);o[11]=(uint8_t)len;
    memcpy(o+12,s,len);return (int)(1+h+body);
}
int gw_mqtt_publish(uint8_t *o,size_t cap,const char *topic,const uint8_t *p,size_t n)
{
    size_t len=bounded_len(topic,129),body,h,i;uint8_t rl[4];
    if(!o||(!p&&n)||!len||len>128||n>1024)return -1;
    for(i=0;i<len;i++)if(topic[i]=='#'||topic[i]=='+')return -1;
    body=2+len+n;h=remaining(rl,body);if(cap<1+h+body)return -1;
    o[0]=0x30;memcpy(o+1,rl,h);o[1+h]=(uint8_t)(len>>8);o[2+h]=(uint8_t)len;
    memcpy(o+3+h,topic,len);if(n)memcpy(o+3+h+len,p,n);
    return (int)(1+h+body);
}
