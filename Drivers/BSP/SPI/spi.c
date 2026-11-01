/**
 ****************************************************************************************************
 * @file        spi.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-06-12
 * @brief       SPI驱动代码
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

#include "./BSP/SPI/spi.h"

/* SPI句柄 */
SPI_HandleTypeDef g_spi2_handle = {0};

/**
 * @brief   初始化SPI2
 * @param   无
 * @retval  无
 */
void spi2_init(void)
{
    /* 配置SPI2 */
    g_spi2_handle.Instance = SPI2_SPI;
    g_spi2_handle.Init.Mode = SPI_MODE_MASTER;                                                  /* 工作模式 */
    g_spi2_handle.Init.Direction = SPI_DIRECTION_2LINES;                                        /* 通信模式 */
    g_spi2_handle.Init.DataSize = SPI_DATASIZE_8BIT;                                            /* 数据帧大小 */
    g_spi2_handle.Init.CLKPolarity = SPI_POLARITY_LOW;                                          /* 时钟极性 */
    g_spi2_handle.Init.CLKPhase = SPI_PHASE_1EDGE;                                              /* 时钟相位 */
    g_spi2_handle.Init.NSS = SPI_NSS_SOFT;                                                      /* 片选信号 */
    g_spi2_handle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;                           /* 波特率分频系数 */
    g_spi2_handle.Init.FirstBit = SPI_FIRSTBIT_MSB;                                             /* MSB或LSB传输 */
    g_spi2_handle.Init.TIMode = SPI_TIMODE_DISABLE;                                             /* TI模式 */
    g_spi2_handle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;                             /* CRC校验 */
    g_spi2_handle.Init.CRCPolynomial = 0;                                                       /* CRC多项式 */
    g_spi2_handle.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;                                     /* CRC长度 */
    g_spi2_handle.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;                                        /* NSSP信号 */
    g_spi2_handle.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;                                      /* NSSP极性 */
    g_spi2_handle.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;                               /* FiFo阈值 */
    g_spi2_handle.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;    /* 发送CRC计算初始化模式 */
    g_spi2_handle.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;    /* 接收CRC计算初始化模式 */
    g_spi2_handle.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;                       /* 数据传输前延时 */
    g_spi2_handle.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;         /* 数据传输间延时 */
    g_spi2_handle.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;                 /* 自动管理连续传输 */
    g_spi2_handle.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;                     /* 禁用时保持IO状态 */
    g_spi2_handle.Init.IOSwap = SPI_IO_SWAP_DISABLE;                                            /* 交替MISO和MOSI引脚 */
    HAL_SPI_Init(&g_spi2_handle);
}

/**
 * @brief   HAL库SPI2初始化MSP函数
 * @param   无
 * @retval  无
 */
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef gpio_init_struct = {0};
    
    if (hspi->Instance == SPI2_SPI)
    {
        /* 配置时钟 */
        rcc_periph_clk_init_struct.PeriphClockSelection |= RCC_PERIPHCLK_SPI123;
        rcc_periph_clk_init_struct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL2;
        HAL_RCCEx_PeriphCLKConfig(&rcc_periph_clk_init_struct);
        
        /* 时钟使能 */
        SPI2_SPI_CLK_ENABLE();
        SPI2_SCK_GPIO_CLK_ENABLE();
        SPI2_MISO_GPIO_CLK_ENABLE();
        SPI2_MOSI_GPIO_CLK_ENABLE();
        
        /* 配置SCK引脚 */
        gpio_init_struct.Pin = SPI2_SCK_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init_struct.Alternate = SPI2_SCK_GPIO_AF;
        HAL_GPIO_Init(SPI2_SCK_GPIO_PORT, &gpio_init_struct);
        
        /* 配置MISO引脚 */
        gpio_init_struct.Pin = SPI2_MISO_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init_struct.Alternate = SPI2_MISO_GPIO_AF;
        HAL_GPIO_Init(SPI2_MISO_GPIO_PORT, &gpio_init_struct);
        
        /* 配置MOSI引脚 */
        gpio_init_struct.Pin = SPI2_MOSI_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init_struct.Alternate = SPI2_MOSI_GPIO_AF;
        HAL_GPIO_Init(SPI2_MOSI_GPIO_PORT, &gpio_init_struct);
    }
}

/**
 * @brief   设置SPI2通信波特率
 * @param   speed: SPI2波特率分频系数
 * @arg     SPI_BAUDRATEPRESCALER_2: 2分频
 * @arg     SPI_BAUDRATEPRESCALER_4: 2分频
 * @arg     SPI_BAUDRATEPRESCALER_8: 8分频
 * @arg     SPI_BAUDRATEPRESCALER_16: 16分频
 * @arg     SPI_BAUDRATEPRESCALER_32: 32分频
 * @arg     SPI_BAUDRATEPRESCALER_64: 64分频
 * @arg     SPI_BAUDRATEPRESCALER_128: 128分频
 * @arg     SPI_BAUDRATEPRESCALER_256: 256分频
 * @retval  无
 */
void spi2_set_speed(uint32_t speed)
{
    __HAL_SPI_DISABLE(&g_spi2_handle);
    g_spi2_handle.Instance->CFG1 &= ~SPI_CFG1_MBR_Msk;
    g_spi2_handle.Instance->CFG1 |= speed;
    __HAL_SPI_ENABLE(&g_spi2_handle);
}
/**
 * @brief   SPI2读写一字节数据
 * @param   txdata: 待写入的一字节数据
 * @retval  读取到的一字节数据
 */
uint8_t spi2_read_write_byte(uint8_t txdata)
{
    uint8_t rxdata;
    
    if (HAL_SPI_TransmitReceive(&g_spi2_handle, &txdata, &rxdata, 1, 1000) != HAL_OK)
    {
        return 0;
    }
    
    return rxdata;
}
