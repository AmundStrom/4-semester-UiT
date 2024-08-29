#include "scheduler.h"
#include "screen.h"
#include "thread.h"
#include "util.h"

/* Dining philosphers threads. */

enum
{
	THINK_TIME = 9999,
	EAT_TIME = THINK_TIME,
};

volatile int forks_initialized = 0;
semaphore_t fork[3];
int num_eating = 0;
int scroll_eating = 0;
int caps_eating = 0;

static lock_t l;
static condition_t num_con;
static condition_t caps_con;
static condition_t scroll_con;

int num_eaten = 0;
int scroll_eaten = 0;
int caps_eaten = 0;

/* Set to true if status should be printed to screen */
int print_to_screen;

enum
{
	LED_NONE = 0x00,
	LED_SCROLL = 0x01,
	LED_NUM = 0x02,
	LED_CAPS = 0x04,
	LED_ALL = 0x07
};

/* Turns keyboard LEDs on or off according to bitmask.
 *
 * Bitmask is composed of the following three flags:
 * 0x01 -- SCROLL LOCK LED enable flag
 * 0x02 -- NUM LOCK LED enable flag
 * 0x04 -- CAPS LOCK LED enable flag
 *
 * Bitmask = 0x00 thus disables all LEDS, while 0x07
 * enables all LEDS.
 *
 * See http://www.computer-engineering.org/ps2keyboard/
 * and http://forum.osdev.org/viewtopic.php?t=10053
 */
static void update_keyboard_LED(unsigned char bitmask) {
	/* Make sure that bitmask only contains bits for status LEDs  */
	bitmask &= 0x07;

	/* Wait for keyboard buffer to be empty */
	while (inb(0x64) & 0x02)
		;
	/* Tells the keyboard to update LEDs */
	outb(0x60, 0xed);
	/* Wait for the keyboard to acknowledge LED change message */
	while (inb(0x60) != 0xfa)
		;
	/* Write bitmask to keyboard */
	outb(0x60, bitmask);

	ms_delay(100);
}

static void think_for_a_random_time(void) {
	volatile int foo;
	int i, n;

	n = rand() % THINK_TIME;
	for (i = 0; i < n; i++)
		if (foo % 2 == 0)
			foo++;
}

static void eat_for_a_random_time(void) {
	volatile int foo;
	int i, n;

	n = rand() % EAT_TIME;
	for (i = 0; i < n; i++)
		if (foo % 2 == 0)
			foo++;
}

/* Odd philosopher */
void num(void) {
	print_to_screen = 1;

	/* Initialize monitor */
	lock_init(&l);
	condition_init(&num_con);
	condition_init(&caps_con);
	condition_init(&scroll_con);

	forks_initialized = 1;
	if (print_to_screen) {
		print_str(PHIL_LINE, PHIL_COL, "Phil.");
		print_str(PHIL_LINE + 1, PHIL_COL, "Running");
	}

	while (1) {
		think_for_a_random_time();

		/* Enter monitor */
		lock_acquire(&l);

		/* If either caps or scroll is eating, wait */
		while( caps_eating != 0 || scroll_eating != 0 ){
			// HALT("num");
			condition_wait(&l, &num_con);
		}

		/* Enable NUM-LOCK LED and disable the others */
		update_keyboard_LED(LED_NUM);

		num_eating = 1;

		num_eaten++;
		/* With three forks only one philosopher at a time can eat */
		ASSERT(scroll_eating + caps_eating == 0);

		if (print_to_screen) {
			print_str(PHIL_LINE, PHIL_COL, "Phil.");
			print_str(PHIL_LINE + 1, PHIL_COL, "Num    ");
			print_str(PHIL_LINE - 1, PHIL_COL + 35, "Num    ");
			print_int(PHIL_LINE - 1, PHIL_COL + 43, num_eaten);
		}

		eat_for_a_random_time();

		num_eating = 0;

		/* Signal clear for caps and scroll */
		condition_signal(&caps_con);
		condition_signal(&scroll_con);

		/* Leave monitor */
		lock_release(&l);
	}
}

void caps(void) {
	/* Wait until num hasd initialized forks */
	while (forks_initialized == 0)
		yield();

	while (1) {
		think_for_a_random_time();

		/* Enter monitor */
		lock_acquire(&l);

		/* If either num or scroll is eating, wait */
		while( num_eating != 0 || scroll_eating != 0 ){
			// HALT("caps");
			condition_wait(&l, &caps_con);
		}

		/* Enable CAPS-LOCK LED and disable the others */
		update_keyboard_LED(LED_CAPS);

		caps_eating = 1;

		caps_eaten++;
		/* With three forks only one philosopher at a time can eat */
		ASSERT(scroll_eating + num_eating == 0);

		if (print_to_screen) {
			print_str(PHIL_LINE, PHIL_COL, "Phil.");
			print_str(PHIL_LINE + 1, PHIL_COL, "Caps   ");
			print_str(PHIL_LINE, PHIL_COL + 35, "Caps   ");
			print_int(PHIL_LINE, PHIL_COL + 43, caps_eaten);
		}

		eat_for_a_random_time();

		caps_eating = 0;

		/* Signal clear for num and scroll */
		condition_signal(&num_con);
		condition_signal(&scroll_con);

		/* Leave monitor */
		lock_release(&l);
	}
}

void scroll_th(void) {
	/* Wait until num hasd initialized forks */
	while (forks_initialized == 0)
		yield();

	while (1) {
		think_for_a_random_time();

		/* Enter monitor */
		lock_acquire(&l);

		/* If either num or caps is eating, wait */
		while( num_eating != 0 || caps_eating != 0 ){
			// HALT("scroll");
			condition_wait(&l, &scroll_con);
		}

		/* Enable SCROLL-LOCK LED and disable the others */
		update_keyboard_LED(LED_SCROLL);

		scroll_eating = 1;

		scroll_eaten++;
		/* With three forks only one philosopher at a time can eat */
		ASSERT(caps_eating + num_eating == 0);

		if (print_to_screen) {
			print_str(PHIL_LINE, PHIL_COL, "Phil.");
			print_str(PHIL_LINE + 1, PHIL_COL, "Scroll ");
			print_str(PHIL_LINE + 1, PHIL_COL + 35, "Scroll ");
			print_int(PHIL_LINE + 1, PHIL_COL + 43, scroll_eaten);
		}

		eat_for_a_random_time();

		scroll_eating = 0;

		/* Signal clear for num and caps */
		condition_signal(&num_con);
		condition_signal(&caps_con);

		/* Leave monitor */
		lock_release(&l);
	}
}
