/* P1P2Serial: Library for reading/writing Daikin/Rotex P1P2 protocol
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * 20190303 v0.01 initial release; read works; write only partly tested
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

static uint16_t ticks_per_semibit=0;
static uint16_t ticks_per_bit=0;

static uint8_t rx_state;
static uint8_t rx_byte;
static uint8_t rx_parity;
static uint16_t rx_target;
static uint16_t rx_stop_ticks;
static volatile uint8_t rx_buffer_head;
static volatile uint8_t rx_buffer_tail;
#define RX_BUFFER_SIZE 80
static volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint8_t delta_buffer[RX_BUFFER_SIZE];

static volatile uint8_t tx_state=0;
static uint8_t tx_byte;
static uint8_t tx_bit;
static uint8_t tx_paritybit;
static volatile uint8_t tx_buffer_head;
static volatile uint8_t tx_buffer_tail;
#define TX_BUFFER_SIZE 80
static volatile uint8_t tx_buffer[TX_BUFFER_SIZE];

static volatile uint8_t time_msec=0;

#ifndef INPUT_PULLUP
#define INPUT_PULLUP INPUT
#endif

#define MAX_COUNTS_PER_BIT  2900  // < 65536 / 22

void P1P2Serial::init(uint32_t cycles_per_bit)
{
	if (cycles_per_bit < MAX_COUNTS_PER_BIT) {
		CONFIG_TIMER_NOPRESCALE();
	} else {
		cycles_per_bit /= 8;
		if (cycles_per_bit < MAX_COUNTS_PER_BIT) {
			CONFIG_TIMER_PRESCALE_8();
		} else {
			return; // baud rate too low for P1P2Serial
		}
	}
	ticks_per_bit = cycles_per_bit;
	// stop rx after 9.5 bits (start bit +8 data bits + 1/2 parity bit)
	rx_stop_ticks = (cycles_per_bit * 19) / 2;
	pinMode(INPUT_CAPTURE_PIN, INPUT_PULLUP);
	digitalWrite(OUTPUT_COMPARE_A_PIN, HIGH);
	ticks_per_semibit = cycles_per_bit/2;
	pinMode(OUTPUT_COMPARE_A_PIN, OUTPUT);
	rx_state = 0;
	rx_buffer_head = 0;
	rx_buffer_tail = 0;
	tx_state = 0;
	tx_buffer_head = 0;
	tx_buffer_tail = 0;
	CONFIG_CAPTURE_FALLING_EDGE();
	ENABLE_INT_INPUT_CAPTURE();
	// set up millisecond timer
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
  if (time_msec < 255) time_msec++;
}

/****************************************/
/**           Transmission             **/
/****************************************/

void P1P2Serial::writeByte(uint8_t b)
{
	uint8_t intr_state, head;

	head = tx_buffer_head + 1;
	if (head >= TX_BUFFER_SIZE) head = 0;
	while (tx_buffer_tail == head) ; // wait until space in buffer
	intr_state = SREG;
	cli();
	if (tx_state) {
		tx_buffer[head] = b;
		tx_buffer_head = head;
	} else {
		tx_state = 1;
		tx_byte = b;
		tx_bit = 0;
		tx_paritybit = 0;
		CONFIG_MATCH_CLEAR();
		SET_COMPARE_A(GET_TIMER_COUNT() + 16);
		// in 16 ticks, start bit will follow.
		// this will trigger interrupt for next action to be set up
		// state=1 indicates that (upon start interrupt routine) this start bit has just started
		ENABLE_INT_COMPARE_A();
	}
	SREG = intr_state;
}

ISR(COMPARE_A_INTERRUPT)
{
	uint8_t state, byte, bit, head, tail;
	uint16_t target;
	state = tx_state;
	byte = tx_byte;
	target = GET_COMPARE_A();
	// state indicate in which part of data pattern we are here
	// 1,2 startbit
	// 3,4 .. 17,18 data bits
	// 19,20 parity bit
	// 21,22 stopbit
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
	if (head == tail) {
		// tx buffer empty, no more bytes to send...
		if (state == 21) {
			// state==21, nothing to do, stop bit is already silent, but wait for end of it before finishing up in state 22
			// state 21 takes 1 bit length, in contrast to earlier states which take a semibit length
			tx_state = 22;
			SET_COMPARE_A(target+ticks_per_bit);
			// CONFIG_MATCH_SET(); // does nothing - should be set high already
		} else {
			// state==22, quit transmission mode
			tx_state = 0;
			CONFIG_MATCH_NORMAL(); // not sure we should do this in between transmissions
			DISABLE_INT_COMPARE_A();
		}
	} else {
		// more bytes to send; as we are here half-way parity bit, we set target as time of end of stop bit
		if (++tail >= TX_BUFFER_SIZE) tail = 0;
		tx_buffer_tail = tail;
		tx_byte = tx_buffer[tail];
		tx_bit = 0;
		tx_paritybit = 0;
		if (state == 21) {
			// next bit is startbit
			CONFIG_MATCH_CLEAR();
			SET_COMPARE_A(target+ticks_per_bit); // end of stop pulse part 2, is start of START pulse
		} else {
			// state==22, so we are "out of pattern", and need to re-schedule new transmission
			SET_COMPARE_A(GET_TIMER_COUNT() + 16);
		}
		tx_state = 1;
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
static uint8_t startbit_delta;

ISR(CAPTURE_INTERRUPT)
{
	uint8_t state, head;
	uint16_t capture, target;
	uint16_t offset, offset_overflow;

	//DISABLE_INT_COMPARE_B(); // added; to avoid interrupting the capture interrupt handling
	capture = GET_INPUT_CAPTURE();
#ifdef SUPPRESS_OSCILLATION
	if (capture - prev_edge_capture < SUPPRESS_PERIOD) return; // to suppress oscillations
	prev_edge_capture=capture;
#endif
	state = rx_state;
	if (state==0) {
		startbit_delta = time_msec;
		time_msec = 0;
	}
	if (state == 0) { // if this is first edge, it must be first (start) pulse detected
		// state==0 if this is first (falling) edge
		// start of start pulse
		// so start timer for end of byte receipt
		// and bit should be 0 in this case
		SET_COMPARE_B(capture + rx_stop_ticks);
		ENABLE_INT_COMPARE_B();
		// rx_target set to middle of first bit
		rx_target = capture + ticks_per_bit + ticks_per_semibit;
		rx_state = 1;
		rx_parity = 0;
	} else {
		// state != 0, and bit is 0 so this is a data or parity pulse after a falling edge
		// state is in range 1..9 here (1..8 for data bit; 9 for parity bit received)
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
				// received parity bit
				rx_parity = rx_parity ^ 0x80;
			}
			target += ticks_per_bit;		// reduce time by 1 bit time
			state++;
			if (state >=10) break;
		}
		// we received a zero bit, no need to modify rx_parity, but shift if it is a data bit
	 	if (state < 9) rx_byte >>= 1;
		state++;
		// reduce time by 1 bit time
		target += ticks_per_bit;			
		if (state >= 10) {
			// after all bits processed, there is no need to terminate via COMPARE mechanism any more
			DISABLE_INT_COMPARE_B();	
			head = rx_buffer_head + 1;
			digitalWrite(LED_BUILTIN, rx_parity?HIGH:LOW);
			if (head >= RX_BUFFER_SIZE) head = 0;
			if (head != rx_buffer_tail) {
				rx_buffer[head] = rx_byte;
				if (rx_parity) {
					delta_buffer[head] = 0; // signals parity error
				} else {
					delta_buffer[head] = startbit_delta;
				}
				rx_buffer_head = head;
			}
			rx_state = 0;
			return;
		}
		rx_target = target;
		rx_state = state;
	}
}

ISR(COMPARE_B_INTERRUPT)
// the COMPARE_B_INTERRUPT routine is only called if the parity bit was 1 (i.e., no pulse, so no edge).
// called in the middle of the bit time for the parity bit
{
	uint8_t head, state;

	DISABLE_INT_COMPARE_B();
	state = rx_state;
	while (state < 9) {
		rx_byte = (rx_byte >> 1) | 0x80;
		rx_parity = rx_parity ^ 0x80;
		state++;
	}
	rx_parity = rx_parity ^ 0x80;
	digitalWrite(LED_BUILTIN, rx_parity?HIGH:LOW);
	head = rx_buffer_head + 1;
	if (head >= RX_BUFFER_SIZE) head = 0;
	if (head != rx_buffer_tail) {
		rx_buffer[head] = rx_byte;
		if (rx_parity) {
			delta_buffer[head] = 0; // signals parity error
		} else {
			delta_buffer[head] = startbit_delta;
		}
		rx_buffer_head = head;
	}
	rx_state = 0;
}

int P1P2Serial::read_delta(void)
{
	uint8_t head, tail, out;

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
