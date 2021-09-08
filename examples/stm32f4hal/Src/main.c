/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

#include "fpm.h"
#include <uart_drv.h>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);

/* UART interface prototypes needed for FPM library */
uint16_t uart3_avail(void);
uint16_t uart3_read(uint8_t * bytes, uint16_t len);
void uart3_write(uint8_t * bytes, uint16_t len);

FPM finger;
FPM_System_Params params;

/* tested functions */
void enroll_finger(int16_t fid);
uint8_t get_free_id(int16_t * fid);
uint16_t read_template(uint16_t fid, uint8_t * buffer, uint16_t buff_sz);
void match_prints(int16_t fid, int16_t otherfid);

void enroll_mainloop(void);
void templates_mainloop(void);
void matchprints_mainloop(void);

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    /* Enable GPIO bus clocks */
    MX_GPIO_Init();

    uart_init(USART1, 57600);

    uart_init(USART3, 57600);

    /* set up the instance, supply UART interface functions too */
    finger.address = 0xFFFFFFFF;
    finger.password = 0;
    finger.manual_settings = 0;
    finger.avail_func = uart3_avail;
    finger.read_func = uart3_read;
    finger.write_func = uart3_write;

    /* init fpm instance, supply time-keeping function */
    if (fpm_begin(&finger, HAL_GetTick))
    {
        fpm_read_params(&finger, &params);
        printf("Found fingerprint sensor!\r\n");
        printf("Capacity: %d\r\n", params.capacity);
        printf("Packet length: %d\r\n", fpm_packet_lengths[params.packet_len]);
    }
    else {
        printf("Did not find fingerprint sensor :(\r\n");
        while (1);
    }

    /* uncomment only one to test */
    enroll_mainloop();
    //templates_mainloop();
    //matchprints_mainloop();
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    /** Initializes the CPU, AHB and APB busses clocks
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }
    /** Initializes the CPU, AHB and APB busses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
            |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

int __io_putchar(int ch)
{
    return uart_write(USART1, (uint8_t *)&ch, 1);
}

/* main loop for enroll example */
void enroll_mainloop(void)
{
    while (1) {
        printf("Send any character to enroll a finger...\r\n");
        while (uart_avail(USART1) == 0);
        printf("Searching for a free slot to store the template...\r\n");
        int16_t fid;

        if (get_free_id(&fid))
            enroll_finger(fid);
        else
            printf("No free slot in flash library!\r\n");

        while (uart_read_byte(USART1) != -1);  // clear buffer
    }
}

/* this is equal to the template size for the fingerprint module,
 * some modules have 512-byte templates while others have
 * 768-byte templates. Check the printed result of read_template()
 * to determine the case for your module and adjust the needed buffer
 * size below accordingly */
#define BUFF_SZ     1024

uint8_t template_buffer[BUFF_SZ];

/* main loop for templates example */
void templates_mainloop(void)
{
    while (1) {
        printf("Enter the template ID # you want to read...\r\n");
        int16_t fid = 0;
        while (uart_read_byte(USART1) != -1);
        while (1) {
            while (uart_avail(USART1) == 0);
            char c = uart_read_byte(USART1);
            if (! isdigit(c)) break;
            fid *= 10;
            fid += c - '0';
        }

        /* read the template from its location into the buffer */
        read_template(fid, template_buffer, BUFF_SZ);
    }
}

/* get free ID in sensor database */
uint8_t get_free_id(int16_t * fid)
{
    int16_t p = -1;
    for (int page = 0; page < (params.capacity / FPM_TEMPLATES_PER_PAGE) + 1; page++) {
        p = fpm_get_free_index(&finger, page, fid);
        switch (p) {
            case FPM_OK:
                if (*fid != FPM_NOFREEINDEX) {
                    printf("Free slot at ID %d\r\n", *fid);
                    return 1;
                }
                break;
            default:
                printf("Error code: %d\r\n", p);
                return 0;
        }
    }

    return 0;
}

void matchprints_mainloop(void)
{
    int16_t fid = 18;
    int16_t otherfid = 16;

    while (1) {
        printf("Send any character to match the 2 predefined IDs...\r\n");
        while (uart_avail(USART1) == 0);
        match_prints(fid, otherfid);
        while (uart_read_byte(USART1) != -1);  // clear buffer
    }
}

void enroll_finger(int16_t fid)
{
    int16_t p = -1;
    printf("Waiting for valid finger to enroll\r\n");
    while (p != FPM_OK) {
        p = fpm_get_image(&finger);
        switch (p) {
            case FPM_OK:
                printf("Image taken\r\n");
                break;
            case FPM_NOFINGER:
                printf(".\r\n");
                break;
            default:
                printf("Error code: 0x%X!\r\n", p);
                break;
        }
    }
    // OK success!

    p = fpm_image2Tz(&finger, 1);
    switch (p) {
        case FPM_OK:
            printf("Image converted\r\n");
            break;
        default:
            printf("Error code: 0x%X!\r\n", p);
            return;
    }

    printf("Remove finger\r\n");
    HAL_Delay(2000);
    p = 0;
    while (p != FPM_NOFINGER) {
        p = fpm_get_image(&finger);
        HAL_Delay(10);
    }

    p = -1;
    printf("Place same finger again\r\n");
    while (p != FPM_OK) {
        p = fpm_get_image(&finger);
        switch (p) {
            case FPM_OK:
                printf("Image taken\r\n");
                break;
            case FPM_NOFINGER:
                printf(".\r\n");
                break;
            default:
                printf("Error code: 0x%X!\r\n", p);
                break;
        }
    }

    // OK success!

    p = fpm_image2Tz(&finger, 2);
    switch (p) {
        case FPM_OK:
            printf("Image converted\r\n");
            break;
        default:
            printf("Error code: 0x%X!\r\n", p);
            return;
    }


    // OK converted!
    p = fpm_create_model(&finger);
    switch (p) {
        case FPM_OK:
            printf("Prints matched!\r\n");
            break;
        default:
            printf("Error code: 0x%X!\r\n", p);
            return;
    }

    printf("ID %d...", fid);
    p = fpm_store_model(&finger, fid, 1);
    switch (p) {
        case FPM_OK:
            printf("Stored!\r\n");
            break;
        default:
            printf("Error code: 0x%X!\r\n", p);
            return;
    }
}

uint16_t read_template(uint16_t fid, uint8_t * buffer, uint16_t buff_sz)
{
    int16_t p = fpm_load_model(&finger, fid, 1);
    switch (p) {
        case FPM_OK:
            printf("Template %d loaded.\r\n", fid);
            break;
        default:
            printf("Error code: 0x%X!\r\n", p);
            return 0;
    }

    // OK success!

    p = fpm_download_model(&finger, 1);
    switch (p) {
        case FPM_OK:
            break;
        default:
            printf("Error code: 0x%X!\r\n", p);
            return 0;
    }

    uint8_t read_finished;
    int16_t count = 0;
    uint16_t readlen = buff_sz;
    uint16_t pos = 0;

    while (1) {
        uint8_t ret = fpm_read_raw(&finger, FPM_OUTPUT_TO_BUFFER, buffer + pos, &read_finished, &readlen);
        if (ret) {
            count++;
            pos += readlen;
            readlen = buff_sz - pos;
            if (read_finished)
                break;
        }
        else {
            printf("Error receiving packet %d\r\n", count);
            return 0;
        }
    }

    uint16_t total_bytes = count * fpm_packet_lengths[params.packet_len];

    /* just for pretty-printing */
    uint16_t num_rows = total_bytes / 16;

    printf("Printing template:\r\n");
    printf("---------------------------------------------\r\n");
    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < 16; col++) {
            printf("%X, ", buffer[row * 16 + col]);
        }
        printf("\r\n");
    }
    printf("--------------------------------------------\r\n");

    printf("%d bytes read.\r\n", total_bytes);
    return total_bytes;
}

void match_prints(int16_t fid, int16_t otherfid)
{
    /* read template into slot 2 */
    int16_t p = fpm_load_model(&finger, fid, 1);
    switch (p) {
        case FPM_OK:
            printf("Template %d loaded.\r\n", fid);
            break;
        default:
            printf("Error code: 0x%X!\r\n", p);
            return;
    }

    /* read template into slot 2 */
    p = fpm_load_model(&finger, otherfid, 2);
    switch (p) {
        case FPM_OK:
            printf("Template %d loaded.\r\n", otherfid);
            break;
        default:
            printf("Error code: 0x%X!\r\n", p);
            return;
    }

    uint16_t match_score = 0;
    p = fpm_match_template_pair(&finger, &match_score);
    switch (p) {
        case FPM_OK:
            printf("Match score: %d\r\n", match_score);
            break;
        case FPM_NOMATCH:
            printf("Prints did not match.\r\n");
            break;
        default:
            printf("Error code: 0x%X!\r\n", p);
            return;
    }
}

/* UART interface for fingerprint sensor library -- using USART3 */
uint16_t uart3_avail(void) {
    return uart_avail(USART3);
}

uint16_t uart3_read(uint8_t * bytes, uint16_t len) {
    return uart_read(USART3, bytes, len);
}

void uart3_write(uint8_t * bytes, uint16_t len) {
    uart_write(USART3, bytes, len);
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    while (1);
}

#ifdef  USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{ 
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
