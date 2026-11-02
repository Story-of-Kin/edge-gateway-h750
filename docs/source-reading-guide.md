# 中文注释版源码阅读入口

本次为移植后新增/修改的核心源码、头文件和测试补充中文注释，并展开原本压缩在一行的代码。程序的非注释标记与注释前的 `79ffbab` 版本一致；功能改动不在本次范围内。

## 建议按这个顺序打开

| 顺序 | 文件 | 先弄懂什么 |
| --- | --- | --- |
| 1 | [User/main.c](../User/main.c) | 上电后先准备硬件，再创建任务并启动调度 |
| 2 | [App/gateway.h](../App/gateway.h) | 一份采集数据包含哪些字段，任务共享哪些资源，诊断状态怎么读 |
| 3 | [App/gateway_tasks.c](../App/gateway_tasks.c) | UI、采集、健康监控各自如何循环，什么时候延时、加锁或更新心跳 |
| 4 | [config/gateway_config.example.h](../config/gateway_config.example.h) | 从站地址、寄存器偏移、倍率、字序、Wi-Fi和Flash分区 |
| 5 | [Port/gateway_board.c](../Port/gateway_board.c) | UART/SPI/GPIO如何对应板上接口，RS485收发方向如何控制 |
| 6 | [App/gateway_protocol.c](../App/gateway_protocol.c) | CRC、Modbus正常响应、原始寄存器值与工程值的转换 |
| 7 | [App/gateway_ui.c](../App/gateway_ui.c) 和 [Port/lv_port_h750.c](../Port/lv_port_h750.c) | 数据如何变成标签，再变成RGB565像素输出到屏幕 |
| 8 | [App/gateway_network.c](../App/gateway_network.c) 和 [App/gateway_at.c](../App/gateway_at.c) | AT指令、TCP载荷和MQTT报文是三层不同概念 |
| 9 | [App/gateway_store.c](../App/gateway_store.c) | Flash记录怎么提交、恢复、读出和确认，为什么读出不立即删除 |
| 10 | [User/FreeRTOSConfig.h](../User/FreeRTOSConfig.h) 和 [User/stm32h7xx_it.c](../User/stm32h7xx_it.c) | 任务栈、节拍、优先级、异常入口和看门狗健康条件 |

先读每个文件顶部的中文说明，再读目标函数前的说明，最后沿函数体里的步骤注释往下走。一次只跟一条数据链路，例如“采集任务→RS485读取→CRC校验→共享快照→UI更新”。

## 代码中常见写法

- `.h` 是公共声明和类型定义，`.c` 是实现；先看头文件能了解模块对外提供什么。
- `gw_sample_t` 是自定义结构体类型；结构体把同一份样本的数值和有效标志放在一起。
- `&sample` 表示取得变量地址；函数参数里的 `gw_sample_t *p` 表示接收这种地址。
- `extern` 表示变量在别处定义；`static` 用在函数上会限制可见范围，用在局部变量上可跨函数调用保留值。
- `if (!gateway_flash_init())` 中 `!0` 为真：这个接口返回0表示成功，负数表示失败。不同接口返回约定可能不同，要看函数注释。
- `for (;;)` 是永久循环；任务通常在循环里用延时或阻塞等待让出CPU，避免一直空转。
- `(void)arg` 表示有意不使用这个参数，帮助编译器避免未使用参数警告。
- `pdMS_TO_TICKS(100)` 把100毫秒转换成FreeRTOS节拍数，不能把所有API的参数都想当然地当成毫秒。
- 任务优先级数字越大越高；STM32 NVIC中断优先级数字越小越高，两者不能直接比较。
- `taskENTER_CRITICAL` 保护非常短的共享数据复制；互斥锁用于协调任务访问共享Flash。二者都不是随意包住所有代码的工具。

## 注释覆盖与原始文件编码

本次覆盖 App、Port 的功能代码/接口，main、FreeRTOS配置、异常入口、网关配置、测试场景，以及板厂串口文件中移植的两个回调转发点。ST HAL、FreeRTOS内核、LVGL内部实现及大部分板厂BSP保留原来的注释和版权声明；中文字体数组只增加用途说明。

新增应用代码与本说明使用UTF-8；板厂 `Drivers/SYSTEM/usart/usart.c` 沿用原有GBK编码。若某个编辑器把板厂中文显示为乱码，先按GBK重新打开，不要直接覆盖保存成乱码文本。

## 验证与使用

- 25个修改过的C/头文件与基准版本的非注释代码标记逐项一致，确认只调整注释和格式。
- 使用 `python tests/test_core.py` 运行现有六组生产代码测试，再使用 `scripts/build.ps1` 完整重建。
- 当前Release仍保留原先已上板验证的固件包；注释版源码从仓库当前分支获取，或在本机重新编译。
- 硬件实测范围和缺少外设的限制继续以 [验证记录](verification.md) 为准。
