/* ── Section-attributed definitions for the reserved shmem region.
     The linker script reserves NOLOAD space at 0x80002000, 0x80002800,
     and 0x80004000.  These symbols are placed there. ── */

#include "system_memory_map.h"

volatile IpcRingBuffer ipc_cmd_ring __attribute__((section(".shmem.cmd_ring")));
volatile IpcRingBuffer ipc_res_ring __attribute__((section(".shmem.res_ring")));
volatile IpcStateFlags ipc_sys_flags __attribute__((section(".shmem.state")));
