import re
import struct
from enum import IntEnum
from itertools import chain

class Registers(IntEnum):
    R0=0;   R1=1;   R2=2;   R3=3;   R4=4;   R5=5;   R6=6;   R7=7
    R8=8;   R9=9;   R10=10; R11=11; R12=12; R13=13; R14=14; R15=15
    R16=16; R17=17; R18=18; R19=19; R20=20; R21=21; R22=22; R23=23
    R24=24; R25=25; R26=26; R27=27; R28=28; R29=29; R30=30; R31=31
    PC=32; SP1=33; FR=34; SP0=35; PPTR=36; IMR=37; ITR=38; SLR=39; PPR=40

class OperandType(IntEnum):  # 3 bits
    RR = 0
    RM = 1
    RI = 2
    OA = 3
    NO = 4
    CM = 5

class Opcode(IntEnum):      # 5 bits
    MOV=0;   ADD=1;   SUB=2;   MUL=3;   DIV=4
    OR=5;    AND=6;   NOT=7;   XOR=8;   PUSH=9
    POP=10;  CALL=11;  CMP=12;  CMOV=13; RET=14
    RETI=15; SYSRET=16; SYSCALL=17; HLT=18; COANDSW=19

class OneArgumentMode(IntEnum):
    REGISTER=0
    ADDRESS =1
    IMM     =2

class CMovOperandModes(IntEnum):
    NE=0   # not‑equal (000b)
    GT=1   # greater   (001b)
    LT=2   # less      (010b)
    EQ=4   # equal     (100b)
    LE=6   # ≤         (110b)
    GE=5   # ≥         (101b)

class Assembler:
    _label_def = re.compile(r'^\s*([A-Za-z_]\w*):\s*$')
    _org       = re.compile(r'^\s*\.org\s+(.+)$', re.IGNORECASE)
    _instr     = re.compile(r'^\s*([A-Za-z]+)(?:\s+(.*))?$')
    _reg       = re.compile(r'^(r[0-9]|r[12][0-9]|r3[01]|pc|sp0|sp1|fr|pptr|imr|itr|slr|ppr)$', re.IGNORECASE)
    _mem       = re.compile(r'^\[\s*([^\]+]+?)\s*\]$')
    _split_op  = re.compile(r'\s*,\s*')
    _int       = re.compile(r'^[0-9]+$|^0x[0-9A-Fa-f]+$')

    def __init__(self, pad_on_org = False):
        self.labels = {}
        self.lines  = []
        self.binary= bytearray()
        self.origin= 0
        self.pad_on_org = pad_on_org

    def load(self, text:str):
        for n, raw in enumerate(text.splitlines(),1):
            line = re.sub(r';.*|//.*','', raw).strip()
            if line:
                self.lines.append((n, line))

    def first_pass(self):
        pc = 0
        for lineno, line in self.lines:
            m_lbl = self._label_def.match(line)
            if m_lbl:
                lbl = m_lbl.group(1)
                if lbl in self.labels:
                    raise SyntaxError(f"Line {lineno}: label `{lbl}` redefined")
                self.labels[lbl] = pc
                continue

            m_org = self._org.match(line)
            if m_org:
                pc = int(m_org.group(1),0)
                continue

            size = self._size_of(line)
            pc += size

        self.origin = 0
        for _, line in self.lines:
            if self._org.match(line):
                self.origin = int(self._org.match(line).group(1),0)
                break

    def second_pass(self):
        pc = self.origin
        for lineno, line in self.lines:
            if self._label_def.match(line):
                continue

            m_org = self._org.match(line)
            if m_org:
                new_org = int(m_org.group(1), 0)
                if self.pad_on_org:
                    pad_len = new_org - pc
                    if pad_len > 0:
                        print("Extending binary for ", pad_len)
                        self.binary.extend(b'\x00' * pad_len)
                pc = new_org
                continue

            hdr, op_bytes = self._encode_instr(line, lineno)
            self.binary.extend(hdr)
            self.binary.extend(op_bytes)
            pc += len(hdr) + len(op_bytes)

    def assemble(self, text:str) -> bytes:
        self.load(text)
        self.first_pass()
        self.second_pass()
        return bytes(self.binary)

    def _size_of(self, line:str) -> int:
        hdr, ops = self._encode_instr(line, 0, dry_run=True)
        return len(hdr) + len(ops)

    def _encode_instr(self, line:str, lineno:int, dry_run=False):
        m = self._instr.match(line)
        if not m:
            raise SyntaxError(f"Line {lineno}: can't parse `{line}`")

        op_name = m.group(1).upper()
        operand_text = m.group(2) or ''
        if op_name not in Opcode.__members__:
            raise SyntaxError(f"Line {lineno}: unknown opcode `{op_name}`")

        opcode = Opcode[op_name]
        operands = [o.strip() for o in self._split_op.split(operand_text) if o.strip()]

        otype = self._determine_type(op_name, operands, lineno)
        hdr_byte = ((otype.value & 0b111) << 5) | (opcode.value & 0b11111)
        hdr = bytes([hdr_byte])

        ops = self._encode_operands(otype, operands, lineno)
        return hdr, ops

    def _determine_type(self, op_name:str, ops:list[str], lineno:int) -> OperandType:
        cnt = len(ops)
        if cnt == 0:
            return OperandType.NO
        if op_name == 'CMOV':
            return OperandType.CM

        if op_name in ('PUSH','POP'):
            return OperandType.OA

        if cnt == 2:
            a, b = ops
            a_is_reg = bool(self._reg.match(a))
            b_is_reg = bool(self._reg.match(b))
            b_is_mem = bool(self._mem.match(b))
            b_is_imm = bool(self._int.match(b)) or b in self.labels

            if a_is_reg and b_is_reg:
                return OperandType.RR
            if a_is_reg and (b_is_mem):
                return OperandType.RM
            if a_is_reg and b_is_imm:
                return OperandType.RI

        raise SyntaxError(f"Line {lineno}: can't determine operand type for `{op_name}` {ops}")

    def _encode_operands(self, otype:OperandType, ops:list[str], lineno:int) -> bytes:
        out = bytearray()
        if otype is OperandType.NO:
            return out

        if otype is OperandType.RR:
            r1 = self._parse_reg(ops[0], lineno)
            r2 = self._parse_reg(ops[1], lineno)
            return bytes((r1, r2))

        if otype is OperandType.RM:
            r = self._parse_reg(ops[0], lineno)
            addr = self._parse_mem(ops[1], lineno)
            return bytes([r]) + struct.pack('<Q', addr)

        if otype is OperandType.RI:
            r = self._parse_reg(ops[0], lineno)
            imm = self._parse_imm(ops[1], lineno)
            return bytes([r]) + struct.pack('<Q', imm)

        if otype is OperandType.OA:
            a = ops[0]
            if self._reg.match(a):
                mode = OneArgumentMode.REGISTER
                r    = self._parse_reg(a, lineno)
                return bytes([mode.value]) + bytes([r])
            if self._mem.match(a):
                mode = OneArgumentMode.ADDRESS
                addr = self._parse_mem(a, lineno)
                return bytes([mode.value]) + struct.pack('<Q', addr)

            mode = OneArgumentMode.IMM
            imm  = self._parse_imm(a, lineno)
            return bytes([mode.value]) + struct.pack('<Q', imm)

        if otype is OperandType.CM:
            if len(ops)==3:
                cond, aa, bb = ops
                if cond.upper() in CMovOperandModes.__members__:
                    mode = CMovOperandModes[cond.upper()]
                    r1 = self._parse_reg(aa, lineno)
                    r2 = self._parse_reg(bb, lineno)
                else:
                    raise SyntaxError(f"Line {lineno}: unknown CMOV cond `{cond}`")
            else:
                mode = CMovOperandModes.EQ
                r1 = self._parse_reg(ops[0], lineno)
                r2 = self._parse_reg(ops[1], lineno)

            mode_byte = (mode.value & 0xF) << 4
            return bytes([mode_byte, r1, r2])

        raise AssertionError("unhandled operand type")

    def _parse_reg(self, tok:str, lineno:int) -> int:
        if not self._reg.match(tok):
            raise SyntaxError(f"Line {lineno}: expected register, got `{tok}`")
        return Registers[tok.upper()].value

    def _parse_mem(self, tok:str, lineno:int) -> int:
        m = self._mem.match(tok)
        if not m:
            raise SyntaxError(f"Line {lineno}: expected memory operand, got `{tok}`")
        inner = m.group(1)

        parts = inner.split('+',1)
        label_or_reg = parts[0].strip()
        if label_or_reg in self.labels:
            base = self.labels[label_or_reg]
            offset = 0
        elif self._reg.match(label_or_reg):
            base = 0
            offset = 0
            raise NotImplementedError("reg-relative mem not supported yet")
        else:
            base = int(label_or_reg,0)
            offset=0

        if len(parts)==2:
            off = parts[1].strip()
            offset = int(off,0)
        return base + offset

    def _parse_imm(self, tok:str, lineno:int) -> int:
        if tok in self.labels:
            return self.labels[tok]
        if not self._int.match(tok):
            raise SyntaxError(f"Line {lineno}: expected immediate/label, got `{tok}`")
        return int(tok,0)

if __name__ == '__main__':
    import sys
    import argparse

    def main():
        parser = argparse.ArgumentParser(
            description='Assemble a simple .asm file into binary.'
        )
        parser.add_argument(
            'input',
            metavar='INFILE',
            help='Path to the assembly source file'
        )
        parser.add_argument(
            '-o', '--output',
            metavar='OUTFILE',
            help='Write the assembled machine code to OUTFILE (default: stdout)'
        )
        parser.add_argument(
            '-po', '--pad-org',
            action='store_true',
            help='Zero-pad the output when you see an .org so that the file offset matches the origin'
        )
        args = parser.parse_args()

        try:
            with open(args.input, 'r') as f:
                src = f.read()
        except IOError as e:
            print(f"Error: cannot open input file {args.input!r}: {e}", file=sys.stderr)
            return 1

        asm = Assembler(pad_on_org=args.pad_org)
        try:
            machine = asm.assemble(src)
        except SyntaxError as e:
            print(f"Assembly failed: {e}", file=sys.stderr)
            return 1
        except NotImplementedError as e:
            print(f"Assembly error: {e}", file=sys.stderr)
            return 1

        if args.output:
            try:
                with open(args.output, 'wb') as out:
                    out.write(machine)
            except IOError as e:
                print(f"Error: cannot write to output file {args.output!r}: {e}", file=sys.stderr)
                return 1
        else:
            print(' '.join('0x{:02x}'.format(x) for x in machine))

        return 0

    sys.exit(main())

