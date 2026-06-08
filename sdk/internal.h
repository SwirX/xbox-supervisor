#ifndef SDK_INTERNAL_H
#define SDK_INTERNAL_H

#include <stdint.h>
#include "system_memory_map.h"

int svc_send(uint32_t cmd, const void *req, uint32_t req_len,
             void *resp, uint32_t resp_len);

#endif
