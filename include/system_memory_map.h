#ifndef SYSTEM_MEMORY_MAP_H
#define SYSTEM_MEMORY_MAP_H

#include <stdint.h>

/*
 * Memory Map (8 MB supervisor region)
 * 0x80000000 - 0x807FFFFF   Supervisor code, data, heap, stack
 * 0x807FF000 - 0x807FFFFF   IPC shared memory (4 KB)
 *   0x807FF000   cmd_ring   (IpcRingBuffer, 1280 B)
 *   0x807FF800   res_ring   (IpcRingBuffer, 1280 B)
 *   0x807FFD00   shared_pad (controller_data_s[4], 296 B)
 *   0x807FFF00   state      (IpcStateFlags, 256 B)
 * 0x80800000 onwards         Guest app space
 */

#define SUPERVISOR_LIMIT       0x80800000UL
#define IPC_SHMEM_BASE         0x807FF000UL
#define IPC_SHMEM_SIZE         0x1000UL

#define IPC_CMD_RING_ADDR      ((volatile IpcRingBuffer *)0x807FF000UL)
#define IPC_RES_RING_ADDR      ((volatile IpcRingBuffer *)0x807FF800UL)
#define IPC_SHARED_PAD_ADDR    ((volatile struct controller_data_s *)0x807FFD00UL)
#define IPC_FLAGS_ADDR         ((volatile IpcStateFlags *)0x807FFF00UL)

/* ── IPC Packet ── */
#define PACKET_PAYLOAD_SIZE 56

typedef struct {
    uint32_t cmd_type;
    uint32_t sequence_id;
    uint8_t  payload[PACKET_PAYLOAD_SIZE];
} IpcPacket;

/* ── SPSC Ring Buffer ── */
typedef struct __attribute__((aligned(128))) {
    IpcPacket slots[16];
    volatile uint32_t head;
    uint8_t pad0[124];
    volatile uint32_t tail;
    uint8_t pad1[124];
} IpcRingBuffer;

/* ── IPC State Flags ── */
typedef struct {
    struct __attribute__((aligned(128))) {
        volatile uint32_t target_action;
        volatile uint32_t supervisor_status;
    } out;
    struct __attribute__((aligned(128))) {
        volatile uint32_t current_state;
        volatile uint32_t core1_heartbeat;
    } in;
} IpcStateFlags;

/* ── Shared Controller Data (from input/input.h) ── */
struct controller_data_s {
    signed short s1_x, s1_y, s2_x, s2_y;
    int s1_z, s2_z, lb, rb, start, back, a, b, x, y, up, down, left, right;
    unsigned char lt, rt;
    int logo;
};

/* ── State flags ── */
#define STATE_INIT       0x00000000
#define STATE_POLLING    0x504F4C4C
#define STATE_PAUSE      0x50415553
#define STATE_PAUSED     0x50534400
#define STATE_RESUME     0x5245534D
#define STATE_QUARANTINE 0x5145524E
#define STATE_ACK        0x41434B4E

/* ── Command types ── */
#define CMD_PING          0x01
#define RES_PONG          0x02
#define CMD_VERIFY_GUEST  0x10
#define CMD_EXEC_GUEST    0x11

#endif
