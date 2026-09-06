global gdt_load
global gdt_set_tss_base
global tss_flush

section .data

align 8
gdt_entries:
gdt_null:
    dq 0
gdt_code0:
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
gdt_data0:
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
gdt_code3:
    dw 0xFFFF, 0x0000
    db 0x00, 11111010b, 11001111b, 0x00
gdt_data3:
    dw 0xFFFF, 0x0000
    db 0x00, 11110010b, 11001111b, 0x00
gdt_tss:
    dw 0x0067, 0x0000
    db 0x00, 10001001b, 0x00, 0x00
gdt_end:

gdt_ptr:
    dw gdt_end - gdt_entries - 1
    dd gdt_entries

section .text

gdt_load:
    lgdt [gdt_ptr]
    push dword 0x08
    push dword .flush
    retf
.flush:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

gdt_set_tss_base:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    mov word  [gdt_tss + 2], ax
    shr eax, 16
    mov byte  [gdt_tss + 4], al
    mov byte  [gdt_tss + 7], ah
    pop ebp
    ret

tss_flush:
    mov ax, 0x28
    ltr ax
    ret
