/*
 * ESP8266 AT 流解析接口。一次串口读取不一定对应一整行或一个完整 MQTT 报文。
 * 解析器把不同长度的数据块拼接，并通过 events 位图报告事件。
 */
#ifndef GATEWAY_AT_H
#define GATEWAY_AT_H
#include <stdint.h>
#include <stddef.h>
/*
 * 事件位可同时置位：使用按位或 |= 设置、按位与 & 检查、&=~ 掩码清除。
 * OK=AT成功；SEND_OK=模块发送完成；PROMPT=可发载荷；FAIL/CLOSED=失败/断开；
 * CONNACK=MQTT连接确认；PONG=MQTT心跳响应。
 */
#define GW_AT_OK 1u
#define GW_AT_SEND_OK 2u
#define GW_AT_PROMPT 4u
#define GW_AT_FAIL 8u
#define GW_AT_CLOSED 16u
#define GW_AT_CONNACK 32u
#define GW_AT_PONG 64u
/*
 * line 保存 AT 文本行；ipd_remaining 是当前 +IPD 二进制载荷尚未取完的字节数；
 * mqtt 保存跨多次 +IPD 的 MQTT 包，events 保存已经观察到的事件。
 */
typedef struct
{
    char line[256];
    size_t line_length;
    uint32_t ipd_remaining;
    uint8_t mqtt[512];
    size_t mqtt_length;
    uint32_t events;
} gw_at_parser_t;
/* 清空状态；调用者必须传入有效解析器指针。 */
void gw_at_reset(gw_at_parser_t *p);
/* 喂入任意大小的数据块，不要求块边界与 AT 行或 MQTT 帧边界一致。 */
void gw_at_feed(gw_at_parser_t *p, const uint8_t *data, size_t length);
#endif
