#ifndef XENOLITH_SDK_H
#define XENOLITH_SDK_H

#include "xenolith/types.h"
#include <gfx.h>
#include <input/input.h>
#include <time.h>

void    app_init(void);
void    app_present(void);
GfxCtx *xl_get_framebuffer(void);

void    svc_notify(const char *text);
time_t  svc_get_time(void);
int     svc_launch_app(const char *app_id);
void    svc_shutdown(void);
char   *svc_get_setting(const char *key);
int     svc_set_setting(const char *key, const char *value);

int     xl_input_read(struct controller_data_s *pad);
uint64_t xl_get_ticks(void);

#endif
