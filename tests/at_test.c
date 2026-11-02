/*
 * AT流测试：主动拆分/拼接数据，验证半包、粘包和二进制载荷不会被误判为AT错误。
 */
#include <stdio.h>
#include <string.h>
#include "gateway_at.h"
#define CHECK(x) do{if(!(x)){printf("FAIL %s:%d %s\n",__func__,__LINE__,#x);return 1;}}while(0)
static int split_binary_packets(void){gw_at_parser_t p;const uint8_t a[]={'+','I','P','D',',','2',':',0x20,2};const uint8_t b[]={'+','I','P','D',',','2',':',0,0};gw_at_reset(&p);gw_at_feed(&p,a,sizeof(a));CHECK(!(p.events&GW_AT_CONNACK));gw_at_feed(&p,b,sizeof(b));CHECK(p.events&GW_AT_CONNACK);return 0;}
static int binary_not_text(void){gw_at_parser_t p;const uint8_t s[]={'+','I','P','D',',','1','0',':',0x30,8,0,1,'t','E','R','R','O','R','\r','\n','S','E','N','D',' ','O','K','\r','\n'};gw_at_reset(&p);gw_at_feed(&p,s,sizeof(s));CHECK(!(p.events&GW_AT_FAIL));CHECK(p.events&GW_AT_SEND_OK);return 0;}
static int prompt_and_overflow(void){gw_at_parser_t p;unsigned i;uint8_t x='A';gw_at_reset(&p);gw_at_feed(&p,(const uint8_t*)">",1);CHECK(p.events&GW_AT_PROMPT);for(i=0;i<1000;i++)gw_at_feed(&p,&x,1);CHECK(p.events&GW_AT_FAIL);return 0;}
static int ping_and_reject_connack(void){gw_at_parser_t p;const uint8_t a[]={'+','I','P','D',',','6',':',0x20,2,0,5,0xd0,0};gw_at_reset(&p);gw_at_feed(&p,a,sizeof(a));CHECK(!(p.events&GW_AT_CONNACK));CHECK(p.events&GW_AT_FAIL);CHECK(p.events&GW_AT_PONG);return 0;}
int main(void){int n=0;n+=split_binary_packets();n+=binary_not_text();n+=prompt_and_overflow();n+=ping_and_reject_connack();printf("at parser: %d failed\n",n);return n?1:0;}
