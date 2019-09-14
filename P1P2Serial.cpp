/* P1P2Serial: Library for reading/writing Daikin/Rotex P1P2 protocol
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20190914 v0.9.9 Added writeready()
 * 20190908 v0.9.8 Removed EOB signal in errorbuf results returned by readpacket(); as of now errorbuf contains only real error flags
 * 20190831 v0.9.7 Switch from TIMER0 to TIMER2 to avoid interference with millis() and readBytesUntil(), reduced RX_BUFFER_SIZE to 50
 * 20190824 v0.9.6 Added packetavailable()
 * 20190820 v0.9.5 Changed delay behaviour, timeout added
 * 20190817 v0.9.4 Clean up, bug fixes, improved ms counter, prescaler reset added, time measurement changed, delta/error reporting separated
 * 20190505 v0.9.3 Changed error handling and corrected deltabuf type in readpacket
 * 20190428 v0.9.2 Added setEcho(b), readpacket() and writepacket()
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
static uint16_t ticks_until_mid_paritybit;
static uint16_t prescaler_ratio_timer2_timer1;

static uint8_t rx_state;
static uint8_t rx_byte;
static uint8_t rx_paritycheck;
static uint16_t rx_target;
static volatile uint8_t rx_buffer_head;
static volatile uint8_t rx_buffer_tail;
static volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint8_t error_buffer[RX_BUFFER_SIZE]; // records error status
static volatile uint16_t delta_buffer[RX_BUFFER_SIZE]; // records timing info in ms

static volatile uint8_t tx_state;
static uint8_t tx_byte;
static uint8_t tx_byte_verify;
static uint8_t rx_byte_verify;
bool           rx_do_verify;
static uint8_t tx_bit;
static uint8_t tx_paritybit;
static volatile uint8_t tx_buffer_head;
static volatile uint8_t tx_buffer_tail;
static volatile uint8_t tx_buffer[TX_BUFFER_SIZE];
static volatile uint16_t tx_buffer_delay[TX_BUFFER_SIZE]; // records timing info in ms (16 bits)
static volatile uint16_t time_msec=0;
static volatile uint16_t tx_wait=0;

#define scheduledelay 16 // just enough to schedule timer reliably

#ifndef INPUT_PULLUP
#define INPUT_PULLUP INPUT
#endif

#define MAX_TICKS_PER_BIT 5400  // should be less than (65536 / 12) (as this is a 16 bit counter, and there are 11 bits per byte, plus some margin)

void P1P2Serial::begin(uint32_t baud)
{
  uint32_t cycles_per_bit = ((ALTSS_BASE_FREQ + baud / 2) / baud); // 1667 for 16MHz/9600baud (so < 5400, thus no prescaler needed)
  uint8_t scale = 1;
  if (cycles_per_bit < MAX_TICKS_PER_BIT) {
    CONFIG_TIMER_NOPRESCALE();
  } else {
    // Needed for 9600 baud and clocks > ~51 MHz (Arduino Due):
    cycles_per_bit /= 8;
    if (cycles_per_bit < MAX_TICKS_PER_BIT) {
      CONFIG_TIMER_PRESCALE_8();
      scale = 8;
    } else {
      return; // baud rate too low for P1P2Serial
    }
  }
  ticks_per_bit = cycles_per_bit;
  ticks_per_semibit = ticks_per_bit / 2;
  // use 32-bit cycles_per_bit here to avoid overflow:
  ticks_per_byte_minus_ms = (11 * cycles_per_bit - (ALTSS_BASE_FREQ / 1000)) / scale; // calculate exact length of 1 byte (11 bits, 1.13ms@9600baud) minus 1 millisecond:
  ticks_until_mid_paritybit  = ((cycles_per_bit * 19) / 2) / scale; // for setting timer to mid of parity bit (after 9.5 bit periods)

  pinMode(INPUT_CAPTURE_PIN, INPUT_PULLUP);
  digitalWrite(OUTPUT_COMPARE_A_PIN, HIGH);
  pinMode(OUTPUT_COMPARE_A_PIN, OUTPUT);

  rx_state = 0;
  rx_buffer_head = 0;
  rx_buffer_tail = 0;
  tx_state = 0;
  tx_buffer_head = 0;
  tx_buffer_tail = 0;

  rx_do_verify = 0;

  tx_state = 0;
  time_msec = 0;
  tx_wait = 0;

  CONFIG_CAPTURE_FALLING_EDGE();
  ENABLE_INT_INPUT_CAPTURE();

  // set up millisecond timer - code works currently only on 16MHz CPUs (and tested only on Uno/Mega)
  // Used to work om TIMER0, which is also used for millis and readBytesUntil, so now user TIMER2 to avoid interference
  TIMSK2 = (1<<OCIE2A); // was: TIMSK0 = (1<<OCIE0A); was: s/2/0/g
  TCCR2A = 2;  // CTC mode (WGM02:0=2); no signal on OC2A/OC2B pins; was: s/2/0/g
  TCCR2B = 4;  // timer2 prescaler is 64 (CS02:0=4) ( was: TCCR0B = 3;  // timer0 prescaler is 64 (CS02:0=3))
  OCR2A = 249; // timer2 counts until 249 and wraps; 16 MHz/(64*250) = 1 kHz
  prescaler_ratio_timer2_timer1 = 64/scale;

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
  TIMSK2 = 0;
}

/****************************************/
/**       Millisecond counter          **/
/****************************************/

static uint16_t tx_setdelaytimeout = 2500;

ISR(TIMER2_COMPA_vect)
{
// time_msec counts time in ms from the last start pulse (counting from the leading falling edge of the start pulse)
// max count is 65535 ms (uint16_t)
  if (time_msec < 0xFFFF) time_msec++; 
  // if tx_state =99, a write is scheduled, so check if pause is long enough to start writing
  if ((tx_state == 99) && ((time_msec == tx_wait) || ((time_msec >= tx_wait) && (time_msec >= tx_setdelaytimeout)))) {
    // start writing:
    // in scheduledelay or in ticks_per_byte_minus_ms ticks, falling edge of start bit  is scheduled
    // (if time_msec = 1, (and thus tx_wait = 1): wait exactly 1 byte time
    //                                            after last start bit falling edge).
    // This will trigger an interrupt for next action to be set up.
    // state=1 indicates that (upon start interrupt routine) the start bit has just begun.
    tx_state = 1;
    uint16_t t_delta = scheduledelay;
    if (time_msec == 1) t_delta = ticks_per_byte_minus_ms;
    CONFIG_MATCH_CLEAR();
    SET_COMPARE_A(GET_TIMER_COUNT() + t_delta);
    ENABLE_INT_COMPARE_A();
  }
}

/****************************************/
/**           Transmission             **/
/****************************************/

static uint16_t tx_setdelay = 0;

void P1P2Serial::setDelay(uint16_t t)
// Input parameter: 0 <= t <= 65535
// This sets delay for next byte (and next byte only) when it is added to the transmission buffer.
// The writing of the next byte to be added to the queue will be delayed until (<= v0.9.4: at least; >= v0.9.5: exactly) t milliseconds silence 
//                since last falling edge of start bit has been detected. (v0.9.5:) In addition, writing will follow in case of a timeout situation.
// This means that a delay is introduced since the byte last read, or,
// as we also read back sent data (also for the purpose of verification), since the byte last written.
// If a byte is added to the queue during a silence, the past silence will also be taken into account.
// (<=v0.9.4:) If a byte is added with a setDelay which is shorter than the past silence, transmission will start immediately.
// (>=v0.9.5:) If the past silence is more that t ms, but less than the timeout value, writing will be delayed until either one of the following 2 conditions is met:
//                1) a new byte is received, after which a new pause will be clocked, or
//                2) no new byte is received, but the total pause is or becomes longer than the value set with setDelayTimeout(t)
//                The reason for this change in behaviour is to prevent bus collissions when writing a packet near the end of a pause.
// If the value used as delay timeout is equal to or lower than the delay value, the writing behaviour of >=v0.9.5 equals the behaviour of library version <=v0.9.4
// It is advised to call setDelaytimeout(t) with a value that is larger than the total cycle time of the P1P2 bus behaviour.
// For many products the cycle repeats every 0.8..2s, so a setDelaytimeout(2500) would be appropriate. This is also the default value.
// For some products the cycle time can be larger (15s or so), and a setDelaytimeout(20000) might be appropriate.
// If setDelay(0) is called, or setDelay() is not called, there is no delay at all when writing. Writing will start immediately
//                as soon as a byte is written to the write buffer, if there was no bus action, or when the previous byte write is ready.
// If setDelay(1) is called, a delay of 1.13 ms (11 bits, 1 byte) is set after the last start bit falling edge,
//                so writing is done exactly after the last written or read byte.
//                This may be useful for interfacing according to the HBS timing protocol
//                Note that if setDelay(1) is called when the bus is written to by another device, a bus collision will occur if writing by the other device also continues
//                Use of setDelay(1) is therefore discouraged
//                Note that SetDelay is not (yet) suited for proper HBS timing, as HBS timing measures pauses from the end of the stop bit
//                Timing on the P1P2 bus is not so critical and SetDelay works fine for the P1P2 bus.
// If setDelay (t) is called with 2 <= t <= 65535, a delay of t ms is set.
// It is recommended to NOT use setDelay(0) or setDelay(1), unless really needed, because of the the risk of bus collision.
{
  tx_setdelay = t;
}

void P1P2Serial::setDelayTimeout(uint16_t t)
// Input parameter: 0 <= t <= 65535
// This sets delay timeout as described above
{
  tx_setdelaytimeout = t;
}

bool P1P2Serial::writeready(void)
{
  return (tx_buffer_tail == tx_buffer_head);
}

void P1P2Serial::write(uint8_t b)
{
  uint8_t intr_state, head;

  head = tx_buffer_head + 1;
  if (head >= TX_BUFFER_SIZE) head = 0;
  while (tx_buffer_tail == head) ; // wait until space in write buffer
  intr_state = SREG;
  cli(); 
  // cli() is needed here to avoid a race condition w.r.t. tx_state, which can change in ISR()
  if (tx_state) {
    // if already writing, add byte to write buffer
    tx_buffer[head] = b;
    tx_buffer_delay[head] = tx_setdelay; tx_setdelay=0;
    tx_buffer_head = head;
  } else {
    // if not already writing, start or schedule writing
    tx_byte = b;
    tx_byte_verify = b; // for read-back verification
    tx_bit = 0;
    tx_paritybit = 0;
    // we could initiate start writing here, as we did <=v0.9.4, but we can better leave it to the ISR, which is simpler
    // worst-case it adds some delay, but it makes the operation slightly more predictable
    if ((tx_setdelay) && ((time_msec < tx_setdelay) || (time_msec < tx_setdelaytimeout))) {
      // state 99: start transmission only (in msec ISR) when time_msec becomes == tx_setdelay or >= tx_setdelaytimeout
      tx_state = 99;
      tx_wait = tx_setdelay;
    } else {
      // if tx_setdelay is not set, or if it is set but we are beyond max(tx_setdelay,tx_setdelaytimeout), start writing
      // set tx_state to 1: schedule next start bit falling edge
      // time_msec = 0: should not happen in this part of main if-statement
      // time_msec = 1: wait for end of byte
      // time_msec > 1: asap (after scheduledelay ticks)
      tx_state = 1;
      uint16_t t_delta = scheduledelay;
      CONFIG_MATCH_CLEAR();
      if (time_msec == 1) {
        // determine if we still have to wait for the end of currently received byte?
        uint16_t ticks_wait = ticks_per_byte_minus_ms - TCNT2 * prescaler_ratio_timer2_timer1;
        if (ticks_wait > scheduledelay) {
          // and if so, start writing immediately after that
          t_delta = ticks_wait; 
        }
      }
      SET_COMPARE_A(GET_TIMER_COUNT() + t_delta);
      // in scheduledelay ticks (or perhaps more if time_msec=1), start bit will follow.
      // this will trigger an interrupt for next action to be set up
      // state=1 indicates that (upon start of the interrupt routine) this start bit has just started
      ENABLE_INT_COMPARE_A();
    }
    // next byte will be written without delay
    tx_setdelay = 0;
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
  // state indicates in which part of data pattern once we enter this ISR
  // 1,2 startbit
  // 3,4 .. 17,18 data bits
  // 19,20 parity bit
  // 21 stopbit
  // 23 after stopbit
  // (can't happen here) 99 pausing, pausing until we can schedule next start bit falling edge
  // (can't happen here) 0, currently not writing
  while (state < 21) {
    // calculate next (semi)bit value
    if (state & 1) {
      // state is odd, next semibit will be part 2 of bit (high)
      bit = 1;
    } else if (state < 18) {
      // state=even, <18, next semibit will be data bit part 1, LSB first
      bit = (byte & 1);
      tx_paritybit ^= bit;
      byte >>= 1;
    } else if (state == 18) {
      // state=18, next semibit will be parity bit part 1
      bit = tx_paritybit;
    } else {
      // state=20, next semibit will be stop bit part 1 (high / silence)
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
  if (state == 21) {
    // ready writing byte, enable read-back verification here. If we are here, it is
    //   -not earlier than half-way first data bit (at least one of the data bits and the parity bit is 0)
    //   -not later than half-way parity bit (if parity bit is 0)
    // thus we are in time, but also not too early, to write rx_byte_verify for verifying read-back byte (for verification and collision-detection)
    // as read-back verification occurs during next start bit
    rx_byte_verify = tx_byte_verify;
    rx_do_verify = 1;
  }
  head = tx_buffer_head;
  tail = tx_buffer_tail;
  if (head == tail) {
    // tx buffer empty, there are no more bytes to send...
    if (state == 21) {
      // state==21, nothing to do, stop bit is already silent, but wait for end of it before finishing up in state 23
      // state 21 takes 1 bit length, in contrast to earlier states which take a semibit length
      tx_state = 23;
      SET_COMPARE_A(target+ticks_per_bit);
      // CONFIG_MATCH_SET(); // does nothing - should be set high already
    } else {
      // if in state==23 and buffer still empty, quit transmission mode
      tx_state = 0;
      // CONFIG_MATCH_NORMAL(); // not sure this is needed in between transmissions
      DISABLE_INT_COMPARE_A();
    }
  } else {
    // there are more bytes to send; 
    if (++tail >= TX_BUFFER_SIZE) tail = 0;
    tx_buffer_tail = tail;
    tx_byte = tx_buffer[tail];
    tx_byte_verify = tx_byte;
    delay = tx_buffer_delay[tail];
    tx_bit = 0;
    tx_paritybit = 0;
    if (delay > 1) { // if delay=1, we effectively don't wait and we continue writing!
      // it does not matter whether we are still in stop bit or beyond, we have to wait longer due to the delay setting
      // this relies on the fact that xx has just been reset, by reading back falling start bit edge of byte just sent
      tx_state = 99;
      tx_wait = delay;
      CONFIG_MATCH_NORMAL(); // not sure whether we should need to do this in between transmissions
      DISABLE_INT_COMPARE_A();
    } else {
      if (state == 21) {
        // as we are in (silent, high) stop bit time, we set target time at end of stop bit (= next start bit)
        CONFIG_MATCH_CLEAR();
        SET_COMPARE_A(target+ticks_per_bit);
      } else {
        // state==23: we are beyond stop bit and just received a new byte to be sent but too late to remain in sync
        // we need to re-schedule new transmission
        CONFIG_MATCH_CLEAR();
        SET_COMPARE_A(GET_TIMER_COUNT() + scheduledelay);
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

//#define SUPPRESS_OSCILLATION
//#define SUPPRESS_PERIOD 20
//Use this to ignore edges that are very near previous edges to avoid detection of oscillating edges
//Does not seem necessary

static uint8_t Echo = 1;

void P1P2Serial::setEcho(uint8_t b)
// Set echo mode (verify and read back written data) on or off
// off: no read-back, no verification or bus collission detection
// on (default): each written byte will be read back and received, collission will be detected
// calling Echo has immediate effect
{
  Echo = b;
}

#ifdef SUPPRESS_OSCILLATION
static uint16_t prev_edge_capture;  // previous capture of edge
#endif
static uint16_t startbit_delta;

ISR(CAPTURE_INTERRUPT)
{
// called upon each falling edge detected
// state = 99: at falling edge of start pulse shortly after previous byte (so: store and verify previously received byte)
// state = 0: at falling edge of start pulse, after pause
// state = 1..9: first possible data bit that this edge could belong to
  uint8_t state, head;
  uint16_t capture, target;
  uint16_t offset_overflow;
  uint16_t startbit_delta_prev;

  capture = GET_INPUT_CAPTURE();
#ifdef SUPPRESS_OSCILLATION
  if (capture - prev_edge_capture < SUPPRESS_PERIOD) return; // to suppress oscillations
  prev_edge_capture = capture;
#endif
  state = rx_state;
  if ((state == 99) || (state == 0)) {
    // this is first falling edge, it must be start pulse
    startbit_delta_prev = startbit_delta;
    startbit_delta = time_msec;
    GTCCR = 1; // reset prescaler to improve timing accuracy of msec-timer
    TCNT2 = 0; // start new milli-second timer measurement on falling edge of start bit
    time_msec = 0; // set msec counter back to 0
                   // For HBS conformant timing, we could consider to change this to
                   // time_msec = -2; (change type to signed, change 0xFFFF in ISR to 0x7FFFF)
                   // TCNT2 = <value such that time_msec becomes 0 at end of stop bit>
                   // however for P1P2 the current code works fine
    // start timer for end of byte (at middle of parity bit)
    SET_COMPARE_B(capture + ticks_until_mid_paritybit);
    ENABLE_INT_COMPARE_B();
    // rx_target set to middle of first data bit
    rx_target = capture + ticks_per_bit + ticks_per_semibit;
    rx_state = 1;
    rx_paritycheck = 0;
    if (state == 99) {
      // state==99: we detected a falling edge of start pulse, just after a previous byte was sent or received
      if (Echo || !rx_do_verify) { // if !rx_do_verify, we were receiving (and not reading back our own sent data)
        // store previously received byte
        head = rx_buffer_head + 1;
        if (head >= RX_BUFFER_SIZE) head = 0;
        if (head != rx_buffer_tail) {
          rx_buffer[head] = rx_byte;
          delta_buffer[head] = startbit_delta_prev; // time from previous byte
          error_buffer[head] = 0;
          if (rx_paritycheck) error_buffer[head] |= ERROR_PE; // signals parity error
          if (rx_do_verify) {
            // If in transmission mode/verification mode,
            // check if received byte matches sent byte
            // If not, this is likely caused by a bus collision
            if (rx_byte_verify != rx_byte) { error_buffer[head] |= ERROR_READBACK; digitalWrite(LED_BUILTIN, HIGH); }
          }
          rx_buffer_head = head;
        } else {
          // signal buffer overrun for *previous* byte
          error_buffer[rx_buffer_head] |= ERROR_OVERRUN; 
          digitalWrite(LED_BUILTIN, HIGH);
        }
      }
      rx_do_verify=0;
    }
  } else {
    // state =1..9, we just received a falling edge for a data bit or parity bit
    // check how many high/silent data bits we "missed"
    target = rx_target;
    offset_overflow = 65535 - ticks_per_bit;
    while ( (capture - target) < offset_overflow) {
      rx_byte = (rx_byte >> 1) + 0x80;
      rx_paritycheck = rx_paritycheck ^ 0x80;
      target += ticks_per_bit;    // increase target time by 1 bit time
      state++;
    }
    // we received a (data or parity) bit 0, thus no need to modify rx_paritycheck
    // state=1..8: we received a data bit 0; perform a shift
    // state=9: parity bit 0; nothing to be done
    if (state < 9) rx_byte >>= 1;
    target += ticks_per_bit; // set target time to (one semibit after) next possible falling edge
    state++;
    // state now corresponds to the number of (start+data+parity) bits received so far
    // when all bits are processed, there is no need to terminate via COMPARE mechanism any more
    // so we could finish up here, but we don't do that because we prefer to terminate via the COMPARE mechanism
    // to finish up as this allows us to determine whether we are still in reading mode, and to verify any read-back data.
    rx_target = target;
    rx_state = state;
  }
  // rx_state now corresponds to the number of (start+data+parity) bit (falling/missing) edges observed
}

ISR(COMPARE_B_INTERRUPT)
// The COMPARE_B_INTERRUPT routine is always called during the stop bit (with state=2..10)
// It is also called during the potential location of the next start bit (with state==99), but only if no start bit has been detected,
// this allows detection of an end of communication block.
{
  uint8_t head, state;
  uint16_t target;

  state = rx_state;
  target = rx_target;
  if (state == 99) {
    // no new start bit detected within expected time frame; thus pause in received data detected; quit rx mode
    DISABLE_INT_COMPARE_B();
    rx_state = 0;
    if (Echo || !rx_do_verify) { // if !rx_do_verify, we were receiving (and not reading back our own sent data)
      // store previously received byte
      head = rx_buffer_head + 1;
      if (head >= RX_BUFFER_SIZE) head = 0;
      if (head != rx_buffer_tail) {
        rx_buffer[head] = rx_byte;
        delta_buffer[head] = startbit_delta; // time from previous byte
        error_buffer[head] = SIGNAL_EOB;
        if (rx_paritycheck) error_buffer[head] |= ERROR_PE; // signals parity error
        if (rx_do_verify) {
          // If in transmission mode/verification mode,
          // check if received byte matches sent byte
          // If not, this is likely caused by a bus collision
          if (rx_byte_verify != rx_byte) { error_buffer[head] |= ERROR_READBACK; digitalWrite(LED_BUILTIN, HIGH); }
        }
        rx_buffer_head = head;
      } else {
        // signal buffer overrun for *previous* byte
        error_buffer[rx_buffer_head] |= ERROR_OVERRUN; 
        digitalWrite(LED_BUILTIN, HIGH);
      }
    }
    rx_do_verify=0;
    return;
  }
  // if parity bit was 0, state will be 10, otherwise state will be lower and we need to process the 1s
  while (state < 9) {
    rx_byte = (rx_byte >> 1) | 0x80;
    rx_paritycheck = rx_paritycheck ^ 0x80;
    target += ticks_per_bit;    // increase target time by 1 bit time
    state++;
  }
  if (state == 9) {
    rx_paritycheck = rx_paritycheck ^ 0x80;
    target += ticks_per_bit;    // increase target time by 1 bit time
    //state++;     // state is not used any more
  }
  digitalWrite(LED_BUILTIN, rx_paritycheck ? HIGH : LOW);
  rx_state = 99;       // state=99: wait for next start bit's falling edge
  // rx_target = target;      // not used any more
  SET_COMPARE_B(target + ticks_per_bit);
}

uint16_t P1P2Serial::read_delta(void)
// should only be called if available()==1; otherwise, returns 0
{
  uint8_t head, tail;
  uint16_t out;

  head = rx_buffer_head;
  tail = rx_buffer_tail;
  if (head == tail) return 0;
  if (++tail >= RX_BUFFER_SIZE) tail = 0;
  out = delta_buffer[tail];
  return out;
}

uint8_t P1P2Serial::read_error(void)
// should only be called if available()==1; otherwise, returns 0
{
  uint8_t head, tail;
  uint16_t out;

  head = rx_buffer_head;
  tail = rx_buffer_tail;
  if (head == tail) return 0;
  if (++tail >= RX_BUFFER_SIZE) tail = 0;
  out = error_buffer[tail];
  return out;
}

uint8_t  P1P2Serial::read(void)
// should only be called if available()==1; otherwise, returns 0
{
  uint8_t head, tail, out;

  head = rx_buffer_head;
  tail = rx_buffer_tail;
  if (head == tail) return 0;
  if (++tail >= RX_BUFFER_SIZE) tail = 0;
  out = rx_buffer[tail];
  rx_buffer_tail = tail;
  return out;
}

bool P1P2Serial::available(void)
{
  uint8_t head, tail;

  head = rx_buffer_head;
  tail = rx_buffer_tail;
  if (head >= tail) return head - tail;
  return RX_BUFFER_SIZE + head - tail;
}

bool P1P2Serial::packetavailable(void)
{
  uint8_t head, tail;

  head = rx_buffer_head;
  tail = rx_buffer_tail;
  while (1) {
    if (head == tail) return 0;
    if (++tail >= RX_BUFFER_SIZE) tail = 0;
    if (error_buffer[tail] & SIGNAL_EOB) return 1;
  }
}

void P1P2Serial::flushInput(void)
{
  rx_buffer_head = rx_buffer_tail;
}

uint16_t P1P2Serial::readpacket(uint8_t* readbuf, uint16_t &delta, uint8_t* errorbuf, uint8_t maxlen, uint8_t crc_gen, uint8_t crc_feed)
{
// Reads one packet (in blocking mode)
// To avoid blocking, only call this function if packetavailable()
// stores maximum of maxlen bytes of read data into readbuf
// returns total #bytes received (until v0.9.3: #bytes stored, as of v0.9.4: #bytes received, even if not stored),
//    if (packet size/return value > maxlen) some received bytes were not stored, some error codes may have been missed
// reading continues in case of error
// stores maximum of maxlen bytes of error codes into errorbuf (unless errorbuf = NULL),
// returns timing information (pause on bus before this package) in parameter delta
// If crc_gen is not zero, verifies last byte as CRC byte; CRC byte is also stored and is counted in return value
  uint8_t EOB = 0;
  uint8_t bytecnt = 0;
  uint8_t crc = crc_feed;

  while (!EOB) {
    if (available()) {
      uint8_t error = read_error();
      EOB = (error & SIGNAL_EOB);
      if (errorbuf) errorbuf[bytecnt] = (error & ERROR_FLAGS);
      if (!bytecnt) delta = read_delta();
      uint8_t c = read();
      if ((EOB == 0) || (crc_gen == 0)) {
        if (bytecnt < maxlen) {
          readbuf[bytecnt] = c;
        }
        if (crc_gen != 0) for (uint8_t i = 0; i < 8; i++) {
          crc = (((crc ^ c) & 0x01) ? ((crc >> 1) ^ crc_gen) : (crc >> 1));
          c >>= 1;
        }
      } else {
        // check crc
        if (bytecnt < maxlen) {
          readbuf[bytecnt] = c;
          if (c != crc) { errorbuf[bytecnt] |= ERROR_CRC; digitalWrite(LED_BUILTIN, HIGH); }
        }
      }
      bytecnt++;
    }
  }
  return bytecnt;
}

void P1P2Serial::writepacket(uint8_t* writebuf, uint8_t l, uint16_t t, uint8_t crc_gen, uint8_t crc_feed)
{
// Writes one packet of l bytes, t ms after last bus action; 
// If crc_gen is not zero, adds CRC byte to packet
// Note that t=0 or t=1 increases risk of bus collisions, don't use it if not needed.
  setDelay(t);
  uint8_t crc=crc_feed;
  for (uint8_t i = 0; i < l; i++) {
    uint8_t c = writebuf[i];
    write(c);
    if (crc_gen != 0) for (uint8_t i = 0; i < 8; i++) {
      crc = ((crc ^ c) & 0x01 ? ((crc >> 1) ^ crc_gen) : (crc >> 1));
      c>>=1;
    }
  }
  if (crc_gen) write(crc);
}
