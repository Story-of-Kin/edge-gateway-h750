/**
 ****************************************************************************************************
 * @file        w25qxx.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-06-12
 * @brief       W25QXX驱动代码
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

#ifndef __W25QXX_H
#define __W25QXX_H

#include "./SYSTEM/sys/sys.h"

/* 引脚定义 */
#define W25QXX_CS_GPIO_PORT         GPIOF
#define W25QXX_CS_GPIO_PIN          GPIO_PIN_10
#define W25QXX_CS_GPIO_CLK_ENABLE() do { __HAL_RCC_GPIOF_CLK_ENABLE(); } while (0)

/* IO操作 */
#define W25QXX_CS(x)                do { (x) ?                                                                      \
                                        HAL_GPIO_WritePin(W25QXX_CS_GPIO_PORT, W25QXX_CS_GPIO_PIN, GPIO_PIN_SET):   \
                                        HAL_GPIO_WritePin(W25QXX_CS_GPIO_PORT, W25QXX_CS_GPIO_PIN, GPIO_PIN_RESET); \
                                    } while (0)

/* W25QXX芯片ID定义 */
#define W25Q80                      0xEF13
#define W25Q16                      0xEF14
#define W25Q32                      0xEF15
#define W25Q64                      0xEF16
#define W25Q128                     0xEF17
#define W25Q256                     0xEF18

/* W25QXX指令定义 */
#define W25QXX_WriteEnable          0x06
#define W25QXX_WriteDisable         0x04
#define W25QXX_ReadStatusReg1       0x05
#define W25QXX_ReadStatusReg2       0x35
#define W25QXX_ReadStatusReg3       0x15
#define W25QXX_WriteStatusReg1      0x01
#define W25QXX_WriteStatusReg2      0x31
#define W25QXX_WriteStatusReg3      0x11
#define W25QXX_ReadData             0x03
#define W25QXX_FastReadData         0x0B
#define W25QXX_FastReadDual         0x3B
#define W25QXX_FastReadQuad         0xEB
#define W25QXX_PageProgram          0x02
#define W25QXX_PageProgramQuad      0x32
#define W25QXX_BlockErase           0xD8
#define W25QXX_SectorErase          0x20
#define W25QXX_ChipErase            0xC7
#define W25QXX_PowerDown            0xB9
#define W25QXX_ReleasePowerDown     0xAB
#define W25QXX_DeviceID             0xAB
#define W25QXX_ManufactDeviceID     0x90
#define W25QXX_JedecDeviceID        0x9F
#define W25QXX_Enable4ByteAddr      0xB7
#define W25QXX_Exit4ByteAddr        0xE9
#define W25QXX_SetReadParam         0xC0
#define W25QXX_EnterQPIMode         0x38
#define W25QXX_ExitQPIMode          0xFF

/* 函数声明 */
void w25qxx_init(void);                                             /* 初始化W25QXX */
void w25qxx_write_enable(void);                                     /* 写使能W25QXX */
uint8_t w25qxx_read_sr(uint8_t regno);                              /* 读W25QXX的状态寄存器 */
void w25qxx_write_sr(uint8_t regno, uint8_t sr);                    /* 写W25QXX的状态寄存器 */
uint16_t w25qxx_read_id(void);                                      /* 读W25QXX芯片ID */
void w25qxx_read(uint8_t *pbuf, uint32_t addr, uint16_t datalen);   /* 读W25QXX */
void w25qxx_write(uint8_t *pbuf, uint32_t addr, uint16_t datalen);  /* 写W25QXX */
void w25qxx_erase_chip(void);                                       /* 擦除整个W25QXX芯片 */
void w25qxx_erase_sector(uint32_t saddr);                           /* 擦除W25QXX一个扇区 */

#endif
