    .define FIRMWARE_BASE 0x7FFF000
    .define SAFE_MASK 0xfffffffffffffff8
    .define FB_BASE 0x90000000
    .define KBD_BASE 0x90010000
    .define SCREEN_W 640
    .define SCREEN_H 480
    .define PADDLE_W 100
    .define PADDLE_Y 470
    .define ROW_STRIDE 2560

    .org FIRMWARE_BASE
_start:
    mov itr, _idt_base
    mov imr, SAFE_MASK

    mov r9, 320
    mov r10, 240
    mov r7, 1
    mov r8, -1
    
    mov r11, 270
    mov r12, r11

    mov r13, PADDLE_W
    mov r17, PADDLE_Y
    mul r17, ROW_STRIDE

    mov r18, 0xFFFFFFFF
    mov r19, 0xFF00FF00
    mov r20, 0x00000000

.game_loop:
    mov r21, r10
    mul r21, ROW_STRIDE
    mov r22, r9
    mul r22, 4
    add r21, r22
    mov r23, FB_BASE
    add r23, r21
    mov r24, r20
    str r23, r24

    mov   r22, 0
.clear_old_paddle:
    cmp r22, r13
    mov r30, .after_clear_old_paddle
    cmov ge, pc, r30

    mov r21, r12
    add r21, r22
    mov r25, r21
    mul r25, 4
    mov r26, r17
    add r25, r26
    mov r23, FB_BASE
    add r23, r25
    mov r24, r20
    str r23, r24

    add r22, 1
    mov r30, .clear_old_paddle
    cmov ne, pc, r30
.after_clear_old_paddle:

    mov r25, [KBD_BASE]
    cmp r25, 0
    mov r30, .upd_ball
    cmov eq, pc, r30

    mov r25, [0x90010001]

    cmp r25, 65
    mov r30, .move_left
    cmov eq, pc, r30
    cmp r25, 68
    mov r30, .move_right
    cmov eq, pc, r30

    mov r12, r11
    mov pc, .upd_ball

.move_left:
    mov r25, r11
    sub r25, 10
    cmp r25, 0
    mov r30, .set_p0
    cmov lt, pc, r30
    mov r11, r25
    mov pc, .post_move
.set_p0:
    mov r11, 0
    mov pc, .post_move

.move_right:
    mov r25, r11
    add r25, 10
    cmp r25, 540
    mov r30, .set_pmax
    cmov gt, pc, r30
    mov r11, r25
    mov pc, .post_move
.set_pmax:
    mov r11, 540
    mov pc, .post_move

.post_move:
    mov r12, r11

.upd_ball:
    mov r25, r9
    add r25, r7
    mov r9,  r25
    mov r25, r10
    add r25, r8
    mov r10, r25

    cmp r9, 0
    mov r30, .bx_pos
    cmov ge, pc, r30
    mov r9, 0
    mov r7, -1
    mov pc, .ax_done
.bx_pos:
    cmp r9, 639
    mov r30, .ax_done
    cmov  le, pc, r30
    mov r9, 639
    mov r7, -1
.ax_done:

    cmp r10, 0
    mov r30, .by_after
    cmov gt, pc, r30
    mov r10, 0
    mov r8,  1
    mov pc, .by_after

.by_after:
    cmp r10, 469
    mov r30, .draw
    cmov lt, pc, r30

    cmp r9, r11
    mov r30, .game_over
    cmov lt, pc, r30
    mov r25, r11
    add r25, r13
    cmp r9, r25
    mov r30, .game_over
    cmov ge, pc, r3

    mov r10, 469
    mov r8, -1
    mov pc, .draw

.game_over:
    hlt

.draw:
    mov r21, r10
    mul r21, ROW_STRIDE
    mov r22, r9
    mul r22, 4
    add r21, r22
    mov r23, FB_BASE
    add r23, r21
    mov r24, r18
    str r23, r24

    mov r22, 0
.draw_paddle:
    cmp r22, r13
    mov r30, .loop_end
    cmov ge, pc, r30

    mov r21, r11
    add r21, r22
    mov r25, r21
    mul r25, 4
    mov r26, r17
    add r25, r26
    mov r23, FB_BASE
    add r23, r25
    mov r24, r19
    str r23, r24

    add r22, 1
    mov r30, .draw_paddle
    cmov ne, pc, r30
.loop_end:
    mov pc, .game_loop

_idt_base:
    dq 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
