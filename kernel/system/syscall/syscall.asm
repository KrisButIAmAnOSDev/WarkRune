global syscall_stub
extern syscall_handler

; Extended ABI: EAX=num, EBX=a, ECX=b, EDX=c, ESI=d
; syscall_handler(num, a, b, c, d)
syscall_stub:
    pushad
    ; After pushad: [esp+0]=EDI [esp+4]=ESI(d) [esp+8]=EBP
    ;   [esp+12]=ESP [esp+16]=EBX(a) [esp+20]=EDX(c) [esp+24]=ECX(b) [esp+28]=EAX(num)
    push dword [esp+4]   ; d = saved ESI
    push edx             ; c
    push ecx             ; b
    push ebx             ; a
    push eax             ; num
    call syscall_handler
    add esp, 20
    mov [esp+28], eax    ; write return value into saved EAX slot
    popad
    iret
