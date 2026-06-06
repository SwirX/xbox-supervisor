#include "elf_format.h"
#include "system_memory_map.h"
#include <string.h>

extern uint8_t _binary_guest_application_elf_start[];
extern uint8_t _binary_guest_application_elf_end[];

#define XENON_CACHE_LINE_SIZE  128

int load_embedded_guest_elf(ElfExecPayload *out_payload)
{
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)_binary_guest_application_elf_start;
    uint32_t elf_size = (uint32_t)(_binary_guest_application_elf_end -
                                   _binary_guest_application_elf_start);

    if (elf_size < sizeof(Elf32_Ehdr))
        return -1;
    if (ehdr->e_ident[0] != ELFMAG0 || ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 || ehdr->e_ident[3] != ELFMAG3)
        return -1;

    if (ehdr->e_machine != EM_PPC)
        return -2;

    if (ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf32_Phdr) > elf_size)
        return -1;

    Elf32_Phdr *phdr = (Elf32_Phdr *)(_binary_guest_application_elf_start + ehdr->e_phoff);
    uint32_t first_text_vaddr = 0;
    uint32_t total_text_size  = 0;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        uint32_t vaddr = phdr[i].p_vaddr;
        uint32_t memsz = phdr[i].p_memsz;

        if (vaddr < SUPERVISOR_LIMIT && vaddr + memsz > SUPERVISOR_LIMIT)
            return -3;
        if (vaddr + memsz <= vaddr)
            return -3;

        uint8_t *dest = (uint8_t *)vaddr;

        if (phdr[i].p_flags & PF_X) {
            if (first_text_vaddr == 0)
                first_text_vaddr = vaddr;
            total_text_size += memsz;
        }

        if (phdr[i].p_filesz > 0) {
            if (phdr[i].p_offset + phdr[i].p_filesz > elf_size)
                return -1;
            uint8_t *src = _binary_guest_application_elf_start + phdr[i].p_offset;
            memcpy(dest, src, phdr[i].p_filesz);

            uint32_t fs = vaddr & ~(XENON_CACHE_LINE_SIZE - 1);
            uint32_t fe = (vaddr + phdr[i].p_filesz + XENON_CACHE_LINE_SIZE - 1)
                          & ~(XENON_CACHE_LINE_SIZE - 1);
            for (uint32_t a = fs; a < fe; a += XENON_CACHE_LINE_SIZE)
                __asm__ volatile("dcbst 0, %0" : : "r"(a) : "memory");
        }

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

    __asm__ volatile("sync" : : : "memory");

    out_payload->entry_point      = ehdr->e_entry;
    out_payload->stack_pointer    = 0x9E000000;
    out_payload->guest_text_start = first_text_vaddr;
    out_payload->guest_text_size  = total_text_size;
    return 0;
}
