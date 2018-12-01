#include "stm32f10x.h"
#include "stm32f1xx_it.h"
#include "stm32f10x_conf.h"
#include "uart_drv.h"
#include "stmutils.h"
#include "fpm.h"

#include <stdio.h>

/* handles for UART comms */
uart_t uart1_dev = {USART1};
uart_t uart3_dev = {USART3};

/* prototypes needed for FPM library comms */
uint16_t uart3_avail(void);
uint16_t uart3_read(uint8_t * bytes, uint16_t len);
void uart3_write(uint8_t * bytes, uint16_t len);

FPM finger;
FPM_System_Params params;

void enroll_finger(int16_t fid);
uint8_t get_free_id(int16_t * fid);

void gpio_clocks_enable(void) {
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA
			| RCC_APB2Periph_GPIOB
			| RCC_APB2Periph_GPIOC
			| RCC_APB2Periph_AFIO, ENABLE);
}

int main(void)
{
	SystemInit();
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock / 1000);

	gpio_clocks_enable();
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	/* set up UART ports */
	uart_init(&uart1_dev, 9600);
	uart_init(&uart3_dev, 57600);

	finger.address = 0xFFFFFFFF;
	finger.password = 0;
	finger.avail_func = uart3_avail;
	finger.read_func = uart3_read;
	finger.write_func = uart3_write;

	/* init fpm instance, supply millis and delay functions */
	if (fpm_begin(&finger, millis, delay_ms)) {
		fpm_read_params(&finger, &params);
		printf("Found fingerprint sensor!\r\n");
		printf("Capacity: %d\r\n", params.capacity);
		printf("Packet length: %d\r\n", fpm_packet_lengths[params.packet_len]);
	}
	else {
		printf("Did not find fingerprint sensor :(\r\n");
		while (1);
	}

	while (1) {
		printf("Send any character to enroll a finger...\r\n");
		while (uart_avail(&uart1_dev) == 0);
		printf("Searching for a free slot to store the template...\r\n");
		int16_t fid;
		if (get_free_id(&fid))
			enroll_finger(fid);
		else
			printf("No free slot in flash library!\r\n");
		while (uart_read_byte(&uart1_dev) != -1);  // clear buffer
	}

}

/* get free ID in sensor database */
uint8_t get_free_id(int16_t * fid) {
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

void enroll_finger(int16_t fid) {
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
    delay_ms(2000);
    p = 0;
    while (p != FPM_NOFINGER) {
    	p = fpm_get_image(&finger);
    	delay_ms(10);
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

/* definitions for uart3 control */
uint16_t uart3_avail(void) {
	return uart_avail(&uart3_dev);
}

uint16_t uart3_read(uint8_t * bytes, uint16_t len) {
	return uart_read(&uart3_dev, bytes, len);
}

void uart3_write(uint8_t * bytes, uint16_t len) {
	uart_write(&uart3_dev, bytes, len);
}

