/* Core 1 engine — IPC polling loop.
 *
 * Booted by the supervisor via libxenon's wait[] table. Runs on Core 1 (PIR=1)
 * with full SLB identity mapping already set up by the CRT.
 *
 * Does NOT use printf — all debug output goes through IPC state flags so
 * Core 0's main loop can print it to the console safely.
 */

#include <string.h>
#include "system_memory_map.h"
#include "ipc_ring.h"
#include "elf_format.h"
#include "barrier.h"

/* Assembly dispatch for guest execution (defined in guest.S) */
extern void core1_guest_entry(ElfExecPayload *payload);

void __attribute__((noreturn)) core1_process_engine(void)
{
    volatile IpcStateFlags *flags = IPC_FLAGS_ADDR;
    IpcPacket received;

    /* Announce online */
    flags->in.current_state = STATE_POLLING;
    flags->in.core1_heartbeat = 0;
    cache_flush_range(&flags->in.current_state, sizeof(uint32_t));
    cache_flush_range(&flags->in.core1_heartbeat, sizeof(uint32_t));
    ppc_sync();

    while (1) {
        flags->in.core1_heartbeat++;

        /* Invalidate cmd_ring from L1/L2 to see Core 0's writes */
        __asm__ volatile("dcbf 0, %0" : : "r"((uint32_t)IPC_CMD_RING_ADDR & ~0x7F));
        __asm__ volatile("sync");

        if (ipc_ring_pop((IpcRingBuffer *)IPC_CMD_RING_ADDR, &received)) {

            if (received.cmd_type == CMD_PING) {
                IpcPacket pong;
                memset(&pong, 0, sizeof(pong));
                pong.cmd_type    = RES_PONG;
                pong.sequence_id = received.sequence_id;

                while (!ipc_ring_push((IpcRingBuffer *)IPC_RES_RING_ADDR, &pong));

            } else if (received.cmd_type == CMD_VERIFY_GUEST) {
                /* Pre-flight coherency: icbi the guest code region */
                ElfExecPayload *ep = (ElfExecPayload *)(received.payload);
                uint32_t vaddr = ep->guest_text_start;
                uint32_t size  = ep->guest_text_size;

                uint32_t start = vaddr & ~127;
                uint32_t end   = (vaddr + size + 127) & ~127;
                for (uint32_t a = start; a < end; a += 128)
                    __asm__ volatile("icbi 0, %0" : : "r"(a) : "memory");
                __asm__ volatile("isync" : : : "memory");

                /* Read first word back to confirm coherency */
                uint32_t word = *(volatile uint32_t *)vaddr;

                IpcPacket resp;
                memset(&resp, 0, sizeof(resp));
                resp.cmd_type    = received.cmd_type;
                resp.sequence_id = received.sequence_id;
                ((uint32_t *)resp.payload)[0] = word;

                while (!ipc_ring_push((IpcRingBuffer *)IPC_RES_RING_ADDR, &resp));

            } else if (received.cmd_type == CMD_EXEC_GUEST) {
                /* Jump to guest code — never returns */
                ElfExecPayload *ep = (ElfExecPayload *)(received.payload);
                core1_guest_entry(ep);
                /* unreachable */
            }
        }

        /* Thread yield hint */
        __asm__ volatile("or 27, 27, 27");
    }
}
