/*
 * Implementation of locks and condition variables
 */

#include "common.h"
#include "interrupt.h"
#include "scheduler.h"
#include "thread.h"
#include "util.h"

void lock_init(lock_t *l) {
	/*
	 * no need for critical section, it is callers responsibility to
	 * make sure that locks are initialized only once
	 */
	l->status = UNLOCKED;
	l->waiting = NULL;
}

void lock_acquire(lock_t *l) {
	// enter critical section
	enter_critical();

	/* if lock is UNLOCKED, update to LOCKED,
	 * leave critical section and return */
	if(l->status == UNLOCKED){
		l->status = LOCKED;
		leave_critical();
		return;
	}

	// block current running
	block(&l->waiting);
	
	// leave critical section
	leave_critical();
}

void lock_release(lock_t *l) {
	// enter critical section
	enter_critical();

	/* if block queue is empty, set lock to UNLOCKED.
	 * else unblock first job in queue */
	if(l->waiting == NULL){
		l->status = UNLOCKED;
	}
	else{
		unblock(&l->waiting);
	}

	// leave critical section
	leave_critical();
}

/* condition functions */

void condition_init(condition_t *c) {
	c->waiting = NULL;
}

/*
 * unlock m and block the thread (enqued on c), when unblocked acquire
 * lock m
 */
void condition_wait(lock_t *m, condition_t *c) {
	// enter critical section
	enter_critical();

	// release lock from current running
	lock_release(m);
	// put current running in condition queue
	block(&c->waiting);

	// leave critical section
	leave_critical();

	// eventualy acquire lock when signal is given
	lock_acquire(m);
}

/* unblock first thread enqued on c */
void condition_signal(condition_t *c) {
	// enter critical section
	enter_critical();

	/* if condition queue is not empty, unblock first job */
	if(c->waiting != NULL){
		unblock(&c->waiting);
	}

	// leave critical section
	leave_critical();
}

/* unblock all threads enqued on c */
void condition_broadcast(condition_t *c) {
	// enter critical section
	enter_critical();

	/* while condition queue is not empty, unblock first job */
	while(c->waiting != NULL){
		unblock(&c->waiting);
	}

	// leave critical section
	leave_critical();
}

/* Semaphore functions. */
void semaphore_init(semaphore_t *s, int value) {

	s->waiting = NULL;
	s->count = value;
}

void semaphore_up(semaphore_t *s) {
	// enter critical section
	enter_critical();

	s->count++;
	/* if count is less than or equal to zero, unblock first job in queue */
	if(s->count <= 0){
		unblock(&s->waiting);
	}

	// leave critical section
	leave_critical();
}

void semaphore_down(semaphore_t *s) {
	// enter critical section
	enter_critical();

	s->count--;
	/* if count is less than 0, block current running */
	if(s->count < 0){
		block(&s->waiting);
	}

	// leave critical section
	leave_critical();
}

/*
 * Barrier functions
 */

/* n = number of threads that waits at the barrier */
void barrier_init(barrier_t *b, int n) {

	b->waiting = NULL;	
	b->amount = n;
}

/* Wait at barrier until all n threads reach it */
void barrier_wait(barrier_t *b) {
	// enter critical section
	enter_critical();

	/* if more than one thread is running, block current running */
	if(b->amount != 1){
		b->amount--;
		block(&b->waiting);
	}
	else{ /* else unblock all. This means that every thread has reached the barrier */
		while(b->waiting != NULL){
			b->amount++;
			unblock(&b->waiting);
		}
	}

	// leave critical section
	leave_critical();
}
