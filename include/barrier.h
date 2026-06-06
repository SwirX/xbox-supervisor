#ifndef BARRIER_H
#define BARRIER_H

#include <stdint.h>

/* ── Full memory barrier ── */
static inline void ppc_sync(void)
{
    __asm__ __volatile__ ("sync" : : : "memory");
}

/* ── Lightweight memory barrier (store-store ordering only) ── */
static inline void ppc_lwsync(void)
{
    __asm__ __volatile__ ("lwsync" : : : "memory");
}

/* ── 64-bit MMIO load/store (for XPIC access).
       Replaces libxenon's xenon_io.h ld()/std().
       "b" constraint forces base-address register mode. ── */

static inline uint64_t mmio_ld(volatile void *addr)
{
    uint64_t v;
    __asm__ __volatile__ ("ld %0, 0(%1)" : "=r"(v) : "b"(addr));
    return v;
}

static inline void mmio_st(volatile void *addr, uint64_t val)
{
    __asm__ __volatile__ ("std %1, 0(%0)" : : "b"(addr), "r"(val));
    __asm__ __volatile__ ("eieio" : : : "memory");
    __asm__ __volatile__ ("isync" : : : "memory");
}

/* ── Invalidate a single 128-byte L2 cache line (flush + discard).
 *     Uses dcbf instead of dcbi — dcbi is hypervisor-only on Xenon PPE
 *     and causes a fatal exception.  dcbf writes back dirty data then
 *     invalidates, achieving the same coherency effect. */
static inline void cache_inval_line(volatile void *addr)
{
    __asm__ __volatile__ (
        "dcbf 0, %0\n\t"
        "sync\n\t"
        "isync"
        :
        : "r"(addr)
        : "memory"
    );
}

/* ── Self-contained replacement for libxenon's memdcbf().
       Flushes (dcbf + sync) a range of memory from L1/L2 to RAM,
       making it visible to other cores and devices. ── */
static inline void cache_flush_range(volatile void *addr, uint32_t len)
{
    volatile uint8_t *p = (volatile uint8_t *)((uint32_t)addr & ~0x7F);
    volatile uint8_t *end = (volatile uint8_t *)((uint32_t)addr + len);

    while (p < end) {
        __asm__ __volatile__ ("dcbf 0, %0" : : "r"(p) : "memory");
        p += 128;
    }
    __asm__ __volatile__ ("sync" : : : "memory");
}

#endif /* BARRIER_H */
