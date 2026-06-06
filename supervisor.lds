/* Supervisor linker script — derived from libxenon's app.lds.
 *
 * Changes from upstream app.lds:
 *   1. Heap ends at 0x800F0000 (within the 1 MB supervisor reservation)
 *      instead of 0x9E000000.  This prevents malloc from clobbering the
 *      guest app space at 0x80100000+.
 *   2. Stack top stays at 0x800E0000 (inside the 1 MB region) instead of
 *      0x9E000000.  Core 1's stack is manually provided by the supervisor.
 *   3. IPC shared memory symbols are NOT placed by the linker — they live
 *      at fixed physical addresses (0x800FF000 area) and are accessed via
 *      absolute pointer casts in C code.
 */

ENTRY(_start)

SECTIONS
{
    . = 0x80000000;
    .text : {  *(.except*); *(.pagetable); pagetable_end = .; *(.init*); *(.fini*); *(.text*); }
    .elfldr : { elfldr_start = .;*(.elfldr); elfldr_end = .;}

    .data : { *(.data*) }
    .sdata : { *(.sdata*) }
    .rodata : { *(.rodata*)}

    .ctors : { __CTOR_LIST__ = .; *(.ctors); __CTOR_END__ = .; }
    .dtors : {__DTOR_LIST__ = .; *(.dtors); __DTOR_END__ = .; }

    .eh_frame_hdr : { *(.eh_frame_hdr) }
    .eh_frame ALIGN(0x4): { PROVIDE (__eh_frame_start = .); KEEP (*(.eh_frame)) LONG (0); }
    .gcc_except_table :  { *(.gcc_except_table .gcc_except_table.*) }

    .got : { *(.got) }
    .got2 : { *(.got2) }
    .plt : { *(.plt) }
    .rel : { *(.rel*) }

    bss_start = .;
    .bss : { *(.bss.beginning.lower); *(.bss.beginning.upper); *(.bss*) }
    .sbss : { *(.sbss .sbss.*) }
    bss_end = .;

    /* Heap: ends past libxenon's internal BSS (~6.3 MB).
     * Supervisor reservation covers 0x80000000 - 0x80800000 (8 MB).
     * Guest space starts at 0x80800000. */
    heap_begin = .;
    . = 0x80800000;
    __libc_stack_end = .;
    heap_end = .;
}
