#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <stdlib.h>
#include "system_memory_map.h"

typedef struct {
    uint32_t *fb;
    int width;
    int height;
    int stride;
    int bpp;
} GfxCtx;

static inline void gfx_init(GfxCtx *ctx)
{
    volatile FbInfo *fbi = (volatile FbInfo *)IPC_FB_INFO_ADDR;
    ctx->fb     = (uint32_t *)fbi->base;
    ctx->width  = (int)fbi->width;
    ctx->height = (int)fbi->height;
    ctx->stride = (int)fbi->stride;
    ctx->bpp    = (int)fbi->bpp;
}

static inline int gfx_tile_idx(int x, int y, int s)
{
    return (((y >> 5) * 32 * s) + ((x >> 5) << 10)
            + (x & 3) + ((y & 1) << 2)
            + (((x & 31) >> 2) << 3) + (((y & 31) >> 1) << 6))
           ^ ((y & 8) << 2);
}

static inline void gfx_set_px(const GfxCtx *ctx, int x, int y, uint32_t c)
{
    if (x >= 0 && x < ctx->width && y >= 0 && y < ctx->height)
        ctx->fb[gfx_tile_idx(x, y, ctx->stride)] = c;
}

static inline void gfx_fill_rect(const GfxCtx *ctx, int x, int y, int w, int h, uint32_t c)
{
    for (int row = y; row < y + h && row < ctx->height; row++)
        for (int col = x; col < x + w && col < ctx->width; col++)
            ctx->fb[gfx_tile_idx(col, row, ctx->stride)] = c;
}

static inline void gfx_clear(const GfxCtx *ctx, uint32_t c)
{
    for (int i = 0; i < ctx->stride * ctx->height; i++)
        ctx->fb[i] = c;
}

extern const unsigned char fontdata_8x16[4096];

static inline void gfx_draw_char(const GfxCtx *ctx, int x, int y, unsigned char ch, uint32_t fg)
{
    const unsigned char *g = fontdata_8x16 + (int)ch * 16;
    for (int cy = 0; cy < 16; cy++)
        for (int cx = 0; cx < 8; cx++)
            if (g[cy] & (0x80 >> cx))
                ctx->fb[gfx_tile_idx(x + cx, y + cy, ctx->stride)] = fg;
}

static inline void gfx_draw_str(const GfxCtx *ctx, int x, int y, const char *str,
                                 uint32_t fg, uint32_t bg, int bg_w)
{
    for (int row = y; row < y + 16 && row < ctx->height; row++)
        for (int col = x; col < x + bg_w && col < ctx->width; col++)
            ctx->fb[gfx_tile_idx(col, row, ctx->stride)] = bg;

    while (*str) {
        gfx_draw_char(ctx, x, y, *str++, fg);
        x += 8;
    }
}

static inline void gfx_draw_int(const GfxCtx *ctx, int x, int y, int val,
                                 uint32_t fg, uint32_t bg)
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
    gfx_draw_str(ctx, x, y, buf, fg, bg, 48);
}

static inline uint32_t gfx_pack_color(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

static inline uint32_t gfx_alpha_blend(uint32_t src, uint32_t dst, uint8_t a)
{
    uint8_t inv = 255 - a;
    uint8_t r = (((src >> 24) & 0xFF) * a + ((dst >> 24) & 0xFF) * inv) / 255;
    uint8_t g = (((src >> 16) & 0xFF) * a + ((dst >> 16) & 0xFF) * inv) / 255;
    uint8_t b = (((src >> 8) & 0xFF) * a + ((dst >> 8) & 0xFF) * inv) / 255;
    return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

static inline void gfx_fill_rect_alpha(const GfxCtx *ctx, int x, int y, int w, int h,
                                        uint32_t color, uint8_t alpha)
{
    for (int row = y; row < y + h && row < ctx->height; row++)
        for (int col = x; col < x + w && col < ctx->width; col++) {
            int i = gfx_tile_idx(col, row, ctx->stride);
            ctx->fb[i] = gfx_alpha_blend(color, ctx->fb[i], alpha);
        }
}

static inline uint32_t *gfx_save_region(const GfxCtx *ctx, int x, int y, int w, int h)
{
    uint32_t *buf = (uint32_t *)malloc(w * h * sizeof(uint32_t));
    if (!buf) return NULL;
    for (int row = 0; row < h && row < ctx->height; row++)
        for (int col = 0; col < w && col < ctx->width; col++)
            buf[row * w + col] = ctx->fb[gfx_tile_idx(x + col, y + row, ctx->stride)];
    return buf;
}

static inline void gfx_restore_region(const GfxCtx *ctx, int x, int y,
                                       const uint32_t *data, int w, int h)
{
    for (int row = 0; row < h && row < ctx->height; row++)
        for (int col = 0; col < w && col < ctx->width; col++)
            ctx->fb[gfx_tile_idx(x + col, y + row, ctx->stride)] = data[row * w + col];
}

static inline void gfx_flush(const GfxCtx *ctx)
{
    uint32_t base = (uint32_t)ctx->fb;
    uint32_t size = ctx->height * ctx->stride * ctx->bpp;
    uint32_t p = base & ~0x7F;
    uint32_t end = base + size;
    while (p < end) {
        __asm__ volatile("dcbf 0, %0" : : "r"(p) : "memory");
        p += 128;
    }
    __asm__ volatile("sync" : : : "memory");
}

#endif
