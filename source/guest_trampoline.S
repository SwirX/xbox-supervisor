.section .text, "ax"

.globl core1_guest_entry
core1_guest_entry:
    mr      %r10, %r1
    mflr    %r11

    lwz     %r12, 0(%r3)
    lwz     %r1,  4(%r3)
    lwz     %r4,  8(%r3)
    lwz     %r5,  12(%r3)

    addi    %r5, %r5, 127
    srwi    %r5, %r5, 7
    mtctr   %r5
    cmpwi   %r5, 0
    beq     2f

1:  icbi    0, %r4
    addi    %r4, %r4, 128
    bdnz    1b

2:  isync

    mtctr   %r12
    bctrl

    mr      %r1,  %r10
    mtlr    %r11
    blr
