/*
 * Implementation of the mailbox.
 * Implementation notes:
 *
 * The mailbox is protected with a lock to make sure that only
 * one process is within the queue at any time.
 *
 * It also uses condition variables to signal that more space or
 * more messages are available.
 * In other words, this code can be seen as an example of implementing a
 * producer-consumer problem with a monitor and condition variables.
 *
 * Note that this implementation only allows keys from 0 to 4
 * (key >= 0 and key < MAX_Q).
 *
 * The buffer is a circular array.
 */

#include "common.h"
#include "mbox.h"
#include "thread.h"
#include "util.h"

mbox_t Q[MAX_MBOX];

/*
 * Returns the number of bytes available in the queue
 * Note: Mailboxes with count=0 messages should have head=tail, which
 * means that we return BUFFER_SIZE bytes.
 */
static int space_available(mbox_t *q) {
	if ((q->tail == q->head) && (q->count != 0)) {
		/* Message in the queue, but no space  */
		return 0;
	}
	if (q->tail == q->head) {
		/* Message in the queue, but no space  */
		return BUFFER_SIZE - 1;
	}
	
	return ((q->head + BUFFER_SIZE - q->tail) - 1) % BUFFER_SIZE;
}

/* Initialize mailbox system, called by kernel on startup  */
void mbox_init(void) 
{	
	for (int i = 0; i < MAX_MBOX; i++){
		Q[i].count = 0;
		Q[i].head = 0;	// Head is the First Out, to the right of its index (indicates the start of the buffer)
		Q[i].tail = 0;	// Tail is the Last In, to the left of its index (indicates the end of the buffer)
		Q[i].used = 0;

		lock_init(&Q[i].l);
		condition_init(&Q[i].moreData);
		condition_init(&Q[i].moreSpace);
	}
}

/*
 * Open a mailbox with the key 'key'. Returns a mailbox handle which
 * must be used to identify this mailbox in the following functions
 * (parameter q).
 */
int mbox_open(int key) {
	Q[key].used++;

	return key;
}

/* Close the mailbox with handle q  */
int mbox_close(int q) {
	Q[q].used--;
	
	return 0;
}

/*
 * Get number of messages (count) and number of bytes available in the
 * mailbox buffer (space). Note that the buffer is also used for
 * storing the message headers, which means that a message will take
 * MSG_T_HEADER + m->size bytes in the buffer. (MSG_T_HEADER =
 * sizeof(msg_t header))
 */
int mbox_stat(int q, int *count, int *space) 
{
	lock_acquire(&Q[q].l);

	*count = Q[q].count;
	*space = space_available(&Q[q]);
	
	lock_release(&Q[q].l);
	return 0;
}

/* Fetch a message from queue 'q' and store it in 'm'  */
int mbox_recv(int q, msg_t *m)
{
	/* Enter monitor */
	lock_acquire(&Q[q].l);

	/* Wait if there is no messages */
	while( (Q[q].tail == Q[q].head) && (Q[q].count == 0) ){
		condition_wait(&Q[q].l, &Q[q].moreData);
	}

	int i = 0;
	/* While current character IS NOT null-terminator,
	 * next character.
	 * Uses a circular array */
	while( Q[q].buffer[ (Q[q].head + i) % BUFFER_SIZE ] != '\0' )
	{
		m->body[i] = Q[q].buffer[ (Q[q].head + i) % BUFFER_SIZE ];	// message = current character
		i++;
	}
	m->size = i;	// size of message is amount of characters (not including null-terminator)
	Q[q].head = (Q[q].head + i + 1) % BUFFER_SIZE;	// Update head, size of message INCLUDING null-terminator
	Q[q].count--;

	/* Leave monitor */
	condition_broadcast(&Q[q].moreSpace);

	lock_release(&Q[q].l);

	return 0;
}

/* Insert 'm' into the mailbox 'q'  */
int mbox_send(int q, msg_t *m)
{
	/* Enter monitor */
	lock_acquire(&Q[q].l);

	/* Wait if there is not enough space for message */
	while(space_available(&Q[q]) < m->size){
		condition_wait(&Q[q].l, &Q[q].moreSpace);
	}

	/* Copy message into buffer.
	 * Uses a circular array */
	for(int i = 0; i < m->size; i++){
		Q[q].buffer[ (Q[q].tail + i) % BUFFER_SIZE ] = m->body[i];
	}
	Q[q].buffer[ (Q[q].tail + m->size) % BUFFER_SIZE ] = '\0'; 	// Add null-terminator to end of message in buffer
	Q[q].tail =  (Q[q].tail + m->size + 1) % BUFFER_SIZE;		// Update tail, size of message INCLUDING null-terminator
	Q[q].count++;

	/* Leave monitor */
	condition_broadcast(&Q[q].moreData);
	
	lock_release(&Q[q].l);

	return 0;
}
