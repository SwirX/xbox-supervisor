#include "xenolith/sdk.h"
#include "system_memory_map.h"

void app_present(void)
{
    GfxCtx *gfx = xl_get_framebuffer();
    gfx_flush(gfx);
    __asm__ volatile("sync" : : : "memory");
    *VFB_DIRTY_ADDR = 1;
    __asm__ volatile("sync" : : : "memory");
}
