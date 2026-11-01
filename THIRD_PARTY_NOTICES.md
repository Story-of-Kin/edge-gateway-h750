# 第三方来源与许可

本仓库不是全部原创，也不是对所有文件统一重新许可。保留文件原始头部、版权和许可；根目录 MIT LICENSE 为上游网关项目的声明。

| 部分 | 来源 / 版本 | 说明 |
| --- | --- | --- |
| 网关原始功能、中文字体 | [zgd-dev/-V2.0](https://github.com/zgd-dev/-V2.0)，参考提交 40b4460 | 原始项目 Copyright (c) 2026 zgd，MIT，保留根 LICENSE；未复制上游硬编码 Wi-Fi/云端凭据或其 Git 历史 |
| FreeRTOS | 上游所带 Kernel 10.3.1 | Amazon MIT 声明见各源文件和 `Middlewares/FreeRTOS/LICENSE.txt`；ARMCC RVDS FPU 端口 |
| LVGL | 上游所带 8.3.11 | MIT，见 `Middlewares/LVGL/LICENCE.txt`；本次改动配置、HAL tick 引入和屏幕适配 |
| ST STM32H7 HAL / CMSIS Device | 北极星 H750 HAL 配套例程 | 按各文件头和相关 ST 许可适用；不改变 ST 版权声明 |
| Arm CMSIS Core | 同上 | 依文件头 Apache-2.0 等原有条款适用 |
| ALIENTEK BSP/SYSTEM/启动与 scatter | 正点原子北极星 H750 HAL 触摸屏例程，另取 SPI、PCF8574 驱动 | Copyright 广州市星翼电子科技有限公司 / ALIENTEK；保留声明。配套资料未提供覆盖所有 BSP 的独立 MIT 授权，**不主张其属于 MIT**。进一步商用/再分发请核对板厂授权，官方资料见下方 |

板厂资料入口：https://www.openedv.com/docs/boards/stm32/zdyz_stm32h750_polaris.html

本次板级改动主要为中断路由、MPU 一致性、QSPI 工程配置；新 App/Port 中的协议、记录存储、任务及界面适配保留上游功能来源说明。外部 Flash 下载算法、Keil 安装程序/许可文件、传感器手册和用户账号资料均不随仓库分发。
