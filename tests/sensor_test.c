/*
 * 数值解码测试：检查负温度、字节交换、32位光照字序和倍率。
 */
#include <stdio.h>
#include "gateway_protocol.h"
#define CHECK(x) do{if(!(x)){printf("sensor failure at %d\n",__LINE__);return 1;}}while(0)
int main(void){uint16_t high_first[]={0,1000},low_first[]={1000,0},bytes_swapped[]={0,0xe803};
CHECK(gw_sensor_register(0xff9c,0,1,0.1f)==-10.0f);
CHECK(gw_sensor_register(0x5802,1,0,0.1f)==60.0f);
CHECK(gw_sensor_light(high_first,0,0,1.0f)==1000.0f);
CHECK(gw_sensor_light(low_first,1,0,0.1f)==100.0f);
CHECK(gw_sensor_light(bytes_swapped,0,1,1.0f)==1000.0f);
puts("sensor formats: passed");return 0;}
