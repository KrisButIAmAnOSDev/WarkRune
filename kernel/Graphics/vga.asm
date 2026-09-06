global vbe_framebuffer
global vbe_pitch
global vbe_width
global vbe_height
global vbe_bpp

vbe_framebuffer:  dd 0
vbe_pitch:        dd 0
vbe_width:        dd 0
vbe_height:       dd 0
vbe_bpp:          dd 0

global vbe_init
vbe_init:
    mov eax, [0x5000]
    mov [vbe_framebuffer], eax

    mov eax, [0x5004]
    mov [vbe_pitch], eax

    mov eax, [0x5008]
    mov [vbe_width], eax

    mov eax, [0x500C]
    mov [vbe_height], eax

    mov eax, [0x5010]
    mov [vbe_bpp], eax

    ret
