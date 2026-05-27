OUTPUT_FORMAT("elf32-littleriscv")
OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
    . = 0x10000;
    .text : {
        KEEP(*(.trap.text))
        *(.text.init)
        *(.text)
        *(.text.*)
    }
    .data : {
        *(.data)
        *(.rodata*)
    }
    .bss : {
        *(.bss)
        *(.sbss)
        *(COMMON)
    }
}
