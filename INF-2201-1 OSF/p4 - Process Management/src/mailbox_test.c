#include <stdio.h>

enum{
    SIZE = 5,
    BUFFER_SIZE = 15
};

typedef struct {
	int used; /* Number of processes which has opened this mbox */
	int count; /* Number of messages in mailbox */
	int head;  /* Points to the first free byte in the buffer */
	/* points to oldest message (first to be recived) in buffer */
	int tail;
	char buffer[BUFFER_SIZE];
} mbox_t;

typedef struct  
{
    int size;
    char msg[10];
} msg_t;


static char buff[SIZE];
mbox_t mbox;
msg_t msg[10];

static int space_available(mbox_t *q) {
	if ((q->tail == q->head) && (q->count != 0)) {
		/* Message in the queue, but no space  */
		return 0;
	}
    if (q->tail == q->head) {
		/* Message in the queue, but no space  */
		return BUFFER_SIZE - 1;
	}

	// if (q->tail > q->head) {
	// 	/* Head has wrapped around  */
	// 	return q->tail - q->head;
	// }
	/* Head has a higher index than tail  */
    // return (BUFFER_SIZE - q->tail + q->head) % BUFFER_SIZE;
	return (q->head + BUFFER_SIZE - q->tail - 1) % BUFFER_SIZE;
}


void rec(msg_t *msg){

    int i = 0;
    while(mbox.buffer[ (mbox.head + i) % BUFFER_SIZE ] != '\0'){
        msg->msg[i] = mbox.buffer[ (mbox.head + i) % BUFFER_SIZE ];
        i++;
    }
    msg->size = i;
    mbox.head = (mbox.head + i + 1) % BUFFER_SIZE;

}

void send(msg_t *msg)
{
    int space = space_available(&mbox);
    if(space < msg->size){
        printf("for lite plass\n");
        return;
    }

    for(int i = 0; i < msg->size; i++){
        mbox.buffer[ (mbox.tail + i) % BUFFER_SIZE ] = msg->msg[i];
    }
    mbox.buffer[ (mbox.tail + msg->size) % BUFFER_SIZE ] = '\0';
    mbox.tail = (mbox.tail + msg->size + 1) % BUFFER_SIZE;


}

struct myStruct {
    int element1;
    double element2;
    char element3;
    float element4;
};
typedef struct myStruct myStruct_t;


int main(void){

    mbox.count = 0;
    mbox.head = 0;
    mbox.tail = 0;
    mbox.used = 0;

    msg[0];
    msg[1];

    for(int i = 0; i < SIZE; i++){
        msg[0].msg[i] = 'a';
    }
    msg[0].size = SIZE;

    msg[2];
    msg[3];

    for(int j = 0; j < SIZE; j++){
        msg[2].msg[j] = 'c';
    }
    msg[2].size = SIZE;

    /* SEND */
    int space = space_available(&mbox);
    printf("space: %i\n",space);

    send(&msg[0]);

        /* RECIVE */
        space = space_available(&mbox);
        printf("space: %i\n",space);

        rec(&msg[1]);

    /* SEND */
    space = space_available(&mbox);
    printf("space: %i\n",space);

    send(&msg[2]);

        /* RECIVE */
        space = space_available(&mbox);
        printf("space: %i\n",space);

        rec(&msg[3]);

    space = space_available(&mbox);
    printf("space: %i\n",space);

    printf("head %i, tail %i\n", mbox.head, mbox.tail);
    
    send(&msg[0]);

        printf("head %i, tail %i\n", mbox.head, mbox.tail);

        space = space_available(&mbox);
        printf("space: %i\n",space);

        rec(&msg[1]);

    space = space_available(&mbox);
    printf("space: %i\n",space);

    send(&msg[2]);

        space = space_available(&mbox);
        printf("space: %i\n",space);

        rec(&msg[3]);

    space = space_available(&mbox);
    printf("space: %i\n",space);

    printf("head %i, tail %i\n", mbox.head, mbox.tail);
    send(&msg[0]);
    printf("head %i, tail %i\n", mbox.head, mbox.tail);
    send(&msg[2]);
    printf("head %i, tail %i\n", mbox.head, mbox.tail);
    send(&msg[0]);
    printf("head %i, tail %i\n", mbox.head, mbox.tail);

    for(int k = 0; k < msg[1].size; k++){
        printf("%c", msg[1].msg[k]);
    }

    for(int i = 0; i < msg[3].size; i++){
        printf("%c", msg[3].msg[i]);
    }

    return 0;
}