#ifndef CPU_H
#define CPU_H

#include <stdint.h>

enum registers_id : uint8_t {
	R0,
	R1,
	R2,
	R3,
	R4,
	R5,
	R6,
	R7,
	R8,
	R9,
	R10,
	R11,
	R12,
	R13,
	R14,
	R15,
	R16,
	R17,
	R18,
	R19,
	R20,
	R21,
	R22,
	R23,
	R24,
	R25,
	R26,
	R27,
	R28,
	R29,
	R30,
	R31,
	PC,
	SP1,
	FR,
	SP0,
	PPTR,
	IMR,
	ITR,
	SLR,
	PPR,
};

struct irc;

struct core {
	uint64_t registers[41];
	struct irc* irc;
	struct ram* mem;
	// optional TODO tlb
};

#endif // CPU_H
