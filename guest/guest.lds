ENTRY(guest_start)

SECTIONS
{
    . = 0x81000000;
    .text : { *(.except*) *(.pagetable) *(.init*) *(.fini*) *(.text*) }
    .elfldr : { elfldr_start = .; *(.elfldr); elfldr_end = .; }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .sdata : { *(.sdata*) }
    .ctors : { __CTOR_LIST__ = .; *(.ctors); __CTOR_END__ = .; }
    .dtors : { __DTOR_LIST__ = .; *(.dtors); __DTOR_END__ = .; }
    .eh_frame : { __eh_frame_start = .; *(.eh_frame); }
    .got : { *(.got) }
    .got2 : { *(.got2) }
    bss_start = .;
    .bss : { *(.bss*) }
    .sbss : { *(.sbss .sbss.*) }
    bss_end = .;
    heap_begin = .;
    . = 0x8F000000;
    __libc_stack_end = .;
}
