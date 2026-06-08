#include "xenolith/sdk.h"
#include "internal.h"
#include <string.h>

void svc_notify(const char *text)
{
    svc_send(SVC_NOTIFY, text, strlen(text) + 1, NULL, 0);
}
