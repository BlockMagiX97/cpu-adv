#include <SDL2/SDL.h>
#include <fcntl.h>
#include <inttypes.h>
#include <ncurses.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <cpu.h>
#include <inst.h>
#include <interrupt.h>
#include <mmio.h>
#include <paging.h>

#define FB_WIDTH 640
#define FB_HEIGHT 480
#define FB_PITCH (FB_WIDTH * 4)
#define FB_SIZE (FB_HEIGHT * FB_PITCH) + 20
#define FB_BASE 0x90000000UL
#define STEPS_PER_UPDATE 10000U

#define KBD_BASE (0x90010000UL)
#define KBD_SIZE 0x10
#define ICR_KEYB 11

static struct core cpu;

static inline bool safe_load_bool(volatile bool *ptr) {
	bool val;
	__asm__ volatile("movb %1, %0" : "=r"(val) : "m"(*ptr) : "memory");
	return val;
}

static inline void safe_store_bool(volatile bool *ptr, bool val) {
	__asm__ volatile("xchg %0, %1" : "+m"(*ptr) : "r"(val) : "memory");
}

static const char *reg_names[41] = {
	"R0",	"R1",  "R2",  "R3",	 "R4",	"R5",  "R6",  "R7",	 "R8",
	"R9",	"R10", "R11", "R12", "R13", "R14", "R15", "R16", "R17",
	"R18",	"R19", "R20", "R21", "R22", "R23", "R24", "R25", "R26",
	"R27",	"R28", "R29", "R30", "R31", "PC",  "SP1", "FR",	 "SP0",
	"PPTR", "IMR", "ITR", "SLR", "PPR"};

static const char *opcode_names[21] = {
	"MOV", "ADD",  "SUB",	 "MUL",		"DIV",	"OR",	   "AND",
	"NOT", "XOR",  "PUSH",	 "POP",		"CALL", "CMP",	   "CMOV",
	"RET", "RETI", "SYSRET", "SYSCALL", "HLT",	"COANDSW", "STR"};

static const char *cmov_names[8] = {[NE] = "NE", [GT] = "GT", [LT] = "LT",
									[EQ] = "EQ", [LE] = "LE", [GE] = "GE"};

static SDL_Window *sdl_window = nullptr;
static SDL_Renderer *sdl_renderer = nullptr;
static SDL_Texture *sdl_texture = nullptr;
static uint8_t *fb_mem = nullptr;

static uint8_t kbd_buf[256];
static size_t kbd_head = 0, kbd_tail = 0;

static pthread_mutex_t kbd_mtx = PTHREAD_MUTEX_INITIALIZER;
static bool kbd_pending = false;

static volatile bool paused = true;
static volatile bool snapshot_ready = false;
static volatile bool halted_global = false;

static pthread_mutex_t snap_mtx = PTHREAD_MUTEX_INITIALIZER;
struct cpu_snapshot {
	uint64_t regs[41];
};
static struct cpu_snapshot latest_snapshot;

#define _DEBUG

void *cpu_thread_func(void *arg) {
	struct core *cpu = (struct core *)arg;
	uint64_t step_count = 0;
#ifdef _DEBUG
	uint64_t interval_steps = 0;
	struct timespec last_print_ts;
	clock_gettime(CLOCK_MONOTONIC, &last_print_ts);
#endif

	while (!safe_load_bool(&halted_global)) {
		bool raise_interrupt = false;
		pthread_mutex_lock(&kbd_mtx);
		if (kbd_pending) {
			raise_interrupt = true;
			kbd_pending = false;
		}
		pthread_mutex_unlock(&kbd_mtx);
		if (raise_interrupt)
			irc_raise_interrupt(cpu->irc, ICR_KEYB);

		if (safe_load_bool(&paused)) {
			struct timespec ts = {0, 1000000};
			nanosleep(&ts, nullptr);
			continue;
		}

		if (!cpu_step(cpu)) {
			safe_store_bool(&halted_global, true);
			step_count = 0;
			pthread_mutex_lock(&snap_mtx);
			memcpy(latest_snapshot.regs, cpu->registers,
				   sizeof latest_snapshot.regs);
			safe_store_bool(&snapshot_ready, true);
			pthread_mutex_unlock(&snap_mtx);
			break;
		}

#ifdef _DEBUG
		interval_steps++;

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		double elapsed = (now.tv_sec - last_print_ts.tv_sec) +
						 (now.tv_nsec - last_print_ts.tv_nsec) / 1e9;
		if (elapsed >= 1.0) {
			double ips = interval_steps / elapsed;
			fprintf(stderr, "[DEBUG] Clock speed: %.2f KHz\n", ips / 1e3);
			interval_steps = 0;
			last_print_ts = now;
		}
#endif

		if (++step_count >= STEPS_PER_UPDATE) {
			step_count = 0;
			pthread_mutex_lock(&snap_mtx);
			memcpy(latest_snapshot.regs, cpu->registers,
				   sizeof latest_snapshot.regs);
			safe_store_bool(&snapshot_ready, true);
			pthread_mutex_unlock(&snap_mtx);
		}
	}

	return nullptr;
}

static bool fb_mmio_read(struct core *, uintptr_t offset, void *buf,
						 size_t len) {
	if (offset >= FB_SIZE || offset + len > FB_SIZE)
		return true;
	memcpy(buf, fb_mem + offset, len);
	return true;
}

static bool fb_mmio_write(struct core *, uintptr_t offset, const void *buf,
						  size_t len) {
	if (offset >= FB_SIZE || offset + len > FB_SIZE)
		return true;
	memcpy(fb_mem + offset, buf, len);
	return true;
}

static bool kbd_mmio_read(struct core *, uintptr_t offset, void *buf,
						  size_t len) {
	if (offset >= KBD_SIZE || len != 8)
		return true;
	uint64_t *out = buf;

	if (offset == 0) {
		pthread_mutex_lock(&kbd_mtx);
		*out = (kbd_head != kbd_tail) ? 1 : 0;
		pthread_mutex_unlock(&kbd_mtx);
		return true;
	}
	if (offset == 8) {
		pthread_mutex_lock(&kbd_mtx);
		if (kbd_head == kbd_tail) {
			*out = 0;
		} else {
			*out = kbd_buf[kbd_tail++];
			kbd_tail &= 255;
		}
		pthread_mutex_unlock(&kbd_mtx);
		return true;
	}
	return true;
}

static bool kbd_mmio_write(struct core *, uintptr_t, const void *, size_t) {
	return true;
}

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
	fread(memory->mem + 0x7FFF000, 1, memory->cap, f);
	fclose(f);

	cpu.mem = memory;
	cpu.irc = malloc(sizeof *cpu.irc);
	irc_init(cpu.irc, &cpu);
	cpu_init(&cpu, memory);
	cpu.registers[PC] = 0x7FFF000;
	cpu.registers[PPTR] = 0;
	cpu.registers[IMR] = 0;
	cpu.registers[ITR] = 0;

	freopen("stderr.txt", "w", stderr);
	setvbuf(stderr, nullptr, _IONBF, 0);

	fb_mem = calloc(1, FB_SIZE);
	if (!fb_mem) {
		fprintf(stderr, "Failed to allocate framebuffer\n");
		return 1;
	}
	static struct mmio_hook fb_hook = {.base = FB_BASE,
									   .size = FB_SIZE,
									   .read = fb_mmio_read,
									   .write = fb_mmio_write,
									   .next = nullptr};
	register_mmio_hook(&fb_hook);
	static struct mmio_hook kbd_hook = {
		.base = KBD_BASE,
		.size = KBD_SIZE,
		.read = kbd_mmio_read,
		.write = kbd_mmio_write,
		.next = nullptr,
	};
	register_mmio_hook(&kbd_hook);

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}
	sdl_window =
		SDL_CreateWindow("VGPU Framebuffer", SDL_WINDOWPOS_CENTERED,
						 SDL_WINDOWPOS_CENTERED, FB_WIDTH, FB_HEIGHT, 0);
	sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED);
	sdl_texture =
		SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888,
						  SDL_TEXTUREACCESS_STREAMING, FB_WIDTH, FB_HEIGHT);

	initscr();
	noecho();
	cbreak();
	curs_set(0);
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);

	int H, W;
	getmaxyx(stdscr, H, W);
	int col_w = W / 4;
	int right_w = W - 3 * col_w;
	int top_h = H / 2;
	int bot_h = H - top_h;

	WINDOW *w_inst = newwin(H, col_w, 0, 0);
	WINDOW *w_regs = newwin(H, col_w, 0, col_w * 1);
	WINDOW *w_mem = newwin(H, col_w, 0, col_w * 2);
	WINDOW *w_stack = newwin(top_h, right_w, 0, col_w * 3);
	WINDOW *w_page = newwin(bot_h, right_w, top_h, col_w * 3);

	struct core ui_core = {};
	memcpy(&ui_core, &cpu, sizeof(struct core));

	pthread_t cpu_thread;
	if (pthread_create(&cpu_thread, nullptr, cpu_thread_func, &cpu) != 0) {
		fprintf(stderr, "Failed to launch CPU thread\n");
		return 1;
	}

	const long FRAME_NS = 1000000000L / 60L;
	struct timespec next_frame;
	clock_gettime(CLOCK_MONOTONIC, &next_frame);

	bool show_sp0 = false;

	while (!safe_load_bool(&halted_global)) {
	last_update:
		next_frame.tv_nsec += FRAME_NS;
		if (next_frame.tv_nsec >= 1000000000L) {
			next_frame.tv_sec += 1;
			next_frame.tv_nsec -= 1000000000L;
		}
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_frame, nullptr);

		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT) {
				safe_store_bool(&halted_global, true);
			} else if (ev.type == SDL_KEYDOWN) {
				if (!safe_load_bool(&paused)) {
					SDL_Keycode sym = ev.key.keysym.sym;
					if (sym >= 0x20 && sym <= 0x7E) {
						uint8_t c = (uint8_t)sym;
						kbd_buf[kbd_head++] = c;
						kbd_head &= 255;
						pthread_mutex_lock(&kbd_mtx);
						kbd_pending = true;
						pthread_mutex_unlock(&kbd_mtx);
					}
				}
			}
		}

		int ch = getch();
		if (ch == 'q')
			safe_store_bool(&halted_global, true);
		else if (ch == 'p')
			safe_store_bool(&paused, !safe_load_bool(&paused));
		else if (ch == 't')
			show_sp0 = !show_sp0;

		if (safe_load_bool(&snapshot_ready)) {
			pthread_mutex_lock(&snap_mtx);
			memcpy(ui_core.registers, latest_snapshot.regs,
				   sizeof ui_core.registers);
			safe_store_bool(&snapshot_ready, false);
			pthread_mutex_unlock(&snap_mtx);
		}

		SDL_UpdateTexture(sdl_texture, nullptr, fb_mem, FB_PITCH);
		SDL_RenderClear(sdl_renderer);
		SDL_RenderCopy(sdl_renderer, sdl_texture, nullptr, nullptr);
		SDL_RenderPresent(sdl_renderer);

		werase(w_inst);
		box(w_inst, 0, 0);
		mvwprintw(w_inst, 0, 2, " Disassembly ");
		{
			int rows = H - 2;
			uint64_t pc = ui_core.registers[PC];
			uint64_t addr = pc;
			struct instruction inst;
			char linebuf[128];

			for (int i = 0; i < rows; i++) {
				uint64_t cur = addr;
				uint64_t next = parse_instruction_ro(&ui_core, &inst, addr);

				if (next == 0) {
					uint8_t b = vread8(&ui_core, addr);
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
		}
		wrefresh(w_inst);

		werase(w_regs);
		box(w_regs, 0, 0);
		mvwprintw(w_regs, 0, 2, " Registers ");

		{
			int win_w = col_w - 2;
			int win_h = H - 2;

			int min_w = 0;
			for (int i = 0; i < 41; i++) {
				char tmp[64];
				int len = snprintf(tmp, sizeof tmp, "%s:%016" PRIx64,
								   reg_names[i], 0UL);
				if (len > min_w)
					min_w = len;
			}
			if (min_w > win_w)
				min_w = win_w;

			int max_cols = win_w / min_w;
			if (max_cols < 1)
				max_cols = 1;
			int cols = 1;
			for (int c = max_cols; c >= 1; c--) {
				int rows_needed = (41 + c - 1) / c;
				if (rows_needed <= win_h) {
					cols = c;
					break;
				}
			}

			int col_wd = win_w / cols;
			int per = (41 + cols - 1) / cols;

			for (int c = 0; c < cols; c++) {
				for (int r = 0; r < per; r++) {
					int idx = c * per + r;
					if (idx >= 41)
						break;
					uint64_t v = ui_core.registers[idx];
					int y = 1 + r;
					int x = 1 + c * col_wd;
					if (idx == PC)
						wattron(w_regs, A_BOLD);
					mvwprintw(w_regs, y, x, "%-*s:%016" PRIx64, min_w - 1,
							  reg_names[idx], v);
					if (idx == PC)
						wattroff(w_regs, A_BOLD);
				}
			}
		}
		wrefresh(w_regs);

		werase(w_mem);
		box(w_mem, 0, 0);
		mvwprintw(w_mem, 0, 2, " Memory ");
		{
			int mbw = col_w - 10;
			uint64_t bpr = mbw / 3;
			if (bpr < 1)
				bpr = 1;
			int mrows = H - 2;
			uint64_t base = ui_core.registers[PC] > (bpr * 2)
								? ui_core.registers[PC] - (bpr * 2)
								: 0;
			for (int r = 0; r < mrows; r++) {
				uint64_t a = base + r * bpr;
				mvwprintw(w_mem, 1 + r, 1, "%016" PRIx64 ":", a);
				for (uint64_t b = 0; b < bpr; b++) {
					LOCK_MEM_READ();
					uintptr_t phys =
						vaddr_to_phys(&ui_core, a + b); // TODO manually lock
					uint8_t byte = ui_core.mem->mem[phys];
					UNLOCK_MEM();
					mvwprintw(w_mem, 1 + r, 19 + b * 3, "%02" PRIx8, byte);
				}
			}
		}
		wrefresh(w_mem);

		werase(w_stack);
		box(w_stack, 0, 0);

		if (!show_sp0)
			wattron(w_stack, A_REVERSE);
		mvwprintw(w_stack, 0, 2, "SP1");
		if (!show_sp0)
			wattroff(w_stack, A_REVERSE);

		if (show_sp0)
			wattron(w_stack, A_REVERSE);
		mvwprintw(w_stack, 0, 6, "SP0");
		if (show_sp0)
			wattroff(w_stack, A_REVERSE);

		mvwprintw(w_stack, 0, (right_w / 2) - 2, "Stk");

		{
			int srows = top_h - 2;
			uint64_t sp = ui_core.registers[show_sp0 ? SP0 : SP1];
			for (int i = 0; i < srows; i++) {
				uint64_t a = sp + i * sizeof(uint64_t);
				uint64_t v = vread64(&ui_core, a);
				mvwprintw(w_stack, 1 + i, 1, "%016" PRIx64 ":%016" PRIx64, a,
						  v);
			}
		}
		wrefresh(w_stack);

		werase(w_page);
		box(w_page, 0, 0);
		mvwprintw(w_page, 0, 2, "Page");
		{
			int prow = bot_h - 2;
			uint64_t base = ui_core.registers[PC] & ~0xFFF;
			for (int i = 0; i < prow; i++) {
				uint64_t va = base + i * 0x1000;
				LOCK_MEM_READ();
				uintptr_t pa = vaddr_to_phys_u(&ui_core, va, false);
				UNLOCK_MEM();
				if (pa == 0)
					mvwprintw(w_page, 1 + i, 1, "%016" PRIx64 "-> FAULT", va);
				else
					mvwprintw(w_page, 1 + i, 1, "%016" PRIx64 "-> %016" PRIxPTR,
							  va, pa);
			}
		}
		wrefresh(w_page);
	}
	pthread_join(cpu_thread, nullptr);
	static int fal = 1;
	if (fal) {
		fal = 0;
		goto last_update;
	}

	nodelay(stdscr, FALSE);
	mvprintw(H - 1, 2, "CPU halted. Press any key to exit.");
	refresh();
	getch();

	endwin();
	return 0;
}
