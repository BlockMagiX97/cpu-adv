#ifndef PAGEING_H
#define PAGEING_H

#include <mmio.h>
#include <pthread.h>
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
extern pthread_rwlock_t mem_rwlock;

#define LOCK_MEM_READ() pthread_rwlock_rdlock(&mem_rwlock)
#define LOCK_MEM_WRITE() pthread_rwlock_wrlock(&mem_rwlock)
#define UNLOCK_MEM() pthread_rwlock_unlock(&mem_rwlock)

struct ram *init_memory(uintptr_t precomit);
/* sets err to PAGE_FAULT on interrupt */
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
		LOCK_MEM_READ();                                                       \
		uintptr_t paddr = vaddr_to_phys(c, vaddr);                             \
		if (handle_mmio_read(c, paddr, &ret, sizeof(ret))) {                   \
			UNLOCK_MEM();                                                      \
			return ret;                                                        \
		}                                                                      \
		memcpy(&ret, c->mem->mem + paddr, sizeof(ret));                        \
		UNLOCK_MEM();                                                          \
		return ret;                                                            \
	}                                                                          \
                                                                               \
	bool vwrite##size(struct core *c, uintptr_t vaddr, uint##size##_t val) {   \
		LOCK_MEM_READ();                                                       \
		uintptr_t paddr = vaddr_to_phys(c, vaddr);                             \
		UNLOCK_MEM();                                                          \
		LOCK_MEM_WRITE();                                                      \
		if (handle_mmio_write(c, paddr, &val, sizeof(val))) {                  \
			UNLOCK_MEM();                                                      \
			return true;                                                       \
		}                                                                      \
		memcpy(c->mem->mem + paddr, &val, sizeof(val));                        \
		UNLOCK_MEM();                                                          \
		return true;                                                           \
	}                                                                          \
                                                                               \
	uint##size##_t vread##size##_u(struct core *c, uintptr_t vaddr) {          \
		uint##size##_t ret;                                                    \
		LOCK_MEM_READ();                                                       \
		uintptr_t off = vaddr_to_phys_u(c, vaddr, false);                      \
		if (off == 0) {                                                        \
			UNLOCK_MEM();                                                      \
			return 0;                                                          \
		}                                                                      \
		if (handle_mmio_read(c, off, &ret, sizeof(ret))) {                     \
			UNLOCK_MEM();                                                      \
			return ret;                                                        \
		}                                                                      \
		memcpy(&ret, c->mem->mem + off, sizeof(ret));                          \
		UNLOCK_MEM();                                                          \
		return ret;                                                            \
	}                                                                          \
                                                                               \
	bool vwrite##size##_u(struct core *c, uintptr_t vaddr,                     \
						  uint##size##_t val) {                                \
		LOCK_MEM_READ();                                                       \
		uintptr_t off = vaddr_to_phys_u(c, vaddr, true);                       \
		UNLOCK_MEM();                                                          \
		LOCK_MEM_WRITE();                                                      \
		if (off == 0) {                                                        \
			UNLOCK_MEM();                                                      \
			return false;                                                      \
		}                                                                      \
		if (handle_mmio_write(c, off, &val, sizeof(val))) {                    \
			UNLOCK_MEM();                                                      \
			return true;                                                       \
		}                                                                      \
		memcpy(c->mem->mem + off, &val, sizeof(val));                          \
		UNLOCK_MEM();                                                          \
		return true;                                                           \
	}

__PAGE_GENERATE_FOR_SIZES(__PAGE_GENERATE_FUNCTION_DECLARATIONS)

#endif // PAGING_H
