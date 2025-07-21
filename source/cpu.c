#include <cpu.h>
#include <inst.h>
#include <interrupt.h>
#include <paging.h>
#include <string.h>

// TODO instructions that can't be ran in usermode

static inline uint64_t get_sp(struct core *c) {
	return c->registers[c->registers[PPR] == 0 ? SP1 : SP0];
}

static inline void set_sp(struct core *c, uint64_t v) {
	if (c->registers[PPR] == 0)
		c->registers[SP1] = v;
	else
		c->registers[SP0] = v;
}

static void update_arith_flags(struct core *c, uint64_t res, uint64_t, uint64_t,
							   bool carry, bool overflow) {
	uint64_t fr = c->registers[FR] & ~(FLAG_CF | FLAG_OF | FLAG_ZF | FLAG_SF);
	if (carry)
		fr |= FLAG_CF;
	if (overflow)
		fr |= FLAG_OF;
	if (res == 0)
		fr |= FLAG_ZF;
	if ((int64_t)res < 0)
		fr |= FLAG_SF;
	c->registers[FR] = fr;
}

static void update_logic_flags(struct core *c, uint64_t res) {
	uint64_t fr = c->registers[FR] & ~(FLAG_ZF | FLAG_SF);
	if (res == 0)
		fr |= FLAG_ZF;
	if ((int64_t)res < 0)
		fr |= FLAG_SF;
	c->registers[FR] = fr;
}

static bool cond_ok(struct core *c, enum cmove_argument_mode cond) {
	uint64_t fr = c->registers[FR];
	bool z = (fr & FLAG_ZF) != 0;
	bool s = (fr & FLAG_SF) != 0;
	bool o = (fr & FLAG_OF) != 0;
	switch (cond) {
	case NE:
		return !z;
	case EQ:
		return z;
	case GT:
		return !z && (s == o);
	case LT:
		return s != o;
	case GE:
		return s == o;
	case LE:
		return z || (s != o);
	default:
		return false;
	}
}

void cpu_init(struct core *c, struct ram *mem) {
	memset(c, 0, sizeof(*c));
	c->mem = mem;
	c->irc = malloc(sizeof *c->irc);
	irc_init(c->irc, c);
	c->registers[SP1] = mem->cap;
	c->registers[SP0] = mem->cap - 0x1000;
	c->registers[PC] = 0;
}

bool cpu_step(struct core *c) {
	uint64_t old_pc = c->registers[PC];
	struct instruction inst;
	uint64_t next_pc = parse_instruction(c, &inst, old_pc);
	c->registers[PC] = next_pc;

	uint64_t a, b, res, sp, addr;
	uint8_t r1, r2;

	switch (inst.opcode) {

	case MOV:
		switch (inst.type) {
		case RR:
			r1 = inst.register_register.reg1;
			r2 = inst.register_register.reg2;
			c->registers[r1] = c->registers[r2];
			break;
		case RM:
			r1 = inst.register_memory.reg1;
			addr = inst.register_memory.address;
			c->registers[r1] = vread64(c, addr);
			break;
		case RI:
			r1 = inst.register_imm.reg1;
			c->registers[r1] = inst.register_imm.imm64;
			break;
		default: /* unreachable */
			break;
		}
		break;

	case ADD:
	case SUB:
	case MUL:
	case DIV:
		if (inst.opcode == DIV && inst.type == RR &&
			inst.register_register.reg2 != 0) {
			/* fall through */
		}
		if ((inst.type == RR &&
			 c->registers[inst.register_register.reg2] == 0)) {
			irc_raise_interrupt(c->irc, ICR_DIV_BY_ZERO);
			return true;
		}
		if (inst.type == RR) {
			r1 = inst.register_register.reg1;
			a = c->registers[r1];
			b = inst.register_register.reg2 == 2 /* DIV? */
					? c->registers[inst.register_register.reg2]
					: c->registers[inst.register_register.reg2];
		} else {
			r1 = inst.register_imm.reg1;
			a = c->registers[r1];
			b = inst.register_imm.imm64;
		}
		switch (inst.opcode) {
		case ADD:
			res = a + b;
			update_arith_flags(
				c, res, a, b,
				/* carry*/ (res < a),
				/* overflow*/
				(((int64_t)a > 0 && (int64_t)b > 0 && (int64_t)res < 0) ||
				 ((int64_t)a < 0 && (int64_t)b < 0 && (int64_t)res > 0)));
			c->registers[r1] = res;
			break;
		case SUB:
			res = a - b;
			update_arith_flags(
				c, res, a, b,
				/* borrow*/ (a < b),
				/* overflow*/
				(((int64_t)a > 0 && (int64_t)b < 0 && (int64_t)res < 0) ||
				 ((int64_t)a < 0 && (int64_t)b > 0 && (int64_t)res > 0)));
			c->registers[r1] = res;
			break;
		case MUL:
			res = a * b;
			update_arith_flags(c, res, a, b, (b != 0 && res / b != a), false);
			c->registers[r1] = res;
			break;
		case DIV:
			if (inst.type == RI && b == 0) {
				irc_raise_interrupt(c->irc, ICR_DIV_BY_ZERO);
				return true;
			}
			c->registers[r1] = a / b;
			c->registers[FR] &= ~(FLAG_CF | FLAG_OF | FLAG_ZF | FLAG_SF);
			break;
		default:
			break;
		}
		break;

	case OR:
	case AND:
	case XOR:
	case NOT: {
		bool is_not = (inst.opcode == NOT);
		if ((inst.type != RR && inst.type != RI) ||
			(is_not && inst.type != RR && inst.type != RI)) {
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return true;
		}
		if (inst.type == RR) {
			r1 = inst.register_register.reg1;
			a = c->registers[r1];
			b = is_not ? 0 : c->registers[inst.register_register.reg2];
		} else {
			r1 = inst.register_imm.reg1;
			a = c->registers[r1];
			b = inst.register_imm.imm64;
		}
		switch (inst.opcode) {
		case OR:
			res = a | b;
			break;
		case AND:
			res = a & b;
			break;
		case XOR:
			res = a ^ b;
			break;
		case NOT:
			res = ~a;
			break;
		default:
			res = a;
			break;
		}
		c->registers[r1] = res;
		update_logic_flags(c, res);
		break;
	}

	case PUSH:
		sp = get_sp(c) - 8;
		set_sp(c, sp);
		if (inst.one_arg.mode == REGISTER) {
			vwrite64(c, sp, c->registers[inst.one_arg.reg]);
		} else if (inst.one_arg.mode == IMM) {
			vwrite64(c, sp, inst.one_arg.imm64);
		} else {
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return true;
		}
		break;

	case POP:
		sp = get_sp(c);
		res = vread64(c, sp);
		set_sp(c, sp + 8);
		if (inst.one_arg.mode == REGISTER) {
			c->registers[inst.one_arg.reg] = res;
		} else {
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return true;
		}
		break;

	case CALL:
		sp = get_sp(c) - 8;
		set_sp(c, sp);
		vwrite64(c, sp, next_pc);

		if (inst.one_arg.mode == REGISTER) {
			c->registers[PC] = c->registers[inst.one_arg.reg];
		} else if (inst.one_arg.mode == IMM) {
			c->registers[PC] = inst.one_arg.imm64;
		} else if (inst.one_arg.mode == ADDRESS) {
			uint64_t j2 = vread64(c, inst.one_arg.address);
			c->registers[PC] = j2;
		} else {
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return true;
		}
		return true;

	case RET:
		sp = get_sp(c);
		c->registers[PC] = vread64(c, sp);
		set_sp(c, sp + 8);
		return true;

	case CMP: {
		if (inst.type != RR && inst.type != RI) {
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return true;
		}
		if (inst.type == RR) {
			a = c->registers[inst.register_register.reg1];
			b = c->registers[inst.register_register.reg2];
		} else {
			a = c->registers[inst.register_imm.reg1];
			b = inst.register_imm.imm64;
		}
		res = a - b;
		update_arith_flags(
			c, res, a, b, (a < b),
			(((int64_t)a > 0 && (int64_t)b < 0 && (int64_t)res < 0) ||
			 ((int64_t)a < 0 && (int64_t)b > 0 && (int64_t)res > 0)));
		break;
	}

	case CMOV:
		if (cond_ok(c, inst.cmove.cond)) {
			c->registers[inst.cmove.reg1] = c->registers[inst.cmove.reg2];
		}
		break;

	case SYSCALL:
		sp = get_sp(c) - 8;
		set_sp(c, sp);
		vwrite64(c, sp, next_pc);
		c->registers[PPR] = 0;
		c->registers[PC] = c->registers[SLR];
		return true;

	case SYSRET:
		sp = get_sp(c);
		c->registers[PC] = vread64(c, sp);
		set_sp(c, sp + 8);
		c->registers[PPR] = 1;
		return true;

	case RETI:
		sp = get_sp(c);
		c->registers[PPR] = vread64(c, sp);
		sp += 8;
		c->registers[IMR] = vread64(c, sp);
		sp += 8;
		c->registers[PC] = vread64(c, sp);
		sp += 8;
		set_sp(c, sp);
		c->irc->in_exception = false;
		return true;

	case HLT:
		c->registers[PC] = old_pc;
		return false;

	case COANDSW:
		addr = inst.register_memory.address;
		a = vread64(c, addr);
		b = inst.register_register.reg1;
		if (a == b)
			vwrite64(c, addr, c->registers[R0]);
		c->registers[R0] = a;
		break;

	default:
		irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
		return true;
	}

	return true;
}
