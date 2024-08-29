/* lock.c
 *
 * Implementation of locks.
 */

#include "common.h"
#include "lock.h"
#include "scheduler.h"

void lock_init(lock_t *l) {
    l->status = UNLOCKED;
    l->size = 0;        // Currently waiting amount
    l->count = 0;       // Total waiting amount
}

void lock_acquire(lock_t *l) {
    // If lock is not in use, claim it
    if(l->status == UNLOCKED){
        l->status = LOCKED;
        return;
    }
    // If lock is in use, place current running in waiting queue
    block(l->wait_q, l->count);
    l->size++;
    l->count++;

    // Yield to save context of running task, before being removed from ready queue
    yield();
}

void lock_release(lock_t *l) {
    // If waiting queue is empty, unlock
    if(l->size == 0){
        l->status = UNLOCKED;
        return;
    }
    // If waiting queue is not empty, unblock first in queue
    unblock(l->wait_q, l->size, l->count);
    l->size--;
}
