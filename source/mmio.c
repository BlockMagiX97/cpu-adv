#include <cpu.h>
#include <inttypes.h>
#include <mmio.h>
#include <paging.h>
#include <stdio.h>

static struct mmio_hook *mmio_hooks = nullptr;

void register_mmio_hook(struct mmio_hook *h) {
	h->next = mmio_hooks;
	mmio_hooks = h;
}

bool unregister_mmio_hook(struct mmio_hook *h) {
	struct mmio_hook **prev = &mmio_hooks;
	while (*prev) {
		if (*prev == h) {
			*prev = h->next;
			h->next = nullptr;
			return true;
		}
		prev = &(*prev)->next;
	}
	return false;
}

bool handle_mmio_read(struct core *c, uintptr_t paddr, void *buf, size_t len) {
	struct mmio_hook *hook = NULL;

	for (hook = mmio_hooks; hook; hook = hook->next) {
		if (paddr >= hook->base && paddr + len <= hook->base + hook->size) {
			uintptr_t offset = paddr - hook->base;
			return hook->read(c, offset, buf, len);
		}
	}

	return false;
}

bool handle_mmio_write(struct core *c, uintptr_t addr, const void *buf,
					   size_t len) {
	for (struct mmio_hook *h = mmio_hooks; h; h = h->next) {
		if (addr >= h->base && addr + len <= h->base + h->size)
			return h->write(c, addr - h->base, buf, len);
	}
	return false;
}
