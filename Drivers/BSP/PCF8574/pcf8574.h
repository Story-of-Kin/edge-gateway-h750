/**
 ****************************************************************************************************
 * @file        pcf8574.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-06-12
 * @brief       PCF8574驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:正点原子 北极星 H750开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 
 ****************************************************************************************************
 */

#ifndef __PCF8574_H
#define __PCF8574_H

#include "./SYSTEM/sys/sys.h"

/* 引脚定义 */
#define PCF8574_INT_GPIO_PORT           GPIOG
#define PCF8574_INT_GPIO_PIN            GPIO_PIN_10
#define PCF8574_INT_GPIO_CLK_ENABLE()   do { __HAL_RCC_GPIOG_CLK_ENABLE(); } while (0)

/* IO操作 */
#define PCF8574_INT                     ((HAL_GPIO_ReadPin(PCF8574_INT_GPIO_PORT, PCF8574_INT_GPIO_PIN) == GPIO_PIN_RESET) ? 0 : 1)

/* IIC通讯地址 */
#define PCF8574_IIC_ADDR                0x20

/* 端口功能定义 */
#define BEEP_IO                         0   /* 蜂鸣器控制引脚 */
#define AP_INT_IO                       1   /* 光强传感器中断引脚 */
#define DCMI_PWDN_IO                    2   /* 摄像头电源控制引脚 */
#define USB_PWR_IO                      3   /* USB电源控制引脚 */
#define MD_PD_IO                        4   /* 扬声器功放关断控制引脚 */
#define MPU_INT_IO                      5   /* 六轴传感器中断引脚 */
#define RS485_RE_IO                     6   /* RS485接收使能引脚 */
#define ETH_RESET_IO                    7   /* 以太网复位引脚 */

/* 函数声明 */
uint8_t pcf8574_init(void);                         /* 初始化PCF8574 */
uint8_t pcf8574_read_byte(void);                    /* 读取PCF8574 8位端口数据 */
void pcf8574_write_byte(uint8_t byte);              /* 写入PCF8574 8位端口数据 */
uint8_t pcf8574_read_bit(uint8_t bits);             /* 读取PCF8574某一端口数据 */
void pcf8574_write_bit(uint8_t bits, uint8_t bit);  /* 写入PCF8574某一端口数据 */

#endif
