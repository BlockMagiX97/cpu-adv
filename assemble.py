#!/usr/bin/env python3
import re
import struct
import sys
from enum import IntEnum
from typing import List, Tuple


class Registers(IntEnum):
    R0=0; R1=1; R2=2; R3=3; R4=4; R5=5; R6=6; R7=7
    R8=8; R9=9; R10=10; R11=11; R12=12; R13=13; R14=14; R15=15
    R16=16; R17=17; R18=18; R19=19; R20=20; R21=21; R22=22; R23=23
    R24=24; R25=25; R26=26; R27=27; R28=28; R29=29; R30=30; R31=31
    PC=32; SP1=33; FR=34; SP0=35; PPTR=36; IMR=37; ITR=38; SLR=39; PPR=40


class OperandType(IntEnum):
    RR = 0   # reg,reg
    RM = 1   # reg,mem
    RI = 2   # reg,imm
    OA = 3   # one‑arg (reg | mem | imm)
    NO = 4   # none
    CM = 5   # conditional move (cond, reg, reg)


class Opcode(IntEnum):
    MOV=0; ADD=1; SUB=2; MUL=3; DIV=4
    OR=5; AND=6; NOT=7; XOR=8; PUSH=9
    POP=10; CALL=11; CMP=12; CMOV=13; RET=14
    RETI=15; SYSRET=16; SYSCALL=17; HLT=18; COANDSW=19
    STR=20


class OneArgumentMode(IntEnum):
    REGISTER = 0
    ADDRESS  = 1
    IMM      = 2


class CMovOperandModes(IntEnum):
    NE=0; GT=1; LT=2; EQ=4; LE=6; GE=5


class Assembler:
    _label_def = re.compile(r'^\s*([A-Za-z_]\w*):\s*$')
    _instr     = re.compile(r'^\s*([A-Za-z]+)(?:\s+(.*))?$')
    _reg       = re.compile(r'^(r[0-9]|r[12][0-9]|r3[01]|pc|sp0|sp1|fr|pptr|imr|itr|slr|ppr)$', re.IGNORECASE)
    _mem       = re.compile(r'^\[\s*([^\]]+?)\s*\]$')
    _split_op  = re.compile(r'\s*,\s*')
    _int       = re.compile(r'^[0-9][0-9_]*$|^0[xX][0-9A-Fa-f_]+$')
    _sym       = re.compile(r'^[A-Za-z_]\w*$')

    def __init__(self):
        self.raw_lines: List[Tuple[int,str]] = []
        self.lines: List[Tuple[int,str]]    = []
        self.macros: dict                   = {}
        self.labels: dict                   = {}
        self.binary: bytearray              = bytearray()

    def load(self, text: str):
        for n, raw in enumerate(text.splitlines(), 1):
            code = re.sub(r';.*|//.*', '', raw).strip()
            if code:
                self.raw_lines.append((n, code))

    def unwrap_macros(self):
        for lineno, code in self.raw_lines:
            m = re.match(
                r'^\s*\.?define\s+'
                r'([A-Za-z_]\w*)'
                r'(?:\s*=\s*|\s+)'
                r'(.+)$',
                code, re.IGNORECASE
            )
            if m:
                name, expr = m.group(1), m.group(2).strip()
                self.macros[name] = expr
            else:
                expanded = code
                for _ in range(len(self.macros)):
                    prev = expanded
                    for name, expr in self.macros.items():
                        expanded = re.sub(
                            r'\b' + re.escape(name) + r'\b',
                            expr, expanded
                        )
                    if expanded == prev:
                        break
                self.lines.append((lineno, expanded))

    def first_pass(self):
        pc = 0
        for lineno, line in self.lines:
            mo = re.match(r'^\s*\.?org\s+(.+)$', line, re.IGNORECASE)
            if mo:
                addr = self._parse_imm(mo.group(1).strip(), lineno)
                pc = addr
                continue

            lm = self._label_def.match(line)
            if lm:
                lbl = lm.group(1)
                if lbl in self.labels:
                    raise SyntaxError(f"Line {lineno}: label `{lbl}` redefined")
                self.labels[lbl] = pc
                continue

            size = self._size_of(line, lineno)
            pc += size

    def second_pass(self):
        for lineno, line in self.lines:
            if self._label_def.match(line):
                continue
            if re.match(r'^\s*\.?org\s+(.+)$', line, re.IGNORECASE):
                continue
            hdr, ops = self._encode_instr(line, lineno, dry_run=False)
            self.binary.extend(hdr)
            self.binary.extend(ops)

    def assemble(self, text: str) -> bytes:
        self.load(text)
        self.unwrap_macros()
        self.first_pass()
        self.second_pass()
        return bytes(self.binary)

    def _size_of(self, line: str, lineno: int) -> int:
        hdr, ops = self._encode_instr(line, lineno, dry_run=True)
        return len(hdr) + len(ops)

    def _encode_instr(self, line: str, lineno: int, dry_run: bool) -> Tuple[bytes, bytes]:
        mdir = re.match(r'^\s*\.?(DB|DW|DD|DQ)\s+(.*)$', line, re.IGNORECASE)
        if mdir:
            directive = mdir.group(1).lower()
            args = [o.strip() for o in self._split_op.split(mdir.group(2)) if o.strip()]
            size_map = {'db':1, 'dw':2, 'dd':4, 'dq':8}
            total = size_map[directive] * len(args)
            if dry_run:
                return b'', bytes(total)
            out = bytearray()
            for tok in args:
                val = self._parse_imm(tok, lineno)
                if directive == 'db':
                    out += struct.pack('<B', val & 0xFF)
                elif directive == 'dw':
                    out += struct.pack('<H', val & 0xFFFF)
                elif directive == 'dd':
                    out += struct.pack('<I', val & 0xFFFFFFFF)
                else:
                    out += struct.pack('<Q', val & 0xFFFFFFFFFFFFFFFF)
            return b'', bytes(out)

        m = self._instr.match(line)
        if not m:
            raise SyntaxError(f"Line {lineno}: can't parse `{line}`")
        op_name = m.group(1).upper()
        if op_name not in Opcode.__members__:
            raise SyntaxError(f"Line {lineno}: unknown opcode `{op_name}`")
        opcode = Opcode[op_name]
        ops = [o.strip() for o in self._split_op.split(m.group(2) or '') if o.strip()]
        if op_name == 'STR' and len(ops) == 2:
            a, b = ops
            is_a_imm = bool(self._int.match(a) or self._sym.match(a))
            is_b_reg = bool(self._reg.match(b))
            if is_a_imm and is_b_reg:
                ops = [b, a]
        
        otype = self._determine_type(op_name, ops, lineno)

        hdr = bytes([((otype.value & 0b111) << 5) | (opcode.value & 0b11111)])
        if dry_run:
            if   otype == OperandType.RR: size = 2
            elif otype == OperandType.CM: size = 3
            elif otype in (OperandType.RM, OperandType.RI, OperandType.OA): size = 1 + 8
            else: size = 0
            return hdr, bytes(size)

        ops_bytes = self._encode_operands(otype, ops, lineno)
        return hdr, ops_bytes

    def _determine_type(self, op:str, ops:List[str], lineno:int) -> OperandType:
        cnt = len(ops)
        if op in ('RET','RETI','SYSRET','SYSCALL','HLT'):
            if cnt: raise SyntaxError(f"Line {lineno}: `{op}` takes no operands")
            return OperandType.NO
        
        if op == 'STR':
            if len(ops) != 2:
                raise SyntaxError(f"Line {lineno}: STR needs 2 operands")
            a, b = ops
            a_reg = bool(self._reg.match(a))
            b_reg = bool(self._reg.match(b))
            b_imm = bool(self._int.match(b)) or bool(self._sym.match(b))
            if a_reg and b_reg:
                return OperandType.RR
            if a_reg and b_imm:
                return OperandType.RI

            raise SyntaxError(f"Line {lineno}: invalid STR `{ops}`")

        if op == 'MOV':
            if cnt!=2: raise SyntaxError(f"Line {lineno}: MOV needs 2 operands")
            a,b = ops
            a_reg = bool(self._reg.match(a))
            b_reg = bool(self._reg.match(b))
            b_mem = bool(self._mem.match(b))
            b_imm = bool(self._int.match(b)) or bool(self._sym.match(b))
            if a_reg and b_reg: return OperandType.RR
            if a_reg and b_mem: return OperandType.RM
            if a_reg and b_imm: return OperandType.RI
            raise SyntaxError(f"Line {lineno}: invalid MOV `{ops}`")

        if op in ('ADD','SUB','MUL','DIV','OR','AND','NOT','XOR','CMP'):
            if cnt!=2: raise SyntaxError(f"Line {lineno}: {op} needs 2 operands")
            a,b = ops
            a_reg = bool(self._reg.match(a))
            b_reg = bool(self._reg.match(b))
            b_imm = bool(self._int.match(b)) or bool(self._sym.match(b))
            if a_reg and b_reg: return OperandType.RR
            if a_reg and b_imm: return OperandType.RI
            raise SyntaxError(f"Line {lineno}: invalid {op} `{ops}`")

        if op == 'PUSH':
            if cnt!=1: raise SyntaxError(f"Line {lineno}: PUSH needs 1 operand")
            a=ops[0]
            if self._reg.match(a) or self._int.match(a) or self._sym.match(a) or self._mem.match(a):
                return OperandType.OA
            raise SyntaxError(f"Line {lineno}: invalid PUSH `{a}`")

        if op == 'POP':
            if cnt!=1: raise SyntaxError(f"Line {lineno}: POP needs 1 operand")
            if self._reg.match(ops[0]): return OperandType.OA
            raise SyntaxError(f"Line {lineno}: invalid POP `{ops[0]}`")

        if op == 'CALL':
            if cnt!=1: raise SyntaxError(f"Line {lineno}: CALL needs 1 operand")
            a=ops[0]
            if ( self._reg.match(a)
              or self._int.match(a)
              or self._sym.match(a)
              or self._mem.match(a) ):
                return OperandType.OA
            raise SyntaxError(f"Line {lineno}: invalid CALL `{a}`")

        if op == 'CMOV':
            if cnt==3:
                c,a,b=ops
                if c.upper() not in CMovOperandModes.__members__:
                    raise SyntaxError(f"Line {lineno}: bad CMOV cond `{c}`")
                if self._reg.match(a) and self._reg.match(b):
                    return OperandType.CM
            if cnt==2 and self._reg.match(ops[0]) and self._reg.match(ops[1]):
                return OperandType.CM
            raise SyntaxError(f"Line {lineno}: invalid CMOV `{ops}`")

        if op == 'COANDSW':
            if cnt==2 and self._reg.match(ops[0]) and self._mem.match(ops[1]):
                return OperandType.RM
            raise SyntaxError(f"Line {lineno}: COANDSW needs (reg, mem)")

        raise SyntaxError(f"Line {lineno}: unknown opcode `{op}` or bad operands")

    def _encode_operands(self, otype:OperandType, ops:List[str], lineno:int) -> bytes:
        out = bytearray()
        if otype == OperandType.NO:
            return out

        if otype == OperandType.RR:
            out += bytes(( self._parse_reg(ops[0],lineno),
                           self._parse_reg(ops[1],lineno) ))
            return out

        if otype == OperandType.RM:
            out.append(self._parse_reg(ops[0],lineno))
            addr = self._parse_mem(ops[1],lineno)
            out += struct.pack('<Q', addr)
            return out

        if otype == OperandType.RI:
            out.append(self._parse_reg(ops[0],lineno))
            imm = self._parse_imm(ops[1],lineno)
            out += struct.pack('<Q', imm)
            return out

        if otype == OperandType.OA:
            a=ops[0]
            if self._reg.match(a):
                mode,val = OneArgumentMode.REGISTER.value, self._parse_reg(a,lineno)
                out += bytes((mode,val))
            elif self._mem.match(a):
                mode = OneArgumentMode.ADDRESS.value
                addr = self._parse_mem(a,lineno)
                out.append(mode)
                out += struct.pack('<Q', addr)
            else:
                mode = OneArgumentMode.IMM.value
                imm  = self._parse_imm(a, lineno)
                out.append(mode)
                out += struct.pack('<Q', imm)
            return out

        if otype == OperandType.CM:
            if len(ops)==3:
                cond,a,b = ops
                mode = CMovOperandModes[cond.upper()].value
                r1 = self._parse_reg(a,lineno)
                r2 = self._parse_reg(b,lineno)
            else:
                mode = CMovOperandModes.EQ.value
                r1 = self._parse_reg(ops[0],lineno)
                r2 = self._parse_reg(ops[1],lineno)
            out.append((mode&0xF)<<4)
            out.append(r1)
            out.append(r2)
            return out

        raise AssertionError("Unhandled operand type")

    def _parse_reg(self, tok:str, lineno:int) -> int:
        if not self._reg.match(tok):
            raise SyntaxError(f"Line {lineno}: expected register, got `{tok}`")
        return Registers[tok.upper()].value

    def _parse_mem(self, tok:str, lineno:int) -> int:
        m = self._mem.match(tok)
        if not m:
            raise SyntaxError(f"Line {lineno}: expected memory operand, got `{tok}`")
        inner = m.group(1).strip()
        if '+' in inner:
            base_tok, off_tok = map(str.strip, inner.split('+',1))
            base = (self.labels.get(base_tok)
                 if base_tok in self.labels
                 else int(base_tok,0))
            return base + int(off_tok,0)
        if inner in self.labels:
            return self.labels[inner]
        if self._int.match(inner):
            inner_clean = inner.replace('_','')
            return int(inner_clean,0)
        raise SyntaxError(f"Line {lineno}: unknown memory label/const `{inner}`")

    def _parse_imm(self, tok:str, lineno:int) -> int:
        if tok in self.labels:
            return self.labels[tok]
        if not self._int.match(tok):
            raise SyntaxError(f"Line {lineno}: expected immediate or label, got `{tok}`")
        tok_clean = tok.replace('_','')
        return int(tok_clean, 0)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <input.asm> [-o out.bin]", file=sys.stderr)
        sys.exit(1)
    infile = sys.argv[1]
    outfile = sys.argv[3] if len(sys.argv)>=4 and sys.argv[2]=='-o' else None

    with open(infile) as f:
        src = f.read()

    asm = Assembler()
    try:
        machine = asm.assemble(src)
    except SyntaxError as e:
        print(f"Assembly failed: {e}", file=sys.stderr)
        sys.exit(1)

    if outfile:
        with open(outfile,'wb') as f:
            f.write(machine)
    else:
        print(' '.join(f'0x{b:02x}' for b in machine))


if __name__ == '__main__':
    main()
