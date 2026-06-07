#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cache.h>
#include <time/time.h>
#include <libfat/fat.h>
#include <xenon_smc/xenon_smc.h>
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
#include "svc_framebuffer.h"

extern volatile uint32_t wait[];
extern void core1_process_engine(void);

#define CORE1_STACK_TOP   0x807E0000UL
#define PIR_SPR           1023
#define NUM_CONTROLLERS   4
#define MENU_ITEMS        4
#define GUEST_BASE        0x81000000UL
#define CACHE_LINE_SIZE   128

static const char *menu_labels[MENU_ITEMS] = {
    "Resume Game",
    "Load from USB",
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

static void fill_fb_black(void)
{
    volatile FbInfo *fbi = (volatile FbInfo *)IPC_FB_INFO_ADDR;
    uint32_t *fb = (uint32_t *)fbi->base;
    int count = fbi->stride * fbi->height;
    for (int i = 0; i < count; i++)
        fb[i] = 0xFF000000;
    __asm__ volatile("sync" : : : "memory");
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
    int any_held = 1;
    while (any_held) {
        usb_do_poll();
        any_held = 0;
        for (int port = 0; port < NUM_CONTROLLERS; port++) {
            struct controller_data_s pad;
            get_controller_data(&pad, port);
            if (pad.logo)
                any_held = 1;
        }
    }

    int selection = 0;
    int prev_up = 0, prev_down = 0, prev_logo = 0, prev_a = 0;
    int dirty = 1;

    while (1) {
        usb_do_poll();

        for (int port = 0; port < NUM_CONTROLLERS; port++) {
            struct controller_data_s pad;
            get_controller_data(&pad, port);

            if (pad.logo && !prev_logo)
                return -1;

            if (pad.up && !prev_up && selection > 0) {
                selection--;
                dirty = 1;
                udelay(50000);
            }
            if (pad.down && !prev_down && selection < MENU_ITEMS - 1) {
                selection++;
                dirty = 1;
                udelay(50000);
            }

            if (pad.a && !prev_a)
                return selection;

            prev_up    = pad.up;
            prev_down  = pad.down;
            prev_a     = pad.a;
            prev_logo  = pad.logo;
        }

        if (dirty) {
            draw_menu(selection);
            dirty = 0;
        }

        __asm__ volatile("or 27, 27, 27");
    }
}

static int load_guest_from_usb(ElfExecPayload *out)
{
    FILE *f = fopen("uda:/payload.elf", "rb");
    if (!f) {
        printf("\n  No payload.elf on USB.\n");
        return -1;
    }

    Elf32_Ehdr ehdr;
    if (fread(&ehdr, 1, sizeof(ehdr), f) != sizeof(ehdr)) {
        printf("\n  Bad ELF header.\n");
        fclose(f);
        return -1;
    }

    if (ehdr.e_ident[0] != ELFMAG0 || ehdr.e_ident[1] != ELFMAG1 ||
        ehdr.e_ident[2] != ELFMAG2 || ehdr.e_ident[3] != ELFMAG3) {
        printf("\n  Not an ELF file.\n");
        fclose(f);
        return -1;
    }

    if (ehdr.e_machine != EM_PPC) {
        printf("\n  Not PowerPC ELF.\n");
        fclose(f);
        return -1;
    }

    printf("\n  Loading %s...\n", "payload.elf");

    Elf32_Phdr *phdrs = malloc(ehdr.e_phnum * sizeof(Elf32_Phdr));
    if (!phdrs) {
        fclose(f);
        return -1;
    }

    fseek(f, ehdr.e_phoff, SEEK_SET);
    if (fread(phdrs, sizeof(Elf32_Phdr), ehdr.e_phnum, f) != ehdr.e_phnum) {
        free(phdrs);
        fclose(f);
        return -1;
    }

    uint32_t first_text = 0;
    uint32_t total_text = 0;

    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD)
            continue;

        uint8_t *dest = (uint8_t *)phdrs[i].p_vaddr;

        if (phdrs[i].p_flags & PF_X) {
            if (first_text == 0)
                first_text = phdrs[i].p_vaddr;
            total_text += phdrs[i].p_memsz;
        }

        if (phdrs[i].p_filesz > 0) {
            fseek(f, phdrs[i].p_offset, SEEK_SET);
            if (fread(dest, 1, phdrs[i].p_filesz, f) != phdrs[i].p_filesz) {
                free(phdrs);
                fclose(f);
                return -1;
            }

            uint32_t fs = phdrs[i].p_vaddr & ~(CACHE_LINE_SIZE - 1);
            uint32_t fe = (phdrs[i].p_vaddr + phdrs[i].p_filesz + CACHE_LINE_SIZE - 1)
                          & ~(CACHE_LINE_SIZE - 1);
            for (uint32_t a = fs; a < fe; a += CACHE_LINE_SIZE)
                __asm__ volatile("dcbst 0, %0" : : "r"(a) : "memory");
        }

        if (phdrs[i].p_memsz > phdrs[i].p_filesz) {
            memset(dest + phdrs[i].p_filesz, 0, phdrs[i].p_memsz - phdrs[i].p_filesz);

            uint32_t fs = (phdrs[i].p_vaddr + phdrs[i].p_filesz) & ~(CACHE_LINE_SIZE - 1);
            uint32_t fe = (phdrs[i].p_vaddr + phdrs[i].p_memsz + CACHE_LINE_SIZE - 1)
                          & ~(CACHE_LINE_SIZE - 1);
            for (uint32_t a = fs; a < fe; a += CACHE_LINE_SIZE)
                __asm__ volatile("dcbst 0, %0" : : "r"(a) : "memory");
        }
    }

    __asm__ volatile("sync" : : : "memory");

    free(phdrs);
    fclose(f);

    out->entry_point      = ehdr.e_entry;
    out->stack_pointer    = 0x8F000000;
    out->guest_text_start = first_text;
    out->guest_text_size  = total_text;

    return 0;
}

static void send_exec_guest(ElfExecPayload *payload)
{
    IPC_FLAGS_ADDR->out.supervisor_status = 0;
    cache_flush_range(&IPC_FLAGS_ADDR->out.supervisor_status, sizeof(uint32_t));

    IpcPacket cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_type    = CMD_EXEC_GUEST;
    cmd.sequence_id = 0x0001;
    memcpy(cmd.payload, payload, sizeof(ElfExecPayload));

    while (!ipc_ring_push((IpcRingBuffer *)IPC_CMD_RING_ADDR, &cmd));

    uint32_t magic;
    uint32_t timeout = 2000000;
    do {
        cache_inval_line(&IPC_FLAGS_ADDR->out.supervisor_status);
        magic = IPC_FLAGS_ADDR->out.supervisor_status;
    } while (magic == 0 && timeout--);

    if (magic == 0xDEADBEEF) {
        printf("  Guest OK (0x%08X)\n", magic);
    } else if (magic != 0) {
        printf("  Guest resp 0x%08X\n", magic);
    } else {
        printf("  Guest timeout\n");
    }
}

static void execute_exit_to_xell(void)
{
    printf("\n  Returning to XeLL...\n");
    udelay(500000);
    xenon_smc_power_reboot();
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

    {
        volatile uint32_t *ati = (volatile uint32_t *)0xEC806100UL;
        volatile FbInfo *fbi = (volatile FbInfo *)IPC_FB_INFO_ADDR;
        uint32_t base = ati[4] | 0x80000000UL;
        uint32_t w    = ati[13];
        uint32_t h    = ati[14];
        fbi->base   = base;
        fbi->width  = w;
        fbi->height = h;
        fbi->stride = ((w + 31) >> 5) << 5;
        fbi->bpp    = 4;
        cache_flush_range((void *)fbi, sizeof(FbInfo));
    }
    boot_core1();

    printf("\n[INIT] USB...\n");
    usb_init();
    for (int i = 0; i < 50; i++) {
        usb_do_poll();
        udelay(10000);
    }
    fatInitDefault();

    printf("[SUPV] Ready. Press Guide for menu.\n");

    int menu_open = 0;
    int guide_prev[NUM_CONTROLLERS] = {0};
    ElfExecPayload usb_payload;

    while (1) {
        volatile IpcStateFlags *flags = IPC_FLAGS_ADDR;

        usb_do_poll();

        __asm__ volatile("eieio" : : : "memory");
        *IPC_INPUT_GEN_ADDR = *IPC_INPUT_GEN_ADDR + 1;
        __asm__ volatile("sync" : : : "memory");

        for (int port = 0; port < NUM_CONTROLLERS; port++) {
            struct controller_data_s pad;
            get_controller_data(&pad, port);

            IPC_SHARED_PAD_ADDR[port] = pad;

            if (pad.logo && !guide_prev[port]) {
                if (!menu_open) {
                    signal_pause(flags);
                    if (flags->in.current_state == STATE_PAUSED) {
                        menu_open = 1;
                        int result = handle_menu_input();

                        if (result == 0) {
                            signal_resume(flags);
                            fill_fb_black();
                            console_clrscr();
                            printf("\n[SUPV] Core 1 resumed.\n");
                        } else if (result == 1) {
                            if (load_guest_from_usb(&usb_payload) == 0) {
                                send_exec_guest(&usb_payload);
                            }
                            signal_resume(flags);
                            fill_fb_black();
                            console_clrscr();
                            printf("\n  Guest loaded. Press Guide for menu.\n");
                        } else if (result == 3) {
                            execute_exit_to_xell();
                        }
                        menu_open = 0;
                    }
                }
            }
            guide_prev[port] = pad.logo;
        }

        __asm__ volatile("eieio" : : : "memory");
        *IPC_INPUT_GEN_ADDR = *IPC_INPUT_GEN_ADDR + 1;
        __asm__ volatile("sync" : : : "memory");

        __asm__ volatile("or 27, 27, 27");
    }
}
