#include <barrier.h>
#include "ipc_ring.h"

void ipc_ring_init(volatile IpcRingBuffer *ring)
{
    for (int i = 0; i < 16; ++i) {
        ring->slots[i].cmd_type    = 0;
        ring->slots[i].sequence_id = 0;
    }
    ring->head = 0;
    ring->tail = 0;
    ppc_sync();
}

int ipc_ring_push(volatile IpcRingBuffer *ring, const IpcPacket *packet)
{
    uint32_t head = ring->head;
    uint32_t tail = ring->tail;

    if (((head + 1) & 15) == tail)
        return 0;

    ring->slots[head] = *packet;

    ppc_sync();

    ring->head = (head + 1) & 15;
    return 1;
}

int ipc_ring_pop(volatile IpcRingBuffer *ring, IpcPacket *out_packet)
{
    cache_inval_line((void *)&ring->head);
    uint32_t head = ring->head;
    cache_inval_line((void *)&ring->tail);
    uint32_t tail = ring->tail;

    if (tail == head)
        return 0;

    ppc_lwsync();

    *out_packet = ring->slots[tail];

    ppc_lwsync();

    ring->tail = (tail + 1) & 15;
    return 1;
}

int ipc_ring_has_data(volatile IpcRingBuffer *ring)
{
    uint32_t head = ring->head;
    uint32_t tail = ring->tail;
    ppc_lwsync();
    return (tail != head) ? 1 : 0;
}

int ipc_ring_has_space(volatile IpcRingBuffer *ring)
{
    uint32_t head = ring->head;
    uint32_t tail = ring->tail;
    return (((head + 1) & 15) != tail) ? 1 : 0;
}
