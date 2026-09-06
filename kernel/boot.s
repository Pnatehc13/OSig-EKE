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

/* -------------------------------------------------------------------------
   BSS Section: 4-level Page Tables and Stack
   ------------------------------------------------------------------------- */
.section .bss
.align 4096
pml4_table:
    .skip 4096                  /* Level 4: Page Map Level 4 */
pdpt_table:
    .skip 4096                  /* Level 3: Page Directory Pointer Table */
pd_table:
    .skip 4096                  /* Level 2: Page Directory (uses 2MB huge pages) */

.align 16
stack_bottom:
    .skip 16384                 /* 16 KiB Stack */
stack_top:

/* -------------------------------------------------------------------------
   GDT for 64-bit Long Mode
   ------------------------------------------------------------------------- */
.section .rodata
.align 8
gdt64:
    .quad 0                     /* Null Descriptor */
    /* Code Segment: Executable, Present, Ring 0, Long Mode (L-bit = 1) */
    .quad (1<<41) | (1<<43) | (1<<44) | (1<<47) | (1<<53)
    /* Data Segment: Writable, Present, Ring 0 */
    .quad (1<<41) | (1<<44) | (1<<47)
gdt64_end:
.align 4
gdt64_pointer:
    .word gdt64_end - gdt64 - 1
    .long gdt64

/* -------------------------------------------------------------------------
   Code Section: Starts in 32-bit Protected Mode
   ------------------------------------------------------------------------- */
.section .text
.code32
.global _start
.type _start, @function
_start:
    /* Set up temporary 32-bit stack */
    mov $stack_top, %esp

    /* 1. Point PML4[0] -> PDPT (Present | Writable) */
    mov $pdpt_table, %eax
    or $0x03, %eax
    mov %eax, pml4_table

    /* 2. Point PDPT[0] -> PD (Present | Writable) */
    mov $pd_table, %eax
    or $0x03, %eax
    mov %eax, pdpt_table

    /* 3. Identity map 1GB using 512 huge 2MB pages in PD */
    mov $0, %ecx
.fill_pd:
    mov %ecx, %eax
    shl $21, %eax               /* eax = ecx * 2MB */
    or $0x83, %eax              /* Present | Writable | Huge (2MB) */
    mov %eax, pd_table(, %ecx, 8)
    movl $0, pd_table + 4(, %ecx, 8)
    inc %ecx
    cmp $512, %ecx
    jne .fill_pd

    /* 4. Load CR3 with PML4 address */
    mov $pml4_table, %eax
    mov %eax, %cr3

    /* 5. Enable PAE in CR4 */
    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4

    /* 6. Enable Long Mode in EFER MSR */
    mov $0xC0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr

    /* 7. Enable Paging in CR0 (Enters compatibility mode) */
    mov %cr0, %eax
    or $(1 << 31), %eax
    mov %eax, %cr0

    /* 8. Load 64-bit GDT */
    lgdt (gdt64_pointer)

    /* 9. Far Jump to flush pipeline into pure 64-bit mode */
    ljmp $0x08, $long_mode_start

/* -------------------------------------------------------------------------
   Pure 64-bit Long Mode begins here!
   ------------------------------------------------------------------------- */
.code64
.extern kernel_main
long_mode_start:
    /* Reload data segment registers */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Set up 64-bit Stack Pointer */
    mov $stack_top, %rsp

    /* Call C++ 64-bit kernel_main */
    call kernel_main

    /* Halt if kernel_main returns */
    cli
1:  hlt
    jmp 1b
