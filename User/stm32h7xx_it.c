#include "gateway.h"
extern void xPortSysTickHandler(void);
void NMI_Handler(void){}
void HardFault_Handler(void){gateway_panic(0xf001);}
void MemManage_Handler(void){gateway_panic(0xf002);}
void BusFault_Handler(void){gateway_panic(0xf003);}
void UsageFault_Handler(void){gateway_panic(0xf004);}
void DebugMon_Handler(void){}
void SysTick_Handler(void){HAL_IncTick();if(xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)xPortSysTickHandler();}
