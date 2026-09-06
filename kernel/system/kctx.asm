global ksetjmp
global klongjmp

section .text

ksetjmp:
    mov eax, [esp+4]
    mov [eax+0],  ebx
    mov [eax+4],  esi
    mov [eax+8],  edi
    mov [eax+12], ebp
    mov [eax+16], esp
    mov ecx, [esp]
    mov [eax+20], ecx
    xor eax, eax
    ret

klongjmp:
    mov eax, [esp+4]
    mov ebx, [eax+0]
    mov esi, [eax+4]
    mov edi, [eax+8]
    mov ebp, [eax+12]
    mov esp, [eax+16]
    mov eax, 1
    ret
