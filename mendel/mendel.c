#include	<stddef.h>
#include	<stdint.h>
#include	<string.h>

#include	<avr/io.h>
#include	<avr/interrupt.h>

#include	"machine.h"

#include	"serial.h"
#include	"dda_queue.h"
#include	"dda.h"
#include	"gcode.h"
#include	"timer.h"
#include	"clock.h"
#include	"temp.h"
#include	"sermsg.h"
#include	"watchdog.h"

#ifndef	DEBUG
#define	DEBUG 0
#endif

void io_init(void) {
	// disable modules we don't use
	PRR = MASK(PRTWI) | MASK(PRADC);
	ACSR = MASK(ACD);

	// setup I/O pins
	WRITE(X_STEP_PIN, 0);	SET_OUTPUT(X_STEP_PIN);
	WRITE(X_DIR_PIN,  0);	SET_OUTPUT(X_DIR_PIN);
	WRITE(X_MIN_PIN,  1);	SET_INPUT(X_MIN_PIN);

	WRITE(Y_STEP_PIN, 0);	SET_OUTPUT(Y_STEP_PIN);
	WRITE(Y_DIR_PIN,  0);	SET_OUTPUT(Y_DIR_PIN);
	WRITE(Y_MIN_PIN,  1);	SET_INPUT(Y_MIN_PIN);

	WRITE(Z_STEP_PIN, 0);	SET_OUTPUT(Z_STEP_PIN);
	WRITE(Z_DIR_PIN,  0);	SET_OUTPUT(Z_DIR_PIN);
	WRITE(Z_MIN_PIN,  1);	SET_INPUT(Z_MIN_PIN);

	WRITE(E_STEP_PIN, 0);	SET_OUTPUT(E_STEP_PIN);
	WRITE(E_DIR_PIN,  0);	SET_OUTPUT(E_DIR_PIN);

	#ifdef	HEATER_PIN
		WRITE(HEATER_PIN, 0); SET_OUTPUT(HEATER_PIN);
		#ifdef	HEATER_PWM
			// setup PWM timer: fast PWM, no prescaler
			TCCR0A = MASK(WGM01) | MASK(WGM00);
			TCCR0B = MASK(CS00);
			TIMSK0 = 0;
			OCR0A = 0;
		#endif
	#endif

	#ifdef	FAN_PIN
		disable_fan();
	#endif

	#ifdef	STEPPER_ENABLE_PIN
		power_off();
	#endif

	// setup SPI
	WRITE(SCK, 0);				SET_OUTPUT(SCK);
	WRITE(MOSI, 1);				SET_OUTPUT(MOSI);
	WRITE(MISO, 1);				SET_INPUT(MISO);
	WRITE(SS, 1);					SET_OUTPUT(SS);
}

void init(void) {

	// set up watchdog
	wd_init();

	// set up serial
	serial_init();

	// set up inputs and outputs
	io_init();

	// set up timers
	setupTimerInterrupt();

	// set up clock
	clock_setup();

	// set up default feedrate
	current_position.F = startpoint.F = next_target.target.F = FEEDRATE_SLOW_Z;

	// enable interrupts
	sei();

	// reset watchdog
	wd_reset();

	// say hi to host
	serial_writestr_P(PSTR("Start\nOK\n"));

}

void clock_250ms(void) {
	// reset watchdog
	wd_reset();

	temp_tick();

	if (steptimeout > (30 * 4)) {
		power_off();
	}
	else if (temp_get_target() == 0)
		steptimeout++;

	ifclock(CLOCK_FLAG_1S) {
		if (DEBUG) {
			// current position
			serial_writestr_P(PSTR("Pos: "));
			serwrite_int32(current_position.X);
			serial_writechar(',');
			serwrite_int32(current_position.Y);
			serial_writechar(',');
			serwrite_int32(current_position.Z);
			serial_writechar(',');
			serwrite_int32(current_position.E);
			serial_writechar(',');
			serwrite_uint32(current_position.F);
			serial_writechar('\n');

			// target position
			serial_writestr_P(PSTR("Dst: "));
			serwrite_int32(movebuffer[mb_tail].endpoint.X);
			serial_writechar(',');
			serwrite_int32(movebuffer[mb_tail].endpoint.Y);
			serial_writechar(',');
			serwrite_int32(movebuffer[mb_tail].endpoint.Z);
			serial_writechar(',');
			serwrite_int32(movebuffer[mb_tail].endpoint.E);
			serial_writechar(',');
			serwrite_uint32(movebuffer[mb_tail].endpoint.F);
			serial_writechar('\n');

			// Queue
			print_queue();
		}
		// temperature
		if (temp_get_target())
			temp_print();
	}
}

int main (void)
{
	init();

	// main loop
	for (;;)
	{
		// if queue is full, no point in reading chars- host will just have to wait
		if ((serial_rxchars() != 0) && (queue_full() == 0)) {
			uint8_t c = serial_popchar();
			scan_char(c);
		}

		ifclock(CLOCK_FLAG_250MS) {
			clock_250ms();
		}
	}
}