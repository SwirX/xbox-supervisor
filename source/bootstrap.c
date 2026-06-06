#include <barrier.h>
#include "system_memory_map.h"
#include "ipc_ring.h"

/* Core 1 stack — pulled into .bss */
__attribute__((aligned(16))) uint8_t core1_stack[0x2000];

/* wait[] table from vectors.S — libxenon compatible dispatch */
extern volatile uint32_t wait[12];
extern void core1_process_engine(void);

static void supervisor_early_init(void);
static void bootstrap_core1(void);

/* ── Supervisor entry: called from vectors.S _start on Core 0 ── */

void supervisor_main(void)
{
    supervisor_early_init();
    bootstrap_core1();

    /* Main loop: check Core 1 heartbeat, dispatch IPC */
    volatile IpcStateFlags *flags = IPC_SYS_FLAGS;

    while (1) {
        cache_inval_line(&flags->in.core1_heartbeat);
        uint32_t hb = flags->in.core1_heartbeat;

        __asm__ __volatile__ ("or 27, 27, 27");
        (void)hb;
    }
}

/* ── Early init: zero IPC state, init rings ── */

void supervisor_early_init(void)
{
    volatile IpcStateFlags *flags = IPC_SYS_FLAGS;

    ipc_ring_init(IPC_CMD_RING);
    ipc_ring_init(IPC_RES_RING);

    flags->out.target_action     = 0;
    flags->out.supervisor_status = STATE_INIT;
    flags->in.current_state      = STATE_INIT;
    flags->in.core1_heartbeat    = 0;

    cache_flush_range((void *)flags, sizeof(IpcStateFlags));
    ppc_sync();
}

/* ── Bootstrap Core 1: write wait[] entry, signal wakeup ── */

void bootstrap_core1(void)
{
    volatile IpcStateFlags *flags = IPC_SYS_FLAGS;

    /* 1. Write function pointer + stack into wait[] for thread 2 (PIR=2) */
    wait[2 * 2]     = (uint32_t)core1_process_engine;
    wait[2 * 2 + 1] = (uint32_t)(core1_stack + sizeof(core1_stack) - 0x100);
    cache_flush_range((void *)&wait[4], 8);
    ppc_sync();

    /* 2. Wait for Core 1 to enter STATE_POLLING */
    while (flags->in.current_state != STATE_POLLING) {
        cache_inval_line(&flags->in.current_state);
    }

    /* 3. Send PING to validate IPC channel */
    IpcPacket ping = {
        .cmd_type    = CMD_PING,
        .sequence_id = 0xAA55,
    };
    ipc_ring_push((IpcRingBuffer *)IPC_CMD_RING, &ping);

    /* 4. Fire IPI at Core 1 (thread 2 block).
       Global routing at 0x20050010, per-thread trigger at 0x20052000. */
    mmio_st((void *)0x20050010, 0x140078ULL);
    mmio_st((void *)(0x20050000 + (2 * 0x1000)), 1ULL);
}
