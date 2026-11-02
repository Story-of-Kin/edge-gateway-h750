/*
 * 串口数据分两层解析：外层识别 ESP8266 的 AT 文本和 +IPD 载荷，内层拼 MQTT 报文。
 * 关键约束：TCP 载荷可能包含 ERROR 等普通文字，不能把它误当成 AT 错误。
 */
#include "gateway_at.h"
#include <string.h>
/* 清零整个解析器，适合在准备建立新连接时调用。 */
void gw_at_reset(gw_at_parser_t *p)
{
    memset(p, 0, sizeof(*p));
}
/*
 * 每次追加一个 MQTT 字节，按 Remaining Length 判断何时收齐一帧。
 * 当前只把连接确认和心跳响应转为事件；其他完整报文读完后丢弃。
 */
static void mqtt_byte(gw_at_parser_t *p, uint8_t b)
{
    size_t i, body = 0, multiplier = 1, total;
    if (p->mqtt_length >= sizeof(p->mqtt))
    {
        p->events |= GW_AT_FAIL;
        p->mqtt_length = 0;
        return;
    }
    p->mqtt[p->mqtt_length++] = b;
    if (p->mqtt_length < 2)
        return;
    /* MQTT 剩余长度使用最多四字节变长编码：低 7 位保存数值，最高位表示后面还有字节。 */
    for (i = 1; i < p->mqtt_length && i < 5; i++)
    {
        body += (p->mqtt[i] & 127) * multiplier;
        if (!(p->mqtt[i] & 128))
            break;
        multiplier *= 128;
    }
    if (i == p->mqtt_length)
        return;
    if (i == 5)
    {
        p->events |= GW_AT_FAIL;
        p->mqtt_length = 0;
        return;
    }
    /* 完整包长度 = 1字节控制头 + 剩余长度编码本身的字节数 + 报文体长度。 */
    total = 1 + i + body;
    if (total > sizeof(p->mqtt))
    {
        p->events |= GW_AT_FAIL;
        p->mqtt_length = 0;
        return;
    }
    if (p->mqtt_length < total)
        return;
    /* 0x20 是 CONNACK；只有会话标志与返回码均符合本实现期望时才报告成功。 */
    if (p->mqtt[0] == 0x20 && total == 4)
    {
        if (p->mqtt[2] == 0 && p->mqtt[3] == 0)
            p->events |= GW_AT_CONNACK;
        else
            p->events |= GW_AT_FAIL;
    }
    if (p->mqtt[0] == 0xd0 && total == 2)
        p->events |= GW_AT_PONG;
    p->mqtt_length = 0;
}
/* 扫描本次收到的字节；保留未完成的文本/二进制状态，供下次调用继续拼接。 */
void gw_at_feed(gw_at_parser_t *p, const uint8_t *d, size_t n)
{
    size_t i, j;
    char b;
    uint32_t length;
    if (!p || (!d && n))
        return;
    for (i = 0; i < n; i++)
    {
        /* 优先按二进制处理 +IPD 指定的字节数；其中的换行、>、ERROR 都不作 AT 文本解释。 */
        if (p->ipd_remaining)
        {
            p->ipd_remaining--;
            mqtt_byte(p, d[i]);
            continue;
        }
        /* 没有二进制载荷时，> 表示 CIPSEND 已准备好接收数据。 */
        b = (char)d[i];
        if (b == '>')
        {
            p->events |= GW_AT_PROMPT;
            continue;
        }
        if (p->line_length >= sizeof(p->line) - 1)
        {
            p->line_length = 0;
            p->events |= GW_AT_FAIL;
        }
        if (b == '\r')
            continue;
        /* AT 回车被忽略；收到换行才把累计文本作为一行检查，随后重置行缓冲。 */
        if (b == '\n')
        {
            p->line[p->line_length] = 0;
            if (strstr(p->line, "SEND OK"))
                p->events |= GW_AT_SEND_OK;
            if (!strcmp(p->line, "OK"))
                p->events |= GW_AT_OK;
            if (strstr(p->line, "ERROR") || strstr(p->line, "FAIL"))
                p->events |= GW_AT_FAIL;
            if (strstr(p->line, "CLOSED") || strstr(p->line, "WIFI DISCONNECT"))
                p->events |= GW_AT_CLOSED;
            p->line_length = 0;
            continue;
        }
        p->line[p->line_length++] = b;
        p->line[p->line_length] = 0;
        /* 本实现解析单连接主动接收格式 +IPD,数字长度:；冒号后的指定数量字节交给 MQTT。 */
        if (b == ':' && p->line_length > 6 && !strncmp(p->line, "+IPD,", 5))
        {
            length = 0;
            for (j = 5; j < p->line_length - 1; j++)
            {
                if (p->line[j] < '0' || p->line[j] > '9' || length > 6553)
                {
                    p->events |= GW_AT_FAIL;
                    length = 0;
                    break;
                }
                length = length * 10 + (unsigned)(p->line[j] - '0');
            }
            if (length > 65535)
            {
                p->events |= GW_AT_FAIL;
                length = 0;
            }
            p->ipd_remaining = length;
            p->line_length = 0;
        }
    }
}
