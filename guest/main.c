#include <stdint.h>
#include <svc_framebuffer.h>

#define IPC_INPUT_GEN    ((volatile uint32_t *)0x807FFCFCUL)
#define IPC_FB_INFO_ADDR 0x807FFE80UL
#define IPC_SHARED_PAD ((volatile uint32_t *)0x807FFD00UL)

struct pad_state {
    signed short s1_x, s1_y, s2_x, s2_y;
    int s1_z, s2_z, lb, rb, start, back, a, b, x, y, up, down, left, right;
    unsigned char lt, rt;
    int logo;
};

static struct pad_state read_pad(void)
{
    uint32_t g1, g2;
    struct pad_state pad;
    do {
        g1 = *IPC_INPUT_GEN;
        __asm__ volatile("sync" : : : "memory");
        pad = *(volatile struct pad_state *)IPC_SHARED_PAD;
        __asm__ volatile("sync" : : : "memory");
        g2 = *IPC_INPUT_GEN;
    } while (g1 != g2 || (g1 & 1));
    return pad;
}

int main(void)
{
    volatile FbInfo *fbi = (volatile FbInfo *)IPC_FB_INFO_ADDR;
    int fb_w = fbi->width;
    int fb_h = fbi->height;
    int s    = fbi->stride;
    uint32_t *fb = (uint32_t *)fbi->base;
    uint32_t fb_sz = fb_h * s * fbi->bpp;

    uint32_t bg = 0xFF202025;

    fb_fill_solid(bg, fb, s, fb_h);
    fb_draw_str_bg(20, 10, "GUEST DASHBOARD", 0xFFFFFFFF, bg, fb, fb_w, fb_h, s, 200);
    fb_flush(fbi->base, fb_sz);

    while (1) {
        struct pad_state pad = read_pad();

        int y = 40;
        uint32_t g = 0xFF4488CC, w = 0xFFFFFFFF;

        fb_draw_str_bg(20, y,    "A:",   g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_str_bg(44, y,    pad.a ? "1" : "0", pad.a ? w : 0xFF444444, bg, fb, fb_w, fb_h, s, 16);
        fb_draw_str_bg(90, y,    "B:",   g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_str_bg(114, y,   pad.b ? "1" : "0", pad.b ? w : 0xFF444444, bg, fb, fb_w, fb_h, s, 16);
        fb_draw_str_bg(150, y,   "X:",   g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_str_bg(174, y,   pad.x ? "1" : "0", pad.x ? w : 0xFF444444, bg, fb, fb_w, fb_h, s, 16);
        fb_draw_str_bg(210, y,   "Y:",   g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_str_bg(234, y,   pad.y ? "1" : "0", pad.y ? w : 0xFF444444, bg, fb, fb_w, fb_h, s, 16);

        y += 20;

        fb_draw_str_bg(300, y,   "LB:",  g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_str_bg(340, y,   pad.lb ? "1" : "0", pad.lb ? w : 0xFF444444, bg, fb, fb_w, fb_h, s, 16);
        fb_draw_str_bg(390, y,   "RB:",  g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_str_bg(430, y,   pad.rb ? "1" : "0", pad.rb ? w : 0xFF444444, bg, fb, fb_w, fb_h, s, 16);

        y += 20;

        fb_draw_str_bg(300, y,   "UP:",  g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_str_bg(340, y,   pad.up ? "1" : "0", pad.up ? w : 0xFF444444, bg, fb, fb_w, fb_h, s, 16);
        fb_draw_str_bg(390, y,   "DN:",  g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_str_bg(430, y,   pad.down ? "1" : "0", pad.down ? w : 0xFF444444, bg, fb, fb_w, fb_h, s, 16);

        y += 20;
        fb_draw_str_bg(20, y,    "LT:",  g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_int(56, y, pad.lt, w, bg, fb, fb_w, fb_h, s);
        fb_draw_str_bg(150, y,   "RT:",  g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_int(186, y, pad.rt, w, bg, fb, fb_w, fb_h, s);

        y += 20;
        fb_draw_str_bg(20, y,    "LX:",  g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_int(56, y, pad.s1_x, w, bg, fb, fb_w, fb_h, s);
        fb_draw_str_bg(200, y,   "LY:",  g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_int(236, y, pad.s1_y, w, bg, fb, fb_w, fb_h, s);

        y += 20;
        fb_draw_str_bg(20, y,    "RX:",  g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_int(56, y, pad.s2_x, w, bg, fb, fb_w, fb_h, s);
        fb_draw_str_bg(200, y,   "RY:",  g, bg, fb, fb_w, fb_h, s, 32);
        fb_draw_int(236, y, pad.s2_y, w, bg, fb, fb_w, fb_h, s);

        y += 30;
        fb_draw_str_bg(20, y,    "GUIDE=exit", 0xFF44CC88, bg, fb, fb_w, fb_h, s, 100);

        fb_flush(fbi->base, fb_sz);

        if (pad.logo)
            break;
    }

    fb_fill_solid(0xFF000044, fb, s, fb_h);
    fb_draw_str_bg(20, 40, "EXITED", 0xFFFFFFFF, 0xFF000044, fb, fb_w, fb_h, s, 100);
    fb_flush(fbi->base, fb_sz);

    return 0;
}
