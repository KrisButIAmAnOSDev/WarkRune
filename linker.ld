ENTRY(start)

SECTIONS
{
    . = 0x8000;

    .text :
    {
        *(.text)
    }

    .data :
    {
        *(.data)
    }

    .rodata :
    {
        *(.rodata)
    }

    .bss :
    {
        *(.bss)
        *(COMMON)
    }

    . = 0x300000;

    .pagetab (NOLOAD) :
    {
        *(.pagetab)
    }
}
