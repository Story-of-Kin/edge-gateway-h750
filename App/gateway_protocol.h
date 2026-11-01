#ifndef GATEWAY_PROTOCOL_H
#define GATEWAY_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
uint16_t gw_crc16(const uint8_t *data, size_t length);
float gw_sensor_register(uint16_t raw,int swap_bytes,int is_signed,float scale);
float gw_sensor_light(const uint16_t values[2],int low_word_first,int swap_bytes,float scale);
int gw_modbus_decode(const uint8_t *frame, size_t length, uint8_t address,
                     uint16_t count, uint16_t *values, size_t capacity);
int gw_mqtt_connect(uint8_t *out, size_t capacity, const char *client, uint16_t keepalive);
int gw_mqtt_publish(uint8_t *out, size_t capacity, const char *topic,
                    const uint8_t *payload, size_t length);
#endif
