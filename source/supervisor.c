#include <stdio.h>
#include <string.h>
#include <cache.h>
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
#define MENU_ITEMS       3

static const char *menu_labels[MENU_ITEMS] = {
    "Resume Game",
    "Settings",
    "Exit to XeLL"
};

static void boot_core1(void)
{
    volatile IpcStateFlags *flags = IPC_FLAGS_ADDR;

    wait[2] = (uint32_t)core1_process_engine;
    wait[3] = CORE1_STACK_TOP - 256;
    ppc_sync();

    uint32_t timeout = 5000000;
    while (flags->in.current_state != STATE_POLLING && timeout--) {
        cache_inval_line(&flags->in.current_state);
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

    if (ret < 0)
        return;

    IPC_FLAGS_ADDR->out.supervisor_status = 0;
    cache_flush_range(&IPC_FLAGS_ADDR->out.supervisor_status, sizeof(uint32_t));

    IpcPacket cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_type    = CMD_EXEC_GUEST;
    cmd.sequence_id = 0x0001;
    memcpy(cmd.payload, &payload, sizeof(ElfExecPayload));

    while (!ipc_ring_push((IpcRingBuffer *)IPC_CMD_RING_ADDR, &cmd));

    uint32_t magic;
    uint32_t timeout = 2000000;
    do {
        cache_inval_line(&IPC_FLAGS_ADDR->out.supervisor_status);
        magic = IPC_FLAGS_ADDR->out.supervisor_status;
    } while (magic == 0 && timeout--);
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

static void draw_menu(int selection)
{
    console_clrscr();
    printf("\n  GUIDE MENU\n");
    printf("  ==========\n\n");

    for (int i = 0; i < MENU_ITEMS; i++) {
        if (i == selection)
            printf("  > %s\n", menu_labels[i]);
        else
            printf("    %s\n", menu_labels[i]);
    }

    printf("\n  A=Select  Guide=Close\n");
}

static int handle_menu_input(void)
{
    int selection = 0;
    int prev_up = 0, prev_down = 0, prev_a = 0;
    int dirty = 1;

    while (1) {
        usb_do_poll();

        for (int port = 0; port < NUM_CONTROLLERS; port++) {
            struct controller_data_s pad;
            get_controller_data(&pad, port);

            if (pad.logo)
                return -1;

            if (pad.up && !prev_up && selection > 0) {
                selection--;
                dirty = 1;
            }
            if (pad.down && !prev_down && selection < MENU_ITEMS - 1) {
                selection++;
                dirty = 1;
            }

            if (pad.a && !prev_a)
                return selection;

            prev_up   = pad.up;
            prev_down = pad.down;
            prev_a    = pad.a;
        }

        if (dirty) {
            draw_menu(selection);
            dirty = 0;
        }

        __asm__ volatile("or 27, 27, 27");
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

    uint32_t pir;
    __asm__ volatile("mfspr %0, %1" : "=r"(pir) : "i"(PIR_SPR));

    supervisor_early_init();
    boot_core1();
    launch_guest();
    usb_init();

    printf("\n[SUPV] Ready. Press Guide for menu.\n");

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
                    signal_pause(flags);
                    if (flags->in.current_state == STATE_PAUSED) {
                        menu_open = 1;
                        int result = handle_menu_input();

                        if (result >= 0) {
                            printf("\n  Selected: %s\n", menu_labels[result]);
                            if (result == 2) {
                                printf("\n  Exit not implemented yet.\n");
                            }
                        }

                        signal_resume(flags);
                        console_clrscr();
                        printf("\n[SUPV] Core 1 resumed.\n");
                        menu_open = 0;
                    }
                }
            }
            guide_prev[port] = pad.logo;
        }

        __asm__ volatile("or 27, 27, 27");
    }
}
