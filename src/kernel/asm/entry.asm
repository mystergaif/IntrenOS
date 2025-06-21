; Multiboot 1 header
; You can find the Multiboot specification at https://www.gnu.org/software/grub/manual/multiboot/multiboot.html#Multiboot-header
section .multiboot
align 4
    ; Multiboot header
    dd 0x1BADB002             ; Multiboot magic number
    dd 0x03                   ; Flags: align modules and provide memory map
    dd -(0x1BADB002 + 0x03)   ; Checksum

section .data
    align 4096

; initial stack
section .initial_stack, nobits
    align 4

stack_bottom:
    ; 1 MB of uninitialized data for stack
    resb 104856
stack_top:

; kernel entry, main text section
section .text
    global _start

; define _start, aligned by linker.ld script
_start:
    ; Set up stack
    mov esp, stack_top
    
    ; Clear interrupts
    cli
    
    ; Clear direction flag
    cld
    
    ; Save multiboot info
    push ebx
    
    ; Call kernel main
    extern kmain
    call kmain

loop:
    hlt
    jmp loop
