#include <assert.h>
#include <cpu.h>
#include <defer.h>
#include <interrupt.h>
#include <ncurses.h>
#include <paging.h>
#include <queue.h>

void irc_init(struct irc *irc, struct core *core) {
	irc->core = core;
	irc->irc_to_isr = 10;
	irc->in_exception = irc->in_double_fault = false;
	queue_init(&irc->masked_queue);
}

bool irc_raise_interrupt(struct irc *irc, uint16_t vector) {
	assert(vector != 0);
	const uint16_t vec_mask = vector - 1;
	bool raise_double_fault = false;

	if (irc->in_double_fault) {
		// triple fault; reset CPU TODO
	}

	if (vector <= 4) {
		if (irc->in_exception)
			raise_double_fault = true;
	} else
		irc->in_exception = true;

	const uint64_t mask = (1ULL << vec_mask);
	if ((mask & irc->core->registers[IMR]) != 0) {
		if (vector <= 4) {
			raise_double_fault = true;
			goto push;
		}

		struct interrupt_event e = {vector};
		queue_push(&irc->masked_queue, e);

		return false;
	}
push:

	const uint64_t pc = irc->core->registers[PC];
	const uint64_t imr = irc->core->registers[IMR];
	const uint64_t ppr = irc->core->registers[PPR];

	uint64_t stack = irc->core->registers[SP0];
	stack -= 8;
	vwrite64(irc->core, stack, pc);
	stack -= 8;
	vwrite64(irc->core, stack, imr);
	stack -= 8;
	vwrite64(irc->core, stack, ppr);
	irc->core->registers[SP0] = stack;

	const uintptr_t itr = irc->core->registers[ITR];
	const uintptr_t handler_vaddr = itr + vector * sizeof(uintptr_t);
	irc->core->registers[PC] = vread64(irc->core, handler_vaddr);

	irc->core->registers[IMR] = ~0b111;
	irc->core->registers[PPR] = 0;

	if (raise_double_fault) {
		irc_raise_double_fault(irc);
		return false;
	}

	return true;
}

void irc_raise_double_fault(struct irc *irc) {
	irc->in_double_fault = true;

	const uint64_t pc = irc->core->registers[PC];
	const uint64_t imr = irc->core->registers[IMR];
	const uint64_t ppr = irc->core->registers[PPR];

	uint64_t stack = irc->core->registers[SP0];
	stack -= 8;
	vwrite64(irc->core, stack, pc);
	stack -= 8;
	vwrite64(irc->core, stack, imr);
	stack -= 8;
	vwrite64(irc->core, stack, ppr);
	irc->core->registers[SP0] = stack;

	const uintptr_t itr_handler_0 = irc->core->registers[ITR];
	irc->core->registers[PC] = vread64(irc->core, itr_handler_0);
	irc->core->registers[IMR] = ~0b111;
	irc->core->registers[PPR] = 0;
}

bool irc_on_imr_write(struct irc *irc) {
	const uint64_t new_imr = irc->core->registers[IMR];
	bool raised_any = false;

	while (!queue_is_empty(&irc->masked_queue)) {
		struct interrupt_event e = queue_pop(&irc->masked_queue);

		uint16_t vec = e.irc - 1;
		if (!(new_imr & (1ULL << vec))) {
			irc_raise_interrupt(irc, e.irc);
			raised_any = true;
		} else {
			queue_push(&irc->masked_queue, e);
			break;
		}
	}

	return raised_any;
}
