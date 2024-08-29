#	entry.S
#	This file contains the code for kernel_entry, as well as
#	scheduler_entry, which saves the context of the current process
#	before calling the scheduler. The file is pre-processed before it
#	is assembled, allowing (among other things) the use of macros
#	and C-style comments.

.data
	.align 4
.text
.code32
	.align 4

# Make symbols visible for ld
.globl	kernel_entry
.globl	scheduler_entry
.globl  ready_entry

# void kernel_entry(int fn)
#
# SYSCALL(i) in syslib.c calls this function
# fn = system call number (defined in kernel.h)
#
ke_scratch:
	.long	0
ke_arg:
	.long	0
kernel_entry:
	# Get end time of time used in user level
	push $1
	call process_time
	addl $4, %esp

	# Save eax
	movl	%eax, (ke_scratch)
	
	# Save argument
	movl	4(%esp), %eax
	movl	%eax, (ke_arg)

	# %eax = pcb_t *current_running;
	movl current_running, %eax

	# Preserve user stack
	movl %ebp, 8(%eax)
	movl %esp, 12(%eax)

	# Switch to kernel stack, used to store registers
	movl 0(%eax), %ebp
	movl %ebp, %esp

	push (ke_arg)
	call kernel_entry_helper
	pop (ke_arg)

	ret

loop:
	jmp		loop

#	This function gets called to enter the scheduler, saving registers before doing so.
scheduler_entry:

	# Push registers on stack
	pushfl
	pushal

	# %eax = pcb_t *current_running;
	movl current_running, %eax

	# Save Floating point registers
	fsave 20(%eax)

	# Preserve stack pointers, so we can pop correct
	movl %ebp, 0(%eax)
	movl %esp, 4(%eax)

	# Jump to scheduler for picking next process
	# We want to jump since we dont want to add to the stack between context switching
	jmp scheduler

# Executed when a Proccess/Thread is STATUS_READY
ready_entry:

	# %eax = pcb_t *current_running;
	movl current_running, %eax

	# Find out if a thread or process is current running
	movl 16(%eax), %ecx
	movl $1, %edx
	cmpl %ecx, %edx		# Compare operands
	jne process_switch	# 1 != %ecx

# Only threads execute this
thread_switch:

	# Restore stack pointers, so we can pop correct
	movl 0(%eax), %ebp
	movl 4(%eax), %esp

	# Pop registers from stack
	popal
	popfl

	# Resotre Floating point registers
	movl current_running, %eax
	frstor 20(%eax)

	ret

# Only processes execute this
process_switch:

	# Restore stack pointers, so we can pop correct
	movl 0(%eax), %ebp
	movl 4(%eax), %esp

	# Pop registers from stack
	popal
	popfl

	# eax gets overwritten after pop
	movl current_running, %eax

	# Restore user stack
	movl 8(%eax), %ebp
	movl 12(%eax), %esp

	# Get start time of time used in user level
	push $0
	call process_time
	addl $4, %esp

	ret
