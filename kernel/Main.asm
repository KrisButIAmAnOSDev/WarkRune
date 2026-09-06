[bits 32]

extern kernel_main
global start

start:
    mov esp, 0x90000

    ; write 'K' to VGA text buffer to verify we reached kernel
    mov byte [0xB8000], 'K'
    mov byte [0xB8001], 0x0F

    mov al, 0xAE
    out 0x64, al

    call vbe_init
    call kernel_main

hang:
    hlt
    jmp hang

%include "kernel/Graphics/vga.asm"
%include "kernel/Graphics/text.asm"
%include "kernel/system/idt/idt.asm"
%include "kernel/io/keyboard.asm"
%include "kernel/system/syscall/syscall.asm"
%include "kernel/system/gdt/gdt.asm"
%include "kernel/system/ring3/ring3.asm"
%include "kernel/system/kctx.asm"