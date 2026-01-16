/*
 * Nix-Opus 64-bit Kernel Entry Point
 * Called from UEFI bootloader (already in long mode)
 */

.section .text
.global _start

.code64

_start:
    /* UEFI has already set up long mode and a valid GDT */
    /* RDI contains pointer to boot_info (System V AMD64 ABI) */

    /* Disable interrupts */
    cli

    /* Save boot_info pointer */
    mov %rdi, %r15

    /* Set up our own stack */
    lea kernel_stack_top(%rip), %rsp

    /* Clear direction flag */
    cld

    /* Clear BSS section */
    lea __bss_start(%rip), %rdi
    lea __bss_end(%rip), %rcx
    sub %rdi, %rcx
    jz .bss_done
    shr $3, %rcx            /* Divide by 8 (clear qwords) */
    xor %rax, %rax
    rep stosq
.bss_done:

    /* Restore boot_info pointer to RDI for kernel_main */
    mov %r15, %rdi

    /* Align stack to 16 bytes (required by System V ABI) */
    and $~0xF, %rsp

    /* Call kernel main */
    call kernel_main

    /* Halt if kernel returns */
.halt:
    cli
    hlt
    jmp .halt

/* Stack - 64KB */
.section .bss
.align 16
kernel_stack_bottom:
    .space 65536
.global kernel_stack_top
kernel_stack_top:
