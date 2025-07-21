#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <queue.h>
#include <stdint.h>

struct core;

#define ICR_DIV_BY_ZERO 1
#define ICR_INVALID_OPCODE 2
#define ICR_PAGE_FAULT 3

struct interrupt_table {
	uintptr_t handler[65];
};

struct interrupt_event {
	uint16_t irc;
};

struct irc {
	struct core *core;
	struct interrupt_table *it;
	uint16_t irc_to_isr;
	queue_of(struct interrupt_event) masked_queue;
	bool in_exception;
	bool in_double_fault;
};

void irc_init(struct irc *irc, struct core *core);
void update_it_cache(struct irc *irc);
bool irc_raise_interrupt(struct irc *irc, uint16_t vector);
void irc_raise_double_fault(struct irc *irc);
bool irc_on_imr_write(struct irc *irc);

#endif // INTERRUPT_H
