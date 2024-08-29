/* kernel.h
 *
 * Various definitions used by the kernel and related code.
 */
#ifndef KERNEL_H
#define KERNEL_H

#include "common.h"

/* Cast 0xf00 into pointer to pointer to function returning void
 * ENTRY_POINT is used in syslib.c to declare 'entry_point'
 */
#define ENTRY_POINT ((void (**)())0xf00)

// Constants
enum
{
	/* Number of threads and processes initially started by the kernel. Change
	 * this when adding to or removing elements from the start_addr array.
	 */
	NUM_THREADS = 8,
	NUM_PROCS = 3,
	NUM_TOTAL = (NUM_PROCS + NUM_THREADS),

	SYSCALL_YIELD = 0,
	SYSCALL_EXIT,
	SYSCALL_COUNT,

	//  Stack constants
	STACK_MIN = 0x10000,
	STACK_MAX = 0x20000,
	STACK_OFFSET = 0x0ffc,
	STACK_SIZE = 0x1000
};

// Typedefs

/* The process control block is used for storing various information about
 *  a thread or process
 */
typedef struct pcb_t {
	/* ----- These bytes are used in assembly. DO NOT MODIFY ----- */
	uint32_t kernel_base; 	// 0 bytes 		// Points to bottom of kernel stack, used to store registers
	uint32_t kernel_ptr;	// 4 bytes 		// Points to top of kernel stack
	uint32_t user_base; 	// 8 bytes 		// Points to bottom of user stack, used to run the task, used only by processes
	uint32_t user_ptr; 		// 12 bytes 	// Points to top of user stack
	int is_thread;			// 16 bytes 	// Differentiate between tasks. TRUE = Thread, FALSE = Process
	char fpu[108];			// 20 bytes		// 1 x 108 bytes large array, used for storing Floating Point Registers
	/* ----- These bytes are used in assembly. DO NOT MODIFY ----- */

	struct pcb_t *next, 	// Used when job is in the ready queue
	    *previous;
	int pid; 				// Process identifier
	int pst; 				// Process state (STATUS_FIRST_TIME = 0, STATUS_READY = 1, STATUS_BLOCKED = 2, STATUS_EXITED = 3)
	uint64_t sys_time[3];	// System time, [0] -> start time, [1] -> end time, [2] -> result
	uint64_t user_time[3];	// User time, 	[0] -> start time, [1] -> end time, [2] -> result (Used by processes)
} pcb_t;

// Variables

// The currently running process, and also a pointer to the ready queue
extern pcb_t *current_running;

// Prototypes
void kernel_entry(int fn);
void kernel_entry_helper(int fn);

/**
 * @brief Function that initialize the Program Control Blocks with values and doubly linked list.
 * Threads will occupy the start of the list, and Processes will occupy the end.
 */
void pcb_init(void);

/**
 * @brief Function that initialize a PCB for a thread with the given arguments
 * @param new_pcb Pointer to the PCB that is to be initialized
 * @param next Pointer to the next PCB 
 * @param prev Pointer to the previous PCB
 * @param pid Process ID
 * @param kernel_max The maximum value of the KERNEL stack for this given PCB, since the stack grows downwards
 */
void _pcb_thread(pcb_t *new_pcb, pcb_t *next, pcb_t *prev, int pid, int kernel_max);

/**
 * @brief Function that initialize a PCB for a Process with the given arguments
 * @param new_pcb Pointer to the PCB that is to be initialized
 * @param next Pointer to the next PCB 
 * @param prev Pointer to the previous PCB
 * @param pid Process ID
 * @param kernel_max The maximum value of the KERNEL stack for this given PCB, since the stack grows downwards (Used to store registers)
 * @param user_max The maximum value of the USER stack for this given PCB, since the stack grows downwards (Used to run the task)
 */
void _pcb_process(pcb_t *new_pcb, pcb_t *next, pcb_t *prev, int pid, int kernel_max, int user_max);
#endif
