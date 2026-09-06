#pragma once
#include <stdint.h>

#define ELF_MAGIC    0x464C457F
#define ELF_CLASS32  1
#define ELF_X86      3
#define ELF_EXEC     2
#define PT_LOAD      1
#define PF_X         0x1
#define PF_W         0x2
#define PF_R         0x4

struct ElfHeader {
    uint8_t  magic[4];
    uint8_t  bits;
    uint8_t  endian;
    uint8_t  version;
    uint8_t  abi;
    uint8_t  padding[8];
    uint16_t type;
    uint16_t machine;
    uint32_t version2;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed));

struct ElfProgramHeader {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} __attribute__((packed));

struct ElfLoadResult {
    bool     ok;
    uint32_t entry;
    uint32_t base;
    uint32_t size;
    char     error[64];
};

ElfLoadResult elf_load(const char* path);
void elf_exec(const char* path, char* err_out, int err_sz);
void elf_unload(uint32_t base, uint32_t size);
bool elf_valid(const uint8_t* data);
void elf_exit();
