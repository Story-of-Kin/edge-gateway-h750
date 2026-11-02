/*
 * 协议测试：用已知CRC与报文样例，检查正确输入、截断输入和容量边界。
 */
#include <stdio.h>
#include <string.h>
#include "gateway_protocol.h"
#define CHECK(x) do { if(!(x)){printf("FAIL %s:%d: %s\n",__func__,__LINE__,#x);return 1;} } while(0)
static int crc_vector(void) { CHECK(gw_crc16((const uint8_t*)"123456789",9)==0x4b37);return 0; }
static int modbus_valid(void) {
    /* slave 1, function 3, two registers 0x022b and 0x00fd */
    uint8_t p[]={1,3,4,2,0x2b,0,0xfd,0x4a,0x02}; uint16_t v[2]={0};
    CHECK(gw_modbus_decode(p,sizeof(p),1,2,v,2)==0);CHECK(v[0]==555 && v[1]==253);return 0;
}
static int modbus_invalid(void) {
    uint8_t p[]={1,3,4,2,0x2b,0,0xfd,0x4a,0x02};uint16_t v[2]={0x7777,0x8888};
    CHECK(gw_modbus_decode(p,8,1,2,v,2)<0);CHECK(v[0]==0x7777);
    p[3]^=1;CHECK(gw_modbus_decode(p,9,1,2,v,2)<0);
    CHECK(gw_modbus_decode(p,9,1,2,v,1)<0);return 0;
}
static int mqtt_connect_frame(void) {
    const uint8_t expected[]={0x10,0x0f,0,4,'M','Q','T','T',4,2,0,60,0,3,'a','b','c'};
    uint8_t p[32];int n=gw_mqtt_connect(p,sizeof(p),"abc",60);
    CHECK(n==(int)sizeof(expected));CHECK(memcmp(p,expected,sizeof(expected))==0);
    CHECK(gw_mqtt_connect(p,5,"abc",60)<0);return 0;
}
static int mqtt_publish_frame(void) {
    const uint8_t expected[]={0x30,5,0,1,'t','o','k'};uint8_t p[300],data[130];int n;
    n=gw_mqtt_publish(p,sizeof(p),"t",(const uint8_t*)"ok",2);
    CHECK(n==7 && memcmp(p,expected,7)==0);
    memset(data,'x',sizeof(data));n=gw_mqtt_publish(p,sizeof(p),"t",data,sizeof(data));
    CHECK(n==136 && p[1]==0x85 && p[2]==1);CHECK(gw_mqtt_publish(p,6,"t",data,130)<0);
    CHECK(gw_mqtt_publish(p,sizeof(p),"bad/#",data,1)<0);return 0;
}
int main(void) {
    int failures=0; failures+=crc_vector();failures+=modbus_valid();failures+=modbus_invalid();
    failures+=mqtt_connect_frame();failures+=mqtt_publish_frame();
    printf("protocol: %d failed\n",failures);return failures?1:0;
}
