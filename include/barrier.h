#ifndef BARRIER_H
#define BARRIER_H

#include <stdint.h>

static inline void ppc_sync(void)
{
    __asm__ __volatile__ ("sync" : : : "memory");
}

static inline void ppc_lwsync(void)
{
    __asm__ __volatile__ ("lwsync" : : : "memory");
}

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

#endif
