/* Test payload ELF: compiled as a standalone ELF32 PowerPC binary.
 * The first 4 bytes at the entry point are a known signature
 * that Core 1 reads back during the CMD_VERIFY_GUEST handshake.
 *
 * Build: xenon-gcc -nostartfiles -nostdlib -Ttest_guest.lds \
 *                  -o test_guest.elf test_guest.S
 * Embed: xenon-objcopy -I binary -O elf32-powerpc -B powerpc \
 *                  test_guest.bin test_guest.embed.o
 */

.section .text, "ax"
.globl _start
_start:
    .long 0xDEADBEEF               /* known signature word */
    lis   %r3, 0x8000
    ori   %r3, %r3, 0x6AF0        /* supervisor's _heap_start for status */
    stw   %r4, 0(%r3)             /* write r4 (passed in from Core 1) */
1:  or    27, 27, 27
    b     1b
