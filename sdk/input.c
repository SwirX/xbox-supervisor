#include "xenolith/sdk.h"
#include "system_memory_map.h"

int xl_input_read(struct controller_data_s *pad)
{
    uint32_t g1, g2;
    do {
        g1 = *IPC_INPUT_GEN_ADDR;
        __asm__ volatile("sync" : : : "memory");
        *pad = IPC_SHARED_PAD_ADDR[0];
        __asm__ volatile("sync" : : : "memory");
        g2 = *IPC_INPUT_GEN_ADDR;
    } while (g1 != g2 || (g1 & 1));
    return 0;
}
