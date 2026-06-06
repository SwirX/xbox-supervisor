/* Xbox 360 Supervisor — Ring 0 kernel.
 *
 * Built as a standard libxenon app. Core 0 runs main() with full GPU and
 * console output. Core 1 is booted by libxenon's CRT and spins on the
 * wait[] table — we write a function pointer there to launch the engine.
 *
 * Memory:
 *   0x80000000 - 0x807FFFFF   Supervisor code/data/stack (8 MB)
 *   0x807FF000 - 0x807FFFFF   IPC shared memory
 *   0x80800000 onwards         Guest app space (loaded ELFs)
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
#include "elf_format.h"

/* libxenon's Core 1 dispatch table — defined in crt1.o / startup_from_xell.S */
extern volatile uint32_t wait[];

/* Core 1's polling engine (defined in core1_engine.c) */
extern void core1_process_engine(void);

/* ELF loader: parse embedded guest, load to VMA, fill ElfExecPayload */
extern int load_embedded_guest_elf(ElfExecPayload *out_payload);

/* Core 1 stack: within supervisor's 8 MB region */
#define CORE1_STACK_TOP        0x807E0000UL

static void boot_core1(void)
{
    volatile IpcStateFlags *flags = IPC_FLAGS_ADDR;

    wait[2] = (uint32_t)core1_process_engine;
    wait[3] = CORE1_STACK_TOP - 256;
    ppc_sync();

    printf("[BOOT] Core 1 wait[] set: func=0x%08X stack=0x%08X\n",
           wait[2], wait[3]);

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
    memset((void *)IPC_SHMEM_BASE, 0, IPC_SHMEM_SIZE);
    ipc_ring_init((IpcRingBuffer *)IPC_CMD_RING_ADDR);
    ipc_ring_init((IpcRingBuffer *)IPC_RES_RING_ADDR);
    IPC_FLAGS_ADDR->out.supervisor_status = STATE_INIT;
    IPC_FLAGS_ADDR->in.current_state      = STATE_INIT;
    cache_flush_range((void *)IPC_SHMEM_BASE, IPC_SHMEM_SIZE);
}

static void launch_guest(void)
{
    ElfExecPayload payload;
    int ret = load_embedded_guest_elf(&payload);

    if (ret < 0) {
        printf("[GUEST] No embedded ELF (err=%d) — skipping\n", ret);
        return;
    }

    printf("[GUEST] Embedded ELF loaded:\n");
    printf("        entry=0x%08X  stack=0x%08X\n",
           payload.entry_point, payload.stack_pointer);
    printf("        text_start=0x%08X  text_size=%u\n",
           payload.guest_text_start, payload.guest_text_size);

    /* Clear the supervisor_status flag so we can detect guest writeback */
    IPC_FLAGS_ADDR->out.supervisor_status = 0;
    cache_flush_range(&IPC_FLAGS_ADDR->out.supervisor_status, sizeof(uint32_t));

    /* Send CMD_EXEC_GUEST to Core 1 via IPC command ring */
    IpcPacket cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_type    = CMD_EXEC_GUEST;
    cmd.sequence_id = 0x0001;
    memcpy(cmd.payload, &payload, sizeof(ElfExecPayload));

    while (!ipc_ring_push((IpcRingBuffer *)IPC_CMD_RING_ADDR, &cmd));
    printf("[GUEST] CMD_EXEC_GUEST sent to Core 1\n");

    /* Wait for guest to write magic value back to supervisor_status */
    uint32_t magic;
    uint32_t timeout = 2000000;
    do {
        cache_inval_line(&IPC_FLAGS_ADDR->out.supervisor_status);
        magic = IPC_FLAGS_ADDR->out.supervisor_status;
    } while (magic == 0 && timeout--);

    if (magic == 0) {
        printf("[GUEST] WARNING: guest did not respond (timeout)\n");
    } else {
        printf("[GUEST] Guest returned! magic=0x%08X\n", magic);
    }
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

    uint32_t pir;
    __asm__ volatile("mfspr %0, 0x01B" : "=r"(pir));
    printf("[BOOT] Core 0 running, PIR=%u\n", pir);

    /* ---- Shared memory init ---- */
    supervisor_early_init();
    printf("[BOOT] IPC shared memory @ 0x%08X\n", (uint32_t)IPC_SHMEM_BASE);

    /* ---- Boot Core 1 via libxenon wait[] ---- */
    boot_core1();

    /* ---- Launch test guest on Core 1 ---- */
    launch_guest();

    /* ---- Main loop ---- */
    printf("\n[SUPV] Entering main loop\n");
    printf("       CMD_RING @ 0x%08X\n", (uint32_t)(uintptr_t)IPC_CMD_RING_ADDR);
    printf("       RES_RING @ 0x%08X\n", (uint32_t)(uintptr_t)IPC_RES_RING_ADDR);
    printf("       FLAGS    @ 0x%08X\n\n", (uint32_t)(uintptr_t)IPC_FLAGS_ADDR);

    uint32_t tick = 0;
    while (1) {
        volatile IpcStateFlags *flags = IPC_FLAGS_ADDR;
        cache_inval_line(&flags->in.core1_heartbeat);

        if (flags->in.core1_heartbeat != tick) {
            tick = flags->in.core1_heartbeat;
            printf("[SUPV] Core 1 heartbeat: %u\n", tick);
        }

        __asm__ volatile("or 27, 27, 27");
    }
}
