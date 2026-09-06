[org 0x0500]
[bits 16]

    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7B00
    sti

    mov [boot_drive], dl

    mov ax, 0x4F01
    mov cx, 0x0143
    mov di, 0x2000
    int 0x10
    cmp ax, 0x004F
    jne .fallback

    mov eax, [0x2028]
    mov [0x5000], eax
    movzx eax, word [0x2010]
    mov [0x5004], eax
    movzx eax, word [0x2012]
    mov [0x5008], eax
    movzx eax, word [0x2014]
    mov [0x500C], eax
    movzx eax, byte [0x2019]
    mov [0x5010], eax

    mov ax, 0x4F02
    mov bx, 0x4143
    int 0x10
    jmp .vbe_done

.fallback:
    mov ax, 0x0013
    int 0x10
    mov dword [0x5000], 0x000A0000
    mov dword [0x5004], 320
    mov dword [0x5008], 320
    mov dword [0x500C], 200
    mov dword [0x5010], 8

.vbe_done:
    mov di, 0x6000
    xor ebx, ebx
    xor bp, bp
.e820:
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 20
    int 0x15
    jc .e820_done
    cmp eax, 0x534D4150
    jne .e820_done
    inc bp
    add di, 20
    test ebx, ebx
    jnz .e820
.e820_done:
    mov [0x5F00], bp

    mov si, kload1
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc .disk_err

    mov si, kload2
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc .disk_err

    mov si, kload3
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc .disk_err

    mov si, kload4
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc .disk_err

    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:stage2_pm

.disk_err:
    mov si, err_msg
.print:
    lodsb
    or al, al
    jz $
    mov ah, 0x0E
    int 0x10
    jmp .print

boot_drive db 0x80
err_msg    db 'Stage2 disk error!', 0

kload1:
    db 0x10, 0x00
    dw 127
    dw 0x0000, 0x0800
    dq 16

kload2:
    db 0x10, 0x00
    dw 127
    dw 0xFE00, 0x0800
    dq 143

kload3:
    db 0x10, 0x00
    dw 127
    dw 0xFC00, 0x1800
    dq 270

kload4:
    db 0x10, 0x00
    dw 127
    dw 0xFA00, 0x2800
    dq 397

gdt_start:
    dq 0
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
gdt_end:

gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

[bits 32]
stage2_pm:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    jmp 0x8000
