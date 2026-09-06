[org 0x7C00]
[bits 16]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7B00
    sti

    mov [boot_drive], dl

    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc .err

    mov dl, [boot_drive]
    jmp 0x0050:0x0000

.err:
    mov si, err_msg
.print:
    lodsb
    or al, al
    jz $
    mov ah, 0x0E
    int 0x10
    jmp .print

boot_drive db 0x80
err_msg    db 'Stage1 fail!', 0

dap:
    db 0x10
    db 0x00
    dw 15
    dw 0x0000
    dw 0x0050
    dq 1

times 510 - ($ - $$) db 0
dw 0xAA55
