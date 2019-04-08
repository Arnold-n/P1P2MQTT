/* P1P2Serial: Library for reading/writing Daikin/Rotex P1P2 protocol
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20190409 v0.9.1 Improved setDelay()
 * 20190407 v0.9.0 Improved reading, writing, and meta-data; added support for timed writings and collision detection; added stand-alone hardware-debug mode
 * 20190303 v0.0.1 initial release; support for raw monitoring and hex monitoring
 *
 *     Thanks to Krakra for providing the hints and references to the HBS and MM1192 documents on
 *     https://community.openenergymonitor.org/t/hack-my-heat-pump-and-publish-data-onto-emoncms
 *     to Bart Ellast for providing explanations and sample output from his heat pump, and
 *     to Paul Stoffregen for publishing the AltSoftSerial library.
 *
 * The GPL-licensed P1P2Serial library is based on the MIT-licensed AltSoftSerial,
 *      but please note that P1P2Serial itself is licensed under the GPL v2.0.
 *      The original license for AltSoftSerial is included below.
 *
 ** An Alternative Software Serial Library                                           **
 ** http://www.pjrc.com/teensy/td_libs_AltSoftSerial.html                            **
 ** Copyright (c) 2014 PJRC.COM, LLC, Paul Stoffregen, paul@pjrc.com                 **
 **                                                                                  **
 ** Permission is hereby granted, free of charge, to any person obtaining a copy     **
 ** of this software and associated documentation files (the "Software"), to deal    **
 ** in the Software without restriction, including without limitation the rights     **
 ** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell        **
 ** copies of the Software, and to permit persons to whom the Software is            **
 ** furnished to do so, subject to the following conditions:                         **
 **                                                                                  **
 ** The above copyright notice and this permission notice shall be included in       **
 ** all copies or substantial portions of the Software.                              **
 **                                                                                  **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR       **
 ** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,         **
 ** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE      **
 ** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER           **
 ** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,    **
 ** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN        **
 ** THE SOFTWARE.                                                                    **
 *
 */

#include "P1P2Serial.h"
#include "config/AltSoftSerial_Boards.h"
#include "config/AltSoftSerial_Timers.h"

/****************************************/
/**          Initialization            **/
/****************************************/

static uint16_t ticks_per_bit=0;
static uint16_t ticks_per_semibit=0;
static uint16_t ticks_per_byte_minus_ms=0;
static uint16_t ticks_per_timer0=0;

static uint8_t rx_state;
static uint8_t rx_byte;
static uint8_t rx_parity;
static uint16_t rx_target;
static uint16_t rx_stop_ticks;
static volatile uint8_t rx_buffer_head;
static volatile uint8_t rx_buffer_tail;
#define RX_BUFFER_SIZE 80
static volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint16_t delta_buffer[RX_BUFFER_SIZE];

static volatile uint8_t tx_state=0;
static uint8_t tx_byte;
static uint8_t tx_byte2;
static uint8_t rx_verification;
static uint8_t rx_do_verification;
static uint8_t tx_bit;
static uint8_t tx_paritybit;
static volatile uint8_t tx_buffer_head;
static volatile uint8_t tx_buffer_tail;
#define TX_BUFFER_SIZE 80
static volatile uint8_t tx_buffer[TX_BUFFER_SIZE];
static volatile uint8_t tx_buffer_delay[TX_BUFFER_SIZE];

static volatile uint16_t time_msec=0;
static volatile uint16_t tx_wait=0;

#ifndef INPUT_PULLUP
#define INPUT_PULLUP INPUT
#endif

#define MAX_COUNTS_PER_BIT 5400  // < 65536 / 12

void P1P2Serial::init(uint32_t cycles_per_bit)
{
	if (cycles_per_bit < MAX_COUNTS_PER_BIT) {
		CONFIG_TIMER_NOPRESCALE();
	} else {
		// Needed for 9600 baud and clocks > ~51 MHz (Arduino Due):
		cycles_per_bit /= 8;
		ticks_per_byte_minus_ms /= 8;
		if (cycles_per_bit < MAX_COUNTS_PER_BIT) {
			CONFIG_TIMER_PRESCALE_8();
		} else {
			return; // baud rate too low for P1P2Serial
		}
	}
	ticks_per_bit = cycles_per_bit;
	// 1 byte (11 bits) in 9600 baud takes 1.13 milliseconds;
	// calculate exact length of 1 byte (11 bits) minus 1 millisecond:
	ticks_per_byte_minus_ms = 11 * cycles_per_bit - (ALTSS_BASE_FREQ / 1000);
	// stop rx after 9.5 bit periods (start bit +8 data bits + 1/2 parity bit)
	// note don't use (uint16_t) ticks_per_bit here, use (uint32_t) cycles_per_bit (to avoid overflow)
        ticks_per_timer0 = 64; // assuming prescale factor of timer0=64, prescale factor of TIMER=1;
	rx_stop_ticks = (cycles_per_bit * 19) / 2; 
	pinMode(INPUT_CAPTURE_PIN, INPUT_PULLUP);
	digitalWrite(OUTPUT_COMPARE_A_PIN, HIGH);
	ticks_per_semibit = ticks_per_bit/2;
	pinMode(OUTPUT_COMPARE_A_PIN, OUTPUT);
	rx_state = 0;
	rx_buffer_head = 0;
	rx_buffer_tail = 0;
	tx_state = 0;
	tx_buffer_head = 0;
	tx_buffer_tail = 0;
	CONFIG_CAPTURE_FALLING_EDGE();
	ENABLE_INT_INPUT_CAPTURE();
	// set up millisecond timer - code works currently only on 16MHz CPUs
	TIMSK0 = (1<<OCIE0A);
	TCCR0A = 2;  // TCT mode; no signal on pins
	TCCR0B = 3;  // TCT mode; prescale 64x
	OCR0A = 249; // count until 249; 16 MHz / (64*250) = 1 kHz
	// set up pin for LED to show parity errors
	pinMode(LED_BUILTIN, OUTPUT);
}

void P1P2Serial::end(void)
{
	DISABLE_INT_COMPARE_B();
	DISABLE_INT_INPUT_CAPTURE();
	flushInput();
	flushOutput();
	DISABLE_INT_COMPARE_A();
	TIMSK0 = 0;
}

/****************************************/
/**       Millisecond counter          **/
/****************************************/

ISR(TIMER0_COMPA_vect)
{
	if (time_msec < MAXDELTA) time_msec++; // higher values reserved for error codes in delta reporting
	if ((tx_state == 99) && (time_msec >= tx_wait)) {
#ifdef NO_READ_DURING_WRITE
// TODO: Disable reading
#endif
		tx_state = 1;
		CONFIG_MATCH_CLEAR();
		uint16_t t_delta=16;
		if (time_msec == 1) t_delta=ticks_per_byte_minus_ms;
		SET_COMPARE_A(GET_TIMER_COUNT() + t_delta);
		// in 16 or ticks_per_byte_minus_ms ticks, start bit will follow, 
		// (if time_msec = 1, (and thus tx_wait = 1): wait exactly 1 byte time
		//                                            after last start bit falling edge).
		// This will trigger an interrupt for next action to be set up.
		// state=1 indicates that (upon start interrupt routine) this start bit has just started.
		ENABLE_INT_COMPARE_A();
	}
}

/****************************************/
/**           Transmission             **/
/****************************************/

uint16_t tx_set_delay=0;

void P1P2Serial::setDelay(uint16_t t)
// Input parameter: 1 <= t <= MAXDELTA.
// This sets delay for next byte (and next byte only) added to transmission buffer.
// The next byte added to queue will not be transferred until t milliseconds silence 
// since last falling edge has been detected.
// As we also read back sent data (also for the purpose of verification),
// this means that a delay is introduced since byte last read or written.
// If a byte is added to the queue during a silence, the past silence will also be taken into account.
// If a byte is added with a setDelay which is shorter than the past silence, transmission will start immediately.
// If setDelay(0) is called, or setDelay() is not called, there is no delay, and
//                a risk of a possible collision with an ongoing read
// If setDelay(1) is called, a delay of 1.13 ms (11 bits, 1 byte) is set after
//                the last start bit falling edge, so writing is done exactly
//                after the last read byte (but this may collide if the read
//                is ongoing; this situation also introduces a race condition
//                between the adapter and the bus writing which is not detected
//                as the write is already initiated during the "last" byte read.
//                There is likely no need to use setDelay(1) and its use is discouraged.
// If setDelay (t) is called with 2<=t<=MAXDELTA, a delay of t ms is set
// If setDelay (t) is called with t>MAXDELTA, t is set to MAXDELTA
{
	if (t > MAXDELTA) t = MAXDELTA;
	tx_set_delay = t;
}

void P1P2Serial::writeByte(uint8_t b)
{
	uint8_t intr_state, head;

	head = tx_buffer_head + 1;
	if (head >= TX_BUFFER_SIZE) head = 0;
	while (tx_buffer_tail == head) ; // wait until space in buffer
	intr_state = SREG;
	cli(); 
	// cli() is needed around the SET_COMPARE_A/ENABLE_INT_COMPARE_A instruction pair
	// but not necessarily here?
	if (tx_state) {
		tx_buffer[head] = b;
		tx_buffer_delay[head] = tx_set_delay; tx_set_delay=0;
		tx_buffer_head = head;
	} else {
		tx_byte = b;
		tx_byte2 = b;
		tx_bit = 0;
		tx_paritybit = 0;
		if ((tx_set_delay) && (time_msec < tx_set_delay)) {
			// set millisecond ISR up to start transmission later
			tx_state = 99;
			tx_wait = tx_set_delay; tx_set_delay=0;
		} else {
			tx_state = 1;
			uint16_t t_delta = 16;
			if (time_msec == 1) {
				// compensate for time past since 1ms detection
				if (ticks_per_byte_minus_ms > TCNT0 * ticks_per_timer0 + 16) {
					t_delta = ticks_per_byte_minus_ms - TCNT0 * ticks_per_timer0;
				} else {
					t_delta = 16;
				}
			}
			CONFIG_MATCH_CLEAR();
			SET_COMPARE_A(GET_TIMER_COUNT() + t_delta);
			// in 16 ticks (or more if time_msec=1), start bit will follow.
			// this will trigger an interrupt for next action to be set up
			// state=1 indicates that (upon start interrupt routine) this start bit has just started
			ENABLE_INT_COMPARE_A();
#ifdef NO_READ_DURING_WRITE
// TODO: Disable reading
#endif
		}
	}
	SREG = intr_state;
}

ISR(COMPARE_A_INTERRUPT)
{
	uint8_t state, byte, bit, head, tail;
	uint16_t target, delay;
	state = tx_state;
	byte = tx_byte;
	target = GET_COMPARE_A();
	// state indicates in which part of data pattern we are here
	// 1,2 startbit
	// 3,4 .. 17,18 data bits
	// 19,20 parity bit
	// 21 stopbit
	// 22 after stopbit
	while (state < 21) {
		// calculate next (semi)bit value
		if (state & 1) {
			// state is odd, next semibit will be part 2 of bit (high)
			bit = 1;
		} else if (state < 18) {
			// state==even, <18, next bit will be data bit part 1, LSB first
			bit = (byte & 1);
			tx_paritybit ^= bit;
			byte >>= 1;
		} else if (state ==18) {
			// state==18, next bit will be parity bit part 1
			bit = tx_paritybit;
		} else {
			// state==20, next bit will be stop bit part 1 (high / silence)
			bit = 1;
		}
		target += ticks_per_semibit;
		state++;
		if (bit != tx_bit) {
			if (bit) {
				CONFIG_MATCH_SET();
			} else {
				CONFIG_MATCH_CLEAR();
			}
			SET_COMPARE_A(target);
			tx_bit = bit;
			tx_byte = byte;
			tx_state = state;
			return;
		}
	}
	head = tx_buffer_head;
	tail = tx_buffer_tail;
	// byte sent; we are here not earlier than half-way first data bit, and not later than half-way parity bit
	// so in time to write rx_verification byte for verifying read-back byte (for verification and collision-detection)
	if (state == 21) {
		rx_verification = tx_byte2;
		rx_do_verification = 1;
	}
	if (head == tail) {
		// tx buffer empty, no more bytes to send...
		if (state == 21) {
			// state==21, nothing to do, stop bit is already silent, but wait for end of it before finishing up in state 22
			// state 21 takes 1 bit length, in contrast to earlier states which take a semibit length
			tx_state = 22;
			SET_COMPARE_A(target+ticks_per_bit);
			// CONFIG_MATCH_SET(); // does nothing - should be set high already
		} else {
			// if in state==22 and buffer still empty, quit transmission mode
			tx_state = 0;
			CONFIG_MATCH_NORMAL(); // not sure we should do this in between transmissions
			DISABLE_INT_COMPARE_A();
		}
	} else {
		// there are more bytes to send; 
		if (++tail >= TX_BUFFER_SIZE) tail = 0;
		tx_buffer_tail = tail;
		tx_byte = tx_buffer[tail];
		tx_byte2 = tx_byte;
		delay = tx_buffer_delay[tail];
		tx_bit = 0;
		tx_paritybit = 0;
		if (delay) {
			// it does not matter whether we are still in stop bit or beyond, we have to wait longer
			// this relies on the fact that xx has just been reset, by reading back falling start bit edge of byte just sent
			tx_state = 99;
			tx_wait = delay;
			CONFIG_MATCH_NORMAL(); // not sure whether we should need to do this in between transmissions
			DISABLE_INT_COMPARE_A();
#ifdef NO_READ_DURING_WRITE
// TODO: Enable reading ; tricky, check whether we are triggering a falling edge detection here?; 
// or missing one last byte to verify.
// perhaps wait ?; or signal via rx_do_verification?
#endif
		} else {
			if (state == 21) {
				//as we are in (silent, high) stop bit time, we set target as time of end of stop bit
				// next bit is startbit
				CONFIG_MATCH_CLEAR();
				SET_COMPARE_A(target+ticks_per_bit); // end of stop pulse part 2, is start of START pulse
			} else {
				// state==22, so we are (just) beyond stop bit; "out of pattern" (just received a new byte but too late to remain in sync), and we need to re-schedule new transmission
				CONFIG_MATCH_CLEAR();
				SET_COMPARE_A(GET_TIMER_COUNT() + 16);
			}
			tx_state = 1;
		}
	}
}

void P1P2Serial::flushOutput(void)
{
	while (tx_state) /* wait */ ;
}

/****************************************/
/**            Reception               **/
/****************************************/

//Use this to ignore edges that are very near previous edges to avoid detection of oscillating edges
//Does not seem necessary
//#define SUPPRESS_OSCILLATION
//#define SUPPRESS_PERIOD 20

#ifdef SUPPRESS_OSCILLATION
static uint16_t prev_edge_capture;	// previous capture of edge
#endif
static uint16_t startbit_delta;

ISR(CAPTURE_INTERRUPT)
{
	uint8_t state, head;
	uint16_t capture, target;
	uint16_t offset, offset_overflow;

	capture = GET_INPUT_CAPTURE();
#ifdef SUPPRESS_OSCILLATION
	if (capture - prev_edge_capture < SUPPRESS_PERIOD) return; // to suppress oscillations
	prev_edge_capture=capture;
#endif
	state = rx_state;
	if (state == 99) {
		// DISABLE_INT_COMPARE_B(); // not needed, will be enabled immediately in next step
		head = rx_buffer_head + 1;
		if (head >= RX_BUFFER_SIZE) head = 0;
		if (head != rx_buffer_tail) {
			rx_buffer[head] = rx_byte;
			if (rx_parity) {
				delta_buffer[head] = DELTA_PE; // signals parity error
			} else {
				delta_buffer[head] = startbit_delta; // signals no parity error / no end-of-block / time from previous byte
				if (rx_do_verification) {
					// If in transmission mode/verification mode,
					// check if received byte matches sent byte
					if (rx_verification != rx_byte) delta_buffer[head] = DELTA_COLLISION;
					rx_do_verification = 0;
				}
			}
			rx_buffer_head = head;
		} else {
			delta_buffer[rx_buffer_head] = DELTA_OVERRUN; // overwrite delta of previous byte to signal buffer-overrun (-2)
		}
		state = 0;
	}
	if (state == 0) { // if this is first edge, it must be first (start) pulse detected
		startbit_delta = time_msec;
		TCNT0 = 0; // start new milli-second timer measurement on falling edge of start bit
		time_msec = 0;
		// state==0 if this is first (falling) edge
		// start of start pulse
		// so start timer for end of byte receipt
		// and bit should be 0 in this case
		SET_COMPARE_B(capture + rx_stop_ticks);
		ENABLE_INT_COMPARE_B();
		// rx_target set to middle of first data bit
		rx_target = capture + ticks_per_bit + ticks_per_semibit;
		rx_state = 1;
		rx_parity = 0;
	} else {
		// state != 0, and we just received a falling edge for a data bit or parity bit
		// check how many high/silent bits we received
		// first bit is determined by state:
		// state = 1..8: data bit; state=9: parity bit
		target = rx_target;
		offset_overflow = 65535 - ticks_per_bit;
		while (1) { // check #silences missed and fill in ones for these silences
			offset = capture - target;
			if (offset > offset_overflow) break;
			if (state <= 8) {
				// received data bit 1
				rx_byte = (rx_byte >> 1) + 0x80;
				rx_parity = rx_parity ^ 0x80;
			} else {
				// received parity bit 1
				rx_parity = rx_parity ^ 0x80;
			}
			target += ticks_per_bit;		// increase target time by 1 bit time
			state++;
			if (state >= 10) break;
		}
		// we received a (data or parity) bit 0, thus no need to modify rx_parity
		// if it is a data bit (state<=8), perform a shift
		if (state < 9) rx_byte >>= 1;
		target += ticks_per_bit;	// set target time to (just after) next possible falling edge
		state++;
		// state now corresponds to the number of (start+data+parity) bits received so far
		//if (state >= 10) {
			// after all bits processed, there is no need to terminate via COMPARE mechanism any more
			// and we could finish up here 
			// but we still prefer to terminate via the COMPARE mechanism
			// to finish up as it allows us to determine whether we are still in reading mode.
		//}
		rx_target = target;
		rx_state = state;
	}
	// rx_state now corresponds to the number of (start+data+parity) bit (falling/missing) edges observed
}

ISR(COMPARE_B_INTERRUPT)
// The COMPARE_B_INTERRUPT routine is always called during the parity bit (state <=10),
// It is also called during the next start bit (state==99), but only if no start bit has been detected,
// this allows detection of an end of communication block.
{
	uint8_t head, state;
	uint16_t target;

	state = rx_state;
	target = rx_target;
	if (state==99) {
		// no new start bit detected within expected time frame; thus pause in received data detected
		DISABLE_INT_COMPARE_B();
		rx_state = 0;
		head = rx_buffer_head + 1;
		if (head >= RX_BUFFER_SIZE) head = 0;
		if (head != rx_buffer_tail) {
			rx_buffer[head] = rx_byte;
			if (rx_parity) {
				delta_buffer[head] = DELTA_PE_EOB; // signals parity error & end-of-block
			} else { 
				delta_buffer[head] = DELTA_EOB; // signals end-of-block
				if (rx_do_verification) {
					if (rx_verification != rx_byte) delta_buffer[head] = DELTA_COLLISION;
					rx_do_verification = 0;
				}
			}
			rx_buffer_head = head;
		} else {
			delta_buffer[rx_buffer_head] = DELTA_OVERRUN; // overwrite delta of previous byte to signal buffer-overrun (-2)
		}
		return;
	}
	// if parity bit was 0, state will be 10, otherwise state will be lower and we need to process the 1s
	while (state < 9) {
		rx_byte = (rx_byte >> 1) | 0x80;
		rx_parity = rx_parity ^ 0x80;
		target += ticks_per_bit;		// increase target time by 1 bit time
		state++;
	}
	if (state == 9) {
		rx_parity = rx_parity ^ 0x80;
		target += ticks_per_bit;		// increase target time by 1 bit time
		//state++;				// state is not used any more
	}
	digitalWrite(LED_BUILTIN, rx_parity ? HIGH : LOW);
	rx_state = 99;
	// rx_target = target;      // not used any more
	SET_COMPARE_B(target+ticks_per_bit);
}

uint16_t P1P2Serial::read_delta(void)
{
	uint8_t head, tail;
	uint16_t out;

	head = rx_buffer_head;
	tail = rx_buffer_tail;
	if (head == tail) return -1;
	if (++tail >= RX_BUFFER_SIZE) tail = 0;
	out = delta_buffer[tail];
	return out;
}

int P1P2Serial::read(void)
{
	uint8_t head, tail, out;

	head = rx_buffer_head;
	tail = rx_buffer_tail;
	if (head == tail) return -1;
	if (++tail >= RX_BUFFER_SIZE) tail = 0;
	out = rx_buffer[tail];
	rx_buffer_tail = tail;
	return out;
}

int P1P2Serial::peek(void)
{
	uint8_t head, tail;

	head = rx_buffer_head;
	tail = rx_buffer_tail;
	if (head == tail) return -1;
	if (++tail >= RX_BUFFER_SIZE) tail = 0;
	return rx_buffer[tail];
}

int P1P2Serial::available(void)
{
	uint8_t head, tail;

	head = rx_buffer_head;
	tail = rx_buffer_tail;
	if (head >= tail) return head - tail;
	return RX_BUFFER_SIZE + head - tail;
}

void P1P2Serial::flushInput(void)
{
	rx_buffer_head = rx_buffer_tail;
}

#ifdef ALTSS_USE_FTM0
void ftm0_isr(void)
{
	uint32_t flags = FTM0_STATUS;
	FTM0_STATUS = 0;
	if (flags & (1<<0) && (FTM0_C0SC & 0x40)) altss_compare_b_interrupt();
	if (flags & (1<<5)) altss_capture_interrupt();
	if (flags & (1<<6) && (FTM0_C6SC & 0x40)) altss_compare_a_interrupt();
}
#endif
