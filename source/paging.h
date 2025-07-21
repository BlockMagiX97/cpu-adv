#ifndef PAGEING_H
#define PAGEING_H

#include <stdint.h>
#include <string.h>

struct core;

struct [[gnu::packed]] page_table_entry {
	uint8_t present : 1;
	uint8_t usermode : 1;
	uint8_t write : 1;
	uint8_t execute : 1;
	uint8_t reserved : 4;
	uintptr_t next_page : 56;
};

struct [[gnu::packed]] page_table {
	struct page_table_entry entries[1024];
};

struct ram {
	uint8_t *mem;
	uintptr_t cap;
};

struct ram *init_memory(uintptr_t precomit);
uintptr_t vaddr_to_phys(struct core *c, uintptr_t vaddr);
uintptr_t vaddr_to_phys_u(struct core *c, uintptr_t vaddr, bool write);

#define vaddr_to_ptr(c, v) (vaddr_to_phys(c, v) + (c)->mem->mem)

#define __PAGE_GENERATE_FOR_SIZES(_F)                                          \
	_F(8)                                                                      \
	_F(16)                                                                     \
	_F(32)                                                                     \
	_F(64)

#define __PAGE_GENERATE_FUNCTION_DECLARATIONS(size)                            \
	uint##size##_t vread##size(struct core *c, uintptr_t vaddr);               \
	bool vwrite##size(struct core *c, uintptr_t vaddr, uint##size##_t val);    \
	uint##size##_t vread##size##_u(struct core *c, uintptr_t vaddr);           \
	bool vwrite##size##_u(struct core *c, uintptr_t vaddr, uint##size##_t val);

#define __PAGE_GENERATE_FUNCTION_DEFINITIONS(size)                             \
	uint##size##_t vread##size(struct core *c, uintptr_t vaddr) {              \
		uint##size##_t ret;                                                    \
		memcpy(&ret, c->mem->mem + vaddr_to_phys(c, vaddr), sizeof(ret));      \
		return ret;                                                            \
	}                                                                          \
                                                                               \
	bool vwrite##size(struct core *c, uintptr_t vaddr, uint##size##_t val) {   \
		memcpy(c->mem->mem + vaddr_to_phys(c, vaddr), &val, sizeof(val));      \
		return true;                                                           \
	}                                                                          \
                                                                               \
	uint##size##_t vread##size##_u(struct core *c, uintptr_t vaddr) {          \
		uint##size##_t ret;                                                    \
		uintptr_t off = vaddr_to_phys_u(c, vaddr, false);                      \
		if (off == 0)                                                          \
			return 0;                                                          \
		memcpy(&ret, c->mem->mem + off, sizeof(ret));                          \
		return ret;                                                            \
	}                                                                          \
                                                                               \
	bool vwrite##size##_u(struct core *c, uintptr_t vaddr,                     \
						  uint##size##_t val) {                                \
		uintptr_t off = vaddr_to_phys_u(c, vaddr, true);                       \
		if (off == 0)                                                          \
			return false;                                                      \
		memcpy(c->mem->mem + off, &val, sizeof(val));                          \
		return true;                                                           \
	}

__PAGE_GENERATE_FOR_SIZES(__PAGE_GENERATE_FUNCTION_DECLARATIONS)

#endif // PAGING_H
