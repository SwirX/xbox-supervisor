#include "xenolith/sdk.h"

__attribute__((weak)) time_t svc_get_time(void)
{
    return 0;
}

__attribute__((weak)) int svc_launch_app(const char *app_id)
{
    (void)app_id;
    return XL_ERR_NOT_FOUND;
}

__attribute__((weak)) void svc_shutdown(void)
{
}

__attribute__((weak)) char *svc_get_setting(const char *key)
{
    (void)key;
    return NULL;
}

__attribute__((weak)) int svc_set_setting(const char *key, const char *value)
{
    (void)key;
    (void)value;
    return XL_ERR_NOT_FOUND;
}
