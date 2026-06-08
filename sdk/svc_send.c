#include "xenolith/types.h"
#include "system_memory_map.h"
#include "ipc_ring.h"
#include "barrier.h"
#include <string.h>

static uint32_t _seq;

int svc_send(uint32_t cmd, const void *req, uint32_t req_len,
             void *resp, uint32_t resp_len)
{
    IpcPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.cmd_type    = cmd;
    pkt.sequence_id = ++_seq;
    if (req && req_len > 0) {
        uint32_t cp = req_len < PACKET_PAYLOAD_SIZE ? req_len : PACKET_PAYLOAD_SIZE;
        memcpy(pkt.payload, req, cp);
    }

    while (!ipc_ring_push((IpcRingBuffer *)IPC_RES_RING_ADDR, &pkt));

    if (!resp)
        return XL_OK;

    while (1) {
        cache_inval_line(&((IpcRingBuffer *)IPC_CMD_RING_ADDR)->head);
        IpcPacket rsp;
        if (ipc_ring_pop((IpcRingBuffer *)IPC_CMD_RING_ADDR, &rsp) &&
            rsp.sequence_id == pkt.sequence_id)
        {
            if (resp_len > 0) {
                uint32_t cp = resp_len < PACKET_PAYLOAD_SIZE ? resp_len : PACKET_PAYLOAD_SIZE;
                memcpy(resp, rsp.payload, cp);
            }
            return XL_OK;
        }
        __asm__ volatile("or 27, 27, 27");
    }
}
