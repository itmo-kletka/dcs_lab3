#include "uart_drv.h"

static void uart_clock_on(USART_TypeDef *usart)
{
    if (usart == USART1) {
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
        (void)RCC->APB2ENR;
    } else if (usart == USART6) {
        RCC->APB2ENR |= RCC_APB2ENR_USART6EN;
        (void)RCC->APB2ENR;
    } else if (usart == USART2) {
        RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
        (void)RCC->APB1ENR;
    } else if (usart == USART3) {
        RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
        (void)RCC->APB1ENR;
    } else if (usart == UART4) {
        RCC->APB1ENR |= RCC_APB1ENR_UART4EN;
        (void)RCC->APB1ENR;
    } else if (usart == UART5) {
        RCC->APB1ENR |= RCC_APB1ENR_UART5EN;
        (void)RCC->APB1ENR;
    } else if (usart == UART7) {
        RCC->APB1ENR |= RCC_APB1ENR_UART7EN;
        (void)RCC->APB1ENR;
    } else if (usart == UART8) {
        RCC->APB1ENR |= RCC_APB1ENR_UART8EN;
        (void)RCC->APB1ENR;
    }
}

static uint32_t uart_bus_clock(USART_TypeDef *usart)
{
    uint32_t ppre;

    if (usart == USART1 || usart == USART6) {
        ppre = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    } else {
        ppre = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    }
    return SystemCoreClock >> APBPrescTable[ppre];
}

static int uart_irqn(USART_TypeDef *usart, IRQn_Type *irqn)
{
    if (usart == USART1) {
        *irqn = USART1_IRQn;
        return 1;
    }
    if (usart == USART2) {
        *irqn = USART2_IRQn;
        return 1;
    }
    if (usart == USART3) {
        *irqn = USART3_IRQn;
        return 1;
    }
    if (usart == UART4) {
        *irqn = UART4_IRQn;
        return 1;
    }
    if (usart == UART5) {
        *irqn = UART5_IRQn;
        return 1;
    }
    if (usart == USART6) {
        *irqn = USART6_IRQn;
        return 1;
    }
    if (usart == UART7) {
        *irqn = UART7_IRQn;
        return 1;
    }
    if (usart == UART8) {
        *irqn = UART8_IRQn;
        return 1;
    }
    return 0;
}

static void uart_setup_base(USART_TypeDef *usart, const uart_drv_config_t *cfg)
{
    uint32_t pclk;
    uint32_t cr1 = 0;
    uint32_t cr2 = 0;

    if (usart == 0 || cfg == 0 || cfg->baud == 0u) {
        return;
    }

    uart_clock_on(usart);

    usart->CR1 &= ~USART_CR1_UE;

    if (cfg->word_len == UART_DRV_WORDLEN_9B) {
        cr1 |= USART_CR1_M;
    }
    if (cfg->parity == UART_DRV_PARITY_EVEN) {
        cr1 |= USART_CR1_PCE;
    } else if (cfg->parity == UART_DRV_PARITY_ODD) {
        cr1 |= USART_CR1_PCE | USART_CR1_PS;
    }

    if (cfg->stop_bits == UART_DRV_STOPBITS_2) {
        cr2 |= USART_CR2_STOP_1;
    }

    pclk = uart_bus_clock(usart);
    usart->BRR = (pclk + (cfg->baud / 2u)) / cfg->baud;
    usart->CR2 = cr2;
    usart->CR3 = 0;
    usart->CR1 = cr1 | USART_CR1_TE | USART_CR1_RE;
}

void uart_drv_init_poll(USART_TypeDef *usart, const uart_drv_config_t *cfg)
{
    IRQn_Type irqn;

    if (usart == 0 || cfg == 0) {
        return;
    }

    if (uart_irqn(usart, &irqn)) {
        NVIC_DisableIRQ(irqn);
    }

    uart_setup_base(usart, cfg);
    usart->CR1 |= USART_CR1_UE;
}

void uart_drv_init_it(USART_TypeDef *usart, const uart_drv_config_t *cfg, uint32_t irq_prio)
{
    IRQn_Type irqn;

    if (usart == 0 || cfg == 0) {
        return;
    }

    uart_setup_base(usart, cfg);
    usart->CR1 |= USART_CR1_RXNEIE | USART_CR1_UE;

    if (uart_irqn(usart, &irqn)) {
        NVIC_SetPriority(irqn, irq_prio);
        NVIC_ClearPendingIRQ(irqn);
        NVIC_EnableIRQ(irqn);
    }
}

void uart_drv_set_it_mode(USART_TypeDef *usart, int on, uint32_t irq_prio)
{
    IRQn_Type irqn;

    if (usart == 0) {
        return;
    }

    if (!on) {
        usart->CR1 &= ~(USART_CR1_RXNEIE | USART_CR1_TXEIE | USART_CR1_TCIE | USART_CR1_PEIE);
        usart->CR3 &= ~USART_CR3_EIE;
        if (uart_irqn(usart, &irqn)) {
            NVIC_DisableIRQ(irqn);
        }
        return;
    }

    if (uart_irqn(usart, &irqn)) {
        NVIC_SetPriority(irqn, irq_prio);
        NVIC_ClearPendingIRQ(irqn);
        NVIC_EnableIRQ(irqn);
    }
    usart->CR1 |= USART_CR1_RXNEIE;
}

void uart_drv_set_txe_it(USART_TypeDef *usart, int on)
{
    if (usart == 0) {
        return;
    }
    if (on) {
        usart->CR1 |= USART_CR1_TXEIE;
    } else {
        usart->CR1 &= ~USART_CR1_TXEIE;
    }
}

int uart_drv_txe_it_enabled(USART_TypeDef *usart)
{
    if (usart == 0) {
        return 0;
    }
    return (usart->CR1 & USART_CR1_TXEIE) ? 1 : 0;
}

uint32_t uart_drv_get_status(USART_TypeDef *usart)
{
    if (usart == 0) {
        return 0;
    }
    return usart->SR & (UART_DRV_STAT_PE | UART_DRV_STAT_FE | UART_DRV_STAT_NE |
                        UART_DRV_STAT_ORE | UART_DRV_STAT_RXNE |
                        UART_DRV_STAT_TC | UART_DRV_STAT_TXE);
}

int uart_drv_rx_ready(USART_TypeDef *usart)
{
    if (usart == 0) {
        return 0;
    }
    return (usart->SR & USART_SR_RXNE) ? 1 : 0;
}

int uart_drv_tx_ready(USART_TypeDef *usart)
{
    if (usart == 0) {
        return 0;
    }
    return (usart->SR & USART_SR_TXE) ? 1 : 0;
}

int uart_drv_tx_complete(USART_TypeDef *usart)
{
    if (usart == 0) {
        return 0;
    }
    return (usart->SR & USART_SR_TC) ? 1 : 0;
}

int uart_drv_read_byte(USART_TypeDef *usart, uint8_t *byte)
{
    uint32_t sr;
    uint8_t dr;

    if (usart == 0 || byte == 0) {
        return 0;
    }

    sr = usart->SR;
    if (sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        dr = (uint8_t)(usart->DR & 0xFFu);
        if ((sr & USART_SR_RXNE) && !(sr & (USART_SR_FE | USART_SR_NE | USART_SR_PE))) {
            *byte = dr;
            return 1;
        }
    }
    return 0;
}

int uart_drv_write_byte(USART_TypeDef *usart, uint8_t byte)
{
    if (usart == 0) {
        return 0;
    }
    if (!(usart->SR & USART_SR_TXE)) {
        return 0;
    }
    usart->DR = byte;
    return 1;
}
