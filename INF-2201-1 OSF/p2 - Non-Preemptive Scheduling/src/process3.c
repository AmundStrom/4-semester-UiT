/*
 * Simple counter program, to check the functionality of yield().
 * Each process prints a square spinning.
 */

#include "common.h"
#include "syslib.h"
#include "util.h"

#define ROWS 4
#define COLUMNS 18

static char picture[ROWS][COLUMNS + 1] = {
    "- - - - - - - - - -",
	"/ / / / / / / / / /",
	"| | | | | | | | | |",
	"\\ \\ \\ \\ \\ \\ \\ \\ \\ \\"};

static void draw(int locx, int locy, int plane);
static void print_counter(void);

void _start(void) {
	int locx = 5, locy = 15, loop = 0;

	while (1) {
		draw(locx, locy, loop);
        loop++;
		print_counter();
		delay(DELAY_VAL);
		yield();
	}
}

/* print counter */
static void print_counter(void) {
	static int counter = 0;

	print_str(14, 0, "Process 3 (Spinning)     : ");
	print_int(14, 25, counter++);
}

/* draw square */
static void draw(int locx, int locy, int loop) {
	int i, j;

	for (i = 0; i < ROWS; i++) {
		for (j = 0; j < COLUMNS; j++) {
			/* draw square */
			print_char(locy + i, locx + j, picture[(ROWS + loop) % ROWS][j]);
		}
	}
}
