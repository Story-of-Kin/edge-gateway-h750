/**
 ****************************************************************************************************
 * @file        pcf8574.c
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

#include "./BSP/PCF8574/pcf8574.h"
#include "./BSP/IIC/myiic.h"

/**
 * @brief   PCF8574在位检测
 * @param   无
 * @retval  检测结果
 * @arg     0: 成功
 * @arg     1: 失败
 */
static uint8_t pcf8574_check(void)
{
    uint8_t active;
    
    iic_start();
    iic_send_byte((PCF8574_IIC_ADDR << 1) | 0x00);
    active = iic_wait_ack();
    iic_stop();
    
    return active;
}

/**
 * @brief   初始化PCF8574
 * @param   无
 * @retval  初始化结果
 * @arg     0: 成功
 * @arg     1: 失败
 */
uint8_t pcf8574_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};
    uint8_t active;
    
    /* 使能时钟 */
    PCF8574_INT_GPIO_CLK_ENABLE();
    
    /* 配置INT引脚 */
    gpio_init_struct.Pin = PCF8574_INT_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(PCF8574_INT_GPIO_PORT, &gpio_init_struct);
    
    /* 初始化IIC */
    iic_init();
    
    /* 在位检测 */
    active = pcf8574_check();
    
    /* 所有端口默认输出高电平 */
    pcf8574_write_byte(0xFF);
    
    return active;
}

/**
 * @brief   读取PCF8574 8位端口数据
 * @param   无
 * @retval  8位端口数据
 */
uint8_t pcf8574_read_byte(void)
{
    uint8_t byte;
    
    iic_start();
    iic_send_byte((PCF8574_IIC_ADDR << 1) | 0x01);
    iic_wait_ack();
    byte = iic_read_byte(0);
    iic_stop();
    
    return byte;
}

/**
 * @brief   写入PCF8574 8位端口数据
 * @param   byte: 8位端口数据
 * @retval  无
 */
void pcf8574_write_byte(uint8_t byte)
{
    iic_start();
    iic_send_byte((PCF8574_IIC_ADDR << 1) | 0x00);
    iic_wait_ack();
    iic_send_byte(byte);
    iic_wait_ack();
    iic_stop();
}

/**
 * @brief   读取PCF8574某一端口数据
 * @param   bits: 指定端口
 * @retval  端口数据
 */
uint8_t pcf8574_read_bit(uint8_t bits)
{
    uint8_t byte;
    
    byte = pcf8574_read_byte();
    if ((byte & (1 << bits)) != 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief   写入PCF8574某一端口数据
 * @param   bits: 指定端口
 * @param   bit: 数据
 * @retval  无
 */
void pcf8574_write_bit(uint8_t bits, uint8_t bit)
{
    uint8_t byte;
    byte = pcf8574_read_byte();
    if (bit == 0)
    {
        byte &= ~(1 << bits);
    }
    else
    {
        byte |= (1 << bits);
    }
    pcf8574_write_byte(byte);
}
