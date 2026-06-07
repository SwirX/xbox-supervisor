#ifndef SYSTEM_MEMORY_MAP_H
#define SYSTEM_MEMORY_MAP_H

#include <stdint.h>

/* ── Physical Memory Map ──
 *
 * 0x80000000 - 0x807FFFFF   Supervisor code, data, heap, stack  (8 MB)
 * 0x807FF000 - 0x807FFFFF   IPC shared memory                    (4 KB)
 *   0x807FF000   cmd_ring  (IpcRingBuffer, 1280 B)
 *   0x807FF800   res_ring  (IpcRingBuffer, 1280 B)
 *   0x807FFF00   state     (IpcStateFlags,  256 B)
 * 0x80800000 onwards        Guest app space
 *
 * libxenon's internal BSS (ram_heap, heap, memp_memory, etc.) is
 * ~6.3 MB.  The 8 MB supervisor region accommodates all of it with
 * headroom for the heap allocator.
 */

#define SUPERVISOR_LIMIT       0x80800000UL   /* end of supervisor region */
#define IPC_SHMEM_BASE         0x807FF000UL
#define IPC_SHMEM_SIZE         0x1000UL       /* 4 KB */

#define IPC_CMD_RING_ADDR      ((volatile IpcRingBuffer *)0x807FF000UL)
#define IPC_RES_RING_ADDR      ((volatile IpcRingBuffer *)0x807FF800UL)
#define IPC_FLAGS_ADDR         ((volatile IpcStateFlags *)0x807FFF00UL)

/* ── IPC Packet: 64 bytes flat ── */
#define PACKET_PAYLOAD_SIZE 56

typedef struct {
    uint32_t cmd_type;
    uint32_t sequence_id;
    uint8_t  payload[PACKET_PAYLOAD_SIZE];
} IpcPacket;

/* ── SPSC Ring Buffer: L2 cache-line isolated ── */
typedef struct __attribute__((aligned(128))) {
    IpcPacket slots[16];           /* 16 * 64 = 1024 bytes */
    volatile uint32_t head;        /* written by producer */
    uint8_t pad0[124];
    volatile uint32_t tail;        /* written by consumer */
    uint8_t pad1[124];
} IpcRingBuffer;

/* ── Per-direction state flags: each in its own 128B line ── */
typedef struct {
    struct __attribute__((aligned(128))) {
        volatile uint32_t target_action;
        volatile uint32_t supervisor_status;
    } out;                         /* core 0 -> core 1 */

    struct __attribute__((aligned(128))) {
        volatile uint32_t current_state;
        volatile uint32_t core1_heartbeat;
    } in;                          /* core 1 -> core 0 */
} IpcStateFlags;

/* ── Handshake states ── */
#define STATE_INIT       0x00000000
#define STATE_POLLING    0x504F4C4C          /* "POLL" */
#define STATE_PAUSE      0x50415553          /* "PAUS" — request */
#define STATE_PAUSED     0x50534400          /* "PSD\0" — acknowledge */
#define STATE_RESUME     0x5245534D          /* "RESM" */
#define STATE_QUARANTINE 0x5145524E          /* "QERN" */
#define STATE_ACK        0x41434B4E          /* "ACKN" */

/* ── Command / response types ── */
#define CMD_PING          0x01
#define RES_PONG          0x02
#define CMD_VERIFY_GUEST  0x10
#define CMD_EXEC_GUEST    0x11

#endif /* SYSTEM_MEMORY_MAP_H */
