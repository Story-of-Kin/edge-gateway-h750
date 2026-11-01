#ifndef GATEWAY_BOARD_H
#define GATEWAY_BOARD_H
#include "gateway.h"
void gateway_board_init(void);
int gateway_flash_init(void);
int gateway_flash_selftest(void);
extern const gw_flash_t gateway_flash;
void gateway_uart_msp_init(UART_HandleTypeDef *uart);
void gateway_uart_rx_complete(UART_HandleTypeDef *uart);
int gateway_wifi_send(const void *data,size_t length);
int gateway_wifi_read(uint8_t *out,size_t capacity);
void gateway_wifi_discard(void);
int gateway_modbus_read(uint8_t address,uint16_t reg,uint16_t values[2]);
void gateway_backlight(int enabled);
void gateway_watchdog_start(void);
void gateway_watchdog_feed(void);
#endif
