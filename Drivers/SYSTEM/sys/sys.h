/**
 ****************************************************************************************************
 * @file        sys.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-06-12
 * @brief       系统初始化代码
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

#ifndef __SYS_H
#define __SYS_H

#include "stm32h7xx_hal.h"

/**
 * SYS_SUPPORT_OS用于定义系统文件夹是否支持OS
 * 0,不支持OS
 * 1,支持OS
 */
#define SYS_SUPPORT_OS 0

/* 用于配置各外设时钟 */
extern RCC_PeriphCLKInitTypeDef rcc_periph_clk_init_struct;

/* 函数声明 */
void sys_cache_enable(void);                                                                /* 使能STM32H7的L1-Cache */
uint8_t sys_stm32_clock_init(uint32_t plln, uint32_t pllm, uint32_t pllp, uint32_t pllq);   /* 配置时钟 */
void sys_qspi_enable_memmapmode(void);                                                      /* 使能QSPI内存映射模式 */

#endif
