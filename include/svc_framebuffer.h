#ifndef SVC_FRAMEBUFFER_H
#define SVC_FRAMEBUFFER_H

#include <stdint.h>

/* ── Framebuffer info block — written by supervisor at boot, read by guests ── */

typedef struct {
    uint32_t base;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t bpp;
} FbInfo;

/* ── Xenos 32×32 tiled framebuffer formula ── */

static inline int fb_tile_idx(int x, int y, int s)
{
    return (((y >> 5) * 32 * s) + ((x >> 5) << 10)
            + (x & 3) + ((y & 1) << 2)
            + (((x & 31) >> 2) << 3) + (((y & 31) >> 1) << 6))
           ^ ((y & 8) << 2);
}

/* ── Single pixel, bounds-checked ── */

static inline void fb_set_px(int x, int y, uint32_t c, uint32_t *fb, int w, int h, int s)
{
    if (x >= 0 && x < w && y >= 0 && y < h)
        fb[fb_tile_idx(x, y, s)] = c;
}

/* ── Fill a rectangle pixel-by-pixel (use for small rects) ── */

static inline void fb_fill_rect(int x, int y, int w, int h, uint32_t c,
                                 uint32_t *fb, int fb_w, int fb_h, int s)
{
    for (int row = y; row < y + h && row < fb_h; row++)
        for (int col = x; col < x + w && col < fb_w; col++)
            fb[fb_tile_idx(col, row, s)] = c;
}

/* ── Fill entire framebuffer with a solid color (linear blast, no tiling) ── */

static inline void fb_fill_solid(uint32_t c, uint32_t *fb, int stride, int height)
{
    for (int i = 0; i < stride * height; i++)
        fb[i] = c;
}

/* ── Font glyph data (from libxenon.a, linked via -lxenon) ── */

extern const unsigned char fontdata_8x16[4096];

/* ── Draw a single 8×16 character ── */

static inline void fb_draw_char(int x, int y, unsigned char ch, uint32_t fg,
                                 uint32_t *fb, int fb_w, int fb_h, int s)
{
    const unsigned char *g = fontdata_8x16 + (int)ch * 16;
    for (int cy = 0; cy < 16; cy++)
        for (int cx = 0; cx < 8; cx++)
            if (g[cy] & (0x80 >> cx))
                fb[fb_tile_idx(x + cx, y + cy, s)] = fg;
}

/* ── Draw a string with background erase ──
 *
 * Erases a bg_w × 16 pixel rectangle at (x,y) in bg color, then draws
 * the glyphs on top.  bg_w should be wide enough to cover the longest
 * string that can appear at this position (e.g. 32 for single-digit,
 * 128 for short labels).  This prevents ghosting when the string
 * shrinks between frames.
 */

static inline void fb_draw_str_bg(int x, int y, const char *str, uint32_t fg,
                                   uint32_t bg, uint32_t *fb,
                                   int fb_w, int fb_h, int s, int bg_w)
{
    for (int row = y; row < y + 16 && row < fb_h; row++)
        for (int col = x; col < x + bg_w && col < fb_w; col++)
            fb[fb_tile_idx(col, row, s)] = bg;

    while (*str) {
        fb_draw_char(x, y, *str++, fg, fb, fb_w, fb_h, s);
        x += 8;
    }
}

/* ── Draw an integer value (writes to internal buffer, 32 px bg width) ── */

static inline void fb_draw_int(int x, int y, int val, uint32_t fg, uint32_t bg,
                                uint32_t *fb, int fb_w, int fb_h, int s)
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
    fb_draw_str_bg(x, y, buf, fg, bg, fb, fb_w, fb_h, s, 32);
}

/* ── Flush framebuffer from L1/L2 cache to main memory (dcbf + sync) ── */

static inline void fb_flush(uint32_t base, uint32_t size)
{
    uint32_t p = base & ~0x7F;
    uint32_t end = base + size;
    while (p < end) {
        __asm__ volatile("dcbf 0, %0" : : "r"(p) : "memory");
        p += 128;
    }
    __asm__ volatile("sync" : : : "memory");
}

#endif /* SVC_FRAMEBUFFER_H */
