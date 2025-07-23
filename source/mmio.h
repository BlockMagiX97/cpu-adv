#ifndef MMIO_H
#define MMIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct core;

typedef bool (*mmio_read_fn)(struct core *c, uintptr_t offset, void *buf,
							 size_t len);
typedef bool (*mmio_write_fn)(struct core *c, uintptr_t offset, const void *buf,
							  size_t len);

struct mmio_hook {
	uintptr_t base;
	size_t size;
	mmio_read_fn read;
	mmio_write_fn write;
	struct mmio_hook *next;
};

void register_mmio_hook(struct mmio_hook *h);
bool unregister_mmio_hook(struct mmio_hook *h);

/* returns false if address is not handeled by MMIO */
bool handle_mmio_read(struct core *c, uintptr_t paddr, void *buf, size_t len);
bool handle_mmio_write(struct core *c, uintptr_t paddr, const void *buf,
					   size_t len);

#endif // MMIO_H
