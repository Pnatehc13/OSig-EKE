/* Multiboot Header Constants */
.set ALIGN,    1<<0             /* Align loaded modules on 4KB page boundaries */
.set MEMINFO,  1<<1             /* Request memory map from bootloader */
.set FLAGS,    ALIGN | MEMINFO  /* Multiboot flag field */
.set MAGIC,    0x1BADB002       /* Magic number for Multiboot */
.set CHECKSUM, -(MAGIC + FLAGS) /* Checksum to prove multiboot validity */

/* Multiboot Header Section */
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/* Reserve 16KB Stack for CPU */
.section .bss
.align 16
stack_bottom:
.skip 16384 /* 16 KiB */
stack_top:

/* Kernel Entry Point */
.section .text
.global _start
.type _start, @function
_start:
    /* Set up stack pointer (ESP) */
    mov $stack_top, %esp

    /* Call C++ main function */
    call kernel_main

    /* Disable interrupts and halt CPU if kernel_main returns */
    cli
1:  hlt
    jmp 1b
