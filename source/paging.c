#include <cpu.h>
#include <err.h>
#include <interrupt.h>
#include <paging.h>
#include <sys/mman.h>
#include <pthread.h>

pthread_rwlock_t mem_rwlock = PTHREAD_RWLOCK_INITIALIZER;

struct ram *init_memory(uintptr_t precomit) {
	struct ram *mem = malloc(sizeof *mem);

	if (mem == nullptr)
		return nullptr;

	mem->mem = malloc(precomit * sizeof *mem->mem);
	// mmap(nullptr, precomit, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);

	if (mem->mem == nullptr) {
		free(mem);
		return nullptr;
	}
	mem->cap = precomit;
	return mem;
}

uintptr_t vaddr_to_phys(struct core *c, uintptr_t vaddr) {
#define CHECK_PAGE                                                             \
	if (entry->present == 0) {                                                 \
		vm_error = VM_INT_PF;                                                  \
		return 0;                                                              \
	}

	if (c->registers[PPTR] == 0)
		return vaddr;

	const uintptr_t l1_index = vaddr >> 51;
	const uintptr_t l2_index = (vaddr >> 38) & 0b1111111111111;
	const uintptr_t l3_index = (vaddr >> 25) & 0b1111111111111;
	const uintptr_t l4_index = (vaddr >> 12) & 0b1111111111111;
	const uintptr_t page_frame_index = vaddr & 0b111111111111;

	struct page_table *page_table;
	struct page_table_entry *entry;

	page_table = (struct page_table *)c->mem->mem + c->registers[PPTR];
	entry = &page_table->entries[l1_index];
	CHECK_PAGE;
	page_table = (struct page_table *)(c->mem->mem + entry->next_page);

	entry = &page_table->entries[l2_index];
	CHECK_PAGE;
	page_table = (struct page_table *)(c->mem->mem + entry->next_page);

	entry = &page_table->entries[l3_index];
	CHECK_PAGE;
	page_table = (struct page_table *)(c->mem->mem + entry->next_page);

	entry = &page_table->entries[l4_index];
	CHECK_PAGE;
	page_table = (struct page_table *)(c->mem->mem + entry->next_page);

	return page_table->entries[page_frame_index].next_page;
#undef CHECK_PAGE
}

uintptr_t vaddr_to_phys_u(struct core *c, uintptr_t vaddr, bool write) {
#define CHECK_PAGE                                                             \
	if (entry->present == 0 || (entry->usermode == 0 && !supervisor)) {        \
		vm_error = VM_INT_PF;                                                  \
		irc_raise_interrupt(c->irc, ICR_PAGE_FAULT);                           \
		return 0;                                                              \
	}                                                                          \
	if (!entry->write && write) {                                              \
		vm_error = VM_INT_PF;                                                  \
		irc_raise_interrupt(c->irc, ICR_PAGE_FAULT);                           \
		return 0;                                                              \
	}

	if (c->registers[PPTR] == 0)
		return vaddr;

	const uintptr_t l1_index = vaddr >> 51;
	const uintptr_t l2_index = (vaddr >> 38) & 0b1111111111111;
	const uintptr_t l3_index = (vaddr >> 25) & 0b1111111111111;
	const uintptr_t l4_index = (vaddr >> 12) & 0b1111111111111;
	const uintptr_t page_frame_index = vaddr & 0b111111111111;

	struct page_table *page_table;
	struct page_table_entry *entry;

	bool supervisor = c->registers[PPR] == 0;

	page_table = (struct page_table *)c->mem->mem + c->registers[PPTR];
	entry = &page_table->entries[l1_index];
	CHECK_PAGE;
	page_table = (struct page_table *)(c->mem->mem + entry->next_page);

	entry = &page_table->entries[l2_index];
	CHECK_PAGE;
	page_table = (struct page_table *)(c->mem->mem + entry->next_page);

	entry = &page_table->entries[l3_index];
	CHECK_PAGE;
	page_table = (struct page_table *)(c->mem->mem + entry->next_page);

	entry = &page_table->entries[l4_index];
	CHECK_PAGE;
	page_table = (struct page_table *)(c->mem->mem + entry->next_page);

	entry = &page_table->entries[page_frame_index];
	CHECK_PAGE;

	return entry->next_page;
#undef CHECK_PAGE
}

__PAGE_GENERATE_FOR_SIZES(__PAGE_GENERATE_FUNCTION_DEFINITIONS)
