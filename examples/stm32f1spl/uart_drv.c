#include "uart_drv.h"
#include "stm32f1xx_it.h"

extern uart_t uart1_dev;
extern uart_t uart3_dev;

void uart_init(uart_t * port, uint32_t baud){
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef uart;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    if (port->instance == USART1) {
		gpio.GPIO_Pin = GPIO_Pin_9;
		gpio.GPIO_Mode = GPIO_Mode_AF_PP;
		gpio.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(GPIOA, &gpio);

		gpio.GPIO_Pin = GPIO_Pin_10;
		gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
		GPIO_Init(GPIOA, &gpio);

		RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

		uart.USART_BaudRate = baud;
		uart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
		uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
		uart.USART_Parity = USART_Parity_No;
		uart.USART_StopBits = USART_StopBits_1;
		uart.USART_WordLength = USART_WordLength_8b;
		USART_Init(USART1, &uart);
		USART_Cmd(USART1, ENABLE);
        /* NVIC setup */
        /* Set Interrupt Group Priority */
        NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x3;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
        
        USART_ITConfig(USART1, USART_IT_TXE, DISABLE);
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
        USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
        
    }
    else if (port->instance == USART3){
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

        gpio.GPIO_Pin = GPIO_Pin_10;      // TX pin
        gpio.GPIO_Mode = GPIO_Mode_AF_PP;
        gpio.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOB, &gpio);

        gpio.GPIO_Pin = GPIO_Pin_11;
        gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIOB, &gpio);

        uart.USART_BaudRate = baud;
        uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
        uart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
        uart.USART_WordLength = USART_WordLength_8b;
        uart.USART_Parity = USART_Parity_No;
        uart.USART_StopBits = USART_StopBits_1;
        USART_Init(USART3, &uart);
        USART_Cmd(USART3, ENABLE);
        /* NVIC setup */
        /* Set Interrupt Group Priority */
        NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x2;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);

        USART_ITConfig(USART3, USART_IT_TXE, DISABLE);
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
        USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    }
    
    port->tx_head = 0;
    port->rx_head = 0;
    port->tx_tail = 0;
    port->rx_tail = 0;
    port->ever_written = 0;
    port->timeout = UART_DEFAULT_TIMEOUT;
    port->txi_enabled = 0;
}

uint16_t uart_write(uart_t * port, const uint8_t * bytes, uint16_t len){
    uint16_t n = 0;
    while (len--) {
        if (uart_write_byte(port, *bytes++)) n++;
        else break;
    }
    return n;
}

uint16_t uart_write_byte(uart_t * port, uint8_t c) {
    port->ever_written = 1;
    // If the buffer and the data register is empty, just write the byte
    // to the data register and be done. This shortcut helps
    // significantly improve the effective datarate at high (>
    // 500kbit/s) bitrates, where interrupt overhead becomes a slowdown.
    if (port->tx_head == port->tx_tail && port->txi_enabled == 0 && USART_GetFlagStatus(port->instance, USART_FLAG_TXE) != RESET) {
        USART_SendData(port->instance, c);
        USART_ClearFlag(port->instance, USART_FLAG_TC);
        return 1;
    }
    
    uint16_t i = (port->tx_head + 1) % UART_MAX_TX_SIZE;
    
    // If the output buffer is full, there's nothing for it other than to 
    // wait for the interrupt handler to empty it a bit
    while (i == port->tx_tail) {
        //return 0;
    }

    port->tx_buf[port->tx_head] = c;
    port->tx_head = i;

	USART_ITConfig(port->instance, USART_IT_TXE, ENABLE);
	port->txi_enabled = 1;

    return 1;
}

int16_t uart_read_byte(uart_t * port)
{
    // if the head isn't ahead of the tail, we don't have any characters
    if (port->rx_head == port->rx_tail) {
        return -1;
    } 
    else {
        uint8_t c = port->rx_buf[port->rx_tail];
        port->rx_tail = (uint16_t)(port->rx_tail + 1) % UART_MAX_RX_SIZE;
        return c;
    }
}

int16_t uart_timed_read_byte(uart_t * port) {
    int16_t c;
    uint32_t _startMillis = millis();
    do {
        c = uart_read_byte(port);
        if (c >= 0) return c;
    } while(millis() - _startMillis < port->timeout);
    
    return -1;     // -1 indicates timeout
}

uint16_t uart_read(uart_t * port, uint8_t * bytes, uint16_t len){
    uint16_t count = 0;
    while (count < len) {
        int16_t c = uart_timed_read_byte(port);
        if (c < 0) break;
        *bytes++ = (uint8_t)c;
        count++;
    }
    return count;
}

uint16_t uart_avail(uart_t * port) {
    return ((uint16_t)(UART_MAX_RX_SIZE + port->rx_head - port->rx_tail)) % UART_MAX_RX_SIZE;
}

void uart_flush(uart_t * port) {
	// If we have never written a byte, no need to flush. This special
    // case is needed since there is no way to force the TXC (transmit
    // complete) bit to 1 during initialization
    if (port->ever_written == 0)
        return;

    while (port->txi_enabled == 1 || USART_GetFlagStatus(port->instance, USART_FLAG_TC) == RESET) {
      
    }
    // If we get here, nothing is queued anymore (DRIE is disabled) and
    // the hardware finished tranmission (TXC is set).
}

void uart_set_timeout(uart_t * port, uint16_t tout) {
    port->timeout = tout;
}

static void generic_usart_handler(uart_t * port)
{
    if (USART_GetITStatus(port->instance, USART_IT_RXNE) != RESET)
    {
        unsigned char c = USART_ReceiveData(port->instance);
        uint16_t i = (uint16_t)(port->rx_head + 1) % UART_MAX_RX_SIZE;

        // if we should be storing the received character into the location
        // just before the tail (meaning that the head would advance to the
        // current location of the tail), we're about to overflow the buffer
        // and so we don't write the character or advance the head.
        if (i != port->rx_tail) {
            port->rx_buf[port->rx_head] = c;
            port->rx_head = i;
        }
        //USART_ClearITPendingBit(port->instance, USART_IT_RXNE);
    }
    
    if (USART_GetITStatus(port->instance, USART_IT_TXE) != RESET) {
        unsigned char c = port->tx_buf[port->tx_tail];
        port->tx_tail = (port->tx_tail + 1) % UART_MAX_TX_SIZE;

        USART_SendData(port->instance, c);
        
        // clear the TXC bit. This makes sure flush() won't return until the bytes
        // actually got written
        USART_ClearFlag(port->instance, USART_FLAG_TC);

        if (port->tx_head == port->tx_tail) {
            // Buffer empty, so disable interrupts
            USART_ITConfig(port->instance, USART_IT_TXE, DISABLE);
            port->txi_enabled = 0;
        }
        //USART_ClearITPendingBit(port->instance, USART_IT_TXE);
    }
    if (USART_GetITStatus(port->instance, USART_IT_ORE) != RESET) {
        USART_ReceiveData(port->instance);
        USART_ClearITPendingBit(port->instance, USART_IT_ORE);
    }
}

void USART1_IRQHandler(void)
{
    generic_usart_handler(&uart1_dev);
}

void USART3_IRQHandler(void)
{
    generic_usart_handler(&uart3_dev);
}
