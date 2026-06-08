#include <stdint.h>
#include <string.h>
#include <input/input.h>
#include <gfx.h>
#include <xenos/xenos.h>
#include "system_memory_map.h"
#include "barrier.h"

static int ring_push(volatile IpcRingBuffer *ring, const IpcPacket *pkt)
{
    uint32_t head = ring->head;
    uint32_t tail = ring->tail;
    if (((head + 1) & 15) == tail)
        return 0;
    ring->slots[head] = *pkt;
    ppc_sync();
    ring->head = (head + 1) & 15;
    return 1;
}

#define IPC_INPUT_GEN    ((volatile uint32_t *)0x807FFCFCUL)
#define IPC_SHARED_PAD ((volatile uint32_t *)0x807FFD00UL)

static struct controller_data_s read_pad(void)
{
    uint32_t g1, g2;
    struct controller_data_s pad;
    do {
        g1 = *IPC_INPUT_GEN;
        __asm__ volatile("sync" : : : "memory");
        pad = *(volatile struct controller_data_s *)IPC_SHARED_PAD;
        __asm__ volatile("sync" : : : "memory");
        g2 = *IPC_INPUT_GEN;
    } while (g1 != g2 || (g1 & 1));
    return pad;
}

static void vfb_present(void)
{
    __asm__ volatile("sync" : : : "memory");
    *VFB_DIRTY_ADDR = 1;
    __asm__ volatile("sync" : : : "memory");
}

static int should_exit(void)
{
    __asm__ volatile("dcbf 0, %0; sync; isync" : : "r"(IPC_FLAGS_ADDR) : "memory");
    return IPC_FLAGS_ADDR->out.target_action == STATE_PAUSE;
}

static int wait_a_released(void)
{
    struct controller_data_s pad;
    do {
        pad = read_pad();
        if (should_exit()) return -1;
    } while (pad.a);
    return 0;
}

static int wait_a_pressed(void)
{
    struct controller_data_s pad;
    do {
        pad = read_pad();
        if (should_exit()) return -1;
    } while (!pad.a);
    return 0;
}

int main(void)
{
    GfxCtx gfx;
    gfx_init(&gfx);

    /* ── Test 1: Full-screen colors ── */

    gfx_clear(&gfx, 0xFF0000FF);
    gfx_flush(&gfx); vfb_present();
    if (wait_a_released() < 0) return 1;
    if (wait_a_pressed() < 0) return 1;

    gfx_clear(&gfx, 0x00FF00FF);
    gfx_flush(&gfx); vfb_present();
    if (wait_a_released() < 0) return 1;
    if (wait_a_pressed() < 0) return 1;

    gfx_clear(&gfx, 0x0000FFFF);
    gfx_flush(&gfx); vfb_present();
    if (wait_a_released() < 0) return 1;
    if (wait_a_pressed() < 0) return 1;

    /* ── Test 2: Static "HELLO" once, never redraw ── */

    gfx_clear(&gfx, 0x202025FF);
    gfx_draw_str(&gfx, 20, 10, "HELLO", 0xFFFFFFFF, 0x202025FF, 200);
    gfx_flush(&gfx); vfb_present();

    if (wait_a_released() < 0) return 1;
    if (wait_a_pressed() < 0) return 1;

    /* ── Test 3: Solid rectangle every frame ── */
    if (wait_a_released() < 0) return 1;

    while (1) {
        struct controller_data_s pad = read_pad();

        if (should_exit()) return 1;

        gfx_clear(&gfx, 0x202025FF);

        for (int y = 0; y < 720; y += 4)
            for (int x = 0; x < 640; x += 4)
                gfx.fb[gfx_tile_idx(x, y, gfx.stride)] = 0xFF0000FF;

        for (int y = 0; y < 720; y += 4)
            for (int x = 640; x < 1280; x += 4)
                gfx.fb[gfx_tile_idx(x, y, gfx.stride)] = 0x00FF00FF;

        for (int y = 360; y < 720; y += 4)
            for (int x = 0; x < 640; x += 4)
                gfx.fb[gfx_tile_idx(x, y, gfx.stride)] = 0x0000FFFF;

        gfx_flush(&gfx); vfb_present();

        if (pad.a) break;
        __asm__ volatile("or 27, 27, 27");
    }

    gfx_clear(&gfx, 0x000044FF);
    gfx_draw_str(&gfx, 20, 40, "EXITED", 0xFFFFFFFF, 0x000044FF, 100);
    gfx_flush(&gfx); vfb_present();

    IpcPacket npkt;
    memset(&npkt, 0, sizeof(npkt));
    npkt.cmd_type = SVC_NOTIFY;
    memcpy(npkt.payload, "Xenolith ready", 14);
    while (!ring_push((IpcRingBuffer *)IPC_RES_RING_ADDR, &npkt));

    return 0;
}
