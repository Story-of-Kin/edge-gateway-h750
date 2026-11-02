/*
 * 把网关接口映射到北极星H750硬件。
 * USART1=调试，USART2 PA2/PA3=板载RS485，USART3 PB10/PB11=ESP8266。
 * 数据Flash是SPI2上的W25Q256（32MB），与0x90000000处执行代码的QSPI Flash不同。
 * Flash读写本身不带互斥，调度开始后由应用层g_storage_mutex串行保护。
 */
#include "gateway_board.h"
#include "gateway_protocol.h"
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/MPU/mpu.h"
#include "./BSP/SDRAM/sdram.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/SPI/spi.h"
#include "./BSP/PCF8574/pcf8574.h"
#include <string.h>
static UART_HandleTypeDef wifi_uart, bus_uart;
static IWDG_HandleTypeDef watchdog;
static uint8_t rx_byte;
/* 环形缓冲用两个游标区分写入/读取位置；保留一个空位来区分满和空。 */
static volatile uint16_t rx_head, rx_tail;
static uint8_t rx_ring[2048];
static int flash_ready;
extern SPI_HandleTypeDef g_spi2_handle;
/* SPI2数据Flash片选PF10：低电平选中，高电平结束本次命令。 */
static void cs(int high)
{
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
/* 带100ms超时的SPI发送，把HAL状态转换为统一的0/-1。 */
static int spi_send(const uint8_t *p, uint16_t n)
{
    return HAL_SPI_Transmit(&g_spi2_handle, (uint8_t *)p, n, 100) == HAL_OK ? 0 : -1;
}
/* 带100ms超时的SPI接收；长度受HAL的16位参数限制。 */
static int spi_recv(uint8_t *p, uint16_t n)
{
    return HAL_SPI_Receive(&g_spi2_handle, p, n, 100) == HAL_OK ? 0 : -1;
}
/* 调度器运行时使用任务延时；启动阶段使用HAL延时。 */
static void pause_ms(uint32_t n)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        vTaskDelay(pdMS_TO_TICKS(n));
    else
        HAL_Delay(n);
}
/* 反复读取状态寄存器1（命令05），等待BUSY位清零；超过timeout毫秒返回错误。 */
static int ready(uint32_t timeout)
{
    uint32_t start = HAL_GetTick();
    uint8_t cmd = 5, status;
    int r;
    do
    {
        cs(0);
        r = spi_send(&cmd, 1);
        if (!r)
            r = spi_recv(&status, 1);
        cs(1);
        if (r)
            return -1;
        if (!(status & 1))
            return 0;
        pause_ms(1);
    } while (HAL_GetTick() - start < timeout);
    return -1;
}
/* 发送06写使能命令；每次页编程或扇区擦除前都要重新使能。 */
static int write_enable(void)
{
    uint8_t cmd = 6;
    int r;
    cs(0);
    r = spi_send(&cmd, 1);
    cs(1);
    return r;
}
/* 把所有数据操作限制在24MB至32MB之间，长度检查采用减法防止地址回卷。 */
static int in_data(uint32_t a, uint32_t n)
{
    return flash_ready && a >= GW_DATA_START && a < GW_DATA_END && n <= GW_DATA_END - a;
}
/* 命令13加四字节地址读取，支持超过16MB的地址，不切换芯片全局寻址模式。 */
static int flash_read(void *u, uint32_t a, void *out, uint32_t n)
{
    uint8_t cmd[5] = {0x13, (uint8_t)(a >> 24), (uint8_t)(a >> 16), (uint8_t)(a >> 8), (uint8_t)a};
    int r;
    (void)u;
    if (!out || !in_data(a, n) || n > 65535u)
        return -1;
    cs(0);
    r = spi_send(cmd, 5);
    if (!r)
        r = spi_recv(out, (uint16_t)n);
    cs(1);
    return r;
}
/*
 * 命令12加四字节地址编程；按256字节页边界拆分，防止页内地址回卷。
 * 写之前上层需确保目标位允许1变0；本函数不会自动擦扇区。
 */
static int flash_write(void *u, uint32_t a, const void *data, uint32_t n)
{
    const uint8_t *p = data;
    uint32_t k;
    uint8_t cmd[5];
    int r;
    (void)u;
    if (!data || !in_data(a, n))
        return -1;
    /* 先算当前页还剩多少字节，每次写入不得跨过256字节页边界。 */
    while (n)
    {
        k = 256 - (a & 255);
        if (k > n)
            k = n;
        if (ready(1000) || write_enable())
            return -1;
        cmd[0] = 0x12;
        cmd[1] = (uint8_t)(a >> 24);
        cmd[2] = (uint8_t)(a >> 16);
        cmd[3] = (uint8_t)(a >> 8);
        cmd[4] = (uint8_t)a;
        cs(0);
        r = spi_send(cmd, 5);
        if (!r)
            r = spi_send(p, (uint16_t)k);
        cs(1);
        if (r || ready(1000))
            return -1;
        a += k;
        p += k;
        n -= k;
    }
    return 0;
}
/* 命令21擦除一个4KB扇区；地址必须4KB对齐且完整落在允许的数据分区。 */
static int flash_erase(void *u, uint32_t a)
{
    uint8_t cmd[5] = {0x21, (uint8_t)(a >> 24), (uint8_t)(a >> 16), (uint8_t)(a >> 8), (uint8_t)a};
    int r;
    (void)u;
    if (a % 4096 || !in_data(a, 4096) || ready(1000) || write_enable())
        return -1;
    cs(0);
    r = spi_send(cmd, 5);
    cs(1);
    if (r)
        return r;
    return ready(5000);
}
/* 函数指针表使上层存储代码同时能连接实际SPI驱动和主机测试的Flash模型。 */
const gw_flash_t gateway_flash = {flash_read, flash_write, flash_erase, 0};
/* 配置PF10/SPI2并读取9F JEDEC ID；当前接受Winbond厂商EF、容量代码19的器件。 */
int gateway_flash_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t cmd = 0x9f, id[3] = {0};
    int r;
    __HAL_RCC_GPIOF_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOF, &gpio);
    cs(1);
    spi2_init();
    spi2_set_speed(SPI_BAUDRATEPRESCALER_8);
    cs(0);
    r = spi_send(&cmd, 1);
    if (!r)
        r = spi_recv(id, 3);
    cs(1);
    g_gateway_diag.flash_id = ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
    flash_ready = !r && id[0] == 0xef && id[2] == 0x19;
    return flash_ready ? 0 : -1;
}
/*
 * 先检查专用扇区是否已有本项目标识；若已有，则只读验证。
 * 若扇区全FF，执行一次擦写读回；若有其他内容，跳过以保留未知数据。
 */
int gateway_flash_selftest(void)
{
    static const uint8_t pattern[] = "H750-GATEWAY-DATA-FLASH-V1";
    uint8_t p[sizeof(pattern)];
    uint8_t block[256];
    uint32_t i, j;
    int blank = 1;
    if (flash_read(0, GW_SELFTEST_SECTOR, p, sizeof(p)))
        return -1;
    if (!memcmp(p, pattern, sizeof(p)))
        return 0;
    for (i = 0; i < 4096; i += sizeof(block))
    {
        if (flash_read(0, GW_SELFTEST_SECTOR + i, block, sizeof(block)))
            return -1;
        for (j = 0; j < sizeof(block); j++)
            if (block[j] != 255)
                blank = 0;
    }
    /* 专用扇区若已有未知内容则保留，禁止为自检直接覆盖。 */
    if (!blank)
        return 1;
    if (flash_erase(0, GW_SELFTEST_SECTOR) ||
        flash_write(0, GW_SELFTEST_SECTOR, pattern, sizeof(pattern)) ||
        flash_read(0, GW_SELFTEST_SECTOR, p, sizeof(p)))
        return -1;
    return memcmp(p, pattern, sizeof(p)) ? -1 : 0;
}
/* HAL初始化UART时调用的硬件准备：选AF7复用，配置USART2/3引脚。 */
void gateway_uart_msp_init(UART_HandleTypeDef *h)
{
    GPIO_InitTypeDef gpio = {0};
    if (h->Instance == USART2)
    {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
        gpio.Alternate = GPIO_AF7_USART2;
    }
    else if (h->Instance == USART3)
    {
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        gpio.Pin = GPIO_PIN_10 | GPIO_PIN_11;
        gpio.Alternate = GPIO_AF7_USART3;
    }
    else
        return;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(h->Instance == USART2 ? GPIOA : GPIOB, &gpio);
    /* UART3中断优先级数值6；回调不调用FreeRTOS API。不要与任务优先级数字混为一谈。 */
    if (h->Instance == USART3)
    {
        HAL_NVIC_SetPriority(USART3_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(USART3_IRQn);
    }
}
/* 通用UART参数：指定波特率、8数据位、无校验、1停止位、无硬件流控。 */
static void uart_init(UART_HandleTypeDef *h, USART_TypeDef *instance, uint32_t baud)
{
    h->Instance = instance;
    h->Init.BaudRate = baud;
    h->Init.WordLength = UART_WORDLENGTH_8B;
    h->Init.StopBits = UART_STOPBITS_1;
    h->Init.Parity = UART_PARITY_NONE;
    h->Init.Mode = UART_MODE_TX_RX;
    h->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    h->Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(h) != HAL_OK)
        gateway_panic(0xb001);
}
/* USART3单字节接收中断回调。中断只入队，不解析AT，也不进行耗时处理。 */
void gateway_uart_rx_complete(UART_HandleTypeDef *h)
{
    uint16_t next;
    if (h->Instance != USART3)
        return;
    /*
     * 容量2048是2的幂，&2047实现游标回卷；满时丢弃新字节并记录溢出次数。
     * __DMB保证先写字节、再发布新的写游标。
     */
    next = (rx_head + 1) & 2047;
    if (next == rx_tail)
        g_gateway_diag.rx_overflows++;
    else
    {
        rx_ring[rx_head] = rx_byte;
        __DMB();
        rx_head = next;
    }
    HAL_UART_Receive_IT(&wifi_uart, &rx_byte, 1);
}
/* USART3硬件中断入口，交给HAL判断并调用接收完成等回调。 */
void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&wifi_uart);
}
/* USART3同步发送，timeout=1000ms；协议的SEND OK需要网络层另行等待。 */
int gateway_wifi_send(const void *p, size_t n)
{
    if (!p || n > 65535)
        return -1;
    return HAL_UART_Transmit(&wifi_uart, (uint8_t *)p, (uint16_t)n, 1000) == HAL_OK ? 0 : -1;
}
/* 网络任务是环形缓冲唯一消费者，UART中断是唯一生产者；读完推进rx_tail。 */
int gateway_wifi_read(uint8_t *p, size_t cap)
{
    size_t n = 0;
    if (!p)
        return -1;
    while (rx_tail != rx_head && n < cap)
    {
        p[n++] = rx_ring[rx_tail];
        rx_tail = (rx_tail + 1) & 2047;
    }
    return (int)n;
}
/* 短暂关闭中断后对齐读写游标，随后恢复原先中断开关状态。 */
void gateway_wifi_discard(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    rx_tail = rx_head;
    if (!mask)
        __enable_irq();
}
/*
 * 组装固定的03读取请求，经PCF8574切换RS485方向，接收并验证响应。
 * addr为从站地址，reg为起始寄存器，values至少容纳两个uint16_t。
 */
int gateway_modbus_read(uint8_t addr, uint16_t reg, uint16_t values[2])
{
    /*
     * 请求布局：[从站][03][起始地址高/低][数量高/低=2][CRC低/高]。
     * 正常响应为9字节；短异常响应在本接口中不能按异常码详细解释。
     */
    uint8_t request[8] = {addr, 3, (uint8_t)(reg >> 8), (uint8_t)reg, 0, 2, 0, 0}, response[9];
    uint16_t crc = gw_crc16(request, 6);
    HAL_StatusTypeDef status;
    request[6] = (uint8_t)crc;
    request[7] = (uint8_t)(crc >> 8);
    /* 清理上次残留接收数据和溢出标记，避免旧字节拼入本次响应。 */
    while (__HAL_UART_GET_FLAG(&bus_uart, UART_FLAG_RXNE))
        (void)bus_uart.Instance->RDR;
    __HAL_UART_CLEAR_OREFLAG(&bus_uart);
    /*
     * P6高=发送。HAL轮询发送返回后切回P6低=接收；最多等待200ms响应。
     * 这里是轮询等待，较高优先级RTOS任务仍可抢占；它不是异步Modbus事务队列。
     */
    pcf8574_write_bit(RS485_RE_IO, 1);
    status = HAL_UART_Transmit(&bus_uart, request, 8, 100);
    pcf8574_write_bit(RS485_RE_IO, 0);
    if (status != HAL_OK)
        return -1;
    status = HAL_UART_Receive(&bus_uart, response, 9, 200);
    if (status != HAL_OK)
        return -1;
    return gw_modbus_decode(response, 9, addr, 2, values, 2);
}
/* 在基础时钟/QSPI准备完成后调用，完成应用层使用的板载硬件初始化。 */
void gateway_board_init(void)
{
    /* MPU控制CPU对不同内存区域的访问属性；SDRAM帧缓冲与LTDC共享，设置为不可缓存。 */
    mpu_memory_protection();
    /* CPU与LTDC共享SDRAM，关闭该区CPU缓存，避免像素只停留在CPU缓存而屏幕读不到。 */
    mpu_set_protection(0xc0000000, MPU_REGION_SIZE_64MB, MPU_REGION_NUMBER6,
                       MPU_INSTRUCTION_ACCESS_DISABLE, MPU_REGION_FULL_ACCESS, MPU_ACCESS_SHAREABLE,
                       MPU_ACCESS_NOT_CACHEABLE, MPU_ACCESS_NOT_BUFFERABLE);
    usart_init(115200);
    led_init();
    sdram_init();
    /* 保持以太网PHY复位，避免其MDIO与USART2的PA2复用冲突；RS485默认接收。 */
    pcf8574_init();
    pcf8574_write_bit(ETH_RESET_IO, 0);
    pcf8574_write_bit(RS485_RE_IO, 0);
    uart_init(&bus_uart, USART2, 9600);
    uart_init(&wifi_uart, USART3, 115200);
    HAL_UART_Receive_IT(&wifi_uart, &rx_byte, 1);
}
/* 只切换背光；触摸仍由UI任务定期扫描，因此息屏后仍可检测唤醒。 */
void gateway_backlight(int enabled)
{
    LCD_BL(enabled);
}
/* 独立看门狗使用LSI时钟，分频256、重装4095；HAL初始化会启动计数。 */
void gateway_watchdog_start(void)
{
    watchdog.Instance = IWDG1;
    watchdog.Init.Prescaler = IWDG_PRESCALER_256;
    watchdog.Init.Reload = 4095;
    watchdog.Init.Window = 4095;
    HAL_IWDG_Init(&watchdog);
}
/* 刷新独立看门狗；调用条件由monitor任务决定。 */
void gateway_watchdog_feed(void)
{
    HAL_IWDG_Refresh(&watchdog);
}
