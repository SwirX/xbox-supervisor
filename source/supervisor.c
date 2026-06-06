/* Xbox 360 Supervisor — Ring 0 kernel.
 *
 * Built as a standard libxenon app. Core 0 runs main() with full GPU and
 * console output. Core 1 is booted by libxenon's CRT and spins on the
 * wait[] table — we write a function pointer there to launch the engine.
 *
 * Memory:
 *   0x80000000 - 0x800FFFFF   Supervisor code/data/stack (1 MB)
 *   0x800FF000 - 0x800FFFFF   IPC shared memory
 *   0x80100000 onwards         Guest app space (loaded ELFs)
 *
 * PIR values on Xbox 360: 0 (primary), 1 (secondary).
 * wait[2] = func ptr, wait[3] = stack ptr for Core 1.
 */

#include <stdio.h>
#include <string.h>
#include <xenos/xenos.h>
#include <xenos/xe.h>
#include <xenos/edram.h>
#include <console/console.h>
#include "system_memory_map.h"
#include "ipc_ring.h"
#include "barrier.h"

/* libxenon's Core 1 dispatch table — defined in crt1.o / startup_from_xell.S.
 * Non-boot cores spin here reading wait[PIR*2] and wait[PIR*2+1]. */
extern volatile uint32_t wait[];

/* Core 1's polling engine (defined in core1_engine.c) */
extern void core1_process_engine(void);

/* Core 1 stack: top of supervisor's reserved 8 MB */
#define CORE1_STACK_TOP        0x807E0000UL

static void boot_core1(void)
{
    volatile IpcStateFlags *flags = IPC_FLAGS_ADDR;

    /* libxenon already has Core 1 spinning on wait[]. On Xbox 360 the
     * secondary hardware thread has PIR=1, so wait offset is 2 entries.
     * We write the function and stack pointer — Core 1 picks them up
     * atomically (it checks wait[n]==0 before consuming). */
    wait[2] = (uint32_t)core1_process_engine;
    wait[3] = CORE1_STACK_TOP - 256;       /* 256B of red zone */
    ppc_sync();

    printf("[BOOT] Core 1 wait[] set: func=0x%08X stack=0x%08X\n",
           wait[2], wait[3]);

    /* Poll until Core 1 signals STATE_POLLING */
    uint32_t timeout = 5000000;
    while (flags->in.current_state != STATE_POLLING && timeout--) {
        cache_inval_line(&flags->in.current_state);
    }

    if (flags->in.current_state == STATE_POLLING) {
        printf("[BOOT] Core 1 online! (state=0x%08X)\n\n",
               flags->in.current_state);
    } else {
        printf("[BOOT] WARNING: Core 1 did not signal POLLING\n\n");
    }
}

static void supervisor_early_init(void)
{
    /* Zero IPC shared memory in supervisor's reserved region */
    memset((void *)IPC_SHMEM_BASE, 0, IPC_SHMEM_SIZE);

    /* Initialise ring buffers */
    ipc_ring_init((IpcRingBuffer *)IPC_CMD_RING_ADDR);
    ipc_ring_init((IpcRingBuffer *)IPC_RES_RING_ADDR);

    /* Set initial supervisor state */
    IPC_FLAGS_ADDR->out.supervisor_status = STATE_INIT;
    IPC_FLAGS_ADDR->in.current_state      = STATE_INIT;

    /* Push everything out to L2 */
    cache_flush_range((void *)IPC_SHMEM_BASE, IPC_SHMEM_SIZE);
}

void main(void)
{
    struct XenosDevice xe;

    /* ---- GPU / display init ---- */
    xenos_init(VIDEO_MODE_AUTO);
    Xe_Init(&xe);
    edram_init(&xe);
    console_init();
    console_clrscr();

    printf("XBOX SUPERVISOR v0.1\n");
    printf("====================\n");
    printf("Ring 0 — libxenon based\n\n");

    /* Detect our own PIR */
    uint32_t pir;
    __asm__ volatile("mfspr %0, 0x01B" : "=r"(pir));
    printf("[BOOT] Core 0 running, PIR=%u\n", pir);

    /* ---- Shared memory init ---- */
    supervisor_early_init();
    printf("[BOOT] IPC shared memory @ 0x%08X\n", (uint32_t)IPC_SHMEM_BASE);

    /* ---- Boot Core 1 via libxenon wait[] ---- */
    boot_core1();

    /* ---- Main loop ---- */
    printf("[SUPV] Entering main loop\n");
    printf("       CMD_RING @ 0x%08X\n", (uint32_t)(uintptr_t)IPC_CMD_RING_ADDR);
    printf("       RES_RING @ 0x%08X\n", (uint32_t)(uintptr_t)IPC_RES_RING_ADDR);
    printf("       FLAGS    @ 0x%08X\n\n", (uint32_t)(uintptr_t)IPC_FLAGS_ADDR);

    uint32_t tick = 0;
    while (1) {
        /* Check for responses from Core 1 */
        volatile IpcStateFlags *flags = IPC_FLAGS_ADDR;
        cache_inval_line(&flags->in.core1_heartbeat);

        if (flags->in.core1_heartbeat != tick) {
            tick = flags->in.core1_heartbeat;
            printf("[SUPV] Core 1 heartbeat: %u\n", tick);
        }

        /* TODO: Guide button polling via SMC
         * TODO: GPU compositing via Xenos ring buffers
         * TODO: Guest ELF load-and-execute IPC command */

        /* Brief yield to let Core 1 make progress */
        __asm__ volatile("or 27, 27, 27");
    }
}
