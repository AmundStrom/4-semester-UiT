/*  scheduler.c
 */
#include "common.h"
#include "kernel.h"
#include "scheduler.h"
#include "util.h"

// Used to get time before context switching
uint64_t time_before;

// Call scheduler to run the 'next' process
void yield(void) {
    time_before = get_timer();          // Get time before a task starts context switching

    scheduler_entry();                  // Start context switching

    uint64_t time_after = get_timer();  // Get time after a task finishes context switching
    uint64_t result = time_after - time_before; 

    clear_screen(0,12, 35,13);          // Clear screen before printing
    print_str(12,0, "Contxt sw clock cycles:");
    print_int(12,25, result);
}

/* The scheduler picks the next job to run, and removes blocked and exited
 * processes from the ready queue, before it calls dispatch to start the
 * picked process.
 */
void scheduler(void) {

    /* Remove Blocked and Exited tasks from the ready queue*/
    if( current_running->pst == STATUS_BLOCKED || current_running->pst == STATUS_EXITED )
    {
        pcb_t *tmp_next = current_running->next;
        pcb_t *tmp_prev = current_running->previous;

        current_running = current_running->next;
        current_running->previous = tmp_prev;

        current_running = current_running->previous;
        current_running->next = tmp_next;
    }

    // Get end time of time used in the system
    current_running->sys_time[1] = get_timer();

    // Calculate system and user time
    current_running->sys_time[2] = current_running->sys_time[1] - current_running->sys_time[0];
    current_running->user_time[2] = current_running->user_time[1] - current_running->user_time[0];
    uint64_t result = current_running->sys_time[2] - current_running->user_time[2];

    // print result on the screen
    clear_screen(45, 6 + current_running->pid, 75, 7 + current_running->pid);
    print_int(6 + current_running->pid, 45, result);
    print_int(6 + current_running->pid, 60, current_running->user_time[2]);

    // Next task
    current_running = current_running->next;

    // Get start time of time used in the system
    current_running->sys_time[0] = get_timer();

    // Jump so we dont add to the stack between context switching
    asm volatile ("jmp *%0" : : "r" (dispatch));
}

/* Check status of task and acts accordingly
 * STATUS_FIRST_TIME:   Setup stackframe and jump to address of the task
 * STATUS_READY:        Jump to entry.S to get context of task
 */
void dispatch(void) {

    /* If task is run for the first time, setup stack before executing */
    if( current_running->pst == STATUS_FIRST_TIME ){

        if(current_running->is_thread == TRUE){ 
            /* This is a Thread */
            // Setup kernel stack, used to store registers
            asm volatile ("movl %0, %%ebp" : : "r" (current_running->kernel_base));
            asm volatile ("movl %%ebp, %%esp" : :);
        }else{
            /* This is a Process */
            // Setup user stack, used to run the task
            asm volatile ("movl %0, %%ebp" : : "r" (current_running->user_base));
            asm volatile ("movl %%ebp, %%esp" : :);
        }

        current_running->pst = STATUS_READY;
        // Jump so we dont add to the stack between context switching
        asm volatile ("jmp *%0" : : "r" (current_running->kernel_ptr));
    }

    /* If task is ready jump to conext switch */
    if( current_running->pst == STATUS_READY ){
        // Jump so we dont add to the stack between context switching
        asm volatile ("jmp *%0" : : "r" (ready_entry));
    }

/* A task should never reach this point, but if it does there is a fatal error... */
print_str(1,1, "TASK REACHED END OF DISPATCH, DOES NOT HAVE VALID STATUS. EXITING...");
asm("hlt");
}

/* Set status = STATUS_EXITED and call scheduler_entry() which will remove the
 * job from the ready queue and pick next process to run.
 */
void exit(void) {
    current_running->pst = STATUS_EXITED;
    yield();
}

void block(pcb_t **q, int count)
{
    current_running->pst = STATUS_BLOCKED;                      // Update status
    q[(NUM_THREADS + count) % NUM_THREADS] = current_running;   // Place current running into waiting queue
}

void unblock(pcb_t **q, int size, int count)
{
    pcb_t *unblocked = q[(NUM_THREADS + count - size) % NUM_THREADS];   // Get first in waiting queue
    unblocked->pst = STATUS_READY;                                      // Update status

    // Add to end of ready queue
    pcb_t *unblocked_prev = current_running->previous;

    unblocked->next = current_running;
    unblocked->previous = unblocked_prev;

    current_running->previous = unblocked;
    unblocked_prev->next = unblocked;
}

void process_time(int arg) {
    current_running->user_time[arg] = get_timer();
}