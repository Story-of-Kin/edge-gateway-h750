# 硬件、接线与配置

本次核对资料：北极星底板 `CNF750_H750 V1.4.pdf`、核心板 `STM32H750_CORE V1.2.pdf`、`北极星STM32H750硬件参考手册_V1.1.pdf`，以及板厂 HAL 触摸屏、SPI、RS485 例程。资料从[板厂官方入口](https://www.openedv.com/docs/boards/stm32/zdyz_stm32h750_polaris.html)获取。实物版本与资料不同应重新核对。

## 板上资源

| 功能 | 本固件使用资源 |
| --- | --- |
| RGB 屏/触摸 | 板厂 LCD/LTDC 与 GT9xxx/FT5206 自动识别 BSP，800×480 横屏 RGB565 |
| SDRAM | 0xC0000000 起、64MB；帧缓冲放此区域，MPU 设置为不可缓存，避免 CPU/LTDC 像素不一致 |
| 执行代码 | 片内启动 + 0x90000000 映射的独立 W25Q64 QSPI |
| 数据 Flash | W25Q256 32MB，SPI2 PB13 SCK/PB14 MISO/PB15 MOSI，PF10 CS；只访问最后 8MB |
| RS485 | USART2 PA2 TX/PA3 RX，板载 SP3485；PCF8574 P6 控制收发，高为发、低为收 |
| Wi-Fi | USART3 PB10 TX/PB11 RX，115200 8N1，3.3V 逻辑 |
| 调试输出 | USART1 PA9 TX/PA10 RX，115200 8N1；DAP 的 COM 口是否接到该 UART 取决于实际连线 |
| 以太网 | 禁用，PCF8574 P7 将 PHY 保持复位，释放与 UART2 共用的 PA2 |

不要把 SPI 数据存储移到 QSPI，否则会覆盖正在执行的代码。W25Q256 使用独立四字节操作码：0x13 读取、0x12 编程、0x21 4KB 擦除；没有整片擦除命令。数据 Flash 前 24MB 不擦写。

## RS485 跳线和传感器

断电接线。在底板 **P9** 插两组跳线：**3—5、4—6**。

- P9-3 `USART2_TX` → P9-5 `RS485_RX`。这里 RX 指收发器输入，连接 SP3485 **DI（4 脚）**。
- P9-4 `USART2_RX` ← P9-6 `RS485_TX`，来自 SP3485 **RO（1 脚）**。
- 不同时把 USART2 接到 RS232 路由。板上 RS232 电平不能接 ESP 或 MCU UART。

开发板 RS485 端子与两只传感器 A、B 对应连接，参考地按器件手册连接；传感器电源由匹配外部直流电源提供。不同厂商 A/B 标识可能存在差异，通信失败时先按说明书检查 D+/D−，不要靠软件反复改地址掩盖接线错误。

开发板已有 120Ω，另一只仅在总线另一端且原设备未带终端时安装。12V/24V 传感器供电不得接到开发板 3.3V/5V 排针。

## ESP8266

| 开发板 | ESP8266 AT 模块 |
| --- | --- |
| PB10 / USART3 TX | AT 串口 RX |
| PB11 / USART3 RX | AT 串口 TX |
| GND | GND |
| 符合模块要求的电源 | VCC/模块电源输入；裸芯片为 3.3V，带稳压板按其手册 |

优先使用核心板引出的 PB10/PB11 按信号直连。若通过底板 ATK MODULE/P10，先按板卡和所购模块引脚表确认转接路由；模块接口针序不能由外形推断，也不能与 RS232/GPS 路由同时占用。

模块须处于正常启动、EN 有效、已刷 AT 固件。部分 ESP8266 AT 固件把命令串口放在不同 GPIO，不能只看模块丝印 TX/RX。先用串口验证 `AT`、`ATE0`、`AT+CWMODE=1` 可用，再接本工程。正常模式为 CIPMUX=0、CIPMODE=0；固件通过 CIPSEND 长度和 `+IPD` 接收 MQTT。

## 传感器配置

当前通信固定为 Modbus RTU **9600 8N1**，功能码 03，每设备读两个连续寄存器。寄存器地址使用报文中的零基偏移；手册若写 40001/40002，需要核对其实际报文偏移，不能直接把 40002 填入起始地址。

| 本地配置宏 | 默认值 | 含义 |
| --- | --- | --- |
| `GW_TEMP_ADDRESS` / `GW_TEMP_REGISTER` | 1 / 0x0001 | 温湿度从站和起始寄存器 |
| `GW_TEMP_INDEX` / `GW_HUMI_INDEX` | 0 / 1 | 两个返回寄存器中温度/湿度的位置，范围 0 或 1 |
| `GW_TEMP_SWAP_BYTES` | 0 | 两个温湿度寄存器内部是否交换字节；0 不交换，1 交换 |
| `GW_TEMP_SCALE` / `GW_HUMI_SCALE` | 0.1f / 0.1f | 温度按有符号 16 位、湿度按无符号 16 位，再乘倍率 |
| `GW_LIGHT_ADDRESS` / `GW_LIGHT_REGISTER` | 2 / 0x0002 | 光照从站和起始寄存器 |
| `GW_LIGHT_LOW_WORD_FIRST` | 0 | 32 位光照低字是否在前；0 高字在前、1 低字在前 |
| `GW_LIGHT_SWAP_BYTES` / `GW_LIGHT_SCALE` | 0 / 1.0f | 光照每字内部换字节及倍率 |

仅返回单个寄存器、IEEE float、功能码 04 或非标准字段的传感器需要另外调整采集长度/解码，不应通过随意倍率当成已兼容。读取超时、地址/长度/CRC 不符不产生有效测量。

## 内存与时间基准

FreeRTOS 使用 ARMCC 的 ARM_CM4F 通用 FPU 端口；该实现支持当前实测的 M7 r1p1 内核（CPUID 0x411FC271），含对不支持的 M7 r0p0/r0p1 的检查。HAL 毫秒计时与 FreeRTOS 共用 SysTick，优先级 15；UART3 IRQ 为 6，优先级数值不小于最大可调用 RTOS API 的边界 5，并且回调不调用 RTOS API。不要直接改回 F4 的 DMA/寄存器配置。

UI 只有一个任务调用 LVGL；采集通过临界区快照共享数据，Flash 使用互斥锁。空闲约 10 秒关闭背光，触摸唤醒。看门狗仅在 UI/采集/网络心跳都未超时的条件下喂狗。
