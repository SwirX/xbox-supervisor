/* Core 1 guest entry trampoline.
 *
 * Called from core1_engine.c when CMD_EXEC_GUEST is received.
 * r3 = pointer to ElfExecPayload { entry_point, stack_pointer,
 *                                   guest_text_start, guest_text_size }
 *
 * Invalidates the guest's text region from I-cache, sets stack,
 * and branches to the guest entry point. Never returns.
 */

.section .text, "ax"

.globl core1_guest_entry
core1_guest_entry:
    /* Load exec payload fields */
    lwz     %r12, 0(%r3)       /* entry_point */
    lwz     %r1,  4(%r3)       /* stack_pointer  (new stack for guest) */
    lwz     %r4,  8(%r3)       /* guest_text_start */
    lwz     %r5,  12(%r3)      /* guest_text_size */

    /* I-cache invalidation: (size + 127) >> 7 = number of 128B lines */
    addi    %r5, %r5, 127
    srwi    %r5, %r5, 7
    mtctr   %r5
    cmpwi   %r5, 0
    beq     2f

1:  icbi    0, %r4
    addi    %r4, %r4, 128
    bdnz    1b

2:  isync

    /* Jump to guest */
    mtctr   %r12
    bctr
