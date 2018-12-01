#ifndef UART_DRV_H_
#define UART_DRV_H_

#ifdef __cplusplus
extern "C" {
#endif
    
#include "stm32f10x_conf.h"

#define UART_MAX_TX_SIZE       256
#define UART_MAX_RX_SIZE       256

#define UART_DEFAULT_TIMEOUT    500

typedef struct {
    USART_TypeDef * instance;
    volatile uint8_t ever_written;
    volatile uint8_t txi_enabled;
    uint16_t timeout;
    
    volatile uint16_t tx_head;
    volatile uint16_t tx_tail;
    
    volatile uint16_t rx_head;
    volatile uint16_t rx_tail;
    
    uint8_t tx_buf[UART_MAX_TX_SIZE];
    uint8_t rx_buf[UART_MAX_RX_SIZE];
} uart_t;

void uart_init(uart_t * huart, uint32_t baud);
uint16_t uart_write_byte(uart_t * port, uint8_t c);
uint16_t uart_write(uart_t * huart, const uint8_t * bytes, uint16_t len);
int16_t uart_read_byte(uart_t * port);
int16_t uart_timed_read_byte(uart_t * port);
uint16_t uart_read(uart_t * huart, uint8_t * bytes, uint16_t len);
uint16_t uart_avail(uart_t * huart);
void uart_flush(uart_t * uart);
void uart_set_timeout(uart_t * port, uint16_t tout);

void USART1_IRQHandler(void);
void USART3_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif
