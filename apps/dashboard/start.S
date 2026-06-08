.section .text, "ax"
.globl guest_start
guest_start:
    lis %r1, __libc_stack_end@h
    ori %r1, %r1, __libc_stack_end@l
    li %r3, 0
    stwu %r3, -64(%r1)
    bl main
1:  b 1b
