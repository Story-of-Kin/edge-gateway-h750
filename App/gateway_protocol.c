/*
 * 可移植的 Modbus/MQTT 编解码和传感器数值转换。
 * 这些函数只操作内存，实际串口发送在板级接口执行，便于用主机测试验证边界。
 */
/* Bounded serializers for the protocols used by zgd-dev/-V2.0. */
#include "gateway_protocol.h"
#include <string.h>
/*
 * Modbus CRC16：初值0xFFFF、多项式0xA001，逐字节异或并进行8次移位。
 * p 为输入字节数组，n 为字节数；不修改输入。
 */
uint16_t gw_crc16(const uint8_t *p, size_t n)
{
    uint16_t crc = 0xffff;
    unsigned bit;
    if (!p && n)
        return 0;
    while (n--)
    {
        crc ^= *p++;
        for (bit = 0; bit < 8; bit++)
            crc = (uint16_t)((crc >> 1) ^ ((crc & 1) ? 0xa001 : 0));
    }
    return crc;
}
/*
 * 仅解码03正常响应：[地址][03][数据字节数][寄存器数据...][CRC低][CRC高]。
 * p/n 是报文与长度，a 是期望地址，c 是寄存器数，v/z 是输出数组与容量。
 */
int gw_modbus_decode(const uint8_t *p, size_t n, uint8_t a, uint16_t c, uint16_t *v, size_t z)
{
    size_t i;
    uint16_t crc;
    /* 在访问字段或写输出前检查报文长度和缓冲容量；失败不会写入一半的测量值。 */
    if (!p || !v || c == 0 || c > 125 || z < c || n != (size_t)c * 2 + 5 || p[0] != a ||
        p[1] != 3 || p[2] != c * 2)
        return -1;
    crc = gw_crc16(p, n - 2);
    if (p[n - 2] != (uint8_t)crc || p[n - 1] != (uint8_t)(crc >> 8))
        return -2;
    /* Modbus 寄存器在线路中采用高字节在前，转换为本机 uint16_t 后再交给应用解码。 */
    for (i = 0; i < c; i++)
        v[i] = (uint16_t)((p[3 + i * 2] << 8) | p[4 + i * 2]);
    return 0;
}
/* 必要时交换同一个16位寄存器内的高低字节，注意它与交换两个寄存器的次序不同。 */
static uint16_t sensor_word(uint16_t raw, int swap_bytes)
{
    return swap_bytes ? (uint16_t)((raw << 8) | (raw >> 8)) : raw;
}
/* 先处理字节序，再决定有符号/无符号解释，最后乘传感器说明书给定的倍率。 */
float gw_sensor_register(uint16_t raw, int swap_bytes, int is_signed, float scale)
{
    uint16_t word = sensor_word(raw, swap_bytes);
    return (is_signed ? (float)(int16_t)word : (float)word) * scale;
}
/* 依 low_word_first 选择哪个寄存器是高16位，再拼接成32位值并换算。 */
float gw_sensor_light(const uint16_t values[2], int low_word_first, int swap_bytes, float scale)
{
    uint32_t hi = sensor_word(values[low_word_first ? 1 : 0], swap_bytes),
             lo = sensor_word(values[low_word_first ? 0 : 1], swap_bytes);
    return (float)((hi << 16) | lo) * scale;
}
/* 有上限地检查字符串长度，避免超长配置导致无界扫描；到达 max 即返回 max。 */
static size_t bounded_len(const char *p, size_t max)
{
    size_t n = 0;
    if (!p)
        return max;
    while (n < max && p[n])
        n++;
    return n;
}
/* 按 MQTT 的128进制变长规则编码剩余长度，返回编码占用的字节数。 */
static size_t remaining(uint8_t *p, size_t n)
{
    size_t i = 0;
    do
    {
        uint8_t b = (uint8_t)(n % 128);
        n /= 128;
        if (n)
            b |= 128;
        p[i++] = b;
    } while (n);
    return i;
}
/*
 * 构造 CONNECT 包：协议名 MQTT、版本4（即3.1.1）、clean session=1。
 * client ID 必须有1至128字节；此函数只构造报文，不代表已连接。
 */
int gw_mqtt_connect(uint8_t *o, size_t cap, const char *s, uint16_t keep)
{
    size_t len = bounded_len(s, 129), body, h;
    uint8_t rl[4];
    if (!o || !len || len > 128)
        return -1;
    body = 12 + len;
    h = remaining(rl, body);
    if (cap < 1 + h + body)
        return -1;
    /* 0x10 是 CONNECT 固定头；先写长度编码，再写可变头和客户端标识。 */
    o[0] = 0x10;
    memcpy(o + 1, rl, h);
    o += 1 + h;
    o[0] = 0;
    o[1] = 4;
    memcpy(o + 2, "MQTT", 4);
    o[6] = 4;
    o[7] = 2;
    o[8] = (uint8_t)(keep >> 8);
    o[9] = (uint8_t)keep;
    o[10] = (uint8_t)(len >> 8);
    o[11] = (uint8_t)len;
    memcpy(o + 12, s, len);
    return (int)(1 + h + body);
}
/*
 * 构造 QoS0 发布报文，不含 packet identifier，也不会等待 PUBACK。
 * 拒绝通配符主题和超容量输入；调用者需传入有效长度的 payload。
 */
int gw_mqtt_publish(uint8_t *o, size_t cap, const char *topic, const uint8_t *p, size_t n)
{
    size_t len = bounded_len(topic, 129), body, h, i;
    uint8_t rl[4];
    if (!o || (!p && n) || !len || len > 128 || n > 1024)
        return -1;
    for (i = 0; i < len; i++)
        if (topic[i] == '#' || topic[i] == '+')
            return -1;
    body = 2 + len + n;
    h = remaining(rl, body);
    if (cap < 1 + h + body)
        return -1;
    /* 0x30 表示普通 QoS0 PUBLISH；主题前的两个字节保存主题长度。 */
    o[0] = 0x30;
    memcpy(o + 1, rl, h);
    o[1 + h] = (uint8_t)(len >> 8);
    o[2 + h] = (uint8_t)len;
    memcpy(o + 3 + h, topic, len);
    if (n)
        memcpy(o + 3 + h + len, p, n);
    return (int)(1 + h + body);
}
