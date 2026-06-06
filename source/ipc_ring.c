/* Lock-free SPSC ring buffer for Core 0 ↔ Core 1 IPC.
 *
 * Uses absolute addresses defined in system_memory_map.h — no extern
 * symbols, no section attributes.  Both cores access the same physical
 * memory through the identity-mapped SLB set up by libxenon's CRT.
 *
 * Producer (Core 0 → cmd_ring, Core 1 → res_ring) writes slots then
 * issues sync before advancing head.  Consumer reads slots after lwsync
 * barrier on tail.  Single-writer, single-reader — no atomic CAS needed.
 */

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

    /* Check full: (head + 1) & mask == tail */
    if (((head + 1) & 15) == tail)
        return 0;

    ring->slots[head] = *packet;

    /* Ensure slot data globally visible before consumer reads head */
    ppc_sync();

    ring->head = (head + 1) & 15;
    return 1;
}

int ipc_ring_pop(volatile IpcRingBuffer *ring, IpcPacket *out_packet)
{
    uint32_t head = ring->head;
    uint32_t tail = ring->tail;

    if (tail == head)
        return 0;

    /* Order head load before slot data load */
    ppc_lwsync();

    *out_packet = ring->slots[tail];

    /* Order slot data load before tail advance (frees the slot) */
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
