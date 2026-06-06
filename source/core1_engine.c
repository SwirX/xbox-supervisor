#include <string.h>
#include "system_memory_map.h"
#include "ipc_ring.h"
#include "elf_format.h"
#include "barrier.h"

extern void core1_guest_entry(ElfExecPayload *payload);

void __attribute__((noreturn)) core1_process_engine(void)
{
    volatile IpcStateFlags *flags = IPC_FLAGS_ADDR;
    IpcPacket received;

    flags->in.current_state = STATE_POLLING;
    flags->in.core1_heartbeat = 0;
    cache_flush_range(&flags->in.current_state, sizeof(uint32_t));
    cache_flush_range(&flags->in.core1_heartbeat, sizeof(uint32_t));
    ppc_sync();

    while (1) {
        flags->in.core1_heartbeat++;

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
                ElfExecPayload *ep = (ElfExecPayload *)(received.payload);
                uint32_t vaddr = ep->guest_text_start;
                uint32_t size  = ep->guest_text_size;

                uint32_t start = vaddr & ~127;
                uint32_t end   = (vaddr + size + 127) & ~127;
                for (uint32_t a = start; a < end; a += 128)
                    __asm__ volatile("icbi 0, %0" : : "r"(a) : "memory");
                __asm__ volatile("isync" : : : "memory");

                uint32_t word = *(volatile uint32_t *)vaddr;

                IpcPacket resp;
                memset(&resp, 0, sizeof(resp));
                resp.cmd_type    = received.cmd_type;
                resp.sequence_id = received.sequence_id;
                ((uint32_t *)resp.payload)[0] = word;

                while (!ipc_ring_push((IpcRingBuffer *)IPC_RES_RING_ADDR, &resp));

            } else if (received.cmd_type == CMD_EXEC_GUEST) {
                ElfExecPayload *ep = (ElfExecPayload *)(received.payload);
                core1_guest_entry(ep);
            }
        }

        __asm__ volatile("or 27, 27, 27");
    }
}
