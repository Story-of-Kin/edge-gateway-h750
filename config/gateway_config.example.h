#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H
/* Copy overrides to gateway_config.local.h and define GATEWAY_LOCAL_CONFIG.
 * The private file is ignored by Git. Never put real credentials here. */
#ifdef GATEWAY_LOCAL_CONFIG
#include "gateway_config.local.h"
#endif
#ifndef GW_WIFI_SSID
#define GW_WIFI_SSID ""
#endif
#ifndef GW_WIFI_PASSWORD
#define GW_WIFI_PASSWORD ""
#endif
#ifndef GW_MQTT_CLIENT_ID
#define GW_MQTT_CLIENT_ID ""
#endif
#ifndef GW_MQTT_HOST
#define GW_MQTT_HOST "bemfa.com"
#endif
#ifndef GW_MQTT_PORT
#define GW_MQTT_PORT 9501
#endif
#ifndef GW_MQTT_TOPIC
#define GW_MQTT_TOPIC "stm32"
#endif
#ifndef GW_TEMP_ADDRESS
#define GW_TEMP_ADDRESS 1
#endif
#ifndef GW_TEMP_REGISTER
#define GW_TEMP_REGISTER 0x0001
#endif
#ifndef GW_TEMP_SCALE
#define GW_TEMP_SCALE 0.1f
#endif
#ifndef GW_HUMI_SCALE
#define GW_HUMI_SCALE 0.1f
#endif
#ifndef GW_LIGHT_ADDRESS
#define GW_LIGHT_ADDRESS 2
#endif
#ifndef GW_LIGHT_REGISTER
#define GW_LIGHT_REGISTER 0x0002
#endif
#ifndef GW_TEMP_INDEX
#define GW_TEMP_INDEX 0
#endif
#ifndef GW_HUMI_INDEX
#define GW_HUMI_INDEX 1
#endif
#ifndef GW_TEMP_SWAP_BYTES
#define GW_TEMP_SWAP_BYTES 0
#endif
#ifndef GW_LIGHT_LOW_WORD_FIRST
#define GW_LIGHT_LOW_WORD_FIRST 0
#endif
#ifndef GW_LIGHT_SWAP_BYTES
#define GW_LIGHT_SWAP_BYTES 0
#endif
#ifndef GW_LIGHT_SCALE
#define GW_LIGHT_SCALE 1.0f
#endif
#if (GW_TEMP_INDEX < 0 || GW_TEMP_INDEX > 1 || GW_HUMI_INDEX < 0 || GW_HUMI_INDEX > 1)
#error Sensor indices must be 0 or 1 within the two-register response
#endif
#define GW_LOG_BASE 0x01801000u
#define GW_LOG_SIZE 0x003ff000u
#define GW_CACHE_BASE 0x01c00000u
#define GW_CACHE_SIZE 0x00400000u
#define GW_SELFTEST_SECTOR 0x01800000u
#define GW_SENSOR_PERIOD_MS 2000u
#define GW_SCREEN_IDLE_MS 10000u
#endif
