/* CRT stubs for symbols referenced by libxenon.a objects.
   These are needed because we link with -nostartfiles but still
   pull in objects from libxenon.a that reference CRT symbols.

   Also provides memcpy/memset since we link -nostdlib. */

#include <stdint.h>

void *memcpy(void *dest, const void *src, unsigned long n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dest;
}

void *memset(void *s, int c, unsigned long n)
{
    unsigned char *p = s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

/* Weak placeholders for the embedded ELF payload symbols.
   When objcopy embeds a binary, it provides strong definitions
   that override these.  Without an embedded payload, the mapper
   sees start == end and returns error -1. */
uint8_t _binary_guest_application_elf_start[0] __attribute__((weak));
uint8_t _binary_guest_application_elf_end[0] __attribute__((weak));

/* Called from the vector 0x500 slow path for non-IPI interrupts.
   libxenon's default ex_interrupt handler is a stub that prints "Irq\n"
   and returns — we do the same without requiring printf. */
void _libxenon_irq_dispatch(void)
{
    /* no-op: libxenon USB/network are polled, not interrupt-driven.
       SMC/guide-button polling happens in supervisor_main loop. */
}
