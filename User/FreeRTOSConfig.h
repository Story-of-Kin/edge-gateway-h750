/*
 * FreeRTOS编译配置：这些宏控制内核保留哪些能力，以及节拍、堆和中断约束。
 * 任务优先级与NVIC中断优先级是两套数值：任务数字大优先，NVIC数字小优先。
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H
#include <stdint.h>
extern uint32_t SystemCoreClock;
void gateway_panic(uint32_t code);
/* 抢占式调度：有更高优先级就绪任务时，可切换执行它。 */
#define configUSE_PREEMPTION 1
#define configCPU_CLOCK_HZ SystemCoreClock
/* 1000Hz节拍，即1ms一次；pdMS_TO_TICKS用于把毫秒换成内核节拍数。 */
#define configTICK_RATE_HZ 1000u
/* 可用任务优先级为0..7；空闲任务为0，main.c中业务任务使用2、3、4。 */
#define configMAX_PRIORITIES 8
/* 最小任务栈以StackType_t为单位，此端口通常每单位4字节，不等于字节数。 */
#define configMINIMAL_STACK_SIZE 128
/* 内核动态堆96KB，用于任务栈、队列等；LVGL另有自己的96KB内存池。 */
#define configTOTAL_HEAP_SIZE (96u * 1024u)
#define configMAX_TASK_NAME_LEN 16
/* 0表示使用32位节拍计数；计时差采用无符号减法应对正常回卷。 */
#define configUSE_16_BIT_TICKS 0
/* 开启互斥锁，供多个任务共享Flash等资源；互斥锁具有优先级继承机制。 */
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 0
#define configUSE_COUNTING_SEMAPHORES 1
/* 本工程用xTaskCreate等动态分配接口；没有启用静态对象分配接口。 */
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 0
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
/* 启用栈溢出检查与内存分配失败钩子，触发后统一记录致命错误码。 */
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK 1
/* 开启软件定时器服务任务；LVGL自己的定时处理仍由UI任务调用。 */
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY 2
#define configTIMER_QUEUE_LENGTH 8
#define configTIMER_TASK_STACK_DEPTH 256
#define configUSE_TRACE_FACILITY 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
/*
 * H750使用4个中断优先级位。调用FreeRTOS FromISR接口的中断，优先级数值须>=5。
 * 下方左移4位的宏把逻辑优先级放到NVIC寄存器有效高位。
 */
#define configPRIO_BITS 4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY (15 << 4)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5 << 4)
/* 这些INCLUDE宏决定是否编译相应API，设1后任务才能使用对应功能。 */
#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
/* 内核断言失败进入统一故障处理；保留错误码以供SWD检查。 */
#define configASSERT(x)                                                                            \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
            gateway_panic(0xa001);                                                                 \
    } while (0)
/* 把FreeRTOS端口函数名映射到启动文件要求的中断入口名，避免重复定义。 */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#endif
