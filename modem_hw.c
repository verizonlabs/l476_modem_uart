/* Copyright (c) 2017 Verizon. All rights reserved. */
#include <stm32l4xx_hal.h>

#include "modem_hw.h"

/* Settings for Sequans Monarch chipset (Nimbelink CAT-M1 board) */
#define MODEM_UART_INST			USART1
#define MODEM_UART_BAUD			921600
#define MODEM_UART_CLK_ENABLE()		__HAL_RCC_USART1_CLK_ENABLE()
#define MODEM_UART_IRQ_PRIORITY		5
#define MODEM_UART_IRQn			USART1_IRQn

#define MODEM_TX_CLK_ENABLE()		__HAL_RCC_GPIOA_CLK_ENABLE();
#define MODEM_TX_AF			GPIO_AF7_USART1
#define MODEM_TX_PORT			GPIOA
#define MODEM_TX_PIN			GPIO_PIN_9
#define MODEM_RX_CLK_ENABLE()		__HAL_RCC_GPIOA_CLK_ENABLE();
#define MODEM_RX_AF			GPIO_AF7_USART1
#define MODEM_RX_PORT			GPIOA
#define MODEM_RX_PIN			GPIO_PIN_10
#define MODEM_RTS_CLK_ENABLE()		__HAL_RCC_GPIOB_CLK_ENABLE();
#define MODEM_RTS_PORT			GPIOB
#define MODEM_RTS_PIN			GPIO_PIN_5
#define MODEM_CTS_CLK_ENABLE()		__HAL_RCC_GPIOB_CLK_ENABLE();
#define MODEM_CTS_PORT			GPIOB
#define MODEM_CTS_PIN			GPIO_PIN_3

#define MODEM_RESET_CLK_ENABLE()	__HAL_RCC_GPIOB_CLK_ENABLE();
#define MODEM_RESET_PORT		GPIOB
#define MODEM_RESET_PIN			GPIO_PIN_4

#define MODEM_PWR_EN_DELAY_MS		1000
#define MODEM_RESET_DELAY_MS		1000
#define MODEM_CTS_TIMEOUT_MS		1500

#define TIMEOUT_MS			5000
/* End of modem hardware configuration. */

extern void sys_delay(uint32_t delay_ms);
extern uint64_t sys_get_tick_ms(void);

static rw_port_tx_cb modem_tx_cb;
static rw_port_rx_cb modem_rx_cb;
static UART_HandleTypeDef modem_uart;
static GPIO_InitTypeDef reset_gpio;
static GPIO_InitTypeDef rts_gpio;
static GPIO_InitTypeDef cts_gpio;

void USART1_IRQHandler(void)
{
	volatile USART_TypeDef *uart_instance =
		(volatile USART_TypeDef *)(MODEM_UART_INST);
	volatile uint32_t uart_sr_reg = uart_instance->ISR;

	if (uart_sr_reg & USART_ISR_PE)
		uart_instance->ICR |= USART_ICR_PECF;
	else if (uart_sr_reg & USART_ISR_FE)
		uart_instance->ICR |= USART_ICR_FECF;
	else if (uart_sr_reg & USART_ISR_NE)
		uart_instance->ICR |= USART_ICR_NCF;
	else if (uart_sr_reg & USART_ISR_ORE)
		uart_instance->ICR |= USART_ICR_ORECF;
	else {
		uint8_t data = uart_instance->RDR;
		__SEV();
		modem_rx_cb(1, &data);
	}
}

static void uart_pin_init(void)
{
	GPIO_InitTypeDef uart_gpio;

	MODEM_TX_CLK_ENABLE();
	uart_gpio.Pin = MODEM_TX_PIN;
	uart_gpio.Mode = GPIO_MODE_AF_PP;
	uart_gpio.Pull = GPIO_PULLUP;
	uart_gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	uart_gpio.Alternate = MODEM_TX_AF;
	HAL_GPIO_Init(MODEM_TX_PORT, &uart_gpio);

	MODEM_RX_CLK_ENABLE();
	uart_gpio.Pin = MODEM_RX_PIN;
	uart_gpio.Alternate = MODEM_RX_AF;
	HAL_GPIO_Init(MODEM_RX_PORT, &uart_gpio);

	MODEM_RTS_CLK_ENABLE();
	uart_gpio.Pin = MODEM_RTS_PIN;
	HAL_GPIO_Init(MODEM_RTS_PORT, &uart_gpio);

	MODEM_CTS_CLK_ENABLE();
	uart_gpio.Pin = MODEM_CTS_PIN;
	HAL_GPIO_Init(MODEM_CTS_PORT, &uart_gpio);

	MODEM_UART_CLK_ENABLE();
}

bool modem_port_init(rw_port_tx_cb tx_cb, rw_port_rx_cb rx_cb)
{
	if (rx_cb == NULL)
		return false;
	modem_tx_cb = tx_cb;
	modem_rx_cb = rx_cb;
	USART_TypeDef *uart_instance = (USART_TypeDef *)(MODEM_UART_INST);

	/* Initialize the emulated RTS / CTS lines */
	MODEM_RTS_CLK_ENABLE();
	rts_gpio.Pin = MODEM_RTS_PIN;
	rts_gpio.Mode = GPIO_MODE_OUTPUT_PP;
	rts_gpio.Pull = GPIO_NOPULL;
	rts_gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(MODEM_RTS_PORT, &rts_gpio);
	HAL_GPIO_WritePin(MODEM_RTS_PORT, MODEM_RTS_PIN, GPIO_PIN_RESET);

	MODEM_CTS_CLK_ENABLE();
	cts_gpio.Pin = MODEM_CTS_PIN;
	cts_gpio.Mode = GPIO_MODE_INPUT;
	cts_gpio.Pull = GPIO_NOPULL;
	cts_gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(MODEM_CTS_PORT, &cts_gpio);

	/* Initialize the serial port. */
	uart_pin_init();
	modem_uart.Instance = MODEM_UART_INST;
	modem_uart.Init.BaudRate = MODEM_UART_BAUD;
	modem_uart.Init.WordLength = UART_WORDLENGTH_8B;
	modem_uart.Init.StopBits = UART_STOPBITS_1;
	modem_uart.Init.Parity = UART_PARITY_NONE;
	modem_uart.Init.Mode = UART_MODE_TX_RX;
	modem_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	modem_uart.Init.OverSampling = UART_OVERSAMPLING_16;
	modem_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;

	if (HAL_UART_Init(&modem_uart) != HAL_OK)
		return false;

	/* Enable Error Interrupts: (Frame error, noise error, overrun error) */
	SET_BIT(uart_instance->CR3, USART_CR3_EIE);

	/* Enable the UART Parity Error and Data Register not empty Interrupts */
	SET_BIT(uart_instance->CR1, USART_CR1_PEIE | USART_CR1_RXNEIE);

	/* Enable interrupt */
	HAL_NVIC_SetPriority(MODEM_UART_IRQn, MODEM_UART_IRQ_PRIORITY, 0);
	HAL_NVIC_EnableIRQ(MODEM_UART_IRQn);
	return true;
}

void modem_port_tx(size_t sz, const uint8_t data[])
{
	if (data == NULL)
		return;

	/* Mark the controller busy. */
	HAL_GPIO_WritePin(MODEM_RTS_PORT, MODEM_RTS_PIN, GPIO_PIN_SET);

	/* Ensure the modem is ready to receive commands. */
	uint32_t start = sys_get_tick_ms();
	while (HAL_GPIO_ReadPin(MODEM_CTS_PORT, MODEM_CTS_PIN) == GPIO_PIN_SET) {
		uint32_t now = sys_get_tick_ms();
		if (now - start > MODEM_CTS_TIMEOUT_MS) {
			HAL_GPIO_WritePin(MODEM_RTS_PORT, MODEM_RTS_PIN, GPIO_PIN_RESET);
			return;
		}
		sys_delay(10);
	}

	HAL_StatusTypeDef s = HAL_UART_Transmit(&modem_uart, (uint8_t *)data,
			sz, TIMEOUT_MS);

	/* Mark the controller free. */
	HAL_GPIO_WritePin(MODEM_RTS_PORT, MODEM_RTS_PIN, GPIO_PIN_RESET);

	if (!modem_tx_cb)
		return;

	modem_tx_cb(s != HAL_OK ? 0 : sz, s != HAL_OK ? true : false);
}

void modem_hw_init(void)
{
	/* Minimum delay after power up. */
	sys_delay(MODEM_PWR_EN_DELAY_MS);

	/* Initialize the reset line. */
	MODEM_RESET_CLK_ENABLE();
	reset_gpio.Pin = MODEM_RESET_PIN;
	reset_gpio.Mode = GPIO_MODE_OUTPUT_PP;
	reset_gpio.Pull = GPIO_NOPULL;
	reset_gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
	HAL_GPIO_Init(MODEM_RESET_PORT, &reset_gpio);
	HAL_GPIO_WritePin(MODEM_RESET_PORT, MODEM_RESET_PIN, GPIO_PIN_RESET);
}

void modem_hw_reset(void)
{
	HAL_GPIO_WritePin(MODEM_RESET_PORT, MODEM_RESET_PIN, GPIO_PIN_SET);
	sys_delay(MODEM_RESET_DELAY_MS);
	HAL_GPIO_WritePin(MODEM_RESET_PORT, MODEM_RESET_PIN, GPIO_PIN_RESET);
}
