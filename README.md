## Generic C library for R3xx/ZFMxx/FPMxx/R551 fingerprint modules

To use it, the following interface needs to be provided to the library, which expects function pointers with these signatures:

    /* to get the millisecond count since reset */
    uint32_t (*fpm_millis_func)(void);
    
    /* to read from the UART port connected to the module */
    uint16_t (*fpm_uart_read_func)(uint8_t * bytes, uint16_t len);
    
    /* to write to the UART port connected to the module */
    void (*fpm_uart_write_func)(uint8_t * bytes, uint16_t len);
    
    /* to get the current number of available bytes to be read */
    uint16_t (*fpm_uart_avail_func)(void);

It is assumed that UART interrupts are in use, especially for RX events, typically with incoming data being read into a buffer.
Check the examples for details.
