#include <stdio.h>
#include <string.h>
#include <xenos/xenos.h>
#include <xenos/xe.h>
#include <xenos/edram.h>
#include <console/console.h>
#include <usb/usbmain.h>
#include <input/input.h>
#include "system_memory_map.h"
#include "ipc_ring.h"
#include "barrier.h"
#include "elf_format.h"

extern volatile uint32_t wait[];
extern void core1_process_engine(void);
extern int load_embedded_guest_elf(ElfExecPayload *out_payload);

#define CORE1_STACK_TOP  0x807E0000UL
#define PIR_SPR          1023
#define NUM_CONTROLLERS  4

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

    IPC_FLAGS_ADDR->out.supervisor_status = 0;
    cache_flush_range(&IPC_FLAGS_ADDR->out.supervisor_status, sizeof(uint32_t));

    IpcPacket cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_type    = CMD_EXEC_GUEST;
    cmd.sequence_id = 0x0001;
    memcpy(cmd.payload, &payload, sizeof(ElfExecPayload));

    while (!ipc_ring_push((IpcRingBuffer *)IPC_CMD_RING_ADDR, &cmd));
    printf("[GUEST] CMD_EXEC_GUEST sent to Core 1\n");

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

static void signal_pause(volatile IpcStateFlags *flags)
{
    flags->out.target_action = STATE_PAUSE;
    cache_flush_range(&flags->out.target_action, sizeof(uint32_t));
    ppc_sync();

    uint32_t timeout = 2000000;
    while (flags->in.current_state != STATE_PAUSED && timeout--) {
        cache_inval_line(&flags->in.current_state);
    }
}

static void signal_resume(volatile IpcStateFlags *flags)
{
    flags->out.target_action = STATE_RESUME;
    cache_flush_range(&flags->out.target_action, sizeof(uint32_t));
    ppc_sync();

    uint32_t timeout = 2000000;
    while (flags->in.current_state != STATE_POLLING && timeout--) {
        cache_inval_line(&flags->in.current_state);
    }
}

void main(void)
{
    struct XenosDevice xe;

    xenos_init(VIDEO_MODE_AUTO);
    Xe_Init(&xe);
    edram_init(&xe);
    console_init();
    console_clrscr();

    printf("XBOX SUPERVISOR v0.1\n");
    printf("====================\n");
    printf("Ring 0 libxenon based\n\n");

    uint32_t pir;
    __asm__ volatile("mfspr %0, %1" : "=r"(pir) : "i"(PIR_SPR));
    printf("[BOOT] Core 0 running, PIR=%u\n", pir);

    supervisor_early_init();
    printf("[BOOT] IPC shared memory @ 0x%08X\n", (uint32_t)IPC_SHMEM_BASE);

    boot_core1();

    launch_guest();

    printf("\n[INIT] Starting USB...\n");
    int usb_ok = usb_init();
    printf("[INIT] USB init: %s\n\n", usb_ok == 0 ? "OK" : "FAILED");

    printf("[SUPV] Entering main loop\n\n");

    int menu_open = 0;
    int guide_prev[NUM_CONTROLLERS] = {0};

    while (1) {
        volatile IpcStateFlags *flags = IPC_FLAGS_ADDR;

        usb_do_poll();

        for (int port = 0; port < NUM_CONTROLLERS; port++) {
            struct controller_data_s pad;
            get_controller_data(&pad, port);

            if (pad.logo && !guide_prev[port]) {
                if (!menu_open) {
                    printf("[GUIDE] Opening menu...\n");
                    signal_pause(flags);
                    if (flags->in.current_state == STATE_PAUSED) {
                        menu_open = 1;
                        printf("[GUIDE] Menu open. Press Guide to close.\n");
                    } else {
                        printf("[GUIDE] WARNING: Core 1 did not pause\n");
                    }
                } else {
                    printf("[GUIDE] Closing menu...\n");
                    signal_resume(flags);
                    if (flags->in.current_state == STATE_POLLING) {
                        menu_open = 0;
                        printf("[GUIDE] Menu closed.\n");
                    } else {
                        printf("[GUIDE] WARNING: Core 1 did not resume\n");
                    }
                }
            }
            guide_prev[port] = pad.logo;
        }

        __asm__ volatile("or 27, 27, 27");
    }
}
