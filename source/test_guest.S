/* Test guest payload for Core 1.
 *
 * Minimal returnable payload — assembled as a flat ELF, embedded into the
 * supervisor via objcopy -I binary.  When Core 1 executes it:
 *   1. Writes a magic value (0xDEADBEEF) to the IPC state flags
 *   2. Returns via blr -> back to core1_engine polling loop
 *
 * Link address: the supervisor's elf_mapper loads us at 0x80800000
 * (start of guest app space).
 *
 * The ElfExecPayload provides:
 *   entry_point    = this function (0x80800000)
 *   stack_pointer  = Core 1's existing stack (0x807E0000 - 256)
 *   guest_text_start = 0x80800000
 *   guest_text_size  = size of this function
 */

.section .text, "ax"
.globl _start

.org 0
_start:
    /* Write magic value to IPC state flags: supervisor_status = 0xDEADBEEF
     * IPC_FLAGS_ADDR = 0x807FFF00
     * flags->out.supervisor_status is at offset 4 within the struct
     * (out starts at 0, target_action at +0, supervisor_status at +4) */
    lis     %r4, 0x807F
    ori     %r4, %r4, 0xFF00     /* r4 = 0x807FFF00 (IPC_FLAGS_ADDR) */

    lis     %r5, 0xDEAD
    ori     %r5, %r5, 0xBEEF     /* r5 = 0xDEADBEEF */

    stw     %r5, 4(%r4)          /* flags->out.supervisor_status = 0xDEADBEEF */

    /* Flush the write out to L2 so Core 0 sees it */
    dcbf    0, %r4
    sync

    /* Return to caller (core1_guest_entry which used bctrl) */
    blr
