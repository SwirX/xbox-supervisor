#include "xenolith/sdk.h"

uint64_t xl_get_ticks(void)
{
    uint32_t lo, hi;
    __asm__ volatile("mftbu %0; mftb %1; mftbu %2"
        : "=r"(hi), "=r"(lo), "=r"(hi)
        :
        : "memory");
    return ((uint64_t)hi << 32) | lo;
}
