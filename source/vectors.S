/* Supervisor vectors: entry point, exception handlers, Core 1 dispatch.
 *
 * _start at 0x80000100: sets up stack, calls supervisor_main.
 * Exception handlers: DSI, ISI, External, Alignment, Program — all spin.
 * core1_guest_entry: dispatches on Core 1 for CMD_EXEC_GUEST.
 *
 * SPRG usage:
 *   SPRG0 — libxenon (r31 save)
 *   SPRG1 — libxenon (HV context base)
 *   SPRG2, SPRG3 — our scratch
 */

.section .vectors, "ax"

/* ── Entry point ── */
.org 0x100
.globl _start
_start:
    mfspr    %r3, 1023       /* PIR */
    cmpldi   %r3, 0
    beq      .L_core0

    /* Secondary cores: spin in wait[] polling loop (libxenon compatible) */
    lis      %r3, wait@h
    ori      %r3, %r3, wait@l
    mfspr    %r5, 1023
    slwi     %r5, %r5, 3
    add      %r3, %r3, %r5
1:  lwz      %r4, 0(%r3)
    cmpwi    %r4, 0
    beq      1b
    lwz      %r1, 4(%r3)
    mtctr    %r4
    bctrl
    li       %r4, 0
    stw      %r4, 0(%r3)
    b        1b

.L_core0:
    /* Core 0: set up additional SLB entry for XPIC (ESID 0x2, 0x20000000-0x2FFFFFFF).
       XeLL's SLB only covers ESIDs 0x8-0xF. */
    lis      %r4, 0x0002               /* VSID = 0x200 */
    sldi     %r4, %r4, 32              /* shift to high 32 bits */
    lis      %r5, 0x2000               /* ESID = 0x20000000, V=1, B=0 (256MB), C=0 */
    ori      %r5, %r5, 0x0010          /* bit 36 = V */
    slbmte   %r4, %r5
    isync

    /* Set up stack, call supervisor_main */
    lis      %r1, __libc_stack_end@h
    ori      %r1, %r1, __libc_stack_end@l
    addis    %r1, %r1, -4
    bl       supervisor_main
1:  b        1b

/* ── Exception handlers: spin (prevents XeLL from catching faults) ── */
.org 0x300
_dsi_handler:     b _dsi_handler
.org 0x400
_isi_handler:     b _isi_handler
.org 0x600
_alignment:       b _alignment
.org 0x700
_program:         b _program

/* ── External interrupt: early EOI + IPI dispatch ── */
.org 0x500
.globl _external_interrupt
_external_interrupt:
    mtsprg2   %r3
    mtsprg3   %r4

    mfspr     %r3, 1023               /* PIR */
    cmpldi    %r3, 0
    bne       .L_secondary_wakeup     /* if PIR != 0, redirect to polling loop */

    /* Core 0: normal IPI handling */
    mfspr     %r3, 1023
    rldicl    %r3, %r3, 0, 60
    sldi      %r3, %r3, 12

    lis       %r4, 0x2005
    add       %r4, %r4, %r3

    ld        %r3, 0(%r4)             /* read IACK */
    std       %r3, 0(%r4)             /* early EOI */

    cmpldi    %r3, 1

    mfsprg3   %r4
    mfsprg2   %r3

    beq       .L_ipc_fast

    /* Slow path: non-IPI interrupt — libxenon stub */
    bl        _libxenon_irq_dispatch
    rfi

.L_ipc_fast:
    bl        _ipc_irq_handler
    rfi

/* ── Secondary core wakeup (PIR != 0): jump directly to polling loop ── */
.L_secondary_wakeup:
    /* Calculate wait[] entry for this core: wait + PIR * 8
       wait[PIR*2]     = function pointer (set by bootstrap_core1)
       wait[PIR*2 + 1] = stack pointer */
    lis       %r4, wait@h
    ori       %r4, %r4, wait@l
    slwi      %r3, %r3, 3             /* PIR * 8 */
    add       %r4, %r4, %r3
    lwz       %r1, 4(%r4)             /* load stack */
    lwz       %r12, 0(%r4)            /* load function pointer */
    mtsprg3   %r4                     /* save r4 (restore later) */
    mtsprg2   %r3                     /* save r3 */
    mtctr     %r12
    bctr                               /* jump to core1_process_engine */

/* ── Core 1 guest entry dispatch ── */
.globl core1_guest_entry
core1_guest_entry:
    lwz       %r12, 0(%r3)       /* entry_point */
    lwz       %r1,  4(%r3)       /* stack_pointer */
    lwz       %r4,  8(%r3)       /* guest_text_start */
    lwz       %r5,  12(%r3)      /* guest_text_size */

    /* Convert bytes to 128-byte line count: (size + 127) >> 7 */
    addi      %r5, %r5, 127
    srwi      %r5, %r5, 7
    mtctr     %r5

1:  icbi      0, %r4
    addi      %r4, %r4, 128
    bdnz      1b
    isync

    mtctr     %r12
    bctr

.section .bss
.globl wait
wait:
    .space 12 * 4
