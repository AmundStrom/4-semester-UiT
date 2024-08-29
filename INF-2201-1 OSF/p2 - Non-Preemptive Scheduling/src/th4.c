/* Simple program that increments by 1 each time and prints if there is a prime number.
 * Prime number algorithm: https://www.javatpoint.com/prime-number-program-in-c
 */

#include "scheduler.h"
#include "th.h"
#include "util.h"

#define PROGRESS_LINE 13


/*
 * This thread runs indefinitely, which means that the
 * scheduler should never run out of processes.
 */
void thread4(void) {

    unsigned int number = 0;
    int flag = 0;
    int m = 0;
	while (1) {
        number++;
        flag = 0;
        m = number / 2;

        for(int i = 2; i <= m; i++)
        {
       		if(number % i == 0)
            { 
				flag = 1;
				break;
			}
        }
        if(flag == 0)
        {
            print_str(PROGRESS_LINE, 0, "This is a prime number: ");
            print_int(PROGRESS_LINE, 25, number);
        }
        
		yield();
	}
}
