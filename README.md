# WarkRune

A 32-bit x86 hobby operating system written from scratch in C++17 and NASM.

Everything in this project (bootloader, kernel, drivers, filesystem, libc, shell)
was written by hand for this project. See [License](#license) below.

## Features

- Custom MBR bootloader (stage1 + stage2) with VBE 800x600x32 framebuffer
- 32-bit protected mode kernel at 0x8000
- GDT, IDT, TSS, PIC, PIT
- Preemptive round-robin scheduler
- Memory management: E820, bitmap PMM, first-fit kmalloc, 2-level paging
- FAT12 read/write filesystem with BPB parsing, 12-bit cluster chain, 8.3 names
- ELF32 loader (ring 3 user programs)
- Drivers: ATA PIO, PS/2 keyboard, PS/2 mouse, 16550 serial, PC speaker
- ACPI shutdown/reboot (RSDP/RSDT/FADT/DSDT)
- Custom printf/sprintf implementation (no libc)
- Interactive shell with `ls`, `cat`, `cd`, `pwd`, `mkdir`, `touch`, `rm`, `rmdir`, `fat12` test

## Requirements

- `nasm`
- `g++` with 32-bit support (`-m32`)
- `ld` (i386 ELF)
- `qemu-system-i386`
- `mtools` (`mcopy`)
- `dosfstools` (`mkfs.fat`)
- `make`

## Build & Run

```sh
make run
```

This runs `make clean && make` then launches QEMU. The disk image
(`Img/os.img`) is a 16MB raw image containing a FAT12 filesystem with
a sample `HELLO.TXT`.

## License

MIT. See [LICENSE](LICENSE).

The 8x8 bitmap font in `kernel/Graphics/assets/font_8x8.asm` is the
standard IBM PC CGA/VGA ROM font (Code Page 437), treated as public domain.
All other code is original.
