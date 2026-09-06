global jump_usermode

section .text

jump_usermode:
    push ebp
    mov ebp, esp
    mov ebx, [ebp + 8]
    mov ecx, [ebp + 12]

    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push dword 0x23
    push ecx
    pushf
    pop eax
    or eax, 0x200
    push eax
    push dword 0x1B
    push ebx
    iret
