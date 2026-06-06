#ifndef ELF_FORMAT_H
#define ELF_FORMAT_H

#include <stdint.h>

#define EI_NIDENT 16

/* ELF32 header — big-endian, direct-map on Xenon */
typedef struct {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf32_Ehdr;

/* ELF32 program header */
typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

/* Payload delivered via IPC ring for EXEC_GUEST commands.
   Fits in the 56-byte packet payload area. */
typedef struct __attribute__((packed)) {
    uint32_t entry_point;
    uint32_t stack_pointer;
    uint32_t guest_text_start;
    uint32_t guest_text_size;
} ElfExecPayload;

/* ELF constants */
#define ELFMAG0      0x7f
#define ELFMAG1      'E'
#define ELFMAG2      'L'
#define ELFMAG3      'F'
#define EM_PPC       20
#define PT_LOAD      1
#define PF_X         0x1  /* executable */
#define PF_W         0x2  /* writable  */
#define PF_R         0x4  /* readable  */

#endif /* ELF_FORMAT_H */
