/*
 * uart_drv.h
 *
 *  Created on: 4 Sep 2021
 *      Author: rho
 */

#ifndef UART_DRV_H_
#define UART_DRV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

#include <stdint.h>
#include <stdbool.h>

#define UART_MAX_TX_SIZE       256
#define UART_MAX_RX_SIZE       256

#define UART_DEFAULT_TIMEOUT    500

void uart_init(USART_TypeDef * instance, uint32_t baud_rate);

uint16_t uart_write_byte(USART_TypeDef * instance, uint8_t c);
uint16_t uart_write(USART_TypeDef * instance, const uint8_t * bytes, uint16_t len);

int16_t uart_read_byte(USART_TypeDef * instance);
uint16_t uart_read(USART_TypeDef * instance, uint8_t * bytes, uint16_t len);

uint16_t uart_avail(USART_TypeDef * instance);
void uart_flush(USART_TypeDef * instance);
void uart_set_timeout(USART_TypeDef * instance, uint16_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* UART_DRV_H_ */
