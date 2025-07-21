#include <inttypes.h>
#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cpu.h>
#include <inst.h>
#include <interrupt.h>
#include <paging.h>

static const char *reg_names[41] = {
	"R0",	"R1",  "R2",  "R3",	 "R4",	"R5",  "R6",  "R7",	 "R8",
	"R9",	"R10", "R11", "R12", "R13", "R14", "R15", "R16", "R17",
	"R18",	"R19", "R20", "R21", "R22", "R23", "R24", "R25", "R26",
	"R27",	"R28", "R29", "R30", "R31", "PC",  "SP1", "FR",	 "SP0",
	"PPTR", "IMR", "ITR", "SLR", "PPR"};

static const char *opcode_names[20] = {
	"MOV", "ADD",  "SUB",	 "MUL",		"DIV",	"OR",	  "AND",
	"NOT", "XOR",  "PUSH",	 "POP",		"CALL", "CMP",	  "CMOV",
	"RET", "RETI", "SYSRET", "SYSCALL", "HLT",	"COANDSW"};

static const char *cmov_names[8] = {[NE] = "NE", [GT] = "GT", [LT] = "LT",
									[EQ] = "EQ", [LE] = "LE", [GE] = "GE"};

static void format_inst(struct instruction *inst, uint64_t pc, char *buf,
						size_t buf_sz) {
	int off = snprintf(buf, buf_sz, "%016" PRIx64 ": %s", pc,
					   opcode_names[inst->opcode]);
	char *p = buf + off;
	size_t rem = buf_sz - off;

	switch (inst->type) {
	case RR:
		snprintf(p, rem, " %s, %s", reg_names[inst->register_register.reg1],
				 reg_names[inst->register_register.reg2]);
		break;
	case RM:
		snprintf(p, rem, " %s, [0x%016" PRIx64 "]",
				 reg_names[inst->register_memory.reg1],
				 inst->register_memory.address);
		break;
	case RI:
		snprintf(p, rem, " %s, 0x%016" PRIx64,
				 reg_names[inst->register_imm.reg1], inst->register_imm.imm64);
		break;
	case OA:
		if (inst->one_arg.mode == REGISTER)
			snprintf(p, rem, " %s", reg_names[inst->one_arg.reg]);
		else if (inst->one_arg.mode == ADDRESS)
			snprintf(p, rem, " [0x%016" PRIx64 "]", inst->one_arg.address);
		else
			snprintf(p, rem, " 0x%016" PRIx64, inst->one_arg.imm64);
		break;
	case CM:
		snprintf(p, rem, " %s %s, %s", cmov_names[inst->cmove.cond],
				 reg_names[inst->cmove.reg1], reg_names[inst->cmove.reg2]);
		break;
	case NO:
		break;
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <binary>\n", argv[0]);
		return 1;
	}

	struct ram *memory = init_memory(1 << 30);
	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		perror("fopen");
		return 1;
	}
	fread(memory->mem + 0x7FFFFF, 1, memory->cap, f);
	fclose(f);

	struct core cpu;
	cpu.mem = memory;
	cpu.irc = malloc(sizeof *cpu.irc);
	irc_init(cpu.irc, &cpu);
	cpu_init(&cpu, memory);
	cpu.registers[PC] = 0x7FFFFF;
	cpu.registers[PPTR] = 0;
	cpu.registers[IMR] = 0;
	cpu.registers[ITR] = 0;

	initscr();
	noecho();
	cbreak();
	curs_set(0);
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);

	int H, W;
	getmaxyx(stdscr, H, W);

	int col_w = W / 4;
	int right_w = W - col_w * 3;
	int top_h = H / 2;
	int bot_h = H - top_h;

	WINDOW *w_inst = newwin(H, col_w, 0, 0);
	WINDOW *w_regs = newwin(H, col_w, 0, col_w);
	WINDOW *w_mem = newwin(H, col_w, 0, col_w * 2);
	WINDOW *w_stack = newwin(top_h, right_w, 0, col_w * 3);
	WINDOW *w_page = newwin(bot_h, right_w, top_h, col_w * 3);

	bool paused = false, halted = false;
	while (!halted) {
		int ch = getch();
		if (ch == 'q')
			break;
		if (ch == 'p')
			paused = !paused;

		if (!paused && !cpu_step(&cpu)) {
			halted = true;
		}

		werase(w_inst);
		box(w_inst, 0, 0);
		mvwprintw(w_inst, 0, 2, " Disassembly ");
		int rows = H - 2;
		uint64_t pc = cpu.registers[PC];
		uint64_t addr = pc;
		struct instruction inst;
		for (int i = 0; i < rows; i++) {
			uint64_t cur = addr;
			uint64_t next = parse_instruction_ro(&cpu, &inst, addr);
			char linebuf[128];

			if (next == 0) {
				uint8_t b = vread8(&cpu, addr);
				snprintf(linebuf, sizeof linebuf,
						 "%016" PRIx64 ": [0x%02" PRIx8 "]", cur, b);
				next = addr + 1;
			} else {
				format_inst(&inst, cur, linebuf, sizeof linebuf);
			}

			if (cur == pc)
				wattron(w_inst, A_REVERSE);
			mvwprintw(w_inst, 1 + i, 1, "%.*s", col_w - 2, linebuf);
			if (cur == pc)
				wattroff(w_inst, A_REVERSE);

			addr = next;
		}
		wrefresh(w_inst);

		werase(w_regs);
		box(w_regs, 0, 0);
		mvwprintw(w_regs, 0, 2, " Registers ");
		int rw = col_w - 2;
		int cols = rw / 20;
		if (cols < 1)
			cols = 1;
		int per = (41 + cols - 1) / cols;
		for (int c = 0; c < cols; c++) {
			for (int r = 0; r < per; r++) {
				int idx = c * per + r;
				if (idx >= 41)
					break;
				uint64_t v = cpu.registers[idx];
				int y = 1 + r, x = 1 + c * 20;
				if (idx == PC)
					wattron(w_regs, A_BOLD);
				mvwprintw(w_regs, y, x, "%-4s:%016" PRIx64, reg_names[idx], v);
				if (idx == PC)
					wattroff(w_regs, A_BOLD);
			}
		}
		wrefresh(w_regs);

		werase(w_mem);
		box(w_mem, 0, 0);
		mvwprintw(w_mem, 0, 2, " Memory ");
		int mbw = col_w - 10;
		uint64_t bpr = mbw / 3;
		if (bpr < 1)
			bpr = 1;
		int mrows = H - 2;
		uint64_t m0 = pc > (bpr * 2) ? pc - bpr * 2 : 0;
		for (int r = 0; r < mrows; r++) {
			uint64_t a = m0 + r * bpr;
			mvwprintw(w_mem, 1 + r, 1, "%016" PRIx64 ":", a);
			for (uint64_t b = 0; b < bpr; b++) {
				uintptr_t phys = vaddr_to_phys(&cpu, a + b);
				uint8_t byte = cpu.mem->mem[phys];
				mvwprintw(w_mem, 1 + r, 19 + b * 3, "%02" PRIx8, byte);
			}
		}
		wrefresh(w_mem);

		werase(w_stack);
		box(w_stack, 0, 0);
		mvwprintw(w_stack, 0, 2, " Stack (SP1) ");
		int srows = top_h - 2;
		uint64_t sp = cpu.registers[SP1];
		for (int i = 0; i < srows; i++) {
			uint64_t a = sp + i * sizeof(uint64_t);
			uint64_t v = vread64(&cpu, a);
			mvwprintw(w_stack, 1 + i, 1, "%016" PRIx64 ":%016" PRIx64, a, v);
		}
		wrefresh(w_stack);

		werase(w_page);
		box(w_page, 0, 0);
		mvwprintw(w_page, 0, 2, " Page Trans ");
		int prow = bot_h - 2;
		uint64_t base = pc & ~0xFFF;
		for (int i = 0; i < prow; i++) {
			uint64_t va = base + i * 0x1000;
			uintptr_t pa = vaddr_to_phys_u(&cpu, va, false);
			if (pa == 0)
				mvwprintw(w_page, 1 + i, 1, "%016" PRIx64 " -> FAULT", va);
			else
				mvwprintw(w_page, 1 + i, 1, "%016" PRIx64 " -> %016" PRIxPTR,
						  va, pa);
		}
		wrefresh(w_page);

		napms(50);
	}

	nodelay(stdscr, FALSE);
	mvprintw(H - 1, 2, "CPU halted or exception. Press any key to exit.");
	refresh();
	getch();

	endwin();
	return 0;
}
