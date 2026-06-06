/* Test guest linker script — loads at start of guest app space (0x80800000).
 * Minimal: just .text, no CRT, no BSS, no data. */

ENTRY(_start)

SECTIONS
{
    . = 0x80800000;
    .text : { *(.text*) }
}
