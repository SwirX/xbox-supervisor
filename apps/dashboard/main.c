#include <xenolith/sdk.h>

int main(void)
{
    app_init();
    GfxCtx *gfx = xl_get_framebuffer();

    gfx_clear(gfx, 0x101018FF);

    int y = 30;
    gfx_draw_str(gfx, 40, y, "XENOLITH DASHBOARD", 0x00FF66FF, 0x101018FF, 400);
    y += 50;

    int count = svc_get_app_count();
    if (count == 0) {
        gfx_draw_str(gfx, 40, y, "No apps installed", 0x888888FF, 0x101018FF, 300);
    } else {
        char buf[64];
        int sel = 0;
        while (1) {
            gfx_clear(gfx, 0x101018FF);
            gfx_draw_str(gfx, 40, 30, "XENOLITH DASHBOARD", 0x00FF66FF, 0x101018FF, 400);

            int lines = count;
            if (lines > 10) lines = 10;

            for (int i = 0; i < lines; i++) {
                svc_get_app_name(i, buf, sizeof(buf));
                uint32_t color = (i == sel) ? 0xFFFFFFFF : 0x888888FF;
                gfx_draw_str(gfx, 60, 80 + i * 36, buf, color, 0x101018FF, 300);
            }
            app_present();

            struct controller_data_s pad;
            xl_input_read(&pad);
            if (pad.a || pad.b)
                break;
        }
    }

    app_present();

    svc_notify("Dashboard loaded");

    while (1) {
        struct controller_data_s pad;
        xl_input_read(&pad);
        /* idle forever — supervisor handles guide button */
    }
}
