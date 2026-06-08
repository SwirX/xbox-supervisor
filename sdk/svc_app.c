#include "xenolith/sdk.h"
#include "system_memory_map.h"
#include "xenolith/types.h"
#include <string.h>

int svc_get_app_count(void)
{
    int32_t count = 0;
    if (svc_send(SVC_GET_APP_COUNT, NULL, 0, &count, sizeof(count)) != XL_OK)
        return 0;
    return (int)count;
}

int svc_get_app_name(int idx, char *buf, int buf_len)
{
    struct {
        int32_t total;
        int32_t index;
        char name[48];
    } r;
    memset(&r, 0, sizeof(r));
    if (svc_send(SVC_GET_APP_INFO, &idx, sizeof(idx), &r, sizeof(r)) != XL_OK)
        return -1;
    if (r.index != idx || buf_len < 1)
        return -1;
    strncpy(buf, r.name, (size_t)buf_len - 1);
    buf[buf_len - 1] = '\0';
    return 0;
}
