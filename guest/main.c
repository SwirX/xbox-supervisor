#include <stdint.h>
#include <input/input.h>
#include <gfx.h>
#include <xenos/xenos.h>

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

static void wait_a_released(void)
{
    struct controller_data_s pad;
    do { pad = read_pad(); } while (pad.a);
}

static void wait_a_pressed(void)
{
    struct controller_data_s pad;
    do { pad = read_pad(); } while (!pad.a);
}

int main(void)
{
    GfxCtx gfx;
    gfx_init(&gfx);

    /* ── Test 1: Full-screen colors ── */

    /* RED */
    gfx_clear(&gfx, 0xFF0000FF);
    gfx_flush(&gfx);
    wait_a_released();
    wait_a_pressed();

    /* GREEN */
    gfx_clear(&gfx, 0x00FF00FF);
    gfx_flush(&gfx);
    wait_a_released();
    wait_a_pressed();

    /* BLUE */
    gfx_clear(&gfx, 0x0000FFFF);
    gfx_flush(&gfx);
    wait_a_released();
    wait_a_pressed();

    /* ── Test 2: Static "HELLO" once, never redraw ── */

    gfx_clear(&gfx, 0x202025FF);
    gfx_draw_str(&gfx, 20, 10, "HELLO", 0xFFFFFFFF, 0x202025FF, 200);
    gfx_flush(&gfx);

    /* Spin — no framebuffer writes, wait for A to advance */
    wait_a_released();
    wait_a_pressed();

    /* ── Test 3: Solid rectangle every frame ── */

    while (1) {
        struct controller_data_s pad = read_pad();

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

        gfx_flush(&gfx);

        if (pad.a) break;
        __asm__ volatile("or 27, 27, 27");
    }

    gfx_clear(&gfx, 0x000044FF);
    gfx_draw_str(&gfx, 20, 40, "EXITED", 0xFFFFFFFF, 0x000044FF, 100);
    gfx_flush(&gfx);

    return 0;
}
