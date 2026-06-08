#include <xenolith/sdk.h>

int main(void)
{
    app_init();
    GfxCtx *gfx = xl_get_framebuffer();

    svc_notify("SDK demo running");

    /* Test 1: Full-screen colors */
    gfx_clear(gfx, 0xFF0000FF);
    app_present();

    struct controller_data_s pad;
    do { xl_input_read(&pad); } while (!pad.a);
    do { xl_input_read(&pad); } while (pad.a);

    gfx_clear(gfx, 0x00FF00FF);
    app_present();
    do { xl_input_read(&pad); } while (!pad.a);
    do { xl_input_read(&pad); } while (pad.a);

    gfx_clear(gfx, 0x0000FFFF);
    app_present();
    do { xl_input_read(&pad); } while (!pad.a);
    do { xl_input_read(&pad); } while (pad.a);

    /* Test 2: Text */
    gfx_clear(gfx, 0x202025FF);
    gfx_draw_str(gfx, 20, 10, "XENOLITH SDK", 0xFFFFFFFF, 0x202025FF, 200);
    app_present();
    do { xl_input_read(&pad); } while (!pad.a);
    do { xl_input_read(&pad); } while (pad.a);

    svc_notify("Demo complete");

    gfx_clear(gfx, 0x000044FF);
    gfx_draw_str(gfx, 20, 40, "DONE", 0xFFFFFFFF, 0x000044FF, 100);
    app_present();

    return 0;
}
