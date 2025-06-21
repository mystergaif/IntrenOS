ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot ALIGN(4K) :
    {
        *(.multiboot)
    }

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    /DISCARD/ :
    {
        *(.note*)
        *(.iplt)
        *(.igot*)
        *(.rel*)
        *(.fini)
        *(.eh_frame)
    }
} 