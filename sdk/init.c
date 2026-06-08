#include "xenolith/sdk.h"
#include "system_memory_map.h"

static GfxCtx _gfx;

GfxCtx *xl_get_framebuffer(void)
{
    return &_gfx;
}

void app_init(void)
{
    volatile FbInfo *fbi = (volatile FbInfo *)IPC_FB_INFO_ADDR;
    _gfx.fb     = (uint32_t *)fbi->base;
    _gfx.width  = (int)fbi->width;
    _gfx.height = (int)fbi->height;
    _gfx.stride = (int)fbi->stride;
    _gfx.bpp    = (int)fbi->bpp;
}
