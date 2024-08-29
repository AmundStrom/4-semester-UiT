/*
 * memory.c
 * Note:
 * There is no separate swap area. When a data page is swapped out,
 * it is stored in the location it was loaded from in the process'
 * image. This means it's impossible to start two processes from the
 * same image without screwing up the running. It also means the
 * disk image is read once. And that we cannot use the program disk.
 *
 * Best viewed with tabs set to 4 spaces.
 */

#include "common.h"
#include "interrupt.h"
#include "kernel.h"
#include "memory.h"
#include "scheduler.h"
#include "thread.h"
#include "tlb.h"
#include "usb/scsi.h"
#include "util.h"

/* Use virtual address to get index in page directory.  */
inline uint32_t get_directory_index(uint32_t vaddr);

/*
 * Use virtual address to get index in a page table.  The bits are
 * masked, so we essentially get a modulo 1024 index.  The selection
 * of which page table to index into is done with
 * get_directory_index().
 */
inline uint32_t get_table_index(uint32_t vaddr);

inline void table_map_present(uint32_t *table, uint32_t vaddr, uint32_t paddr, int user);

inline void directory_insert_table(uint32_t *directory, uint32_t vaddr, uint32_t *table, int user);

/* Debug-function. 
 * Write all memory addresses and values by with 4 byte increment to output-file.
 * Output-file name is specified in bochsrc-file by line:
 * com1: enabled=1, mode=file, dev=serial.out
 * where 'dev=' is output-file. 
 * Output-file can be changed, but will by default be in 'serial.out'.
 * 
 * Arguments
 * title:		prefix for memory-dump
 * start:		memory address
 * end:			memory address
 * inclzero:	binary; skip address and values where values are zero
 */
static void 
rsprintf_memory(uint32_t start, uint32_t end, uint32_t inclzero){
	uint32_t numpage, paddr;
	char *header;

	// rsprintf("%s\n", title);

	numpage = 0;
	header = "========================== PAGE NUMBER %02d ==========================\n";

	for(paddr = start; paddr < end; paddr += sizeof(uint32_t)) {

		/* Print header if address is page-aligned. */
		if(paddr % PAGE_SIZE == 0) {
			rsprintf(header, numpage);
			numpage++;
		}
		/* Avoid printing address entries with no value. */
		if(	!inclzero && *(uint32_t*)paddr == 0x0) {
			continue;
		}
		/* Print: 
		 * Entry-number from current page. 
		 * Physical main memory address. 
		 * Value at address.
		 */
		rsprintf("%04d - Memory Loc: 0x%08x ~~~~~ Mem Val: 0x%08x\n",
					((paddr - start) / sizeof(uint32_t)) % PAGE_N_ENTRIES,
					paddr,
					*(uint32_t*)paddr );
	}
}

/* Spinlock to control the access to memory allocation */
static int memory_lock;

/* The next free memory address */
static uint32_t next_free_mem;

/* The kernel's page directory, which is shared with the kernel threads */
static uint32_t *kernel_page_directory = NULL;

/* Lock to control the access to page- allocation and handling. It is acquired 
 * and released in setup_page_table() and page_fault_handler() */
static lock_t paging_lock;

/* Holds meta-data about frames in the physical memory */
static frame_t frame[PAGEABLE_PAGES];

/* Holds meta-data about swap space on disk */
static swap_page_t swap_page[SWAPABLE_PAGES];

/* Holds number of pages in swap space on disk */
static uint32_t swap_count;

/* Debug print enabled == TRUE. Debug print disabled == FALSE */
static int debug_print;

/* (4 * 1024) bytes large buffer containing only 0 for clearing a page on disk */
static uint32_t zero_buffer[PAGE_SIZE / sizeof(uint32_t)];

/*
 * init_memory()
 *
 * called once by _start() in kernel.c
 * You need to set up the virtual memory map for the kernel here.
 */
void init_memory(void) 
{
	/* Debug print enabled == TRUE. Debug print disabled == FALSE */
	debug_print = FALSE;

	/* Create buffer with only 0, used for clearing swap-space on disk */
	for (uint32_t i = 0; i < (PAGE_SIZE / sizeof(uint32_t)); zero_buffer[i++] = 0)
		;

	spinlock_init(&memory_lock);
	lock_init(&paging_lock);

	/* initialize meta-data about frames in physical memory */
	next_free_mem = MEM_START;
	for(int i = 0; i < PAGEABLE_PAGES; i++) {
		frame[i].paddr = allocate_page();
		frame[i].free = TRUE;
		frame[i].pinned = FALSE;
		frame[i].fifo = 0;
	}

	/* initialize meta-data about swap-space on disk */
	swap_count = 0;
	uint32_t next_free_disk = WRITE_START;
	for(int i = 0; i < SWAPABLE_PAGES; i++) {
		swap_page[i].daddr = next_free_disk;	// 8 sectors interval between pages on disk
		swap_page[i].free = TRUE;
		next_free_disk += SECTORS_PER_PAGE;
	}
	
	/*
	 * Allocate memory for the page directory. A page directory
	 * is exactly the size of one page.
	 */
	kernel_page_directory = get_frame(TRUE, NULL, 0);

	/* This takes care of all the mapping that the kernel needs  */
	make_common_map(kernel_page_directory, 0);
}

/*
 * Sets up a page directory and page table for a new process or thread.
 */
void setup_page_table(pcb_t *p)
{
	lock_acquire(&paging_lock);

	if (p->is_thread) {
		/*
		 * Threads use the kernels page directory, so just set
		 * a pointer to that one and return.
		 */
		p->page_directory = kernel_page_directory;
		lock_release(&paging_lock);
		return;
	}

	/* Create page direcotry for a process, and map the kernel */
	p->page_directory = get_frame(TRUE, NULL, 0);
	make_common_map(p->page_directory, 1);

	lock_release(&paging_lock);
}

/*
 * called by exception_14 in interrupt.c (the faulting address is in
 * current_running->fault_addr)
 *
 * Interrupts are on when calling this function.
 */
static int counter = 0;
void page_fault_handler(void) 
{
	current_running->page_fault_count++;

	lock_acquire(&paging_lock);

	int p_bit = current_running->error_code & PE_P;				// present bit
	int rw_bit = (current_running->error_code & PE_RW) >> 1;	// read/write bit
	int u_bit = (current_running->error_code & PE_US) >> 2;		// user bit

	if(debug_print) {
		counter++;
		rsprintf("\n");
		rsprintf("============== Page fault %i ==============\n", counter);
		rsprintf("- Fault address: 			%x\n", current_running->fault_addr);
		rsprintf("- Error code: 				%i\n", current_running->error_code);
		rsprintf("- cr->directory 			%x\n", current_running->page_directory);
		rsprintf("- cr->pid 					%i\n", current_running->pid);
		rsprintf("- cr->page_fault_counter 	%i\n", current_running->page_fault_count);
		rsprintf("- present: %i, rd/wr: %i, user: %i\n", p_bit, rw_bit, u_bit);
		rsprintf("\n");
	}

	// if(!u_bit)
	// 	HALT("user bit page fault");

	// if(!rw_bit)
	// 	HALT("read write page fault");

	if(!p_bit) {
		present_bit_handler(TRUE);
		lock_release(&paging_lock);
		return;
	}

	HALT("Not able to handle pagefault     ");

	lock_release(&paging_lock);
}


static void present_bit_handler(int user)
{
	int pinned = FALSE;		// if a page should be pinned or not, by default not pinned
	uint32_t *frame;

	uint32_t pde_index = get_directory_index(current_running->fault_addr);							// page directory entry index
	uint32_t *pde = (uint32_t *)(current_running->page_directory[pde_index] & PE_BASE_ADDR_MASK);	// page directory entry (page table)
	uint32_t pde_p_bit = current_running->page_directory[pde_index] & PE_P;							// page directory entry present bit

	if(debug_print)
		rsprintf("pde_index: %i, pde: %x, pde_p_bit: %i\n", pde_index, pde, pde_p_bit);

	/* Insert page directory entry (page table), if not present */
	if(!pde_p_bit)
	{
		/* Get an empty frame (page table) */
		frame = get_frame(TRUE, current_running->page_directory, pde_index);

		/* Make a directory entry with the frame */
		directory_insert_table(current_running->page_directory, current_running->fault_addr, frame, user);
		return;
	}

	uint32_t pte_index = get_table_index(current_running->fault_addr);	// page table entry index
	uint32_t *pte = (uint32_t *)(pde[pte_index] & PE_BASE_ADDR_MASK);	// page table entry (physical page)
	uint32_t pte_p_bit = pde[pte_index] & PE_P;							// page table entry present bit

	if(debug_print)
		rsprintf("pte_index: %i, pte: %x, pte_p_bit: %i\n", pte_index, pte, pte_p_bit);

	/* Insert page table entry (frame with data/code), if not present */
	if(!pte_p_bit)
	{
		uint32_t start, read_size = SECTORS_PER_PAGE;

		/* If faulting address is stack, pin page */
		if(current_running->fault_addr == current_running->user_stack)
			pinned = TRUE;
		
		/* Get an empty frame (code/data) */
		frame = get_frame(pinned, pde, pte_index);

		/* First time read from process directory */
		if(pte == 0) {
			/* calculate the location where the missing page resides on disk */
			// mask the faulting virtual address
			uint32_t vaddr_masked =	(current_running->fault_addr & PE_BASE_ADDR_MASK);
			// offset which the faulting address thinks it is operating
			uint32_t offset = (vaddr_masked - current_running->start_pc) / SECTOR_SIZE;
			// the actual base address plus the offset
			start = current_running->swap_loc + offset;

			/* Calculate how many sectors to read */
			if(current_running->swap_size < read_size)
				read_size = current_running->swap_size;
			current_running->swap_size -= SECTORS_PER_PAGE;
		}
		/* else, read from swap space */
		else {
			/* Page table entry is the address to page on disk */
			start = (uint32_t)pte / SECTOR_SIZE;

			/* Calculate the index where the page reside on disk */
			uint32_t idx = (start - WRITE_START) / SECTORS_PER_PAGE;
			swap_page[idx].free = TRUE;
			swap_count--;

			scrprintf(2, 0, "pages in swap: %i  ", swap_count);
		}

		/* read a page from the disk, and write it into the frame */
		scsi_read((int)start, (int)read_size, (char *)frame);
		/* Make a table entry with the frame */
		table_map_present(pde, current_running->fault_addr, (uint32_t)frame, user);
		return;
	}
}


static uint32_t free_frames = PAGEABLE_PAGES, pinned_frames = 0;
static uint32_t *get_frame(int pinned, uint32_t *base, uint32_t index) 
{
	uint32_t i = 0, tmp = 0, score = 0;

	spinlock_acquire(&memory_lock);

	/* Get index of a free frame or the "First in" frame */
	for(; i < PAGEABLE_PAGES; i++)
	{
		/* If frame is free */
		if(frame[i].free == TRUE)
			break;

		/* If frame is not pinned, increment */
		if(frame[i].pinned == FALSE)
			frame[i].fifo++;

		/* Get index of "First in" */
		if(frame[i].fifo > score) {
			score = frame[i].fifo;
			tmp = i;
		}
	}

	/* If there are no free pages */
	if(i == PAGEABLE_PAGES)
	{
		i = tmp;	// index of "First in"

		/* Flush the page talbe entry that is about to get evicted, from the TLB.
		 * The page talbe entry points to a frame containing code/data. */
		flush_tlb_entry(frame[i].base[frame[i].index] & PE_BASE_ADDR_MASK);

		/* Get index of free page in swap-space */
		int idx = 0;
		for(; idx < SWAPABLE_PAGES; idx++){
			if(swap_page[idx].free == TRUE)
				break;
		}
		ASSERT2(idx != SWAPABLE_PAGES, "No more swapable pages!");
		
		/* The page talbe entry points to a address on disk */
		frame[i].base[frame[i].index] = (swap_page[idx].daddr * SECTOR_SIZE) & ~PE_P;

		/* Clear page the swap-space on disk */
		scsi_write((int)swap_page[idx].daddr, (int)SECTORS_PER_PAGE, (char *)zero_buffer);

		/* Write the evicted frame into the swap-space on disk */
		scsi_write((int)swap_page[idx].daddr, (int)SECTORS_PER_PAGE, (char *)frame[i].paddr);

		/* Clear the evicted frame in physical memory */
		for (uint32_t j = 0; j < (PAGE_SIZE / sizeof(uint32_t)); frame[i].paddr[j++] = 0)
			;

		swap_page[idx].free = FALSE;
		swap_count++;
	}
	else {
		free_frames -= 1;
	}
	if(pinned)
		pinned_frames += 1;

	scrprintf(0, 0, "free pages: %i  ", free_frames);
	scrprintf(1, 0, "pinned pages: %i  ", pinned_frames);
	scrprintf(2, 0, "pages in swap: %i  ", swap_count);

	frame[i].base = base;
	frame[i].index = index;
	frame[i].pinned = pinned;
	frame[i].free = FALSE;
	frame[i].fifo = 0;

	spinlock_release(&memory_lock);
	return frame[i].paddr;
}


/*
 * This sets up mapping for memory that should be shared between the
 * kernel and the user process. We need this since an interrupt or
 * exception doesn't change to another page directory, and we need to
 * have the kernel mapped in to handle the interrupts. So essentially
 * the kernel needs to be mapped into the user process address space.
 *
 * The user process can't access the kernel internals though, since
 * the processor checks the privilege level set on the pages and we
 * only set USER privileges on the pages the user process should be
 * allowed to access.
 *
 * Note:
 * - we identity map the pages, so that physical address is
 *   the same as the virtual address.
 *
 * - The user processes need access video memory directly, so we set
 *   the USER bit for the video page if we make this map in a user
 *   directory.
 */
static void make_common_map(uint32_t *page_directory, int user) {
	uint32_t *page_table, addr;

	/* Allocate memory for the page table */
	uint32_t pde_index = get_directory_index((uint32_t)page_directory);
	page_table = get_frame(TRUE, page_directory, pde_index);

	/* Identity map the first 640KB of base memory */
	for(addr = 0; addr < 640 * 1024; addr += PAGE_SIZE)
		table_map_present(page_table, addr, addr, 0);

	/* Identity map the video memory, from 0xb8000-0xb8fff. */
	table_map_present(page_table, (uint32_t)SCREEN_ADDR, (uint32_t)SCREEN_ADDR, user);

	/*
	 * Identity map in the rest of the physical memory so the
	 * kernel can access everything in memory directly.
	 */
	for(addr = MEM_START; addr < MAX_PHYSICAL_MEMORY; addr += PAGE_SIZE)
		table_map_present(page_table, addr, addr, 0);

	/*
	 * Insert in page_directory an entry for virtual address 0
	 * that points to physical address of page_table.
	 */
	directory_insert_table(page_directory, 0, page_table, user);
}


/*
 * Returns a pointer to a freshly allocated page in physical
 * memory. The address is aligned on a page boundary.  The page is
 * zeroed out before the pointer is returned.
 */
static uint32_t *allocate_page(void) {
	uint32_t *page = (uint32_t *)alloc_memory(PAGE_SIZE);
	int i;

	for (i = 0; i < 1024; page[i++] = 0)
		;

	return page;
}


/*
 * Allocate size bytes of physical memory. This function is called by
 * allocate_page() to allocate memory for a page directory or a page
 * table,and by loadproc() to allocate memory for code + data + user
 * stack.
 *
 * Note: if size < 4096 bytes, then 4096 bytes are used, beacuse the
 * memory blocks must be aligned to a page boundary.
 */
static uint32_t alloc_memory(uint32_t size) {
	uint32_t ptr;

	spinlock_acquire(&memory_lock);
	ptr = next_free_mem;
	next_free_mem += size;

	if ((next_free_mem & 0xfff) != 0) {
		/* align next_free_mem to page boundary */
		next_free_mem = (next_free_mem & 0xfffff000) + 0x1000;
	}
#ifdef DEBUG
	scrprintf(11, 1, "%x", next_free_mem);
	scrprintf(12, 1, "%x", MEM_END);
	scrprintf(13, 1, "%x", size);
#endif /* DEBUG */
	ASSERT2(next_free_mem < MAX_PHYSICAL_MEMORY, "Memory exhausted!");
	spinlock_release(&memory_lock);
	return ptr;
}