#ifndef INST_H
#define INST_H

#include <cpu.h>
#include <stdint.h>

enum operand_type : uint8_t {
	RR = 0, // two‐register
	RM = 1, // reg + memory
	RI = 2, // reg + immediate
	OA = 3, // one‐arg (reg|mem|imm)
	NO = 4, // no‐arg
	CM = 5	// conditional move
};

enum opcode : uint8_t {
	MOV = 0,
	ADD = 1,
	SUB = 2,
	MUL = 3,
	DIV = 4,
	OR = 5,
	AND = 6,
	NOT = 7,
	XOR = 8,
	PUSH = 9,
	POP = 10,
	CALL = 11,
	CMP = 12,
	CMOV = 13,
	RET = 14,
	RETI = 15,
	SYSRET = 16,
	SYSCALL = 17,
	HLT = 18,
	COANDSW = 19
};

enum one_argument_mode : uint8_t {
	REGISTER = 0,
	ADDRESS = 1,
	IMM = 2,
};

enum cmove_argument_mode : uint8_t {
	NE = 0, // 000b
	GT = 1, // 001b
	LT = 2, // 010b
	EQ = 4, // 100b
	LE = 6, // 110b
	GE = 5	// 101b
};

struct instruction {
	enum operand_type type : 3;
	enum opcode opcode : 5;

	union {
		struct {
			enum registers_id reg1;
			enum registers_id reg2;
		} register_register;

		struct {
			enum registers_id reg1;
			uint64_t address;
		} register_memory;

		struct {
			enum registers_id reg1;
			uint64_t imm64;
		} register_imm;

		struct {
			uint8_t mode;
			union {
				enum registers_id reg;
				uint64_t address;
				uint64_t imm64;
			};
		} one_arg;

		struct {
			enum cmove_argument_mode cond;
			enum registers_id reg1;
			enum registers_id reg2;
		} cmove;
	};
};

int is_valid_reg(uint8_t r);

uint64_t parse_instruction(struct core *c, struct instruction *inst,
						   uint64_t old_pc);

uint64_t parse_instruction_ro(struct core *c, struct instruction *inst,
							  uint64_t old_pc);

#endif // INST_H
