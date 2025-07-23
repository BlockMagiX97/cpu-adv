-> CISC
-> 64 bits
-> Byte addressed memmory

-> Interrupt Table (IT)
    | qword -> address handler

| R0 -> R31 (general purpose) -> 64 bits
| PC (Program Counter) -> 64 bits
-> SP0 -> 64 bits
-> SP1 -> 64 bits
| FR (flag register) -> 64 bits
| CF, ZF (EF), OF, LF, GF, SF
-> PTTR (Page Table Top Register)
-> IMR (Interrupt Mask Register) each bit masks interrupt -> 64 bits
 | Queue -> Unmask -> Deliver
-> ITR (Interrupt Table Register)
-> SLR (Syscall Landing Register) -> 64 bit

-> Protection
    PPR (Processor Priviledge Register) 0 -> Kernel/Supervisor 1 -> User => 64 bits

Interrupt -> Push SP0 (kernel stack)

65 interrupts total

0 -> Double Fault (failed to service exception)

Interrupt 1 -> Div by Zero
Interrupt 2 -> Invalid Opcode
Interrupt 3 -> Page Fault
Interrupt 4 -> Protection Fault

Interrupt Routing Chipset (IRC) Hardware Interrupt (timer, keyboard, disk etc) per core
    IRC0 (interrupt 10) -> Timer tick => 0 + irc->irc_to_isr
    IRC1 (interrupt 11) -> Keyboard Event => 1 + irc->irc_to_isr
    IRC2 (interrupt 12) -> Floppy disk event
    IRC3 (interrupt 13) -> Inter Processor Interrupt

64 bits
12 offset to page (4kb pages)

-> 4 level paging each table indexs 13 bits into next table
; 8192 page table level ENTRIES per page

bit 0 -> present
bit 1 -> !supervisor
bit 2 -> write
bit 3 -> execute
bits 4->19 -> reserved
bis 20->32 offset

# ISA

    RR -> (2 register operants)
    RM -> (register + memory)
    RI -> (register + imm)

    OA -> (one argument (register, memory, immediate))
    NO -> (no argument)
    CM -> (4 bits flag (ELG), REG1 REG2)

-> ALU (Arythemtico Logic)
MOV => RR, RM, RI
ADD  => RR, RI
SUB => RR, RI
MUL => RR, RI
DIV => RR, RI
OR => RR, RI
AND => RR, RI
NOT => RR, RI
XOR => RR, RI

PUSH => OA(r), OA(i)
POP => OA(r)

CALL => OA(r), OA(i) (<= OA(i) is for anything, addresses and labels)
CMP => RR, RI
CMOV => RR, RI (3 functions that operate on flags register((e)quals, (l)ess, (g)reater (n)ot-equals) -> (le) (ge))

RET => NO
RETI => NO
SYSRET => NO
SYSCALL => NO

HLT => NO
COANDSW => RM (Register => Expected, R0 => New) Memory Operand -> PTR
```c
COANDSW RN (register expected) ML (memory ptr)
    orig = *ML
    if (orig == RN)
        *ML = R0
    return orig
```
STR => RR, RI (RI is inverse, stores the register into an IMM value, assemblers should inverse the operands)

```c
enum registers_id : uint8_t {
    R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15, R16, R17, R18, R19, R20, R21, R22, R23, R24, R25, R26, R27, R28, R29, R30, R31, 
    PC, SP1, FR,
    SP0, PPTR, IMR, ITR, SLR, PPR,
};

struct instruction {
    unsigned char type : 3;
    unsigned char opcode : 5;

    union {
        struct {
            enum registers reg1;
            enum registers reg2;
        } RegisterRegister;
        struct {
            enum registers reg1;
            unsigned long long address;
        } RegisterMemory;
        struct {
            enum registers reg1;
            unsigned long long imm64;
        } RegisterImm;
        struct {
            unsigned char : 2;
            union {
                enum registers reg;
                unsigned long long address;
                unsigned long long imm64;
            }
        }
    }
}
```

Interrupt push mechanism
PC -> IMR -> PPR => SP0

Reti pop ( PPR -> IMR -> PC ) => SP0
