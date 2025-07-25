    .define FIRMWARE_BASE     0x7FFF000
    .define SAFE_MASK         0xfffffffffffffff8
    .define FB_BASE           0x90000000
    .define KB_BASE           0x90010000
    .define KB_KEY            0x90010008

    .org FIRMWARE_BASE
_start:
    mov itr, _idt_base
    mov imr, 0
    mov r31, 0

    mov r0, 100 ; pos_x 
    mov r1, 100 ; pos_y

.key_input: 
    cmp r31, 0
    mov r30, .key_input
    cmov eq, pc, r30

    cmp r31, 115 ; s
    mov r30, .add_y
    cmov eq, pc, r30

    cmp r31, 119 ; w
    mov r30, .sub_y
    cmov eq, pc, r30

    cmp r31, 97 ; a
    mov r30, .sub_x
    cmov eq, pc, r30

    cmp r31, 100 ; d
    mov r30, .add_x
    cmov eq, pc, r30

    mov r31, 0
    mov pc, .key_input

.add_y:
    mov r31, 0
    add r1, 1
    mov pc, .draw_pixel
.add_x:
    mov r31, 0
    add r0, 1
    mov pc, .draw_pixel
.sub_y:
    mov r31, 0
    sub r1, 1
    mov pc, .draw_pixel
.sub_x:
    mov r31, 0
    sub r0, 1
    mov pc, .draw_pixel

.draw_pixel:
    mov r5, r1
    mul r5, 640
    add r5, r0
    mul r5, 4
    add r5, FB_BASE

    mov r6, 0xFF0000FF
    str r5, r6

    mov pc, .key_input

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
    db 0
_isr_div_by_zero:
    hlt
    db 1
_isr_invalid_opcode:
    hlt
    db 2
_isr_page_fault:
    hlt
    db 3
_isr_prot_fault:
    hlt
    db 4

_syscall: 
    hlt
    db 0x69

_isr_keyboard:
    mov r31, [KB_KEY]
    mov imr, 0
    reti
idle_loop:
    add r1, 1
    mov pc, idle_loop
