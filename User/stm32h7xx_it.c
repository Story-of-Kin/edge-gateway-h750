/*
 * 异常和系统滴答入口。硬件按向量表跳入这些函数，函数名必须与启动文件一致。
 * SVC/PendSV处理由FreeRTOS端口提供；USART3入口在Port/gateway_board.c中。
 */
#include "gateway.h"
extern void xPortSysTickHandler(void);
/* 不可屏蔽中断入口，本工程未增加专用处理。 */
void NMI_Handler(void)
{
}
/* 硬件严重异常：记录0xF001，调试时可进一步检查堆栈和故障状态寄存器。 */
void HardFault_Handler(void)
{
    gateway_panic(0xf001);
}
/* 内存访问保护异常，例如违反MPU访问权限。 */
void MemManage_Handler(void)
{
    gateway_panic(0xf002);
}
/* 总线访问异常，例如访问不可用外设/内存。 */
void BusFault_Handler(void)
{
    gateway_panic(0xf003);
}
/* 用法异常，例如非法指令或某些非法执行状态。 */
void UsageFault_Handler(void)
{
    gateway_panic(0xf004);
}
/* 调试监视异常入口，本工程未增加处理。 */
void DebugMon_Handler(void)
{
}
/*
 * 每1ms到达一次：先维护HAL的毫秒计数，调度器启动后再维护FreeRTOS节拍。
 * 调度器启动前不能直接调用RTOS节拍处理，因为任务相关结构还未就绪。
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xPortSysTickHandler();
}
