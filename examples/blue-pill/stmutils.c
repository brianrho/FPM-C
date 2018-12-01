#include <stdio.h>
#include <string.h>
#include <math.h>
#include "stmutils.h"
#include "uart_drv.h"

#define MAX_FTOSTR_LEN		50
static char tempbuf[MAX_FTOSTR_LEN + 1];

extern uart_t uart1_dev;

static char * dtostrf(float number, unsigned char prec, char *s) {

    if(isnan(number)) {
        strcpy(s, "nan");
        return s;
    }
    if(isinf(number)) {
        strcpy(s, "inf");
        return s;
    }

    if(number > 4294967040.0 || number < -4294967040.0) {
        strcpy(s, "ovf");
        return s;
    }
    char* out = s;
    // Handle negative numbers
    if(number < 0.0) {
        *out = '-';
        ++out;
        number = -number;
    }

    // Round correctly so that print(1.999, 2) prints as "2.00"
    float rounding = 0.5;
    for(uint8_t i = 0; i < prec; ++i)
        rounding /= 10.0;

    number += rounding;

    // Extract the integer part of the number and print it
    unsigned int int_part = (unsigned int) number;
    float remainder = number - (float) int_part;
    out += sprintf(out, "%d", int_part);

    // Print the decimal point, but only if there are digits beyond
    if(prec > 0) {
        *out = '.';
        ++out;
    }

    while (prec-- > 0) {
        remainder *= 10.0;
        if((int)remainder == 0){
                *out = '0';
                 ++out;
        }
    }
    sprintf(out, "%d", (int) remainder);

    return s;
}

char * ftostr(float value) {
	return dtostrf(value, 2, tempbuf);
}

char * ftostrp(float value, unsigned char prec) {
	return dtostrf(value, prec, tempbuf);
}

int _write(int fd, char *str, int len) {
	return uart_write(&uart1_dev, (uint8_t *)str, len);
	//return len; //uart_write(&uart1_dev, (uint8_t *)str, len);
}
