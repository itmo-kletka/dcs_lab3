#include "stm32f4xx_hal.h"
#include "uart.h"

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void USART6_IRQHandler(void)
{
    uart_irq_handler();
}
