/* ELF loader for guest applications.
 *
 * Loads an embedded ELF binary (objcopy -I binary) from the supervisor's
 * linker symbols into guest app space at 0x80100000+.
 *
 * Supervisor code reserves 0x80000000-0x800FFFFF.  Underflow past that
 * is rejected.  There is no upper bound — guest apps can span the whole
 * remaining 256 MB.
 *
 * Communication is via the returned ElfExecPayload, which Core 0 sends
 * to Core 1 through the IPC command ring.
 */

#include "elf_format.h"
#include "system_memory_map.h"
#include <string.h>

/* Linker symbols from objcopy binary embedding */
extern uint8_t _binary_guest_application_elf_start[];
extern uint8_t _binary_guest_application_elf_end[];

#define XENON_CACHE_LINE_SIZE     128

/* Parse the embedded ELF, load PT_LOAD segments to their target VMAs,
 * flush Core 0's D-cache to L2, and populate the exec payload for Core 1.
 * Returns 0 on success, negative errno on failure. */
int load_embedded_guest_elf(ElfExecPayload *out_payload)
{
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)_binary_guest_application_elf_start;
    uint32_t elf_size = (uint32_t)(_binary_guest_application_elf_end -
                                   _binary_guest_application_elf_start);

    /* 1. Validate ELF magic */
    if (elf_size < sizeof(Elf32_Ehdr))
        return -1;
    if (ehdr->e_ident[0] != ELFMAG0 || ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 || ehdr->e_ident[3] != ELFMAG3)
        return -1;

    /* 2. Validate architecture */
    if (ehdr->e_machine != EM_PPC)
        return -2;

    /* 3. Validate program header table fits */
    if (ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf32_Phdr) > elf_size)
        return -1;

    Elf32_Phdr *phdr = (Elf32_Phdr *)(_binary_guest_application_elf_start + ehdr->e_phoff);
    uint32_t first_text_vaddr = 0;
    uint32_t total_text_size  = 0;

    /* 4. Iterate program headers */
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        uint32_t vaddr = phdr[i].p_vaddr;
        uint32_t memsz = phdr[i].p_memsz;

        /* Reject segments that overlap the supervisor's footprint */
        if (vaddr < SUPERVISOR_LIMIT && vaddr + memsz > SUPERVISOR_LIMIT)
            return -3;
        if (vaddr + memsz <= vaddr)  /* wraparound */
            return -3;

        uint8_t *dest = (uint8_t *)vaddr;

        /* Track executable range for Core 1's I-cache invalidation */
        if (phdr[i].p_flags & PF_X) {
            if (first_text_vaddr == 0)
                first_text_vaddr = vaddr;
            total_text_size += memsz;
        }

        /* 4a. Copy initialized data */
        if (phdr[i].p_filesz > 0) {
            if (phdr[i].p_offset + phdr[i].p_filesz > elf_size)
                return -1;
            uint8_t *src = _binary_guest_application_elf_start + phdr[i].p_offset;
            memcpy(dest, src, phdr[i].p_filesz);

            /* Flush written lines from Core 0's D-cache to shared L2 */
            uint32_t fs = vaddr & ~(XENON_CACHE_LINE_SIZE - 1);
            uint32_t fe = (vaddr + phdr[i].p_filesz + XENON_CACHE_LINE_SIZE - 1)
                          & ~(XENON_CACHE_LINE_SIZE - 1);
            for (uint32_t a = fs; a < fe; a += XENON_CACHE_LINE_SIZE)
                __asm__ volatile("dcbst 0, %0" : : "r"(a) : "memory");
        }

        /* 4b. Zero BSS tail */
        if (memsz > phdr[i].p_filesz) {
            memset(dest + phdr[i].p_filesz, 0, memsz - phdr[i].p_filesz);

            uint32_t fs = ((uint32_t)dest + phdr[i].p_filesz)
                          & ~(XENON_CACHE_LINE_SIZE - 1);
            uint32_t fe = ((uint32_t)dest + memsz + XENON_CACHE_LINE_SIZE - 1)
                          & ~(XENON_CACHE_LINE_SIZE - 1);
            for (uint32_t a = fs; a < fe; a += XENON_CACHE_LINE_SIZE)
                __asm__ volatile("dcbst 0, %0" : : "r"(a) : "memory");
        }
    }

    /* 5. Global sync — all flushes reach L2 before Core 1 reads */
    __asm__ volatile("sync" : : : "memory");

    /* 6. Populate exec payload */
    out_payload->entry_point      = ehdr->e_entry;
    out_payload->stack_pointer    = 0x9E000000;   /* top of guest stack */
    out_payload->guest_text_start = first_text_vaddr;
    out_payload->guest_text_size  = total_text_size;
    return 0;
}
