#include <assert.h>
#include <inst.h>
#include <interrupt.h>
#include <ncurses.h>
#include <paging.h>

#define is_valid_reg(r) ((r) <= PPR)

uint64_t parse_instruction(struct core *c, struct instruction *inst,
						   uint64_t old_pc) {
	uint8_t header = vread8(c, old_pc);
	inst->type = header >> 5;
	inst->opcode = header & 0x1F;
	uint64_t new_pc = old_pc + 1;

	switch (inst->type) {
	case NO:
		switch (inst->opcode) {
		case RET:
		case RETI:
		case SYSRET:
		case SYSCALL:
		case HLT:
			return new_pc;
		default:
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return 0;
		}

	case OA:
		switch (inst->opcode) {
		case PUSH:
		case POP:
		case CALL:
			break;
		default:
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return 0;
		}
		inst->one_arg.mode = vread8(c, new_pc++);
		if (inst->one_arg.mode == REGISTER) {
			inst->one_arg.reg = vread8(c, new_pc++);
			if (!is_valid_reg(inst->one_arg.reg)) {
				irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
				return 0;
			}
		} else if (inst->one_arg.mode == ADDRESS) {
			inst->one_arg.address = vread64(c, new_pc);
			new_pc += 8;
		} else if (inst->one_arg.mode == IMM) {
			inst->one_arg.imm64 = vread64(c, new_pc);
			new_pc += 8;
		} else {
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return 0;
		}
		return new_pc;

	case RR: {
		switch (inst->opcode) {
		case MOV:
		case ADD:
		case SUB:
		case MUL:
		case DIV:
		case OR:
		case AND:
		case NOT:
		case XOR:
		case CMP:
			break;
		default:
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return 0;
		}
		uint8_t r1 = vread8(c, new_pc++);
		uint8_t r2 = vread8(c, new_pc++);
		if (!is_valid_reg(r1) || !is_valid_reg(r2)) {
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return 0;
		}
		inst->register_register.reg1 = r1;
		inst->register_register.reg2 = r2;
		return new_pc;
	}

	case RM: {
		switch (inst->opcode) {
		case MOV:
		case COANDSW:
			break;
		default:
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return 0;
		}
		uint8_t r = vread8(c, new_pc++);
		if (!is_valid_reg(r)) {
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return 0;
		}
		inst->register_memory.reg1 = r;
		inst->register_memory.address = vread64(c, new_pc);
		new_pc += 8;
		return new_pc;
	}

	case RI: {
		switch (inst->opcode) {
		case MOV:
		case ADD:
		case SUB:
		case MUL:
		case DIV:
		case OR:
		case AND:
		case NOT:
		case XOR:
		case CMP:
			break;
		default:
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return 0;
		}
		uint8_t r = vread8(c, new_pc++);
		if (!is_valid_reg(r)) {
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return 0;
		}
		inst->register_imm.reg1 = r;
		inst->register_imm.imm64 = vread64(c, new_pc);
		new_pc += 8;
		return new_pc;
	}

	case CM:
		if (inst->opcode != CMOV) {
			irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
			return 0;
		}
		{
			uint8_t mb = vread8(c, new_pc++);
			uint8_t cond = mb >> 4;
			if (cond > GE) {
				irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
				return 0;
			}
			inst->cmove.cond = (enum cmove_argument_mode)cond;

			uint8_t r1 = vread8(c, new_pc++);
			uint8_t r2 = vread8(c, new_pc++);
			if (!is_valid_reg(r1) || !is_valid_reg(r2)) {
				irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
				return 0;
			}
			inst->cmove.reg1 = r1;
			inst->cmove.reg2 = r2;
			return new_pc;
		}

	default:
		irc_raise_interrupt(c->irc, ICR_INVALID_OPCODE);
		return 0;
	}
}

uint64_t parse_instruction_ro(struct core *c, struct instruction *inst,
							  uint64_t old_pc) {
	uint8_t header = vread8(c, old_pc);
	inst->type = header >> 5;
	inst->opcode = header & 0x1F;
	uint64_t new_pc = old_pc + 1;

	switch (inst->type) {
	case NO:
		switch (inst->opcode) {
		case RET:
		case RETI:
		case SYSRET:
		case SYSCALL:
		case HLT:
			return new_pc;
		default:
			return 0;
		}

	case OA:
		switch (inst->opcode) {
		case PUSH:
		case POP:
		case CALL:
			break;
		default:
			return 0;
		}
		inst->one_arg.mode = vread8(c, new_pc++);
		if (inst->one_arg.mode == REGISTER) {
			inst->one_arg.reg = vread8(c, new_pc++);
			if (!is_valid_reg(inst->one_arg.reg)) {
				return 0;
			}
		} else if (inst->one_arg.mode == ADDRESS) {
			inst->one_arg.address = vread64(c, new_pc);
			new_pc += 8;
		} else if (inst->one_arg.mode == IMM) {
			inst->one_arg.imm64 = vread64(c, new_pc);
			new_pc += 8;
		} else {
			return 0;
		}
		return new_pc;

	case RR: {
		switch (inst->opcode) {
		case MOV:
		case ADD:
		case SUB:
		case MUL:
		case DIV:
		case OR:
		case AND:
		case NOT:
		case XOR:
		case CMP:
		case STR:
			break;
		default:
			return 0;
		}
		uint8_t r1 = vread8(c, new_pc++);
		uint8_t r2 = vread8(c, new_pc++);
		if (!is_valid_reg(r1) || !is_valid_reg(r2)) {
			return 0;
		}
		inst->register_register.reg1 = r1;
		inst->register_register.reg2 = r2;
		return new_pc;
	}

	case RM: {
		switch (inst->opcode) {
		case MOV:
		case COANDSW:
			break;
		default:
			return 0;
		}
		uint8_t r = vread8(c, new_pc++);
		if (!is_valid_reg(r)) {
			return 0;
		}
		inst->register_memory.reg1 = r;
		inst->register_memory.address = vread64(c, new_pc);
		new_pc += 8;
		return new_pc;
	}

	case RI: {
		switch (inst->opcode) {
		case MOV:
		case ADD:
		case SUB:
		case MUL:
		case DIV:
		case OR:
		case AND:
		case NOT:
		case XOR:
		case CMP:
		case STR:
			break;
		default:
			return 0;
		}
		uint8_t r = vread8(c, new_pc++);
		if (!is_valid_reg(r)) {
			return 0;
		}
		inst->register_imm.reg1 = r;
		inst->register_imm.imm64 = vread64(c, new_pc);
		new_pc += 8;
		return new_pc;
	}

	case CM:
		if (inst->opcode != CMOV)
			return 0;
		{
			uint8_t mb = vread8(c, new_pc++);
			uint8_t cond = mb >> 4;
			if (cond > GE) {
				return 0;
			}
			inst->cmove.cond = (enum cmove_argument_mode)cond;

			uint8_t r1 = vread8(c, new_pc++);
			uint8_t r2 = vread8(c, new_pc++);
			if (!is_valid_reg(r1) || !is_valid_reg(r2)) {
				return 0;
			}
			inst->cmove.reg1 = r1;
			inst->cmove.reg2 = r2;
			return new_pc;
		}

	default:
		return 0;
	}
}