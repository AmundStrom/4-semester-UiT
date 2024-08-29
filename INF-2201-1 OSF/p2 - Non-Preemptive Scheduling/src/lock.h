/*  lock.h
 */

#ifndef LOCK_H
#define LOCK_H

// Includes
#include "kernel.h"

// Constants
enum
{
	LOCKED = 1,
	UNLOCKED = 0
};

// Typedefs
typedef struct {
	int status;		// Status of lock
	int size;		// Currently waiting amount
	int count;		// Total waiting amount
	pcb_t *wait_q[NUM_THREADS];		// Waiting queue
} lock_t;

//  Prototypes

/**
 * @brief Initialize the lock
 */
void lock_init(lock_t *);

/**
 * @brief The first task claims the lock, otherwise place the task in waiting queue
 */
void lock_acquire(lock_t *);

/**
 * @brief If waiting queue is empty release the lock, otherwise the first task in waiting queue claims the lock
 */
void lock_release(lock_t *);

#endif
