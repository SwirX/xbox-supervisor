#ifndef IPC_RING_H
#define IPC_RING_H

#include "system_memory_map.h"

/* ── Ring buffer operations ── */

/* Must be called once before any push/pop on a ring */
void ipc_ring_init(volatile IpcRingBuffer *ring);

/* Push one packet to the ring.  Returns 1 on success, 0 if full. */
int ipc_ring_push(volatile IpcRingBuffer *ring, const IpcPacket *packet);

/* Pop one packet from the ring.  Returns 1 on success, 0 if empty. */
int ipc_ring_pop(volatile IpcRingBuffer *ring, IpcPacket *out_packet);

/* Non-destructive peek — 1 if data available, 0 if empty */
int ipc_ring_has_data(volatile IpcRingBuffer *ring);

/* Non-destructive peek — 1 if space available, 0 if full */
int ipc_ring_has_space(volatile IpcRingBuffer *ring);

#endif /* IPC_RING_H */
