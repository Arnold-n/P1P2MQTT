/* P1P2MQTT: Library for reading/writing Daikin/Rotex P1P2 protocol
 *
 * Copyright (c) 2019-2024 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 * 20240512 v0.9.46 Mitsubishi Heavy Industries (MHI) with increased TX_BUFFER_SIZE/RX_BUFFER_SIZE and data-conversion, error mask
 * 20230604 v0.9.38 H-link branch merged into main branch
 * 20230604 v0.9.37 Support for V1.2 hardware
 * 20221028 v0.9.23 ADC code
 * 20220918 v0.9.22 scopemode also for writes, focused on actual errors, fake error generation for test purposes, removing OLDP1P2LIB
 * 20220830 v0.9.18 version alignment with example programs (last version supporting OLDP1P2LIB)
 * 20220817 v0.9.17 read-back-verification bug fixes in new and old library, scopemode
 * 20220811 v0.9.16 Added S_TIMER switch making TIMER0 use optional
 * 20220808 v0.9.15 LEDs on P1P2-ESP-Interface all on until first byte received
 * 20220802 v0.9.14 major rewrite of send and receive method to spread CPU load over time and allow 8MHz ATmega operation
 *                  (until v0.9.18, old library version is still available as fall-back solution (#define OLDP1P2LIB below))
 * 20220511 v0.9.12 various minor bug fixes
 * 20200109 v0.9.11 allow short pauses between bytes within a packet (for KLIC-DA device, to avoid detecting each byte as individual packet)
 * 20190914 v0.9.10 upon bus collision detection, write buffer is emptied
 * 20190914 v0.9.9 Added writeready()
 * 20190908 v0.9.8 Removed EOP signal in errorbuf results returned by readpacket(); as of now errorbuf contains only real error flags
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
 * The CC BY-NC-ND 4.0 licensed P1P2MQTT library is based on the MIT-licensed AltSoftSerial,
 *      but please note that P1P2MQTT itself is licensed under the CC BY-NC-ND 4.0.
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

// TODO: fix LED code for Arduino targets

#include "P1P2MQTT.h"

// New library version 0.9.14 rewritten for quick and more predictable interrupt handling.
// The new library runs on an 8MHz ATmega328P(B) (tested) and maybe also on an ATmega2560 (not tested)
// This library uses
// 16-bit timer1 (or timer5 on ATmega2560) (no prescaling, no reset, free running full scale)
// 8-bit timer2 (1kHz for ms timer, prescaled depending on F_CPU, CTC mode, and reset)
// 8-bit timer0 (125Hz for s timer, prescaled depending on F_CPU, CTC mode, and reset)
// for timing P1/P2 (semi-)bits (9600 Baud)
//                        16MHz    8MHz
// CPU cycles_per_bit      1667     833
// related interrupts:
//              ICP    PB0

#ifdef __AVR_ATmega2560__

#error ATmega2560 code has not been tested, use with caution.

// RW using timer5
#define INPUT_CAPTURE_PIN               48 // PL1
#define INPUT_CAPTURE_PIN_VALUE         (PINL & 0x02) // PL1
#define CONFIG_RW_TIMER()               (TIMSK5 = 0, TCCR5A = 0, TCCR5B = (1 << ICNC5) | (1 << CS50))   // noise canceler, no prescaler
#define CONFIG_CAPTURE_FALLING_EDGE()   (TCCR5B &= ~(1 << ICES5))
#define CONFIG_CAPTURE_RISING_EDGE()    (TCCR5B |= (1 << ICES5))
#define ENABLE_INT_INPUT_CAPTURE()      (TIFR5 = (1 << ICF5), TIMSK5 |= (1 << ICIE5))
#define DISABLE_INT_INPUT_CAPTURE()     (TIMSK5 &= ~(1 << ICIE5))
#define RESET_INPUT_CAPTURE()           (TIFR5 = (1 << ICF5))
#define INPUT_CAPTURED()                (TIFR5 & (1 << ICF5))
#define GET_INPUT_CAPTURE()             (ICR5)
#define GET_TIMER_R_COUNT()             (TCNT5)
#define ENABLE_INT_COMPARE_R()          (TIFR5 = (1 << OCF5B), TIMSK5 |= (1 << OCIE5B))
#define DISABLE_INT_COMPARE_R()         (TIMSK5 &= ~(1 << OCIE5B))
#define CLEAR_COMPARE_R_FLAG()          (TIFR5 = (1 << OCF5B))
#define SET_COMPARE_R(val)              (OCR5B = (val))
#define CAPTURE_INTERRUPT               TIMER5_CAPT_vect
#define COMPARE_R_INTERRUPT             TIMER5_COMPB_vect

#define OUTPUT_COMPARE_PIN              46 // PL3
#define CONFIG_MATCH_INIT()             (CONFIG_MATCH_SET(), TCCR5C |= (1 << FOC5A))
#define CONFIG_MATCH_NORMAL()           (TCCR5A = TCCR5A & ~((1 << COM5A1) | (1 << COM5A0)))
#define CONFIG_MATCH_CLEAR()            (TCCR5A = (TCCR5A | (1 << COM5A1)) & ~(1 << COM5A0))
#define CONFIG_MATCH_SET()              (TCCR5A = TCCR5A | ((1 << COM5A1) | (1 << COM5A0)))
#define ENABLE_INT_COMPARE_W()          (TIFR5 |= (1 << OCF5A), TIMSK5 |= (1 << OCIE5A))
#define ENABLE_INT_COMPARE_W()          (TIFR5 |= (1 << OCF5B), TIMSK5 |= (1 << OCIE5B))
#define DISABLE_INT_COMPARE_W()         (TIMSK5 &= ~(1 << OCIE5A))
#define GET_COMPARE_W()                 (OCR5A)
#define GET_COMPARE_R()                 (OCR5B)
#define GET_TIMER_W_COUNT()             (TCNT5)
#define SET_COMPARE_W(val)              (OCR5A = (val))
#define COMPARE_W_INTERRUPT             TIMER5_COMPA_vect

// use LED_BUILTIN on PB7 on ATmega2560
#define LED_ERROR                       LED_BUILTIN
#define DIGITAL_RESET_LED_ERROR         (PORTB &= 0x7F)
#define DIGITAL_WRITE_LED_ERROR(val)    ((val) ? (PORTB |= 0x80) : (PORTB &= 0x7F))
#define DIGITAL_SET_LED_ERROR           (PORTB |= 0x80)

#elif ((defined __AVR_ATmega328P__) || (defined __AVR_ATmega328PB__))

// RW using timer1
#define INPUT_CAPTURE_PIN               8 // PB0
#define INPUT_CAPTURE_PIN_VALUE         (PINB & 0x01) // PB0
#define CONFIG_RW_TIMER()               (TIMSK1 = 0, TCCR1A = 0, TCCR1B = (1 << ICNC1) | (1 << CS10))   // noise canceler, no prescaler
#define CONFIG_CAPTURE_FALLING_EDGE()   (TCCR1B &= ~(1 << ICES1))
#define CONFIG_CAPTURE_RISING_EDGE()    (TCCR1B |= (1 << ICES1))
#define ENABLE_INT_INPUT_CAPTURE()      (TIFR1 = (1 << ICF1), TIMSK1 |= (1 << ICIE1))
#define DISABLE_INT_INPUT_CAPTURE()     (TIMSK1 &= ~(1 << ICIE1))
#define RESET_INPUT_CAPTURE()           (TIFR1 = (1 << ICF1))
#define INPUT_CAPTURED()                (TIFR1 & (1 << ICF1))
#define GET_INPUT_CAPTURE()             (ICR1)
#define GET_TIMER_R_COUNT()             (TCNT1)
#define ENABLE_INT_COMPARE_R()          (TIFR1 = (1 << OCF1B), TIMSK1 |= (1 << OCIE1B))
#define DISABLE_INT_COMPARE_R()         (TIMSK1 &= ~(1 << OCIE1B))
#define CLEAR_COMPARE_R_FLAG()          (TIFR1 = (1 << OCF1B))
#define SET_COMPARE_R(val)              (OCR1B = (val))
#define CAPTURE_INTERRUPT               TIMER1_CAPT_vect
#define COMPARE_R_INTERRUPT             TIMER1_COMPB_vect


#define OUTPUT_COMPARE_PIN              9 // PB1
#define CONFIG_MATCH_INIT()             (CONFIG_MATCH_SET(), TCCR1C |= (1 << FOC1A))
#define CONFIG_MATCH_NORMAL()           (TCCR1A = TCCR1A & ~((1 << COM1A1) | (1 << COM1A0)))
#define CONFIG_MATCH_CLEAR()            (TCCR1A = (TCCR1A | (1 << COM1A1)) & ~(1 << COM1A0))
#define CONFIG_MATCH_SET()              (TCCR1A = TCCR1A | ((1 << COM1A1) | (1 << COM1A0)))
#define ENABLE_INT_COMPARE_W()          (TIFR1 |= (1 << OCF1A), TIMSK1 |= (1 << OCIE1A))
#define DISABLE_INT_COMPARE_W()         (TIMSK1 &= ~(1 << OCIE1A))
#define GET_COMPARE_W()                 (OCR1A)
#define GET_COMPARE_R()                 (OCR1B)
#define GET_TIMER_W_COUNT()             (TCNT1)
#define SET_COMPARE_W(val)              (OCR1A = (val))
#define COMPARE_W_INTERRUPT             TIMER1_COMPA_vect

// use LED_BUILTIN (on PB5) on Arduino Uno
#define LED_ERROR                       LED_BUILTIN
#define DIGITAL_WRITE_LED_ERROR(val)    ((val) ? (PORTB |= 0x20) : (PORTB &= 0xDF)) // assume PB5 on Arduino Uno
#define DIGITAL_SET_LED_ERROR           (PORTB |= 0x20)
#define DIGITAL_RESET_LED_ERROR         (PORTB &= 0xDF)

#else /* __AVR_ATmega2560__ */
#error Only ATmega328P or ATmega2560 supported
#endif /* __AVR_ATmega2560__ */

#if F_CPU <= 8000000L
// Assume we are on P1P2-ESP-interface with LED_ERROR on PD3, overrule earlier defines
#define LED_ERROR PD3
#define DIGITAL_SET_LED_ERROR           (PORTD |= 0x08)
#define DIGITAL_RESET_LED_ERROR         (PORTD &= 0xF7)
#define DIGITAL_WRITE_LED_ERROR(val)    ((val) ? (PORTD |= 0x08) : (PORTD &= 0xF7))
#endif /* F_CPU */

// 4 leds:        on   P1P2-ESP-interface   /   Arduino ATmega250     / Arduino Uno
// LED_POWER (white) on     PC2             /         pin 35          /    pin A2
// LED_READ  (green) on     PD6             /          n/a            /    pin  6
// LED_WRITE (blue) on      PD5 (*)         /          n/a            /    pin  5
// LED_ERROR (red) on       PD3 (*)         /      LED_BUILTIN        /  LED_BUILTIN
// (*) in v1.2 LEDs inverted logic: to 3V3 instead of to GND

#define LED_POWER PC2
#define LED_READ PD6
#define LED_WRITE PD5
#define DIGITAL_SET_LED_POWER           (PORTC |= 0x04)
#define DIGITAL_RESET_LED_POWER         (PORTC &= 0xFB)
#define DIGITAL_WRITE_LED_POWER(val)    ((val) ? (DIGITAL_SET_LED_POWER) : (DIGITAL_RESET_LED_POWER))

#define DIGITAL_WRITE_LED_ERROR(val)    ((val) ? (DIGITAL_SET_LED_ERROR) : (DIGITAL_RESET_LED_ERROR))

// v1.2 uses LEDs to 3V3 instead of to GND for R,W
#define DIGITAL_SET_LED_READ            { if (_ledRW_reverse) { PORTD &= 0xBF;} else { PORTD |= 0x40;}}
#define DIGITAL_RESET_LED_READ          { if (_ledRW_reverse) { PORTD |= 0x40;} else { PORTD &= 0xBF;}}
#define DIGITAL_SET_LED_WRITE           { if (_ledRW_reverse) { PORTD &= 0xDF;} else { PORTD |= 0x20;}}
#define DIGITAL_RESET_LED_WRITE         { if (_ledRW_reverse) { PORTD |= 0x20;} else { PORTD &= 0xDF;}}

// timer2 for milliseconds, timer0 for seconds
#if F_CPU == 16000000L
#define CONFIG_MS_TIMER()               (TCCR2A = 2, TCCR2B = 4, OCR2A = 249)  // CTC mode, wraps 16MHz/(64*250)=1kHz
#define CONFIG_S_TIMER()                (TCCR0A = 2, TCCR0B = 5, OCR0A = 124)  // CTC mode, wraps 16MHz/(1024*125)=125Hz
#elif F_CPU == 8000000L
#define CONFIG_MS_TIMER()               (TCCR2A = 2, TCCR2B = 3, OCR2A = 249)  // CTC mode, wraps  8MHz/(32*250)=1kHz
#define CONFIG_S_TIMER()                (TCCR0A = 2, TCCR0B = 4, OCR0A = 249)  // CTC mode, wraps  8MHz/(256*250)=125Hz
#else /* F_CPU */
#error F_CPU not supported
#endif /* F_CPU */
#define RESET_MS_TIMER()                (GTCCR = 2, SET_MS_TIMER(0), time_msec = 0)
#define PRESET_MS_TIMER()               (GTCCR = 2, SET_MS_TIMER(0), time_msec = 1)
#define RESET_ENABLE_MS_TIMER()         (RESET_MS_TIMER(), TIFR2 = (1 << OCF2A), TIMSK2 = (1 << OCIE2A))
#define PRESET_ENABLE_MS_TIMER()        (PRESET_MS_TIMER(), TIFR2 = (1 << OCF2A), TIMSK2 = (1 << OCIE2A))
#define DISABLE_MS_TIMER()              (TIMSK2 = 0)
#define SET_MS_TIMER(val)               (TCNT2 = (val))
#define RESET_ENABLE_S_TIMER()          (time_sec = 0, time_millisec = 0, TCNT0 = 0, GTCCR = 1, TIFR0 = (1 << OCF0A), TIMSK0 = (1 << OCIE0A))
#define DISABLE_S_TIMER()               (TIMSK0 = 0)
#define MS_TIMER_COMP_vect              TIMER2_COMPA_vect
#define S_TIMER_COMP_vect               TIMER0_COMPA_vect

/****************************************/
/**        CPU load measurement        **/
/****************************************/

#ifdef MEASURE_LOAD

// CLOCK msg 32 bytes ca 33ms 33000 us so irq_time is 16-bit counter 1us resolution, irq_lapsed is also 1us resolution
// TIMER1 runs at 8 MHZ / at 64 kB overflows every 8ms.
// if ISR does not take longer than 64k/8 = 8us,

volatile uint16_t irq_start_time = 0, irq_start = 0, irq_w = 0, irq_r = 0, irq_time = 0, irq_lapsed_w = 0, irq_lapsed_r = 0;
volatile uint8_t irq_ovf = 0, irq_busy = 0;
// assume 8MHz: TODO for 16MHz, use >> 4
//
#define IRQ_START { irq_start_time = GET_TIMER_W_COUNT(); }
#define IRQ_STOP  { irq_time += (GET_TIMER_W_COUNT() - irq_start_time) >> 3; }
#define IRQ_BEGIN { irq_time = 0; irq_start = GET_TIMER_W_COUNT(); irq_ovf = 0; TIFR1 = (1 << TOV1); irq_busy = 1; };
#define IRQ_END_R { irq_r = irq_time;  irq_w = 0; irq_lapsed_r = ((GET_TIMER_W_COUNT() - irq_start) >> 3) + ((irq_ovf + ((TIFR1 & (1 << TOV1)) ? 1 : 0)) << 13); irq_busy = 0; };
#define IRQ_END_W { irq_w = irq_time;  irq_r = 0; irq_lapsed_w = ((GET_TIMER_W_COUNT() - irq_start) >> 3) + ((irq_ovf + ((TIFR1 & (1 << TOV1)) ? 1 : 0)) << 13); irq_busy = 0; };

// (share range range = 0 .. 255, perhaps even 256?)

#define OVF_vect                        TIMER1_OVF_vect
#define ENABLE_OVF                      (TIFR1 = (1 << TOV1), TIMSK1 |= (1 << TOIE1))


#else /* MEASURE_LOAD */

#define OVF_vect   TIMER1_OVF_vect
#define ENABLE_OVF ;
#define IRQ_START  ;
#define IRQ_STOP   ;
#define IRQ_BEGIN  ;
#define IRQ_END_R  ;
#define IRQ_END_W  ;

#endif /* MEASURE_LOAD */

/************************/
/**  ADC for voltages  **/
/************************/

static bool _use_ADC = false;
static bool _ledRW_reverse = false;
static byte ADMUX0 = 0;
static byte ADMUX1 = 0;
static uint16_t V0min = 0x3FF << ADC_AVG_SHIFT;
static uint16_t V0max = 0x0000;
static uint32_t V0avg = 0x00;
static uint16_t V1min = 0x3FF << ADC_AVG_SHIFT;
static uint16_t V1max = 0x0000;
static uint32_t V1avg = 0x00;

static uint16_t V0cnt = 0;
static uint16_t V1cnt = 0;
static uint32_t V0sum0 = 0x00;
static uint32_t V0sum = 0x00;
static uint32_t V1sum0 = 0x00;
static uint32_t V1sum = 0x00;

// hard-coded for 8 MHz ATmega328P

#define ADC_TRIGGER                     (ADCSRA = 0xDE)  // set ADSC to 1
#define ADC_INTERRUPT                   ADC_vect
#define ADC_VALUE                       (ADCL + (ADCH << 8))
#define ADC_ADC0                        (ADMUX = ADMUX0)
#define ADC_ADC1                        (ADMUX = ADMUX1)
#define ADC_INT_DISABLE                 (ADCSRA = 0x86)
#define ADC_INT_ENABLE                  (ADCSRA = 0x8E)

ISR(ADC_vect) {
  static bool ADC0used = true;
  uint16_t V = ADC_VALUE;
  if (ADC0used) {
    ADC0used = false;
    ADC_ADC1;
    V0cnt ++;
    V0sum0 += V;
    if (!(V0cnt << (16 - ADC_AVG_SHIFT))) { // sum (avg) a few samples before min/max check
      V0sum += V0sum0;
      if (V0sum0 < V0min) V0min = V0sum0;
      if (V0sum0 > V0max) V0max = V0sum0;
      V0sum0 = 0;
      if (!(V0cnt << ADC_CNT_SHIFT)) {
        // sum 4k samples for average calculation approximately every second
        V0avg = V0sum;
        V0sum = 0;
      }
    }
  } else {
    ADC0used = true;
    ADC_ADC0;
    V1cnt ++;
    V1sum0 += V;
    if (!(V1cnt << (16 - ADC_AVG_SHIFT))) { // sum samples (16 samples if ADC_AVG_SHIFT1 == 4)
      V1sum += V1sum0;
      if (V1sum0 < V1min) V1min = V1sum0;
      if (V1sum0 > V1max) V1max = V1sum0;
      V1sum0 = 0;
      if (!(V1cnt << ADC_CNT_SHIFT)) {
        V1avg = V1sum;
        V1sum = 0;
      }
    }
  }
  ADC_TRIGGER;
}

void P1P2MQTT::ADC_results(uint16_t &V0_min, uint16_t &V0_max, uint32_t &V0_avg,
                             uint16_t &V1_min, uint16_t &V1_max, uint32_t &V1_avg) {
// if use_ADC is true in P1P2MQTT::begin,
// ADC measurements are done alternating on pins ADC_pin0 and ADC_pin1
// at 8MHz the average measured voltage is updated approximately every second
// V0_avg and V1_avg are summations of 2^(16 - ADC_CNT_SHIFT)) samples (default: 4k)
// V0_min/max and V1_min/max are sample summations over 2^ADC_AVG_SHIFT samples (default: 16)
// the mimimum and maximum values are reset upon each call of ADC_results
  if (_use_ADC) {
    ADC_INT_DISABLE;
    V0_min = V0min;
    V0_max = V0max;
    V0_avg = V0avg;
    V1_min = V1min;
    V1_max = V1max;
    V1_avg = V1avg;
    V0min = 0x3FF << ADC_AVG_SHIFT;
    V0max = 0x000;
    V1min = 0x3FF << ADC_AVG_SHIFT;
    V1max = 0x000;
    ADC_INT_ENABLE;
  }
}

/****************************************/
/**          LED routines (v1.2)       **/
/****************************************/

void P1P2MQTT::ledPower(bool ledOn) {
  DIGITAL_WRITE_LED_POWER(ledOn);
}

void P1P2MQTT::ledError(bool ledOn) {
  DIGITAL_WRITE_LED_ERROR(ledOn);
}

/****************************************/
/**          Initialization            **/
/****************************************/

static uint16_t Rticks_per_bit = 0;
static uint16_t Rticks_per_semibit = 0;
static uint16_t Rticks_per_bit_and_semibit = 0;
static uint16_t Rticks_suppression = 0;
static uint16_t Wticks_per_semibit = 0;
static uint16_t Wticks_per_bit_and_semibit = 0;

static uint8_t rx_state;
static uint8_t rx_byte;
static uint8_t rx_paritycheck;
static uint16_t rx_target;
static volatile uint8_t rx_buffer_head;
static volatile uint8_t rx_buffer_head2;
static volatile uint8_t rx_buffer_tail;
static volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile errorbuf_t error_buffer[RX_BUFFER_SIZE]; // records error status
static volatile uint16_t delta_buffer[RX_BUFFER_SIZE]; // records timing info in ms

static volatile uint8_t tx_state;
static volatile uint8_t tx_rx_state;
static uint8_t tx_byte;
static uint8_t tx_bit;
static uint8_t tx_byte_verify;
static uint8_t tx_paritybit;
static volatile uint8_t tx_buffer_head;
static volatile uint8_t tx_buffer_tail;
static volatile uint8_t tx_buffer[TX_BUFFER_SIZE];
static volatile uint16_t tx_buffer_delay[TX_BUFFER_SIZE]; // records timing info in ms (16 bits)
static volatile uint16_t time_msec = 0;
static volatile uint16_t tx_wait = 0;
static volatile uint8_t  time_sec_cnt = 0;
static volatile int32_t time_sec = 0;
static volatile int32_t time_millisec = 0;

#define scheduledelay Wticks_per_bit_and_semibit // should be more than 1.5 bits for writing

#ifndef INPUT_PULLUP
#define INPUT_PULLUP INPUT
#endif /* INPUT_PULLUP */

void P1P2MQTT::begin(uint32_t baud, bool use_ADC /* = false */, uint8_t ADC_pin0 /* = 0 */, uint8_t ADC_pin1 /* = 1 */, bool ledRW_reverse /* = false */)
{
  uint32_t cycles_per_bit = ((ALTSS_BASE_FREQ + baud / 2) / baud); // 833 cycles at 8MHz

  Rticks_per_bit = cycles_per_bit;
  Rticks_per_semibit = Rticks_per_bit / 2;
  Rticks_per_bit_and_semibit = Rticks_per_bit + Rticks_per_semibit;
  Rticks_suppression = Rticks_per_semibit + Rticks_per_semibit / 4; // to avoid early ISR capture (spike, bouncing rising edge) may lead to very long while loop behaviour and loss of sync.

  Wticks_per_semibit = cycles_per_bit / 2;
  Wticks_per_bit_and_semibit = 3 * Wticks_per_semibit;

  pinMode(LED_POWER, OUTPUT);
  pinMode(LED_READ, OUTPUT);
  pinMode(LED_WRITE, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);

  _ledRW_reverse = ledRW_reverse;
  DIGITAL_SET_LED_POWER;
  DIGITAL_SET_LED_READ;
  DIGITAL_SET_LED_WRITE;
  DIGITAL_SET_LED_ERROR;

  pinMode(INPUT_CAPTURE_PIN, INPUT_PULLUP);
  digitalWrite(OUTPUT_COMPARE_PIN, HIGH);
  pinMode(OUTPUT_COMPARE_PIN, OUTPUT);

  time_msec = 0;
  rx_state = 0;
  rx_buffer_head = 0;
  rx_buffer_head2 = NO_HEAD2;
  rx_buffer_tail = 0;
  tx_state = 0;
  tx_rx_state = 0;
  tx_buffer_head = 0;
  tx_buffer_tail = 0;
  tx_wait = 0;

#ifdef S_TIMER
  CONFIG_S_TIMER();
  RESET_ENABLE_S_TIMER();
#endif /* S_TIMER */

  CONFIG_MS_TIMER();
  RESET_ENABLE_MS_TIMER();

  CONFIG_RW_TIMER();
  CONFIG_CAPTURE_FALLING_EDGE();
  CONFIG_MATCH_INIT();
  // start reading mode
  ENABLE_INT_INPUT_CAPTURE();
  ENABLE_OVF;
  // start ADC measurements on pins ADC_pin0 and ADC_pin1 if use_ADC is true
  _use_ADC = use_ADC;
  if (_use_ADC) {
    ADMUX0 = 0xC0 | ADC_pin0; // 1.1V reference
    ADMUX1 = 0xC0 | ADC_pin1; // 1.1V reference
    DIDR0 = ((1 << ADC_pin0) | (1 << ADC_pin1)) & 0x3F; // disable digital input on analog pins
    ADC_ADC0;      // start with ADC_pin0
    ADCSRB = 0x00; // ACME disabled, free running mode, conversions will be triggered by ADSC
    ADCSRA = 0xDE; // enable ADC, 8MHz/64=125kHz, clear ADC interrupt flag and trigger single ADC conversion
  }
}

void P1P2MQTT::end(void)
{
  DISABLE_INT_COMPARE_R();
  DISABLE_INT_INPUT_CAPTURE();
  flushInput();
  flushOutput();
  DISABLE_INT_COMPARE_W();
  DISABLE_MS_TIMER();
#ifdef S_TIMER
  DISABLE_S_TIMER();
#endif /* S_TIMER */
}

/****************************************/
/**       Uptime counter               **/
/****************************************/

// As the ms counter (timer2) is frequently reset, it cannot be used for longer time measurements
// Use timer0 for (second-based) uptime

#ifdef S_TIMER
ISR(S_TIMER_COMP_vect)
{
  IRQ_START;
// called at 125Hz
  time_millisec += 8;
  if (++time_sec_cnt > 124) {
    time_sec_cnt = 0;
    time_sec++;
  }
  IRQ_STOP;
}
#endif /* S_TIMER */

/****************************************/
/**       Millisecond counter          **/
/****************************************/

static uint16_t tx_setdelaytimeout = 2500;
#ifdef MHI_SERIES
static volatile uint8_t mhiConvert = 1;
#endif
#ifdef SW_SCOPE
volatile byte sw_scope = 0;
volatile byte sw_scope_next = 0;
volatile byte sws_block = 0;
volatile byte sws_error = 0;
volatile byte sws_errorcount = 0;
volatile uint8_t sws_event[SWS_MAX];
volatile uint16_t sws_capture[SWS_MAX];
volatile uint16_t sws_count[SWS_MAX];
volatile uint8_t sws_cnt = 0;

#define SW_SCOPE_LOG_EVENT(capture, event)  \
    if (sw_scope && (sws_errorcount || !sws_error)) { \
      sws_capture[sws_cnt] = capture; \
      sws_event[sws_cnt] = event; \
      if (++sws_cnt == SWS_MAX) sws_cnt = 0; \
      if (sws_error) sws_errorcount--; \
    }

#define SW_SCOPE_LOG_ERROR(capture, event)  \
    if (sw_scope && (sws_errorcount || !sws_error)) { \
      if (!sws_error) { \
        sws_error = 1; \
        sws_errorcount = SWS_MAX >> 1; \
      } \
      sws_capture[sws_cnt] = capture; \
      sws_event[sws_cnt] = event; \
      if (++sws_cnt == SWS_MAX) sws_cnt = 0; \
      if (sws_error) sws_errorcount--; \
    }

#define SW_SCOPE_START_LOG { \
        sws_block = 1; \
        sws_cnt = 0; \
        sws_event[SWS_MAX - 1] = SWS_EVENT_LOOP; \
        sws_error = 0; };

#else /* SW_SCOPE */

#define SW_SCOPE_LOG_EVENT(capture, event) {};
#define SW_SCOPE_LOG_ERROR(capture, event) {};
#define SW_SCOPE_START_LOG {};

#endif /* SW_SCOPE */

#ifdef MEASURE_LOAD
ISR(OVF_vect)
{
  irq_ovf++;
}
#endif


ISR(MS_TIMER_COMP_vect)
{
  IRQ_START;
// time_msec counts time in ms from the last start pulse (counting from the leading falling edge of the start pulse)
// max count is 65535 ms (uint16_t)
  if (time_msec < 0xFFFF) time_msec++;
  // if tx_state =99, a write is scheduled, so check if pause is long enough to start writing
  if (tx_state == 99) {
    if ((time_msec == tx_wait) || ((time_msec >= tx_wait) && (time_msec >= tx_setdelaytimeout))) {
      // start writing:
      // in scheduledelay ticks of timer1, falling edge of start bit  is scheduled
      // This will trigger an interrupt for next action to be set up.
      // tx_state=1 indicates that (upon start interrupt routine) the start bit has just begun.
      tx_state = 1;
      uint16_t t_delta = scheduledelay;

      // switch from reading to writing
      // disable reading when writing
      // in scopemode, capture (both) edges
      // if P1P2Monitor is still processing sws_event data (sws_block), do not start writing new events
#ifdef SW_SCOPE
      // if P1P2Monitor is ready reading data (sws_block = 0), start new log operation in write mode (if sw_scope_next)
      sw_scope = sw_scope_next && !sws_block;
      if (sw_scope) {
        SW_SCOPE_START_LOG;
        // keep INT_INPUT_CAPTURE enabled
        DISABLE_INT_INPUT_CAPTURE();
      } else {
        DISABLE_INT_INPUT_CAPTURE();
      }
#else /* SW_SCOPE */
      DISABLE_INT_INPUT_CAPTURE();
#endif /* SW_SCOPE */
      DISABLE_INT_COMPARE_R();

      DISABLE_MS_TIMER();

      // start writing
      IRQ_STOP;
      IRQ_BEGIN;
      IRQ_START;
      SET_COMPARE_W(GET_TIMER_W_COUNT() + t_delta);
      CONFIG_MATCH_CLEAR();
      CONFIG_CAPTURE_FALLING_EDGE();
      ENABLE_INT_COMPARE_W();
      DIGITAL_SET_LED_WRITE;
    }
  }
  IRQ_STOP;
}

/****************************************/
/**           Transmission             **/
/****************************************/

static uint16_t tx_setdelay = 0;

void P1P2MQTT::setDelay(uint16_t t)
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
// To simplify code, setDelay(0) and setDelay(1) are no longer supported; useless for P1/P2.
// If setDelay (t) is called with 2 <= t <= 65535, a delay of t ms is set.
// If setDelay (t) is called with t < 2, a delay of 2 ms is set.
{
  if (t < 2) t = 2;
  tx_setdelay = t;
}

void P1P2MQTT::setDelayTimeout(uint16_t t)
// Input parameter: 0 <= t <= 65535
// This sets delay timeout as described above
{
  tx_setdelaytimeout = t;
}

bool P1P2MQTT::writeready(void)
{
  return (tx_buffer_tail == tx_buffer_head);
}

uint8_t tx_rx_paritycheck;
uint8_t tx_rx_readbackerror;

void P1P2MQTT::write(uint8_t b)
#ifdef MHI_SERIES
{
  if (mhiConvert) {
    writebyte(~(1<< (b & 0x07)));
    b >>= 3;
    writebyte(~(1<< (b & 0x07)));
    b >>= 3;
    writebyte(~(1<< (b & 0x07)));
  } else {
    writebyte(b);
  }
}

void P1P2MQTT::writebyte(uint8_t b)
#endif
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
    tx_buffer_delay[head] = tx_setdelay; tx_setdelay = 0;
    tx_buffer_head = head;
  } else {
    // if not already writing, (previously: start or) schedule writing
    tx_byte = b;
    tx_byte_verify = b; // for read-back verification
    tx_paritybit = 0;
    tx_rx_paritycheck = 0;
    tx_rx_readbackerror = 0;
    // we could initiate start writing here, as we did <=v0.9.4, but we can better leave it to the ISR, which is simpler
    // it adds some delay (max 1 ms), but it makes operation and timing slightly more predictable
    // set tx_state 99 to start transmission in msec ISR when time_msec becomes == tx_setdelay or >= tx_setdelaytimeout
    tx_state = 99;
    tx_wait = tx_setdelay;
    // next byte after the scheduled one will be written without delay
    tx_setdelay = 0;
  }
  SREG = intr_state;
}

static volatile uint16_t startbit_delta;
static volatile uint8_t Echo = 1;
static volatile uint8_t Allow = ALLOW_PAUSE_BETWEEN_BYTES;
static volatile uint8_t errorMask = ERROR_MASK;

ISR(COMPARE_W_INTERRUPT)
{
  IRQ_START;
  uint8_t state, bit, bit_input, errorhead, head, tail;
  uint16_t delay;
  state = tx_state;
  // state indicates in which part of data pattern we are when entering this ISR
  // 1,2 startbit
  // 3,4 .. 17,18 data bits
  // 19,20 parity bit
  // 21,22 stopbit (21,22 skipped)
  // (removed) 23 after stopbit
  // (can't happen here) 99 pausing, pausing until we can schedule next start bit falling edge
  // (can't happen here) 0, currently not writing

  // SW_SCOPE logs states, error/event, and time of captured edge or counter value
  uint16_t sws_count_temp = GET_TIMER_R_COUNT();
  uint16_t get_compare_w = GET_COMPARE_W();
  uint16_t set_compare_w;

  DIGITAL_RESET_LED_ERROR;
  if (state < 20) {
    set_compare_w = get_compare_w + Wticks_per_semibit;
    SET_COMPARE_W(set_compare_w);
    bit_input = INPUT_CAPTURE_PIN_VALUE;
    if (state & 1) {
      // state is odd, 1-19, next semibit will be part 2 of databit (high)
      CONFIG_MATCH_SET();
      CONFIG_CAPTURE_RISING_EDGE();
      if (state == 1) {
        // state=1, start bit
        startbit_delta = time_msec;
        DISABLE_MS_TIMER();
        tx_rx_readbackerror = 0;
      } else if (state == 19) {
        // state=19, paritybit
        PRESET_ENABLE_MS_TIMER(); // start new milli-second timer measurement here, setting timer on 1msec given we are at paritybit.
      }
      // check bit input signal
      if (state == 1) {
        // state is 1, start bit part 1, should be 0
        if (bit_input) {
          tx_rx_readbackerror = ERROR_SB;
          SW_SCOPE_LOG_ERROR(sws_count_temp, SWS_EVENT_ERR_SB);
        }
      } else if (state & 1) {
        // state is odd and >1, can be bit or parity bit
        // faster to check bit value directly here than to reconstruct tx_rx_byte
        if (bit_input != tx_bit) {
          tx_rx_readbackerror |= ERROR_BE; // bit differs
          SW_SCOPE_LOG_ERROR(sws_count_temp, SWS_EVENT_ERR_BE);
        }
      }
    } else {
      // state is even, 2..18
      if (state < 18) {
        // state is even, <18, next semibit will be data bit part 1, LSB first
        bit = (tx_byte & 1);
        if (!bit) {
          CONFIG_MATCH_CLEAR();
          CONFIG_CAPTURE_FALLING_EDGE();
        }
        tx_paritybit ^= bit;
        tx_byte >>= 1;
      } else {
        // state=18, next semibit will be parity bit part 1
        bit = tx_paritybit;
        if (!bit) {
          CONFIG_MATCH_CLEAR();
          CONFIG_CAPTURE_FALLING_EDGE();
        }
      }
      // verify
      // state is even, check bit data (part 2), should be 1, otherwise suspect bus collission
#ifndef H_SERIES
      // for H-link, this results in bus collision errors being detected, as second bit data is not consistently 1, so omit this check on H_SERIES
      if (!bit_input) {
        tx_rx_readbackerror |= ERROR_BC;
        SW_SCOPE_LOG_ERROR(sws_count_temp, SWS_EVENT_ERR_BC);
      }
#endif /* H_SERIES */
      tx_bit = bit;
    }

    bit_input = INPUT_CAPTURE_PIN_VALUE; // sample again in view of turn-around delay
    // check if we have captured a rising edge?
    if (INPUT_CAPTURED()) {
      // edge captured
      uint16_t capture = GET_INPUT_CAPTURE();
      RESET_INPUT_CAPTURE();
      if (bit_input) {
        SW_SCOPE_LOG_EVENT(capture, SWS_EVENT_EDGE_RISING | state);
      } else {
        SW_SCOPE_LOG_EVENT(capture, SWS_EVENT_EDGE_FALLING_W | state);
      }
    } else {
      if (!bit_input) {
        SW_SCOPE_LOG_ERROR(sws_count_temp, SWS_EVENT_ERR_LOW);
      }
    }
    tx_rx_state = state;
    state++;
    tx_state = state;
    IRQ_STOP;
    return;
  }
  // state = 20
  // 20: next semibit will be stop bit part 1, schedule start bit part 1 if tx_buffer is not empty
  //     no further read-back-verify for stop bit
  bit_input = INPUT_CAPTURE_PIN_VALUE; // sample again in view of turn-around delay
  // check if we have captured a rising edge?
  if (INPUT_CAPTURED()) {
    // edge captured
    uint16_t capture = GET_INPUT_CAPTURE();
    RESET_INPUT_CAPTURE();
    if (bit_input) {
      SW_SCOPE_LOG_EVENT(capture, SWS_EVENT_EDGE_RISING | state);
    } else {
      SW_SCOPE_LOG_EVENT(capture, SWS_EVENT_EDGE_FALLING_W | state);
    }
  } else {
    if (!bit_input) {
      SW_SCOPE_LOG_ERROR(sws_count_temp, SWS_EVENT_ERR_LOW);
    }
  }
  if (tx_rx_readbackerror) {
    DIGITAL_SET_LED_ERROR;
    // As of version 0.9.22: if a bus collision is suspected (=if a read errors occurs during a write), reduce risk on further collissions by emptying write buffer
    tx_buffer_tail = tx_buffer_head;
  }
  // store transmitted byte as it it were received (if buffer space available, and if Echo), and check/store errors
  if (Echo) {
    head = rx_buffer_head + 1;
    if (head >= RX_BUFFER_SIZE) head = 0;
    if (head != rx_buffer_tail) {
      rx_buffer[head] = tx_byte_verify; // cheat, transmitted byte
      delta_buffer[head] = startbit_delta;
      error_buffer[head] = tx_rx_readbackerror;
      rx_buffer_head = head;
    } else {
      // signal buffer overrun for *previous* byte
      head = rx_buffer_head;
      error_buffer[head] |= ERROR_OR;
      DIGITAL_SET_LED_ERROR;
    }
  }
  // more data to write?
  errorhead = head;
  head = tx_buffer_head;
  tail = tx_buffer_tail;
  if (head != tail) {
    // there are more bytes to send;
    if (++tail >= TX_BUFFER_SIZE) tail = 0;
    tx_buffer_tail = tail;
    tx_byte = tx_buffer[tail];
    tx_byte_verify = tx_byte;
    delay = tx_buffer_delay[tail];
    tx_rx_paritycheck = 0;
    tx_paritybit = 0;
    if (delay < 2) {
      // if delay=0 or 1, we effectively don't wait and we continue writing!
      // as we are in (silent, high) stop bit time, we set target time at end of stop bit (= next start bit)
      SET_COMPARE_W(GET_COMPARE_W() + Wticks_per_bit_and_semibit);
      CONFIG_MATCH_CLEAR();
      CONFIG_CAPTURE_FALLING_EDGE();
      tx_state = 1;
      IRQ_STOP;
      return;
    } else {
      // it does not matter whether we are still in stop bit or beyond, we have to wait longer due to the delay setting
      tx_state = 99;
      tx_wait = delay;
    }
  } else {
    // tx buffer empty, there are no more bytes to send...,
    // we don't need to block transmission here until start bit part 1
    // because schedule_delay >= Wticks_per_bit_and_semibit ensures this too
    // switch from writing to reading
    tx_state = 0;
  }
  DISABLE_INT_COMPARE_W();
  CONFIG_CAPTURE_FALLING_EDGE(); // should not be needed, just in case
  ENABLE_INT_INPUT_CAPTURE();
  error_buffer[errorhead] |= SIGNAL_EOP;
  DIGITAL_RESET_LED_WRITE;
  IRQ_STOP;
  IRQ_END_W;
}

void P1P2MQTT::flushOutput(void)
{
  while (tx_state) /* wait */ ;
}

/****************************************/
/**            Reception               **/
/****************************************/

#define SUPPRESS_OSCILLATION
// Use this to ignore edges that are very near previous edges to avoid detection of oscillating edges
// Does not seem necessary for Daikin, but seems necessary for strange (double-freq) start pulse by some H-link2 devices

#ifdef SW_SCOPE
void P1P2MQTT::setScope(byte b)
// Set scope on or off (default off)
// calling setScope has almost-immediate effect, starting from the next packet
{
  sw_scope_next = b;
}
#endif /* SW_SCOPE */


#ifdef MHI_SERIES
void P1P2MQTT::setMHI(uint8_t b)
// Set whether to do 3byte-1byte conversion upon read/write
// default: 0 (?)
{
  mhiConvert = b;
}
#endif

void P1P2MQTT::setErrorMask(uint8_t b)
// Set errorMask
{
  errorMask = b;
}

void P1P2MQTT::setAllow(uint8_t b)
// Set max extra time between bytes
// default: ALLOW_PAUSE_BETWEEN_BYTES
// max 76? (150?)
// min 0
// unit: bit times
{
  Allow = b;
}

void P1P2MQTT::setEcho(uint8_t b)
// Set echo mode (verify and read back written data) on or off
// off: no read-back, no verification or bus collission detection
// on (default): each written byte will be read back and received, collission will be detected
// calling Echo has immediate effect
{
  Echo = b;
}

static uint16_t prev_edge_capture;  // previous capture of edge
#ifdef H_SERIES
static byte firstbyteUncertainty = 0; // 1 signals 11/12 bit uncertainty
#endif /* H_SERIES */

ISR(CAPTURE_INTERRUPT)
{
// called upon each edge during writes if in scopemode
// and
// called upon each falling edge detected during reads
// Daikin/other except H-link2:
// state = 0: at falling edge of start pulse, after pause
// state = 1: at falling edge of start pulse shortly after previous byte (so: store previously received byte)
// state = 2..10: nr of data/parity bit of this falling edge
//
// H-link2:
// state = 2: data bit OR second start bit
// state = 3..9: data bits
// state = 10: data bit OR parity bit
// state = 11: parity bit OR stop bit
// state = 12: should not happen (flag with UC|PE)
  uint8_t state, head;
  uint16_t capture;
  uint16_t offset_overflow;

  IRQ_START;
  capture = GET_INPUT_CAPTURE();
  uint16_t sws_count_temp = GET_TIMER_R_COUNT();
  state = rx_state;

  if (!state) {
    // start reading, time reading:
    IRQ_STOP;
    IRQ_BEGIN;
    IRQ_START;
    DIGITAL_SET_LED_READ;
    DIGITAL_RESET_LED_WRITE;
    DIGITAL_RESET_LED_ERROR;
  }
  if (state) {
    // detect/suppress oscillations or spurious spikes (except when expecting new start pulse, where comparison may fail due to 16-bit limitation)
    if (capture - prev_edge_capture < Rticks_suppression) {
      // log spike
      SW_SCOPE_LOG_EVENT(capture, SWS_EVENT_EDGE_SPIKE | state);
#ifdef SUPPRESS_OSCILLATION
      IRQ_STOP;
      return;
#endif /* SUPPRESS_OSCILLATION */
    }
  }
  switch (state) {
    case  0     :
    case  1     :
#ifdef H_SERIES
                  firstbyteUncertainty = SIGNAL_UC;
#endif /* H_SERIES */
                  // this is first falling edge, it must be start pulse. First confirm received byte, if any (!NO_HEAD2), without SIGNAL_EOP
                  if (rx_buffer_head2 != NO_HEAD2) {
                    rx_buffer_head = rx_buffer_head2;
                    rx_buffer_head2 = NO_HEAD2;
                  }
                  startbit_delta = time_msec;
                  // time_msec = 0; // to prevent a write start to reduce bus collision risk, not needed as MS_TIMER is disabled anyway
                  DISABLE_MS_TIMER();
                  // rx_target set to middle of first data bit
                  rx_target = capture + Rticks_per_bit_and_semibit;
                  rx_state = 2;
                  rx_paritycheck = 0;
                  SET_COMPARE_R(rx_target);
                  ENABLE_INT_COMPARE_R(); // only needed in state = 0 but doesn't hurt to do it in state = 1
#ifdef SW_SCOPE
                  // if P1P2Monitor is ready reading data (sws_block = 0), start new log operation
                  if ((state == 0) && !sws_block) {
                    sw_scope = sw_scope_next;
                    if (sw_scope) SW_SCOPE_START_LOG;
                  }
#endif /* SW_SCOPE */
                  break;
    case 2 ... 9: // data bits (except for H-link2 and state=2, in which case this is a 0 data bit OR a 2nd 0 start bit,
                  //            for now assume it is data bit and correct later if assumption is wrong)
                  rx_byte >>= 1;
                  rx_target += Rticks_per_bit; // set target time to (one semibit after) next possible falling edge
                  SET_COMPARE_R(rx_target);
                  CLEAR_COMPARE_R_FLAG();
                  rx_state = state + 1;
                  break;
    case 10     : // state=10: parity bit (in case of H-link2: parity bit OR data bit)
#ifdef H_SERIES
                  if (firstbyteUncertainty) {
                    // not sure yet regular 11-bit or double-start-bit pattern; we can shift one more time, shifting 0 out of rx_byte
                    rx_byte >>= 1;
                  }
#endif
                  // as (data or parity) bit is 0, no need to modify rx_paritycheck
                  rx_target += Rticks_per_bit; // set target time to (one semibit after) next possible falling edge = in stopbit
                  SET_COMPARE_R(rx_target);    // for non-HLink-2, this COMPARE_R_INTERRUPT is never supposed to be cancelled
                  CLEAR_COMPARE_R_FLAG();
                  rx_state = 11;
                  break;
#ifdef H_SERIES
    case 11     : // only here for 0 parity bit of double-start-bit pattern
                  firstbyteUncertainty = 0; // double-start-bit-pattern, OK, no need to shift back
                  rx_target += Rticks_per_bit + Rticks_per_bit; // set target time to (one semibit after) next possible falling edge (= stop bit, so no edge)
                  SET_COMPARE_R(rx_target); // this COMPARE_R_INTERRUPT never supposed to be cancelled
                  CLEAR_COMPARE_R_FLAG();
                  rx_state = 12;
                  break;
    case 12     : // should not happen, but record byte nevertheless... unless only here for 0 parity bit of double-start-bit pattern or in bounce case
                  SET_COMPARE_R(rx_target + Rticks_per_bit * (1 + Allow));
                  CLEAR_COMPARE_R_FLAG();
                  rx_state = 1;
                  head = rx_buffer_head + 1;
                  if (head >= RX_BUFFER_SIZE) head = 0;
                  if (head != rx_buffer_tail) {
                    rx_buffer[head] = 0xFF;
                    delta_buffer[head] = startbit_delta; // time from previous byte
                    error_buffer[head] = firstbyteUncertainty;
                    // if (firstbyteUncertainty) error_buffer[head] |= ERROR_PE; // signal "case 12 in this function should not happen"
                    // DIGITAL_WRITE_LED_ERROR(rx_paritycheck); // Suppress PE error detection to set LED_ERROR as some PE errors are to be expected
                    rx_buffer_head2 = head;
                  } else {
                    // signal buffer overrun for *previous* byte
                    error_buffer[rx_buffer_head] |= ERROR_OR;
                    DIGITAL_SET_LED_ERROR;
                    rx_buffer_head2 = rx_buffer_head; // so SIGNAL_EOP can be added
                  }
                  firstbyteUncertainty = 0;
                  PRESET_ENABLE_MS_TIMER();
                  break;
#else /* H_SERIES */
    case 11     : // state = 11: we received falling edge during stop bit before COMPARE_R_INTERRUPT ran. Should not happen.
                  break;
#endif /* H_SERIES */
    default     : // Should not happen.
                  break;
  }
  // log falling edge during reading
  SW_SCOPE_LOG_EVENT(capture, SWS_EVENT_EDGE_FALLING_R | state);
  prev_edge_capture = capture;
  IRQ_STOP;
}

ISR(COMPARE_R_INTERRUPT)
// The COMPARE_R_INTERRUPT routine is called during every data bit (state=2..9) or parity bit (state=10), unless there is a falling edge for that bit,
// and also during stop bit (state=11)
// It is also called during the latest potential location of the next start bit (with state==1, taking Allow into account),
// but only if no start bit has been detected, otherwise this interrupt will be cancelled.
// this allows detection of an end of communication block.
//
// H-link2:
// state = 2: data bit OR second start bit
// state = 3..9: data bits
// state = 10: data bit OR parity bit
// state = 11: parity bit OR stop bit
// state = 12: stop bit
{
  IRQ_START;

  uint16_t capture = GET_COMPARE_R();
  uint16_t sws_count_temp = GET_TIMER_R_COUNT();

  // COMPARE_R_INTERRUPT
  uint8_t head;
  uint8_t state;
  state = rx_state;
#ifdef H_SERIES
  uint8_t stopBit = 1;
#endif /* H_SERIES */
  switch (state) {
    case  1     : // no new start bit detected within expected time frame; thus pause in received data detected; register SIGNAL_EOP and quit rx mode
                  DISABLE_INT_COMPARE_R();
                  rx_state = 0;
                  if (rx_buffer_head2 != NO_HEAD2) {
                    rx_buffer_head = rx_buffer_head2;
                    error_buffer[rx_buffer_head] |= SIGNAL_EOP;
                    rx_buffer_head2 = NO_HEAD2;
                  }
                  DIGITAL_RESET_LED_READ;
                  IRQ_STOP;
                  IRQ_END_R;
                  return;
    case 2      : // state = 2: we received a 1 data bit; for H-link2, signal that this cannot be 2nd startbit
#ifdef H_SERIES
                  firstbyteUncertainty = 0;
#endif /* H_SERIES */
                  // fallthrough
    case 3 ... 9: // state = 2 or 3..9: we received a 1 data bit.
                  rx_byte = (rx_byte >> 1) | 0x80;
                  rx_paritycheck ^= 0x80;
                  rx_state = state + 1;
                  rx_target += Rticks_per_bit;
                  SET_COMPARE_R(rx_target);
                  break;
    case 10     : // state = 10: we received a 1 parity bit (or, in case of H-link2, a 1 parity bit or a 1 data bit)
#ifdef H_SERIES
                  if (firstbyteUncertainty) {
                    // not sure yet regular 11-bit or double-start-bit pattern; we can shift one more time, shifting 0 out of rx_byte
                    rx_byte = (rx_byte >> 1) | 0x80; // if 11-bit pattern, this may push 0 out and 1-paritybit in
                  }
#endif /* H_SERIES */
                  rx_paritycheck ^= 0x80;
                  rx_state = 11;
                  rx_target += Rticks_per_bit; // this could be reduced to +semibit if needed timingwise
                  SET_COMPARE_R(rx_target);
#ifdef H_SERIES
                  DIGITAL_WRITE_LED_ERROR(rx_paritycheck);
#else /* H_SERIES */
                  // DIGITAL_WRITE_LED_ERROR(rx_paritycheck); // Suppress PE error detection to set LED_ERROR as some PE errors are to be expected
#endif /* H_SERIES */
                  break;
    case 11     : // state = 11: we are in stop bit; where W should not be hampered by our lengthy activity of finishing up
                  // For H-link2, this can still be 1 parity bit for 12-bit pattern
#ifdef H_SERIES
                  if (firstbyteUncertainty) {
                    if (rx_paritycheck) {
                      // OK, this is assumed to be 12-bit pattern parity bit, not stop bit
                      rx_paritycheck ^= 0x80;
                      stopBit = 0;
                    } else {
                      // OK, this is assumed to be stop bit already, shift back, set stop bit
                      rx_byte <<= 1; // should shift back for 11-bit pattern to avoid parity error
                      // already stopBit = 1;
                    }
                  }
                  // fall-through
    case 12     : // stop bit
#endif /* H_SERIES */
                  // for non-Hlink-2, this is still "case 11" !
                  // we do most of the work here but have to leave some for the next start pulse routine (via rx_buffer_head2)
#ifndef H_SERIES
                  SET_COMPARE_R(rx_target + Rticks_per_bit * (1 + Allow));
#else /* H_SERIES */
                  SET_COMPARE_R(rx_target + Rticks_per_bit * (1 + (stopBit ? 0 : 1) + Allow));
#endif /* H_SERIES */
                  rx_state = 1;
                  head = rx_buffer_head + 1;
                  if (head >= RX_BUFFER_SIZE) head = 0;
                  if (head != rx_buffer_tail) {
                    rx_buffer[head] = rx_byte;
                    delta_buffer[head] = startbit_delta; // time from previous byte
#ifndef H_SERIES
                    error_buffer[head] = 0;
#else /* H_SERIES */
                    error_buffer[head] = firstbyteUncertainty;
#endif /* H_SERIES */
                    if (rx_paritycheck) {
                      error_buffer[head] |= ERROR_PE;
                      SW_SCOPE_LOG_ERROR(capture, SWS_EVENT_ERR_PE);
                    }
#ifdef H_SERIES
                    DIGITAL_WRITE_LED_ERROR(rx_paritycheck);
#else /* H_SERIES */
                    // DIGITAL_WRITE_LED_ERROR(rx_paritycheck); // Suppress PE error detection to set LED_ERROR as some PE errors are to be expected
#endif /* H_SERIES */
                    rx_buffer_head2 = head;
                  } else {
                    // signal buffer overrun for *previous* byte
                    error_buffer[rx_buffer_head] |= ERROR_OR;
                    DIGITAL_SET_LED_ERROR;
                    rx_buffer_head2 = rx_buffer_head; // so SIGNAL_EOP can be added
                  }
#ifdef H_SERIES
                  firstbyteUncertainty = 0;
#endif /* H_SERIES */
                  PRESET_ENABLE_MS_TIMER();
                  break;
    case 0      :
    default     : // Should not happen;
                  break;
  }
  SW_SCOPE_LOG_EVENT(sws_count_temp, SWS_EVENT_SIGNAL_HIGH_R | state);
  IRQ_STOP;
}

uint16_t P1P2MQTT::read_delta(void)
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

errorbuf_t P1P2MQTT::read_error(void)
// should only be called if available()==1; otherwise, returns 0
// and should only be called if packetavailable()==1; otherwise, it may return ERROR_INCOMPLETE
// currently no error checking done here (not for wrong input format, and not for 9th bit value) // TODO
{
  uint8_t head, tail;
  errorbuf_t out;

  head = rx_buffer_head;
  tail = rx_buffer_tail;
  if (head == tail) return 0;
  if (++tail >= RX_BUFFER_SIZE) tail = 0;
#ifdef MHI_SERIES
  if (mhiConvert) {
    out = error_buffer[tail];
    if (head == tail) return (out | ERROR_INCOMPLETE);
    if (++tail >= RX_BUFFER_SIZE) tail = 0;
    out |= error_buffer[tail];
    if (head == tail) return (out | ERROR_INCOMPLETE);
    if (++tail >= RX_BUFFER_SIZE) tail = 0;
    out |= error_buffer[tail];
    return out;
  } else {
    return error_buffer[tail];
  }
#else /* MHI_SERIES */
  return error_buffer[tail];
#endif
}

uint8_t  P1P2MQTT::read(void)
#ifdef MHI_SERIES
// many thanks to HamdiOlgun for reverse engineering byte encoding in MHI protocol (https://community.openhab.org/t/mitsubishi-heavy-x-y-line-protocol/82898/9)
{
  if (mhiConvert) {
    uint8_t b = ~readbyte();
    uint8_t out = 0x00;
    if (b & 0xF0) out += 0x04;
    if (b & 0xCC) out += 0x02;
    if (b & 0xAA) out += 0x01;
    if (!available()) return out;
    b = ~readbyte();
    if (b & 0xF0) out += 0x20;
    if (b & 0xCC) out += 0x10;
    if (b & 0xAA) out += 0x08;
    if (!available()) return out;
    b = ~readbyte();
    // if (b & 0xF0) error, should be 0
    if (b & 0xCC) out += 0x80;
    if (b & 0xAA) out += 0x40;
    return out;
  } else {
    return readbyte();
  }
}

uint8_t P1P2MQTT::readbyte(void)
#endif
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

bool P1P2MQTT::available(void)
{
  uint8_t head, tail;

  head = rx_buffer_head;
  tail = rx_buffer_tail;
  if (head >= tail) return head - tail;
  return RX_BUFFER_SIZE + head - tail;
}

bool P1P2MQTT::packetavailable(void)
{
  uint8_t head, tail;

  head = rx_buffer_head;
  tail = rx_buffer_tail;
  while (1) {
    if (head == tail) return 0;
    if (++tail >= RX_BUFFER_SIZE) tail = 0;
    if (error_buffer[tail] & SIGNAL_EOP) return 1;
  }
}

void P1P2MQTT::flushInput(void)
{
  rx_buffer_head = rx_buffer_tail;
}

#ifdef MHI_SERIES
uint16_t P1P2MQTT::readpacket(uint8_t* readbuf, uint16_t &delta, errorbuf_t* errorbuf, uint8_t maxlen, uint8_t cs_gen)
#else /* MHI_SERIES */
uint16_t P1P2MQTT::readpacket(uint8_t* readbuf, uint16_t &delta, errorbuf_t* errorbuf, uint8_t maxlen, uint8_t crc_gen, uint8_t crc_feed)
#endif
{
// Reads one packet (in blocking mode)
// To avoid blocking, only call this function if packetavailable()
// stores maximum of maxlen bytes of read data into readbuf
// returns total #bytes received (until v0.9.3: #bytes stored, as of v0.9.4: #bytes received, even if not stored),
//    if (packet size/return value > maxlen) some received bytes cannot be stored, and error codes are condensed into errorbuf[maxlen - 1]
// reading continues in case of error
// stores maximum of maxlen bytes of error codes into errorbuf (unless errorbuf = NULL),
// returns timing information (pause on bus before this package) in parameter delta
// If crc_gen is not zero, verifies last byte as CRC byte; CRC byte is also stored and is counted in return value if space is available
// If cs_gen is not zero, verifies last byte as checksum (sum % 256) byte; checksum byte is also stored and is counted in return value if space is available
  uint8_t EOP = 0;
  uint8_t bytecnt = 0;
#ifdef MHI_SERIES
  uint8_t mhicnt = 0;
  uint8_t cs = 0;
#else /* MHI_SERIES */
  uint8_t crc = crc_feed;
#endif
#ifdef H_SERIES
  uint8_t expectedLength = 0xFF; // split H-link2 packets based on 3rd byte
#endif /* H_SERIES */

#ifndef H_SERIES
  while (!EOP) {
#else /* H_SERIES */
  while (!EOP && (expectedLength > bytecnt)) {
#endif /* H_SERIES */
    if (available()) {
      errorbuf_t error = read_error();
      EOP = (error & SIGNAL_EOP);
      if (errorbuf) {
        if (bytecnt < maxlen) {
          errorbuf[bytecnt] = (error & errorMask);
        } else {
          errorbuf[maxlen - 1] |= (error & errorMask);
        }
      }
      if (!bytecnt) delta = read_delta();
      uint8_t c = read();
#ifdef MHI_SERIES
      if ((EOP == 0) || (cs_gen == 0)) {
        if (bytecnt < maxlen) {
          readbuf[bytecnt] = c;
        }
        if (cs_gen != 0) cs += c;
      } else {
        // EOP, cs in use, check cs
        if (bytecnt < maxlen) {
          readbuf[bytecnt] = c;
          if (c != cs) {
            if (bytecnt < maxlen) {
              errorbuf[bytecnt] |= ERROR_CRC_CS;
            } else {
              errorbuf[maxlen - 1] |= ERROR_CRC_CS;
            }
            DIGITAL_SET_LED_ERROR;
          }
        }
      }
#else /* MHI_SERIES */
      if ((EOP == 0) || (crc_gen == 0)) {
        if (bytecnt < maxlen) {
          readbuf[bytecnt] = c;
        }
        if (crc_gen != 0) for (uint8_t i = 0; i < 8; i++) {
          crc = (((crc ^ c) & 0x01) ? ((crc >> 1) ^ crc_gen) : (crc >> 1));
          c >>= 1;
        }
      } else {
        // EOP, crc in use, check crc
        if (bytecnt < maxlen) {
          readbuf[bytecnt] = c;
          if (c != crc) {
            if (bytecnt < maxlen) {
              errorbuf[bytecnt] |= ERROR_CRC_CS;
            } else {
              errorbuf[maxlen - 1] |= ERROR_CRC_CS;
            }
            DIGITAL_SET_LED_ERROR;
          }
        }
      }
#endif
      bytecnt++;
#ifdef H_SERIES
      if (bytecnt == 3) expectedLength = readbuf[2];
#endif /* H_SERIES */
    }
  }
  return bytecnt;
}

#ifdef MHI_SERIES
void P1P2MQTT::writepacket(uint8_t* writebuf, uint8_t l, uint16_t t, uint8_t cs_gen)
#else /* MHI_SERIES */
void P1P2MQTT::writepacket(uint8_t* writebuf, uint8_t l, uint16_t t, uint8_t crc_gen, uint8_t crc_feed)
#endif
{
// Writes one packet of l bytes, t ms after last bus action;
// If crc_gen is not zero, adds CRC byte to packet
// If cs_gen is not zero, adds CS byte to packet
// Note that t=0 or t=1 increases risk of bus collisions, don't use it if not needed (t<2 will be changed to t=2 in new library).
  setDelay(t);
#ifdef MHI_SERIES
  uint8_t cs = 0;
#else /* MHI_SERIES */
  uint8_t crc = crc_feed;
#endif
  for (uint8_t i = 0; i < l; i++) {
    uint8_t c = writebuf[i];
    write(c);
#ifdef MHI_SERIES
    if (cs_gen != 0) cs += c;
#else /* MHI_SERIES */
    if (crc_gen != 0) for (uint8_t i = 0; i < 8; i++) {
      crc = ((crc ^ c) & 0x01 ? ((crc >> 1) ^ crc_gen) : (crc >> 1));
      c >>= 1;
    }
#endif
  }
#ifdef MHI_SERIES
  if (cs_gen) write(cs);
#else /* MHI_SERIES */
  if (crc_gen) write(crc);
#endif
}

int32_t P1P2MQTT::uptime_sec(void)
{
// returns uptime in seconds if S_TIMER is defined, otherwise returns -1; wraps in 65.8 years
#ifdef S_TIMER
  return time_sec;
#else
  return -1;
#endif
}

int32_t P1P2MQTT::uptime_millisec(void)
{ // returns uptime in ms, wraps in 24.8 days
#ifdef S_TIMER
// returns uptime in milliseconds in 8ms resolution, wraps in 24.8 days
  return (time_millisec & 0x7FFFFFFF);
#else
  return -1;
#endif
}
