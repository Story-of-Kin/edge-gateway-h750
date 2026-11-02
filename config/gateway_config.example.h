/*
 * 项目配置入口。初学时可先读本文件，了解“地址、寄存器、倍率、分区”的含义。
 * 实际Wi-Fi/云端凭据放在被Git忽略的gateway_config.local.h，并在Keil定义GATEWAY_LOCAL_CONFIG。
 * 先包含私有配置，再用#ifndef提供默认值，因此私有定义可以覆盖下面的默认配置。
 * 传感器地址/寄存器必须以实际产品手册为准，通信接口当前为9600 8N1、03功能码、两个寄存器。
 */
#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H
/*
 * Copy overrides to gateway_config.local.h and define GATEWAY_LOCAL_CONFIG.
 * The private file is ignored by Git. Never put real credentials here.
 */
#ifdef GATEWAY_LOCAL_CONFIG
#include "gateway_config.local.h"
#endif
/* 无线名称与密码：默认空字符串。空SSID或空client ID会让网络任务保持未配置状态。 */
#ifndef GW_WIFI_SSID
#define GW_WIFI_SSID ""
#endif
#ifndef GW_WIFI_PASSWORD
#define GW_WIFI_PASSWORD ""
#endif
/* MQTT客户端标识，按自己的平台账号要求填写；不是GitHub账号，也不能复用上游作者的凭据。 */
#ifndef GW_MQTT_CLIENT_ID
#define GW_MQTT_CLIENT_ID ""
#endif
/* 默认采用普通TCP连接bemfa.com:9501；本实现没有TLS加密或MQTT用户名密码认证。 */
#ifndef GW_MQTT_HOST
#define GW_MQTT_HOST "bemfa.com"
#endif
#ifndef GW_MQTT_PORT
#define GW_MQTT_PORT 9501
#endif
/* 发布主题：需与自己的云端主题一致；本项目保留stm32作为无凭据示例。 */
#ifndef GW_MQTT_TOPIC
#define GW_MQTT_TOPIC "stm32"
#endif
/*
 * 温湿度设备：默认从站1，从报文偏移0x0001读两个寄存器。
 * 注意手册中的40001等人类编号不一定等于需要填入报文的零基偏移。
 */
#ifndef GW_TEMP_ADDRESS
#define GW_TEMP_ADDRESS 1
#endif
#ifndef GW_TEMP_REGISTER
#define GW_TEMP_REGISTER 0x0001
#endif
/* 工程值=原始值×倍率；默认温度、湿度均乘0.1，温度按有符号16位解释。 */
#ifndef GW_TEMP_SCALE
#define GW_TEMP_SCALE 0.1f
#endif
#ifndef GW_HUMI_SCALE
#define GW_HUMI_SCALE 0.1f
#endif
/* 光照设备：默认从站2，起始偏移0x0002；两个寄存器组合成一个32位无符号数。 */
#ifndef GW_LIGHT_ADDRESS
#define GW_LIGHT_ADDRESS 2
#endif
#ifndef GW_LIGHT_REGISTER
#define GW_LIGHT_REGISTER 0x0002
#endif
/* 温湿度在两个返回寄存器中的位置，只允许0或1；可根据手册互换。 */
#ifndef GW_TEMP_INDEX
#define GW_TEMP_INDEX 0
#endif
#ifndef GW_HUMI_INDEX
#define GW_HUMI_INDEX 1
#endif
/* 0不交换字内字节，1交换；字内字节交换与两个寄存器的先后顺序是不同概念。 */
#ifndef GW_TEMP_SWAP_BYTES
#define GW_TEMP_SWAP_BYTES 0
#endif
/* 光照字序：0=高16位先返回，1=低16位先返回；另可选择各字内部字节交换。 */
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
/*
 * 存储分区只用SPI数据Flash最后8MB，绝不写QSPI代码Flash或前24MB原厂资源。
 * 0x01800000首4KB用于自检；日志从0x01801000至0x01C00000，缓存至0x02000000。
 * 所有END边界均按“包含起点、不包含终点”理解；不要随意改动以免覆盖数据。
 */
#define GW_LOG_BASE 0x01801000u
#define GW_LOG_SIZE 0x003ff000u
#define GW_CACHE_BASE 0x01c00000u
#define GW_CACHE_SIZE 0x00400000u
#define GW_SELFTEST_SECTOR 0x01800000u
/* 每轮采集完成后延时2秒，因此实际采样间隔还包含通信耗时；息屏空闲阈值为10秒。 */
#define GW_SENSOR_PERIOD_MS 2000u
#define GW_SCREEN_IDLE_MS 10000u
#endif
