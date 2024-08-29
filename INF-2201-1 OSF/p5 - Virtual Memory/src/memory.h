#ifndef MEMORY_H
#define MEMORY_H

#include "kernel.h"
#include "tlb.h"
#include "util.h"

enum
{
	/* physical page facts */
	PAGE_SIZE = 4096,
	PAGE_N_ENTRIES = (PAGE_SIZE / sizeof(uint32_t)),
	SECTORS_PER_PAGE = (PAGE_SIZE / SECTOR_SIZE),

	PTABLE_SPAN = (PAGE_SIZE * PAGE_N_ENTRIES),

	/* page directory/table entry bits (PMSA p.235 and p.240) */
	PE_P = 1 << 0,                  /* present */
	PE_RW = 1 << 1,                 /* read/write */
	PE_US = 1 << 2,                 /* user/supervisor */
	PE_PWT = 1 << 3,                /* page write-through */
	PE_PCD = 1 << 4,                /* page cache disable */
	PE_A = 1 << 5,                  /* accessed */
	PE_D = 1 << 6,                  /* dirty */
	PE_BASE_ADDR_BITS = 12,         /* position of base address */
	PE_BASE_ADDR_MASK = 0xfffff000, /* extracts the base address */

	/* Constants to simulate a very small physical memory. */
	MEM_START = 0x100000, /* 1MB */
	PAGEABLE_PAGES = 33,
	MAX_PHYSICAL_MEMORY = (MEM_START + (PAGEABLE_PAGES + 1) * PAGE_SIZE),

	/* number of kernel page tables */
	N_KERNEL_PTS = 1,

	PAGE_DIRECTORY_BITS = 22,         /* position of page dir index */
	PAGE_TABLE_BITS = 12,             /* position of page table index */
	PAGE_DIRECTORY_MASK = 0xffc00000, /* page directory mask */
	PAGE_TABLE_MASK = 0x003ff000,     /* page table mask */
	PAGE_MASK = 0x00000fff,           /* page offset mask */
	/* used to extract the 10 lsb of a page directory entry */
	MODE_MASK = 0x000003ff,

	PAGE_TABLE_SIZE = (1024 * 4096 - 1), /* size of a page table in bytes */

	/* 
	 * Start sector on disk for swap-space.
	 * When compiling, createimage.c notes the sector where it writes data into, in the terminal.
	 * Process 4 is the last information written to the image.
	 * So swap-space must exist after this, and varies depending size of files. 
	 */
	WRITE_START = 320,
	SWAPABLE_PAGES = 20		/* Set in createimage.c */
};

/* Contains meta-data about frames on physical memory */
struct frame{
	uint32_t *paddr; 		/* Physical frame address */
	uint32_t *base;			/* Page dir/table base, for "parent page" */
	uint32_t index;			/* page dir/table index, for "parent page" */
	uint32_t pinned;		/* Frame is pinned == TRUE. Frame is not pinned == FALSE */
	uint32_t free;			/* Frame is free == TRUE. Frame is not free == FALSE */
	uint32_t fifo;			/* Queue count */
};
typedef struct frame frame_t;

/* Contains meta-data about pages on disk */
struct swap_page{
	uint32_t daddr; 		/* Disk address */
	uint32_t free;			/* Page is free == TRUE. Page is not free == FALSE */
};
typedef struct swap_page swap_page_t;

/* Prototypes */
/* Initialize the memory system, called from kernel.c: _start() */
void init_memory(void);

/*
 * Set up a page directory and page table for the process. Fill in
 * any necessary information in the pcb.
 */
void setup_page_table(pcb_t *p);

/*
 * Page fault handler, called from interrupt.c: exception_14().
 * Should handle demand paging
 */
void page_fault_handler(void);

/**
 * @brief Will handle a pagefault if present bit is not set. Will handle pages that have been evicted.
 * @param user Privlage level
 */
static void present_bit_handler(int user);

/**
 * @brief Returns address of an empty frame in physical memory. Will handle eviction of a frame if there is no free frames.
 * @param pinned Frame should be pinned == TRUE. Frame should NOT be pinned == FALSE
 * @param base Base address of "parent-" directory or table, 
 * 			   depending on if frame should be a page direcotry entry or page table entry.
 * @param index Index of entry in "parent-" directory or table,
 * 				depending on if frame should be a page direcotry entry or page table entry.
 * @return Returns address of an empty frame in physical memory.
 */
static uint32_t *get_frame(int pinned, uint32_t *base, uint32_t index);

static void make_common_map(uint32_t *page_directory, int user);

static uint32_t *allocate_page(void);

static uint32_t alloc_memory(uint32_t size);

/* Use virtual address to get index in page directory. */
inline uint32_t get_directory_index(uint32_t vaddr) {
	return (vaddr & PAGE_DIRECTORY_MASK) >> PAGE_DIRECTORY_BITS;
}

/* Use virtual address to get index in a page table.  */
inline uint32_t get_table_index(uint32_t vaddr) {
	return (vaddr & PAGE_TABLE_MASK) >> PAGE_TABLE_BITS;
}

/*
 * Maps a page as present in the page table.
 *
 * 'vaddr' is the virtual address which is mapped to the physical
 * address 'paddr'.
 *
 * If user is nonzero, the page is mapped as accessible from a user
 * application.
 */
inline void table_map_present(uint32_t *table, uint32_t vaddr, uint32_t paddr, int user) {
	int access = PE_RW | PE_P;
	int index = get_table_index(vaddr);

	if (user)
		access |= PE_US;
	
	table[index] = (paddr & ~PAGE_MASK) | access;
}

/*
 * Make an entry in the page directory pointing to the given page
 * table.  vaddr is the virtual address the page table start with
 * table is the physical address of the page table
 *
 * If user is nonzero, the page is mapped as accessible from a user
 * application.
 */
inline void directory_insert_table(uint32_t *directory, uint32_t vaddr, uint32_t *table, int user) {
	int access = PE_RW | PE_P;
	int index = get_directory_index(vaddr);

	if (user)
		access |= PE_US;

	uint32_t taddr = (uint32_t)table;

	directory[index] = (taddr & ~PAGE_MASK) | access;
}

#endif /* !MEMORY_H */
