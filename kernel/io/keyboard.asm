global kbd_handler
global read_scancode
global kbd_shift_down

section .data
align 4
kbd_buf times 32 db 0   ; ring buffer, power-of-2 size
kbd_head dd 0            ; write index (ISR writes here)
kbd_tail dd 0            ; read index  (read_scancode reads here)
kbd_shift_down db 0

section .text

kbd_handler:
    pushad
    in      al, 0x60

    ; compute next head = (head + 1) & 31
    mov     ebx, [kbd_head]
    mov     ecx, ebx
    inc     ecx
    and     ecx, 31
    cmp     ecx, [kbd_tail]
    je      .full           ; buffer full — drop scancode

    mov     [kbd_buf + ebx], al
    mov     [kbd_head], ecx

.full:
    mov     al, 0x20
    out     0x20, al
    popad
    iret

read_scancode:
    cli
    xor     eax, eax

    mov     ebx, [kbd_tail]
    cmp     ebx, [kbd_head]
    je      .empty          ; buffer empty — return 0

    mov     al, [kbd_buf + ebx]
    inc     ebx
    and     ebx, 31
    mov     [kbd_tail], ebx

.empty:
    sti
    ret
