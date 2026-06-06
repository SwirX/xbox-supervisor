#include <stdint.h>

#define XENON_UART_BASE   0xea001000
#define UART_FIFO_TX      (*(volatile uint32_t *)(XENON_UART_BASE + 0x14))
#define UART_REG_STATUS   (*(volatile uint32_t *)(XENON_UART_BASE + 0x18))

/* Bit 25 of status indicates Transmitter Holding Register Empty */
#define UART_STATUS_THRE  0x02000000

void uart_init(void)
{
    /* XeLL has already initialised the baud rate generator (115200, 8N1).
       Read status to clear any lingering state. */
    uint32_t dummy = UART_REG_STATUS;
    (void)dummy;
}

void uart_putc(char c)
{
    /* Spin until transmitter FIFO can accept a byte */
    while (!(UART_REG_STATUS & UART_STATUS_THRE)) {
        __asm__ volatile("nop");
    }

    /* Write character in the upper byte of the 32-bit FIFO register */
    UART_FIFO_TX = ((uint32_t)c << 24) & 0xFF000000;
}

void uart_print(const char *str)
{
    while (*str) {
        if (*str == '\n')
            uart_putc('\r');
        uart_putc(*str++);
    }
}
