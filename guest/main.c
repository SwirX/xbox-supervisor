#include <stdint.h>
#include <input/input.h>
#include <gfx.h>

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

int main(void)
{
    GfxCtx gfx;
    gfx_init(&gfx);

    uint32_t bg = 0xFF202025;

    gfx_clear(&gfx, bg);
    gfx_draw_str(&gfx, 20, 10, "GUEST DASHBOARD", 0xFFFFFFFF, bg, 200);
    gfx_flush(&gfx);

    while (1) {
        struct controller_data_s pad = read_pad();

        int y = 40;
        uint32_t g = 0xFF4488CC, w = 0xFFFFFFFF;

        gfx_draw_str(&gfx, 20, y,    "A:",   g, bg, 32);
        gfx_draw_str(&gfx, 44, y,    pad.a ? "1" : "0", pad.a ? w : 0xFF444444, bg, 16);
        gfx_draw_str(&gfx, 90, y,    "B:",   g, bg, 32);
        gfx_draw_str(&gfx, 114, y,   pad.b ? "1" : "0", pad.b ? w : 0xFF444444, bg, 16);
        gfx_draw_str(&gfx, 150, y,   "X:",   g, bg, 32);
        gfx_draw_str(&gfx, 174, y,   pad.x ? "1" : "0", pad.x ? w : 0xFF444444, bg, 16);
        gfx_draw_str(&gfx, 210, y,   "Y:",   g, bg, 32);
        gfx_draw_str(&gfx, 234, y,   pad.y ? "1" : "0", pad.y ? w : 0xFF444444, bg, 16);

        y += 20;

        gfx_draw_str(&gfx, 300, y,   "LB:",  g, bg, 32);
        gfx_draw_str(&gfx, 340, y,   pad.lb ? "1" : "0", pad.lb ? w : 0xFF444444, bg, 16);
        gfx_draw_str(&gfx, 390, y,   "RB:",  g, bg, 32);
        gfx_draw_str(&gfx, 430, y,   pad.rb ? "1" : "0", pad.rb ? w : 0xFF444444, bg, 16);

        y += 20;

        gfx_draw_str(&gfx, 300, y,   "UP:",  g, bg, 32);
        gfx_draw_str(&gfx, 340, y,   pad.up ? "1" : "0", pad.up ? w : 0xFF444444, bg, 16);
        gfx_draw_str(&gfx, 390, y,   "DN:",  g, bg, 32);
        gfx_draw_str(&gfx, 430, y,   pad.down ? "1" : "0", pad.down ? w : 0xFF444444, bg, 16);

        y += 20;
        gfx_draw_str(&gfx, 20, y,    "LT:",  g, bg, 32);
        gfx_draw_int(&gfx, 56, y, pad.lt, w, bg);
        gfx_draw_str(&gfx, 150, y,   "RT:",  g, bg, 32);
        gfx_draw_int(&gfx, 186, y, pad.rt, w, bg);

        y += 20;
        gfx_draw_str(&gfx, 20, y,    "LX:",  g, bg, 32);
        gfx_draw_int(&gfx, 56, y, pad.s1_x, w, bg);
        gfx_draw_str(&gfx, 200, y,   "LY:",  g, bg, 32);
        gfx_draw_int(&gfx, 236, y, pad.s1_y, w, bg);

        y += 20;
        gfx_draw_str(&gfx, 20, y,    "RX:",  g, bg, 32);
        gfx_draw_int(&gfx, 56, y, pad.s2_x, w, bg);
        gfx_draw_str(&gfx, 200, y,   "RY:",  g, bg, 32);
        gfx_draw_int(&gfx, 236, y, pad.s2_y, w, bg);

        y += 30;
        gfx_draw_str(&gfx, 20, y,    "GUIDE=exit", 0xFF44CC88, bg, 100);

        gfx_flush(&gfx);

        if (pad.logo)
            break;
    }

    gfx_clear(&gfx, 0xFF000044);
    gfx_draw_str(&gfx, 20, 40, "EXITED", 0xFFFFFFFF, 0xFF000044, 100);
    gfx_flush(&gfx);

    return 0;
}
