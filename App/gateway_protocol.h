/*
 * 协议工具接口：不依赖 STM32 寄存器或 FreeRTOS，可在电脑上直接运行测试。
 * 返回整数长度的 MQTT 函数：正数=生成的报文长度，负数=参数或容量错误。
 */
#ifndef GATEWAY_PROTOCOL_H
#define GATEWAY_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
/* 计算 Modbus RTU 的 CRC16；发送帧时 CRC 低字节在前。 */
uint16_t gw_crc16(const uint8_t *data, size_t length);
/* 解析单个16位传感器寄存器：可交换内部字节，可按有符号数解释，最后乘倍率。 */
float gw_sensor_register(uint16_t raw, int swap_bytes, int is_signed, float scale);
/* 把两个寄存器组合成32位无符号光照值；支持低字在前、字节交换和倍率。 */
float gw_sensor_light(const uint16_t values[2], int low_word_first, int swap_bytes, float scale);
/*
 * 校验03响应的地址、功能码、长度和CRC，全部通过才写 values。
 * count/capacity 均以16位寄存器个数计；返回0=成功、-1=格式错误、-2=CRC错误。
 */
int gw_modbus_decode(const uint8_t *frame, size_t length, uint8_t address, uint16_t count,
                     uint16_t *values, size_t capacity);
/* 构造 MQTT 3.1.1 CONNECT，干净会话，无 username/password；keepalive 单位为秒。 */
int gw_mqtt_connect(uint8_t *out, size_t capacity, const char *client, uint16_t keepalive);
/* 构造 QoS0 PUBLISH；capacity 是 out 字节容量，length 是 payload 字节数。 */
int gw_mqtt_publish(uint8_t *out, size_t capacity, const char *topic, const uint8_t *payload,
                    size_t length);
#endif
