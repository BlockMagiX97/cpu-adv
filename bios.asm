    .define FIRMWARE_BASE     0x7FFFFF
    .define SAFE_MASK         0x0FFFFFFFFFFFFFFF
    .define GPU_MMIO_BASE     0x3FFE0000ULL
    .define GPU_REG_CONTROL   (GPU_MMIO_BASE + 0x10)
    .define GPU_REG_WIDTH     (GPU_MMIO_BASE + 0x00)
    .define GPU_REG_HEIGHT    (GPU_MMIO_BASE + 0x08)
    .define GPU_REG_FB_BASE   (GPU_MMIO_BASE + 0x20)
    .define GPU_CTRL_ENABLE   0x1
    .define GPU_CTRL_GRAPHICS 0x4

    .org FIRMWARE_BASE
_start:
    mov    SP0, 0x0000_F0000
    mov    SP1, 0x0000_80000

    mov    ITR, _idt_base
    mov    IMR, SAFE_MASK

    mov    SLR, _syscall

    ; mov    R1, GPU_REG_WIDTH
    mov    R2, 640
    str    R1, R2
    str    0x200, R1

    ; mov    R1, GPU_REG_HEIGHT
    mov    R2, 480
    str    R1, R2

    ; mov    R1, GPU_REG_CONTROL
    ; mov    R2, GPU_CTRL_ENABLE | GPU_CTRL_GRAPHICS
    str    R1, R2

    ; mov    R3, GPU_REG_FB_BASE

    ;; for y in 0..479:
    ;;   for x in 0..639:
    ;;      color = ((x*0xFF/639) << 16) | ((y*0xFF/479) << 8)
    ;;      fb[y*640 + x] = color
    mov    R4, 0                ;; y = 0
loop_y:
    mov    R5, 0                ;; x = 0
loop_x:
    ;; compute red = x * 255 / 639
    mov    R6, R5
    mul    R6, 255
    div    R6, 639

    ;; compute green = y * 255 / 479
    mov    R7, R4
    mul    R7, 255
    div    R7, 479

    ;; assemble color: R6<<16 | R7<<8
    mov    R8, R6
    mul    R8, 65536
    mov    R9, R7
    mul    R9, 256
    or     R8, R9

    ;; compute pixel address: FB + 4*(y*640 + x)
    mov    R9, R4
    mul    R9, 640
    add    R9, R5
    mul    R9, 4
    ; mov    R10, GPU_REG_FB_BASE
    add    R10, R9

    str    R10, R8

    add    R5, 1
    cmp    R5, 640
    mov R31, loop_x
    cmov lt, pc, R31

    add    R4, 1
    cmp    R4, 480
    mov R31, loop_y
    cmov lt, pc, R31

    mov pc, idle_loop


_idt_base:
    dq     _isr_double_fault
    dq     _isr_div_by_zero
    dq     _isr_invalid_opcode
    dq     _isr_page_fault
    dq     _isr_prot_fault
    ;; other entries ignored

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

idle_loop:
    hlt
    mov pc, idle_loop
