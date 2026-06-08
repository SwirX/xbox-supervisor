#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <cache.h>
#include <time/time.h>
#include <libfat/fat.h>
#include <xenon_smc/xenon_smc.h>
#include <xenon_soc/xenon_power.h>
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
#include "gfx.h"

extern volatile uint32_t wait[];
extern void core1_process_engine(void);
extern void wakeup_cpus(void);

#define CORE1_STACK_TOP   0x807E0000UL
#define PIR_SPR           1023
#define NUM_CONTROLLERS   4
#define MENU_ITEMS        4
#define GUEST_BASE        0x81000000UL
#define CACHE_LINE_SIZE   128
#define BLADE_W           320

/* ── Color palette — RGBA byte order (Byte0=R, Byte1=G, Byte2=B, Byte3=A) ── */
#define PAL_BLADE_BG     0x101010BB
#define PAL_STRIP_BG     0x1A1A1AFF
#define PAL_ACCENT       0x00FF66FF
#define PAL_TEXT_SEL     0xFFFFFFFF
#define PAL_TEXT_UNSEL   0x888888FF
#define PAL_TEXT_HINT    0x666666FF
#define PAL_RTC          0x00FF66FF

/* ── Clock overlay region (top-right, outside blade) ── */
#define CLOCK_SX         1100
#define CLOCK_SY         32
#define CLOCK_SW         180
#define CLOCK_SH         56

/* ── ARGB ↔ RGBA byte-order conversion (Guest ARGB vs Supervisor RGBA) ── */
static inline uint32_t argb_to_rgba(uint32_t p)
{
    uint8_t a = (p >> 24) & 0xFF;
    uint8_t r = (p >> 16) & 0xFF;
    uint8_t g = (p >> 8) & 0xFF;
    uint8_t b = p & 0xFF;
    return (r << 24) | (g << 16) | (b << 8) | a;
}

static inline uint32_t rgba_to_argb(uint32_t p)
{
    uint8_t r = (p >> 24) & 0xFF;
    uint8_t g = (p >> 16) & 0xFF;
    uint8_t b = (p >> 8) & 0xFF;
    uint8_t a = p & 0xFF;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

/* ── Input repeat state machine ── */
#define TB_PER_US        800
#define INITIAL_HOLD_MS  500
#define REPEAT_INTERVAL_MS 100
#define INITIAL_HOLD_TICKS  ((uint64_t)INITIAL_HOLD_MS * 1000 * TB_PER_US)
#define REPEAT_TICKS        ((uint64_t)REPEAT_INTERVAL_MS * 1000 * TB_PER_US)
#define LS_DEADZONE       15000

static inline uint64_t tb_ticks(void)
{
    uint32_t tbl, tbu;
    __asm__ volatile("mftbu %0; mftb %1" : "=r"(tbu), "=r"(tbl) : : "memory");
    return ((uint64_t)tbu << 32) | tbl;
}

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

static void update_pads_and_gen(void)
{
    __asm__ volatile("eieio" : : : "memory");
    *IPC_INPUT_GEN_ADDR = *IPC_INPUT_GEN_ADDR + 1;
    __asm__ volatile("sync" : : : "memory");
    for (int port = 0; port < NUM_CONTROLLERS; port++) {
        struct controller_data_s pad;
        get_controller_data(&pad, port);
        pad.logo = 0;
        IPC_SHARED_PAD_ADDR[port] = pad;
    }
    __asm__ volatile("eieio" : : : "memory");
    *IPC_INPUT_GEN_ADDR = *IPC_INPUT_GEN_ADDR + 1;
    __asm__ volatile("sync" : : : "memory");
}

static uint32_t guide_backup[BLADE_W * 720] __attribute__((aligned(128)));
static uint32_t clock_backup[CLOCK_SW * CLOCK_SH] __attribute__((aligned(128)));

static void restore_blade_and_clock(const GfxCtx *gfx)
{
    for (int row = 0; row < gfx->height; row++)
        for (int col = 0; col < BLADE_W; col++)
            gfx->fb[gfx_tile_idx(col, row, gfx->stride)] =
                rgba_to_argb(guide_backup[row * BLADE_W + col]);
    for (int row = 0; row < CLOCK_SH; row++)
        for (int col = 0; col < CLOCK_SW; col++)
            gfx->fb[gfx_tile_idx(CLOCK_SX + col, CLOCK_SY + row, gfx->stride)] =
                rgba_to_argb(clock_backup[row * CLOCK_SW + col]);
    gfx_flush(gfx);
}

static void draw_clock(const GfxCtx *gfx)
{
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    char tbuf[16], dbuf[32];
    strftime(tbuf, sizeof(tbuf), "%H:%M", lt);
    strftime(dbuf, sizeof(dbuf), "%a, %b %d", lt);
    int tx_time = 1280 - 40 - ((int)strlen(tbuf) * 8);
    int tx_date = 1280 - 40 - ((int)strlen(dbuf) * 8);
    gfx_draw_str(gfx, tx_time, 36, tbuf, PAL_RTC, 0, 0);
    gfx_draw_str(gfx, tx_date, 56, dbuf, PAL_RTC, 0, 0);
}

static int run_guide_menu(const GfxCtx *gfx)
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

    for (int row = 0; row < gfx->height; row++)
        for (int col = 0; col < BLADE_W; col++)
            guide_backup[row * BLADE_W + col] =
                argb_to_rgba(gfx->fb[gfx_tile_idx(col, row, gfx->stride)]);
    for (int row = 0; row < CLOCK_SH; row++)
        for (int col = 0; col < CLOCK_SW; col++)
            clock_backup[row * CLOCK_SW + col] =
                argb_to_rgba(gfx->fb[gfx_tile_idx(CLOCK_SX + col, CLOCK_SY + row, gfx->stride)]);

    gfx_fill_rect(gfx, 0, 0, BLADE_W, gfx->height, PAL_BLADE_BG);
    gfx_fill_rect(gfx, BLADE_W - 1, 0, 1, gfx->height, PAL_ACCENT);
    gfx_fill_rect(gfx, 8, 8, BLADE_W - 16, 24, PAL_STRIP_BG);
    gfx_draw_str(gfx, 16, 12, "GUIDE", PAL_ACCENT, 0, 0);
    gfx_fill_rect(gfx, 16, 34, BLADE_W - 32, 1, PAL_ACCENT);

    draw_clock(gfx);

    {
        uint32_t b = (uint32_t)gfx->fb;
        uint32_t e = b + gfx->stride * gfx->height * gfx->bpp;
        for (uint32_t p = b & ~0x7F; p < e; p += 128)
            __asm__ volatile("dcbf 0, %0" : : "r"(p) : "memory");
        __asm__ volatile("sync" : : : "memory");
    }

    int selection = 0;
    int prev_up = 0, prev_down = 0, prev_logo = 0, prev_a = 0;
    int ls_up_prev = 0, ls_down_prev = 0;
    int active_dir = 0, repeat_phase = 0;
    uint64_t hold_start = 0, last_repeat = 0;
    int dirty = 1;
    time_t last_clock = time(NULL);

    while (1) {
        usb_do_poll();

        time_t now_sec = time(NULL);
        if (now_sec != last_clock) {
            last_clock = now_sec;
            for (int row = 0; row < CLOCK_SH; row++)
                for (int col = 0; col < CLOCK_SW; col++)
                    gfx->fb[gfx_tile_idx(CLOCK_SX + col, CLOCK_SY + row, gfx->stride)] =
                        rgba_to_argb(clock_backup[row * CLOCK_SW + col]);
            draw_clock(gfx);
            {
                uint32_t b = (uint32_t)gfx->fb + CLOCK_SY * gfx->stride * gfx->bpp;
                uint32_t e = b + CLOCK_SH * gfx->stride * gfx->bpp;
                for (uint32_t p = b & ~0x7F; p < e; p += 128)
                    __asm__ volatile("dcbf 0, %0" : : "r"(p) : "memory");
                __asm__ volatile("sync" : : : "memory");
            }
        }

        if (dirty) {
            for (int i = 0; i < MENU_ITEMS; i++) {
                int item_y = 40 + i * 28;
                gfx_fill_rect(gfx, 0, item_y - 2, BLADE_W - 1, 20, PAL_BLADE_BG);
                if (i == selection) {
                    gfx_fill_rect(gfx, 8, item_y - 2, BLADE_W - 16, 20, PAL_STRIP_BG);
                    gfx_draw_str(gfx, 16, item_y, menu_labels[i], PAL_TEXT_SEL, 0, 0);
                } else {
                    gfx_draw_str(gfx, 16, item_y, menu_labels[i], PAL_TEXT_UNSEL, 0, 0);
                }
            }

            gfx_draw_str(gfx, 16, 40 + MENU_ITEMS * 28 + 8,
                         "A=Select  Guide=Close", PAL_TEXT_HINT, 0, 0);

            {
                uint32_t b = (uint32_t)gfx->fb;
                uint32_t e = b + BLADE_W * gfx->height * 4;
                for (uint32_t p = b & ~0x7F; p < e; p += 128)
                    __asm__ volatile("dcbf 0, %0" : : "r"(p) : "memory");
                __asm__ volatile("sync" : : : "memory");
            }

            dirty = 0;
        }

        int want_up = 0, want_down = 0;
        struct controller_data_s pad;
        memset(&pad, 0, sizeof(pad));
        for (int port = 0; port < NUM_CONTROLLERS; port++) {
            struct controller_data_s raw;
            get_controller_data(&raw, port);
            if (raw.logo || raw.a || raw.up || raw.down ||
                raw.left || raw.right || raw.s1_y || raw.s1_x)
                pad = raw;
        }

        if (pad.logo && !prev_logo) {
            restore_blade_and_clock(gfx);
            return -1;
        }
        if (pad.a && !prev_a) {
            restore_blade_and_clock(gfx);
            return selection;
        }

        int ls_up = (pad.s1_y < -LS_DEADZONE);
        int ls_down = (pad.s1_y > LS_DEADZONE);
        int held_up = pad.up || ls_up;
        int held_down = pad.down || ls_down;

        if (pad.up && !prev_up) want_up = 1;
        if (pad.down && !prev_down) want_down = 1;
        if (ls_up && !ls_up_prev) want_up = 1;
        if (ls_down && !ls_down_prev) want_down = 1;

        if (want_up || want_down) {
            active_dir = want_up ? -1 : 1;
            hold_start = tb_ticks();
            last_repeat = hold_start;
            repeat_phase = 0;
            if (want_up && selection > 0) { selection--; dirty = 1; }
            if (want_down && selection < MENU_ITEMS - 1) { selection++; dirty = 1; }
        }

        if (active_dir != 0) {
            int still = (active_dir == -1 && held_up) || (active_dir == 1 && held_down);
            if (!still) {
                active_dir = 0;
                repeat_phase = 0;
            } else {
                uint64_t n = tb_ticks();
                uint64_t elapsed = n - hold_start;
                if (!repeat_phase) {
                    if (elapsed >= INITIAL_HOLD_TICKS) {
                        repeat_phase = 1;
                        last_repeat = n;
                        if (active_dir == -1 && selection > 0) { selection--; dirty = 1; }
                        if (active_dir == 1 && selection < MENU_ITEMS - 1) { selection++; dirty = 1; }
                    }
                } else {
                    if (n - last_repeat >= REPEAT_TICKS) {
                        last_repeat += REPEAT_TICKS;
                        if (active_dir == -1 && selection > 0) { selection--; dirty = 1; }
                        if (active_dir == 1 && selection < MENU_ITEMS - 1) { selection++; dirty = 1; }
                    }
                }
            }
        }

        prev_up    = pad.up;
        prev_down  = pad.down;
        prev_a     = pad.a;
        prev_logo  = pad.logo;
        ls_up_prev = ls_up;
        ls_down_prev = ls_down;

        if (dirty) {
            char dbg[64];
            int dy = 40 + MENU_ITEMS * 28 + 8 + 18;
            snprintf(dbg, sizeof(dbg), "U:%d D:%d L:%d R:%d Y:%d",
                     pad.up, pad.down, pad.left, pad.right, pad.s1_y);
            gfx_fill_rect(gfx, 8, dy - 2, BLADE_W - 16, 20, PAL_BLADE_BG);
            gfx_draw_str(gfx, 16, dy, dbg, PAL_TEXT_HINT, 0, 0);
            {
                uint32_t b = (uint32_t)gfx->fb;
                uint32_t e = b + BLADE_W * gfx->height * 4;
                for (uint32_t p = b & ~0x7F; p < e; p += 128)
                    __asm__ volatile("dcbf 0, %0" : : "r"(p) : "memory");
                __asm__ volatile("sync" : : : "memory");
            }
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

static void service_loop(GfxCtx *gfx, ElfExecPayload *usb_payload)
{
    int menu_open = 0;
    int guide_prev[NUM_CONTROLLERS] = {0};

    while (1) {
        volatile IpcStateFlags *flags = IPC_FLAGS_ADDR;

        usb_do_poll();

        __asm__ volatile("eieio" : : : "memory");
        *IPC_INPUT_GEN_ADDR = *IPC_INPUT_GEN_ADDR + 1;
        __asm__ volatile("sync" : : : "memory");

        for (int port = 0; port < NUM_CONTROLLERS; port++) {
            struct controller_data_s pad;
            get_controller_data(&pad, port);

            if (pad.logo && !guide_prev[port]) {
                if (!menu_open) {
                    flags->out.target_action = STATE_PAUSE;
                    cache_flush_range(&flags->out.target_action, sizeof(uint32_t));
                    ppc_sync();
                    xenon_sleep_thread(1);
                    ppc_sync();

                    menu_open = 1;
                    int result = run_guide_menu(gfx);

                    wakeup_cpus();
                    ppc_sync();
                    flags->out.target_action = STATE_RESUME;
                    cache_flush_range(&flags->out.target_action, sizeof(uint32_t));
                    ppc_sync();
                    if (result == 0) {
                        /* resume */
                    } else if (result == 1) {
                        if (load_guest_from_usb(usb_payload) == 0) {
                            update_pads_and_gen();
                            send_exec_guest(usb_payload);
                        }
                        gfx_clear(gfx, 0x000000FF);
                        gfx_flush(gfx);
                    } else if (result == 3) {
                        execute_exit_to_xell();
                    }
                    menu_open = 0;
                }
            }
            guide_prev[port] = pad.logo;

            pad.logo = 0;
            IPC_SHARED_PAD_ADDR[port] = pad;
        }

        __asm__ volatile("eieio" : : : "memory");
        *IPC_INPUT_GEN_ADDR = *IPC_INPUT_GEN_ADDR + 1;
        __asm__ volatile("sync" : : : "memory");

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

    GfxCtx gfx_sv;
    gfx_init(&gfx_sv);

    boot_core1();

    printf("\n[INIT] USB...\n");
    usb_init();
    for (int i = 0; i < 50; i++) {
        usb_do_poll();
        udelay(10000);
    }
    fatInitDefault();

    printf("[SUPV] Ready. Press Guide for menu.\n");

    ElfExecPayload usb_payload;
    service_loop(&gfx_sv, &usb_payload);
}
