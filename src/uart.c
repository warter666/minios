#include "kernel.h"

void uart_init(void)
{
    UART_CTL = 0;               /* disable while configuring */
    UART_IBRD = 27;             /* 12 MHz / (16 * 115200) = 6.51 with board's
                                   default clock divider — QEMU does not verify
                                   baud, these are the standard 115200 values */
    UART_FBRD = 8;
    UART_LCRH = 0x70;           /* 8N1, FIFO enabled */
    UART_CTL = 0x301;           /* UARTEN | TXE | RXE */
}

void uart_putc(char c)
{
    while (UART_FR & UART_FR_TXFF)
        ;
    if (c == '\n')
        uart_putc('\r');
    UART_DR = (uint8_t)c;
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

int uart_try_getc(void)
{
    if (UART_FR & UART_FR_RXFE)
        return -1;
    return (int)(UART_DR & 0xFF);
}

void uart_write_syscall(const char *buf, uint32_t len)
{
    /* len == 0xFFFFFFFF means "NUL-terminated" (used by the printf helpers) */
    if (len == 0xFFFFFFFF) {
        while (*buf)
            uart_putc(*buf++);
        return;
    }
    for (uint32_t i = 0; i < len; i++)
        uart_putc(buf[i]);
}
