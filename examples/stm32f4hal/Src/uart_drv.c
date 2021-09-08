/*
 * uart_drv.c
 *
 *  Created on: 4 Sep 2021
 *      Author: rho
 */

#include <uart_drv.h>

typedef struct {
    UART_HandleTypeDef huart;

    uint16_t timeout;

    volatile uint16_t tx_head;
    volatile uint16_t tx_tail;

    volatile uint16_t rx_head;
    volatile uint16_t rx_tail;

    uint8_t tx_buf[UART_MAX_TX_SIZE];
    uint8_t rx_buf[UART_MAX_RX_SIZE];
} uart_t;

static uart_t uart1_handle = {.huart.Instance = USART1};
static uart_t uart3_handle = {.huart.Instance = USART3};

void USART1_IRQHandler(void);
void USART3_IRQHandler(void);

extern void Error_Handler(void);

static inline uart_t * uart_get_handle(USART_TypeDef * instance)
{
    switch ((uint32_t)instance)
    {
    case (uint32_t)USART1: return &uart1_handle;
    case (uint32_t)USART3: return &uart3_handle;

    default:
        Error_Handler();
        return 0;
    }
}

/**
 * @brief UART peripheral initialization
 * This function configures the hardware resources
 * @param huart: UART handle pointer
 * @retval None
 */
void uart_init(USART_TypeDef * instance, uint32_t baud_rate)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    uart_t * handle = uart_get_handle(instance);
    UART_HandleTypeDef * huart = &(handle->huart);

    if (instance == USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();

        /**USART1 GPIO Configuration
		PA9     ------> USART1_TX
		PA10     ------> USART1_RX
         */
        GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        huart->Init.BaudRate = baud_rate;
        huart->Init.WordLength = UART_WORDLENGTH_8B;
        huart->Init.StopBits = UART_STOPBITS_1;
        huart->Init.Parity = UART_PARITY_NONE;
        huart->Init.Mode = UART_MODE_TX_RX;
        huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
        huart->Init.OverSampling = UART_OVERSAMPLING_16;
        if (HAL_UART_Init(huart) != HAL_OK)
        {
            Error_Handler();
        }

        /* USART1 interrupt Init */
        HAL_NVIC_SetPriority(USART1_IRQn, 0x3, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    }
    else if (instance == USART3)
    {
        /* Peripheral clock enable */
        __HAL_RCC_USART3_CLK_ENABLE();

        __HAL_RCC_GPIOB_CLK_ENABLE();
        /**USART3 GPIO Configuration
		PB10     ------> USART3_TX
		PB11     ------> USART3_RX
         */
        GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        huart->Init.BaudRate = baud_rate;
        huart->Init.WordLength = UART_WORDLENGTH_8B;
        huart->Init.StopBits = UART_STOPBITS_1;
        huart->Init.Parity = UART_PARITY_NONE;
        huart->Init.Mode = UART_MODE_TX_RX;
        huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
        huart->Init.OverSampling = UART_OVERSAMPLING_16;
        if (HAL_UART_Init(huart) != HAL_OK)
        {
            Error_Handler();
        }

        /* USART3 interrupt Init */
        HAL_NVIC_SetPriority(USART3_IRQn, 0x3, 0);
        HAL_NVIC_EnableIRQ(USART3_IRQn);
    }

    __HAL_UART_DISABLE_IT(huart, UART_IT_TXE);
    __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_RXNE);
    __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);

    handle->tx_head = 0;
    handle->rx_head = 0;
    handle->tx_tail = 0;
    handle->rx_tail = 0;

    handle->timeout = UART_DEFAULT_TIMEOUT;
}

uint16_t uart_write(USART_TypeDef * instance, const uint8_t * bytes, uint16_t len)
{
    uint16_t n = 0;

    while (len--)
    {
        if (uart_write_byte(instance, *bytes++)) n++;
        else break;
    }

    return n;
}

uint16_t uart_write_byte(USART_TypeDef * instance, uint8_t c)
{
    uart_t * handle = uart_get_handle(instance);

    uint16_t curr_head = handle->tx_head;
    uint16_t curr_tail = handle->tx_tail;

    /* If the buffer and the data register is empty, just write the byte
     * to the data register and be done. This shortcut helps
     * significantly improve the effective datarate at high (>
     * 500kbit/s) bitrates, where interrupt overhead becomes a slowdown.*/
    if (curr_head == curr_tail && __HAL_UART_GET_FLAG(&handle->huart, UART_FLAG_TXE) != RESET)
    {
        instance->DR = c;
        return 1;
    }

    uint16_t i = (curr_head + 1) % UART_MAX_TX_SIZE;

    /* If the output buffer is full, there's nothing for it other than to
       wait for the interrupt handler to empty it a bit */
    while (i == handle->tx_tail) {    }

    handle->tx_buf[curr_head] = c;
    handle->tx_head = i;

    __HAL_UART_ENABLE_IT(&handle->huart, UART_IT_TXE);

    return 1;
}

int16_t uart_read_byte(USART_TypeDef * instance)
{
    uart_t * handle = uart_get_handle(instance);

    uint16_t curr_head = handle->rx_head;
    uint16_t curr_tail = handle->rx_tail;

    /* if the head isn't ahead of the tail,
     * we don't have any characters */
    if (curr_head == curr_tail) {
        return -1;
    }
    else {
        uint8_t c = handle->rx_buf[curr_tail];
        handle->rx_tail = (uint16_t)(curr_tail + 1) % UART_MAX_RX_SIZE;
        return c;
    }
}

static int16_t uart_timed_read_byte(USART_TypeDef * instance)
{
    uart_t * handle = uart_get_handle(instance);

    int16_t c;
    uint32_t _startMillis = HAL_GetTick();

    do {
        c = uart_read_byte(instance);
        if (c >= 0) return c;
    }
    while (HAL_GetTick() - _startMillis < handle->timeout);

    return -1;     /* -1 indicates timeout */
}

uint16_t uart_read(USART_TypeDef * instance, uint8_t * bytes, uint16_t len)
{
    uint16_t count = 0;

    while (count < len) {
        int16_t c = uart_timed_read_byte(instance);
        if (c < 0) break;

        *bytes++ = (uint8_t)c;
        count++;
    }

    return count;
}

uint16_t uart_avail(USART_TypeDef * instance)
{
    uart_t * handle = uart_get_handle(instance);
    return ((uint16_t)(UART_MAX_RX_SIZE + handle->rx_head - handle->rx_tail)) % UART_MAX_RX_SIZE;
}

void uart_flush(USART_TypeDef * instance)
{
    uart_t * handle = uart_get_handle(instance);

    /* Wait until the TXE interrupt is disabled and
     * any ongoing transmission is complete.
     *
     * Note: TC should be 1 already at Reset, so it's safe to call this function
     * even when no data has ever been sent */
    while (__HAL_UART_GET_IT_SOURCE(&handle->huart, UART_IT_TXE)
            || __HAL_UART_GET_FLAG(&handle->huart, UART_FLAG_TC) == RESET)
    {

    }

    /* If we get here, nothing is queued anymore (TXEIE is disabled) and
     * the hardware finished transmission (TC is set). */
}

void uart_set_timeout(USART_TypeDef * instance, uint16_t timeout)
{
    uart_t * handle = uart_get_handle(instance);
    handle->timeout = timeout;
}

static void generic_usart_handler(uart_t * handle)
{
    USART_TypeDef * instance = handle->huart.Instance;

    if (__HAL_UART_GET_IT_SOURCE(&handle->huart, UART_IT_RXNE)
            && __HAL_UART_GET_FLAG(&handle->huart, UART_FLAG_RXNE))
    {
        unsigned char c = instance->DR & 0xFF;
        uint16_t i = (uint16_t)(handle->rx_head + 1) % UART_MAX_RX_SIZE;

        /* if we should be storing the received character into the location
         * just before the tail (meaning that the head would advance to the
         * current location of the tail), we're about to overflow the buffer
         * and so we don't write the character or advance the head. */
        if (i != handle->rx_tail) {
            handle->rx_buf[handle->rx_head] = c;
            handle->rx_head = i;
        }

        /* in case the flag got set during the read sequence above */
        __HAL_UART_CLEAR_OREFLAG(&handle->huart);
    }
    /* in case ORE is somehow set while RXNE flag is not set */
    else if (__HAL_UART_GET_IT_SOURCE(&handle->huart, UART_IT_RXNE)
            && __HAL_UART_GET_FLAG(&handle->huart, UART_FLAG_ORE))
    {
        __HAL_UART_CLEAR_OREFLAG(&handle->huart);
    }

    if (__HAL_UART_GET_IT_SOURCE(&handle->huart, UART_IT_TXE)
            && __HAL_UART_GET_FLAG(&handle->huart, UART_FLAG_TXE))
    {
        unsigned char c = handle->tx_buf[handle->tx_tail];
        handle->tx_tail = (handle->tx_tail + 1) % UART_MAX_TX_SIZE;

        instance->DR = c;

        if (handle->tx_head == handle->tx_tail) {
            /* Buffer empty, so disable TXE interrupt */
            __HAL_UART_DISABLE_IT(&handle->huart, UART_IT_TXE);
        }
    }
}

void USART1_IRQHandler(void)
{
    uart_t * handle = uart_get_handle(USART1);
    generic_usart_handler(handle);
}

void USART3_IRQHandler(void)
{
    uart_t * handle = uart_get_handle(USART3);
    generic_usart_handler(handle);
}
