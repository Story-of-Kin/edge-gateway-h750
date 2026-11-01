#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H
#include <stdint.h>
extern uint32_t SystemCoreClock;
void gateway_panic(uint32_t code);
#define configUSE_PREEMPTION 1
#define configCPU_CLOCK_HZ SystemCoreClock
#define configTICK_RATE_HZ 1000u
#define configMAX_PRIORITIES 8
#define configMINIMAL_STACK_SIZE 128
#define configTOTAL_HEAP_SIZE (96u*1024u)
#define configMAX_TASK_NAME_LEN 16
#define configUSE_16_BIT_TICKS 0
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 0
#define configUSE_COUNTING_SEMAPHORES 1
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 0
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK 1
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY 2
#define configTIMER_QUEUE_LENGTH 8
#define configTIMER_TASK_STACK_DEPTH 256
#define configUSE_TRACE_FACILITY 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configPRIO_BITS 4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY (15<<4)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5<<4)
#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define configASSERT(x) do{if(!(x))gateway_panic(0xa001);}while(0)
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#endif
