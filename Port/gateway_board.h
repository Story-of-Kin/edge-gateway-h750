/*
 * H750板级适配接口：上层通过这些函数访问具体硬件，不直接操作外设寄存器。
 * 除单独说明外，int 返回值0=成功，负值=失败。
 */
#ifndef GATEWAY_BOARD_H
#define GATEWAY_BOARD_H
#include "gateway.h"
/* 初始化SDRAM、MPU、串口、PCF8574等；调用前必须完成HAL/系统时钟/QSPI基础准备。 */
void gateway_board_init(void);
/* 初始化SPI2并读取JEDEC ID；仅识别支持的32MB器件后允许数据区操作。 */
int gateway_flash_init(void);
/* 专用扇区检查：0通过、1因未知内容跳过、-1失败；不破坏未知数据。 */
int gateway_flash_selftest(void);
/* 传给记录存储模块的底层read/write/erase实现；任务访问必须受存储锁保护。 */
extern const gw_flash_t gateway_flash;
/* 由HAL的全局MSP回调转发，设置USART2/3的时钟、GPIO与中断。 */
void gateway_uart_msp_init(UART_HandleTypeDef *uart);
/* 由HAL接收完成回调转发，把USART3的一个字节写进环形缓冲并重新接收。 */
void gateway_uart_rx_complete(UART_HandleTypeDef *uart);
/* 经USART3同步发送原始字节；此成功仅代表UART发送成功。 */
int gateway_wifi_send(const void *data, size_t length);
/* 非阻塞地取出已有接收字节；返回实际字节数，0表示暂时无数据。 */
int gateway_wifi_read(uint8_t *out, size_t capacity);
/* 丢弃软件环形缓冲的已有字节，主要用于开始新网络会话。 */
void gateway_wifi_discard(void);
/* USART2/RS485主站读取：03功能码、两个寄存器、9600 8N1；失败不生成有效读数。 */
int gateway_modbus_read(uint8_t address, uint16_t reg, uint16_t values[2]);
/* 控制屏幕背光；enabled非0开启，0关闭，界面逻辑和触摸扫描仍继续。 */
void gateway_backlight(int enabled);
/* 启动独立看门狗。标称超时约32秒，实际随LSI频率而变化。 */
void gateway_watchdog_start(void);
/* 重装看门狗计数器；由健康监控任务在检查业务心跳后调用。 */
void gateway_watchdog_feed(void);
#endif
