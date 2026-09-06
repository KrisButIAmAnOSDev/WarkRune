global idt_load
global idt_set_gate
global timer_handler
global exception_divide_zero
global exception_invalid_op
global exception_no_fpu
global exception_double_fault
global exception_stack_fault
global exception_gpf
global exception_page_fault
global exception_overflow

extern _panic
extern _page_fault_handler
extern pit_tick
extern sched_tick

section .data

idt:
    times 256 * 8 db 0

idt_ptr:
    dw 256 * 8 - 1
    dd idt

section .rodata

msg_div_zero    db "Divide by Zero", 0
msg_invalid_op  db "Invalid Opcode", 0
msg_no_fpu      db "FPU Not Available", 0
msg_double      db "Double Fault", 0
msg_stack       db "Stack Fault", 0
msg_gpf         db "General Protection Fault", 0
msg_overflow    db "Overflow", 0

section .text

pic_init:
    mov al, 0x11
    out 0x20, al
    out 0xA0, al

    mov al, 0x20
    out 0x21, al
    mov al, 0x28
    out 0xA1, al

    mov al, 0x04
    out 0x21, al
    mov al, 0x02
    out 0xA1, al

    mov al, 0x01
    out 0x21, al
    out 0xA1, al

    mov al, 0xF8        ; 11111000
    out 0x21, al

    mov al, 0xEF        ; 11101111
    out 0xA1, al

    ret

idt_load:
    call pic_init
    lidt [idt_ptr]
    sti
    ret

idt_set_gate:
    push ebp
    mov ebp, esp
    push eax
    push ebx
    push ecx
    push edx
    mov eax, [ebp+8]
    mov ebx, [ebp+12]
    mov ecx, [ebp+16]
    mov edx, [ebp+20]
    imul eax, 8
    add eax, idt
    mov word [eax],   bx
    mov word [eax+2], cx
    mov byte [eax+4], 0
    mov byte [eax+5], dl
    shr ebx, 16
    mov word [eax+6], bx
    pop edx
    pop ecx
    pop ebx
    pop eax
    pop ebp
    ret

timer_handler:
    pushad                  ; save EAX,ECX,EDX,EBX,ESP_snap,EBP,ESI,EDI
    call pit_tick           ; increment tick counter
    push esp                ; pass current kernel ESP (→ pushad frame) as arg
    call sched_tick         ; returns new ESP in EAX (same or next task)
    add esp, 4              ; remove argument
    mov esp, eax            ; switch to (possibly new) task's kernel stack
    mov al, 0x20
    out 0x20, al            ; send EOI to PIC after context switch
    popad                   ; restore registers from new stack
    iret                    ; return to (possibly new) ring-3 task

global mouse_irq
extern mouse_irq_handler

mouse_irq:
    pushad
    call mouse_irq_handler
    popad
    iret


%macro EXCEPTION_HANDLER 2
%1:
    pushad
    push dword 0
    push dword %2
    call _panic
    add esp, 8
    popad
    iret
%endmacro

EXCEPTION_HANDLER exception_divide_zero,  msg_div_zero
EXCEPTION_HANDLER exception_invalid_op,   msg_invalid_op
EXCEPTION_HANDLER exception_no_fpu,       msg_no_fpu
EXCEPTION_HANDLER exception_double_fault, msg_double
EXCEPTION_HANDLER exception_stack_fault,  msg_stack
EXCEPTION_HANDLER exception_gpf,          msg_gpf
EXCEPTION_HANDLER exception_overflow,     msg_overflow

exception_page_fault:
    pushad
    mov eax, cr2
    push eax
    call _page_fault_handler
    add esp, 4
    popad
    iret
