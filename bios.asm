mov itr, idt
hlt

idt:
    dq 0x99
    dq div_by_zero
    dq 69 ; invalid opcode
    dq 2 ; page fauklt
    dq 3
    dq 4

div_by_zero:
    hlt