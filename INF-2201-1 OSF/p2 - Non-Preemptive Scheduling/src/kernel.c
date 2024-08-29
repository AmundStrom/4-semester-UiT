/*  kernel.c
 */
#include "common.h"
#include "kernel.h"
#include "scheduler.h"
#include "th.h"
#include "util.h"

// Statically allocate some storage for the pcb's
pcb_t pcb[NUM_TOTAL];
// Ready queue and pointer to currently running process
pcb_t *current_running;

/* This is the entry point for the kernel.
 * - Initialize pcb entries
 * - set up stacks
 * - start first thread/process
 */
void _start(void) {
	/* Declare entry_point as pointer to pointer to function returning void
	 * ENTRY_POINT is defined in kernel h as (void(**)()0xf00)
	 */
	void (**entry_point)() = ENTRY_POINT;

	// load address of kernel_entry into memory location 0xf00
	*entry_point = kernel_entry;

	clear_screen(0, 0, 80, 25);

	pcb_init();
	
	/* Threads must occupy the start of the list */
	pcb[0].kernel_ptr = (uint32_t)clock_thread;
	pcb[1].kernel_ptr = (uint32_t)thread2;
	pcb[2].kernel_ptr = (uint32_t)thread3;
	pcb[3].kernel_ptr = (uint32_t)mcpi_thread0;
	pcb[4].kernel_ptr = (uint32_t)mcpi_thread1;
	pcb[5].kernel_ptr = (uint32_t)mcpi_thread2;
	pcb[6].kernel_ptr = (uint32_t)mcpi_thread3;
	pcb[7].kernel_ptr = (uint32_t)thread4;
	/* Processes must occupy the end of the list */
	pcb[8].kernel_ptr = 0x5000;
	pcb[9].kernel_ptr = 0x7000;
	pcb[10].kernel_ptr = 0x9000;

	current_running = &pcb[0];

	/* Time prints */
	print_str(6,40, "pid");
	print_str(6,45, "sys time");
	print_str(6,60, "user time");

	for (int i = 0; i < NUM_TOTAL; i++)
		print_int(6 + pcb[i].pid, 40, pcb[i].pid);
	
	dispatch();

	while (1)
		;
}

void pcb_init(void)
{
	/* Initialize threads */
	for(int i = 0; i < NUM_THREADS; i++){
		_pcb_thread(&pcb[i], 
				   &pcb[i + 1], 									// Next pointer
				   &pcb[(NUM_TOTAL + i - 1) % NUM_TOTAL], 			// Previous pointer, loops around
				   i + 1, 											// PID, starts at 1
				   STACK_MIN + (STACK_SIZE * i) + STACK_OFFSET);	// Calculate the kernel stack max value
	}

	/* Initialize processes */
	int counter = 0;
	for(int i = NUM_THREADS; i < NUM_TOTAL; i++){
		_pcb_process(&pcb[i], 
				    &pcb[(NUM_TOTAL + i + 1) % NUM_TOTAL], 			// Next pointer, loops around
				    &pcb[i - 1], 									// Previous pointer
				    i + 1, 											// PID
				    STACK_MIN + (STACK_SIZE * (i +     counter) ) + STACK_OFFSET,	// Calculate the kernel stack max value
					STACK_MIN + (STACK_SIZE * (i + 1 + counter) ) + STACK_OFFSET);	// Calculate the user stack max value
		counter++;
	}
}

void _pcb_thread(pcb_t *new_pcb, pcb_t *next, pcb_t *prev, int pid, int kernel_max) 
{
	new_pcb->next = next;
	new_pcb->previous = prev;
	new_pcb->pid = pid;
	new_pcb->pst = STATUS_FIRST_TIME;
	new_pcb->kernel_base = kernel_max;
	new_pcb->kernel_ptr = kernel_max;
	new_pcb->is_thread = TRUE;			// TRUE equals thread
	new_pcb->user_base = 0;				// Used by process
	new_pcb->user_ptr = 0;				// Used by process
	for(int i = 0; i < 3; i++){
		new_pcb->sys_time[i] = 0;
		new_pcb->user_time[i] = 0;
	}
}

void _pcb_process(pcb_t *new_pcb, pcb_t *next, pcb_t *prev, int pid, int kernel_max, int user_max)
{
	new_pcb->next = next;
	new_pcb->previous = prev;
	new_pcb->pid = pid;
	new_pcb->pst = STATUS_FIRST_TIME;
	new_pcb->kernel_base = kernel_max;
	new_pcb->kernel_ptr = kernel_max;
	new_pcb->is_thread = FALSE;			// FALSE equals process
	new_pcb->user_base = user_max;		// Used by process
	new_pcb->user_ptr = user_max;		// Used by process
	for(int i = 0; i < 3; i++){
		new_pcb->sys_time[i] = 0;
		new_pcb->user_time[i] = 0;
	}
}

/*  Helper function for kernel_entry, in entry.S. Does the actual work
 *  of executing the specified syscall.
 */
void kernel_entry_helper(int fn)
{
	if( fn == SYSCALL_YIELD ){
		yield();
		return;
	}

	if( fn == SYSCALL_EXIT ){
		exit();
		return;
	}

	if( fn == SYSCALL_COUNT ){
		print_str(2,2,"what is SYSCALL_COUNT, in kernel.c");
		asm ("hlt");
		return;
	}
}
