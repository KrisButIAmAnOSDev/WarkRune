BOOT1_SRC      = bootloader/stage1.asm
BOOT2_SRC      = bootloader/stage2.asm
KERNEL_ASM     = kernel/Main.asm
BOOT1_BIN      = Img/boot1.bin
BOOT2_BIN      = Img/boot2.bin
KERNEL_BIN     = Img/kernel.bin
IMG            = Img/os.img

KERNEL_SRCS = \
	kernel/kernel.cpp \
	kernel/Graphics/text.cpp \
	kernel/Graphics/graphics.cpp \
	kernel/memory/kmalloc.cpp \
	kernel/memory/memory.cpp \
	kernel/memory/paging/paging.cpp \
	kernel/memory/pmm/pmm.cpp \
	kernel/memory/vmm/vmm.cpp \
	kernel/shell/shell.cpp \
	kernel/system/idt/exceptions.cpp \
	kernel/system/libgcc_stubs.cpp \
	kernel/system/pit/pit.cpp \
	kernel/system/cpuid/cpuid.cpp \
	kernel/system/syscall/syscall.cpp \
	kernel/system/gdt/gdt.cpp \
	kernel/system/acpi/acpi.cpp \
	kernel/system/tss/tss.cpp \
	kernel/system/sched/sched.cpp \
	kernel/memory/shmem/shmem.cpp \
	kernel/io/ata.cpp \
	kernel/io/serial/serial.cpp \
	kernel/io/mouse/mouse.cpp \
	kernel/io/pcspk/pcspk.cpp \
	kernel/system/ipc/fd.cpp \
	kernel/fs/fat12.cpp \
	kernel/fs/elf/elf.cpp \

KERNEL_CPPS = $(filter %.cpp, $(KERNEL_SRCS))
KERNEL_CS   = $(filter %.c,   $(KERNEL_SRCS))
CPP_OBJS    = $(KERNEL_CPPS:%.cpp=Img/%.o)
C_OBJS      = $(KERNEL_CS:%.c=Img/%.o)
ALL_OBJS    = Img/main_asm.o $(CPP_OBJS) $(C_OBJS)

CXX      = g++
CC       = gcc
INCLUDES = -I . -I kernel/ -I libs/C -I kernel/system/ -I kernel/klib/

CXXFLAGS = \
	-m32 -std=c++17 \
	-O1 \
	-mno-sse -mno-sse2 -mno-mmx \
	-ffreestanding -fno-exceptions -fno-rtti \
	-fno-stack-protector -fno-use-cxa-atexit \
	-fno-pic -fno-builtin \
	-nostdlib -nostdinc -nostdinc++ \
	$(INCLUDES)

CFLAGS = \
	-m32 -std=c99 \
	-O1 \
	-mno-sse -mno-sse2 -mno-mmx \
	-ffreestanding -fno-pic -fno-stack-protector -fno-builtin \
	-nostdlib -nostdinc \
	$(INCLUDES)

LDFLAGS = -m elf_i386 -T linker.ld --oformat binary

QEMUFLAGS = \
	-drive format=raw,file=$(IMG),if=ide,index=0,media=disk \
	-m 1G \
	-boot order=c,strict=on \
	-cpu pentium3 \
	-display gtk \
	-rtc base=localtime \
	-no-reboot \
	-no-shutdown \
	-serial stdio \
	-d guest_errors,cpu_reset,int \
	-D Img/debug.txt

all: $(IMG)

$(BOOT1_BIN): $(BOOT1_SRC)
	@mkdir -p $(dir $@)
	nasm -f bin $< -o $@

$(BOOT2_BIN): $(BOOT2_SRC)
	@mkdir -p $(dir $@)
	nasm -f bin $< -o $@

Img/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

Img/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: Img/main_asm.o
Img/main_asm.o: $(KERNEL_ASM)
	@mkdir -p $(dir $@)
	nasm -f elf32 $< -o $@

$(KERNEL_BIN): $(ALL_OBJS)
	@mkdir -p $(dir $@)
	ld $(LDFLAGS) -o $@ $(ALL_OBJS)

$(IMG): $(BOOT1_BIN) $(BOOT2_BIN) $(KERNEL_BIN)
	@mkdir -p $(dir $@)
	dd if=/dev/zero      of=$(IMG)       bs=1M count=16      status=none
	dd if=/dev/zero      of=$(IMG:.img=_fat.img) bs=1M count=15  status=none
	mkfs.fat -F 12 -s 16 -n "WARKRUNE" $(IMG:.img=_fat.img) 2>/dev/null || true
	echo "Hello from WarkRune FAT12!" > /tmp/HELLO.TXT
	mcopy -i $(IMG:.img=_fat.img) /tmp/HELLO.TXT ::HELLO.TXT 2>/dev/null || true
	rm -f /tmp/HELLO.TXT
	dd if=$(IMG:.img=_fat.img) of=$(IMG) seek=256 conv=notrunc   status=none
	rm -f $(IMG:.img=_fat.img)
	dd if=$(BOOT1_BIN)   of=$(IMG) seek=0  conv=notrunc      status=none
	dd if=$(BOOT2_BIN)   of=$(IMG) seek=1  conv=notrunc      status=none
	dd if=$(KERNEL_BIN)  of=$(IMG) seek=16 conv=notrunc      status=none

run: $(IMG)
	qemu-system-i386 $(QEMUFLAGS)

debug: $(IMG)
	qemu-system-i386 $(QEMUFLAGS) -s -S

clean:
	rm -rf Img/

.PHONY: all run debug clean
