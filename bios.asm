    .define FIRMWARE_BASE     0x7FFF000
    .define SAFE_MASK         0xfffffffffffffff8
    .define FB_BASE           0x90000000

    .org FIRMWARE_BASE
_start:
    mov itr, _idt_base
    mov imr, 0

    mov r0, 0
    mov r1, 0
    mov r2, 480
    mov r6, 0xFF0000FF
.draw_row:
    mov r3, 640
    mov r4, 0
.draw_pixel:
    mov r5, r1
    mul r5, 640
    add r5, r4
    mul r5, 4
    add r5, FB_BASE

    str r5, r6

    add r4, 1
    cmp r4, 640
    mov r30, .draw_pixel
    cmov ne, pc, r30

    add r1, 1
    cmp r1, r2
    mov r30, .draw_row
    cmov ne, pc, r30

    mov pc, idle_loop

_idt_base:
    dq     _isr_double_fault
    dq     _isr_div_by_zero
    dq     _isr_invalid_opcode
    dq     _isr_page_fault
    dq     _isr_prot_fault
    dq 0
    dq 0
    dq 0
    dq 0
    dq 0
    dq 0
    dq     _isr_keyboard

_isr_double_fault:
    hlt
_isr_div_by_zero:
    hlt
_isr_invalid_opcode:
    hlt
_isr_page_fault:
    hlt
_isr_prot_fault:
    hlt

_syscall:
    hlt

_isr_keyboard:
    mov r7, [color_index]
    add r7, 1
    and r7, 0xF

    mov r20, color_index
    str r20, r7

    mov r6, r7
    mov r8, r6

    mul r6, 0x11
    mov r9, 0x1000000 
    mul r6, r9

    mul r8, 0x21
    mov r10, 0x10000
    mul r8, r10
    add r6, r8

    mov r8, r7
    mul r8, 0x31
    mov r10, 0x100
    mul r8, r10
    add r6, r8
    
    add r6, 0xFF

    mul r6, 21

    mov r0, 0
    mov r1, 0
    mov r2, 480

    mov imr, 0

    mov pc, _start.draw_row

idle_loop:
    mov pc, idle_loop

color_index:
