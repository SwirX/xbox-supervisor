#include <stdint.h>

#define IPC_INPUT_GEN  ((volatile uint32_t *)0x807FFCFCUL)
#define IPC_SHARED_PAD ((volatile uint32_t *)0x807FFD00UL)

struct pad_state {
    signed short s1_x, s1_y, s2_x, s2_y;
    int s1_z, s2_z, lb, rb, start, back, a, b, x, y, up, down, left, right;
    unsigned char lt, rt;
    int logo;
};

struct ati_info {
    uint32_t unknown1[4];
    uint32_t base;
    uint32_t unknown2[8];
    uint32_t width;
    uint32_t height;
};

#define ATI      ((volatile struct ati_info *)0xEC806100UL)
#define WIDTH    (ATI->width)
#define HEIGHT   (ATI->height)
#define FBASE    (ATI->base | 0x80000000UL)
#define STRIDE   (((WIDTH + 31) >> 5) << 5)
#define BYPP     4
#define FB_SZ    (HEIGHT * STRIDE * BYPP)

extern const unsigned char fontdata_8x16[4096];

static void cache_flush_fb(volatile void *addr, uint32_t len)
{
    volatile uint8_t *p = (volatile uint8_t *)((uint32_t)addr & ~0x7F);
    volatile uint8_t *end = (volatile uint8_t *)((uint32_t)addr + len);
    while (p < end) {
        __asm__ volatile("dcbf 0, %0" : : "r"(p) : "memory");
        p += 128;
    }
    __asm__ volatile("sync" : : : "memory");
}

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

static int tile_idx(int x, int y, int s)
{
    return (((y >> 5) * 32 * s) + ((x >> 5) << 10)
            + (x & 3) + ((y & 1) << 2)
            + (((x & 31) >> 2) << 3) + (((y & 31) >> 1) << 6))
           ^ ((y & 8) << 2);
}

static void set_px(int x, int y, uint32_t c, uint32_t *fb, int s)
{
    if (x >= 0 && x < (int)WIDTH && y >= 0 && y < (int)HEIGHT)
        fb[tile_idx(x, y, s)] = c;
}

static void fill_rect(int x, int y, int w, int h, uint32_t c, uint32_t *fb, int s)
{
    for (int row = y; row < y + h && row < (int)HEIGHT; row++)
        for (int col = x; col < x + w && col < (int)WIDTH; col++)
            fb[tile_idx(col, row, s)] = c;
}

static void draw_char(int x, int y, unsigned char ch, uint32_t c, uint32_t *fb, int s)
{
    const unsigned char *g = fontdata_8x16 + (int)ch * 16;
    for (int cy = 0; cy < 16; cy++)
        for (int cx = 0; cx < 8; cx++)
            if (g[cy] & (0x80 >> cx))
                fb[tile_idx(x + cx, y + cy, s)] = c;
}

static void draw_str(int x, int y, const char *str, uint32_t c, uint32_t *fb, int s)
{
    while (*str) {
        draw_char(x, y, *str++, c, fb, s);
        x += 8;
        if (x + 8 > (int)WIDTH) { x = 0; y += 16; }
    }
}

static void draw_int(int x, int y, int val, uint32_t c, uint32_t *fb, int s)
{
    char buf[16];
    char *p = buf;
    int neg = 0;
    int n = val;
    if (n < 0) { neg = 1; n = -n; }
    do { *p++ = '0' + (n % 10); n /= 10; } while (n);
    if (neg) *p++ = '-';
    *p = '\0';
    for (char *a = buf, *b = p - 1; a < b; a++, b--) {
        char t = *a; *a = *b; *b = t;
    }
    draw_str(x, y, buf, c, fb, s);
}

int main(void)
{
    int s = STRIDE;
    uint32_t *fb = (uint32_t *)FBASE;

    fill_rect(0, 0, WIDTH, HEIGHT, 0xFF202025, fb, s);
    draw_str(20, 10, "GUEST DASHBOARD", 0xFFFFFFFF, fb, s);
    cache_flush_fb(fb, FB_SZ);

    while (1) {
        struct pad_state pad = read_pad();

        int y = 40;
        uint32_t g = 0xFF4488CC, w = 0xFFFFFFFF;

        draw_str(20, y,    "A:",   g, fb, s);
        draw_str(44, y,    pad.a ? "1" : "0", pad.a ? w : 0xFF444444, fb, s);
        draw_str(90, y,    "B:",   g, fb, s);
        draw_str(114, y,   pad.b ? "1" : "0", pad.b ? w : 0xFF444444, fb, s);
        draw_str(150, y,   "X:",   g, fb, s);
        draw_str(174, y,   pad.x ? "1" : "0", pad.x ? w : 0xFF444444, fb, s);
        draw_str(210, y,   "Y:",   g, fb, s);
        draw_str(234, y,   pad.y ? "1" : "0", pad.y ? w : 0xFF444444, fb, s);

        y += 20;

        draw_str(300, y,   "UP:",  g, fb, s);
        draw_str(340, y,   pad.up ? "1" : "0", pad.up ? w : 0xFF444444, fb, s);
        draw_str(390, y,   "DN:",  g, fb, s);
        draw_str(430, y,   pad.down ? "1" : "0", pad.down ? w : 0xFF444444, fb, s);

        y += 20;
        draw_str(20, y,    "LT:",  g, fb, s);
        draw_int(56, y, pad.lt, w, fb, s);
        draw_str(150, y,   "RT:",  g, fb, s);
        draw_int(186, y, pad.rt, w, fb, s);

        y += 20;
        draw_str(20, y,    "LX:",  g, fb, s);
        draw_int(56, y, pad.s1_x, w, fb, s);
        draw_str(200, y,   "LY:",  g, fb, s);
        draw_int(236, y, pad.s1_y, w, fb, s);

        y += 20;
        draw_str(20, y,    "RX:",  g, fb, s);
        draw_int(56, y, pad.s2_x, w, fb, s);
        draw_str(200, y,   "RY:",  g, fb, s);
        draw_int(236, y, pad.s2_y, w, fb, s);

        y += 30;
        draw_str(20, y,    "GUIDE=exit", 0xFF44CC88, fb, s);

        cache_flush_fb(fb, FB_SZ);

        if (pad.logo)
            break;
    }

    fill_rect(0, 0, WIDTH, HEIGHT, 0xFF000044, fb, s);
    draw_str(20, 40, "EXITED", 0xFFFFFFFF, fb, s);
    cache_flush_fb(fb, FB_SZ);

    return 0;
}
