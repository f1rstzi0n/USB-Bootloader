#include "main.h"
extern TIM_HandleTypeDef htim7;
void NMI_Handler(void) { for (;;) {} }
void HardFault_Handler(void) { for (;;) {} }
void MemManage_Handler(void) { for (;;) {} }
void BusFault_Handler(void) { for (;;) {} }
void UsageFault_Handler(void) { for (;;) {} }
void DebugMon_Handler(void) {}
void SysTick_Handler(void) {}
void TIM7_IRQHandler(void) { HAL_TIM_IRQHandler(&htim7); }
