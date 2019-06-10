#include "stm32f10x.h"
#include "stm32f1xx_it.h"
#include "stm32f10x_conf.h"
#include "uart_drv.h"
#include "stmutils.h"
#include "fpm.h"

#include <stdio.h>
#include <ctype.h>

/* handles for UART comms, using one for debug and the other for the sensor */
uart_t uart1_dev = {USART1};
uart_t uart3_dev = {USART3};

/* prototypes needed for FPM library comms */
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

	/* set up the instance, supply UART interface functions too */
	finger.address = 0xFFFFFFFF;
	finger.password = 0;
	finger.manual_settings = 0;
	finger.avail_func = uart3_avail;
	finger.read_func = uart3_read;
	finger.write_func = uart3_write;

	/* init fpm instance, supply millis function */
	if (fpm_begin(&finger, millis)) {
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

/* main loop for enroll example */
void enroll_mainloop(void) {
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

/* this is equal to the template size for the fingerprint module,
 * some modules have 512-byte templates while others have
 * 768-byte templates. Check the printed result of read_template()
 * to determine the case for your module and adjust the needed buffer
 * size below accordingly */
#define BUFF_SZ     512

uint8_t template_buffer[BUFF_SZ];

/* main loop for templates example */
void templates_mainloop(void) {
	while (1) {
		printf("Enter the template ID # you want to read...");
		int16_t fid = 0;
		while (uart_read_byte(&uart1_dev) != -1);
		while (1) {
			while (uart_avail(&uart1_dev) == 0);
			char c = uart_read_byte(&uart1_dev);
			if (! isdigit(c)) break;
			fid *= 10;
			fid += c - '0';
		}

		/* read the template from its location into the buffer */
		uint16_t total_read = read_template(fid, template_buffer, BUFF_SZ);
		if (!total_read)
			return;
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

void matchprints_mainloop(void) {
	int16_t fid = 18;
	int16_t otherfid = 16;

	while (1) {
		printf("Send any character to match the 2 predefined IDs...\r\n");
		while (uart_avail(&uart1_dev) == 0);
		match_prints(fid, otherfid);
		while (uart_read_byte(&uart1_dev) != -1);  // clear buffer
	}
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

uint16_t read_template(uint16_t fid, uint8_t * buffer, uint16_t buff_sz) {
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
            printf("%d, ", buffer[row * 16 + col]);
        }
        printf("\r\n");
    }
    printf("--------------------------------------------\r\n");

    printf("%d bytes read.\r\n", total_bytes);
    return total_bytes;
}

void match_prints(int16_t fid, int16_t otherfid) {
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

