/* P1P2Serial: Library for reading/writing Daikin/Rotex P1P2 protocol
 *
 * Version for Mitsubishi Heavy Industries (MHI) with increased TX_BUFFER_SIZE/RX_BUFFER_SIZE and data-conversion
 *
 * Copyright (c) 2019-2023 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 * 20230114 v0.9.31 MHI
 * 20221028 v0.9.23 ADC code
 * 20220918 v0.9.22 scopemode also for writes, focused on actual errors, fake error generation for test purposes, removing OLDP1P2LIB
 * 20220830 v0.9.18 version alignment with example programs
 * 20220817 v0.9.17 read-back-verification bug fixes in new and old library; config SERIALSPEED and OLDP1P2LIB selection depends on F_CPU
 * 20220811 v0.9.16 Added S_TIMER switch making TIMER0 use optional
 * 20220808 v0.9.15 LEDs on P1P2-ESP-Interface all on until first byte received on P1/P2 bus
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
 * The CC BY-NC-ND 4.0 licensed P1P2Serial library is based on the MIT-licensed AltSoftSerial,
 *      but please note that P1P2Serial itself is licensed under the CC BY-NC-ND 4.0.
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

#ifndef P1P2Serial_h
#define P1P2Serial_h

#include <inttypes.h>
#include "Arduino.h"
#include "P1P2Serial_ADC.h"

// Configuration options
//#define MEASURE_LOAD                // measures irq processing time
#define SW_SCOPE                    // records timing info of P1/P2 bus falling edges of start of the packets
//#define GENERATE_FAKE_ERRORS        // disable this for real use!! // only for NEWLIB, and on 8MHz this may add to the CPU load
#define SWS_FAKE_ERR_CNT 3000       // one fake error generated (per error type) per SWS_FAKE_ERR_CNT checks
#define ALLOW_PAUSE_BETWEEN_BYTES 9 // If there is a pause between bytes on the bus which is longer than a 1/4 bit time,
                                    //   P1P2Serial signals an end-of-packet.
                                    // Daikin devices do not add a pause between bytes, but some other controllers do, like the KLIC-DA from Zennios.
                                    // To avoid end-of-packet detection due to such an inter-byte pause, a pause between bytes of at most
                                    // ALLOW_PAUSE_BETWEEN_BYTES bit lengths is accepted; ALLOW_PAUSE_BETWEEN_BYTES should be less than
                                    //   (65536 / Rticks_per_bit) - 2; for 8MHz at most ~76
                                    //   value can be changed with X command
#define S_TIMER                     // support for uptime_sec() in new library, but monopolizes TIMER0, so millis() cannot be used.
                                    // if undefined, TIMER0 is not used, and millis() can be used
                                    // if S_TIMER is undefined, the write budget (and error budget) will not increase over time TODO fix this
// End of configuration options

#define TX_BUFFER_SIZE 65  // write buffer size (1 more than max size needed)
#define RX_BUFFER_SIZE 65  // read buffer (1 more than max size needed), should be <=254
#define NO_HEAD2 0xFF


#define ALTSS_BASE_FREQ F_CPU

// Signalling of error conditions and end-of-packet:
// (changed signalling of error/EOP messages in v0.9.4)
// Use 16 bits for timing information
// Use 8 bits for error code

// read-back-verify errors
#define ERROR_SB                  0x01 // start bit error during write
#define ERROR_BE                  0x02 // data read-back error, likely bus collission
#define ERROR_BC                  0x20 // high bit half read-back error, likely bus collission

// read errors
#define ERROR_PE                  0x04 // parity error

// read + read-back-verify errors
#define ERROR_OR                  0x08 // read buffer overrun
#define ERROR_CS                  0x10 // CS error
                                       // 0x20, 0x40, available for other errors
#define SIGNAL_EOP                0x80 // signaling end of packet, this is not an error flag

#define ERROR_REAL_MASK           0x3F // Error mask to remove SIGNAL_EOP and fake errors
#ifdef GENERATE_FAKE_ERRORS
#define ERROR_FLAGS               0xFF7F
#else /* GENERATE_FAKE_ERRORS */
#define ERROR_FLAGS               0x7F
#endif /* GENERATE_FAKE_ERRORS */

#ifdef MEASURE_LOAD
extern volatile uint16_t irq_w, irq_r, irq_lapsed_w, irq_lapsed_r;
extern volatile uint8_t irq_busy;
#endif

//extern volatile uint8_t toolate;
//extern volatile uint16_t lateness;

#define SWS_MAX 22          // Max # SWS events recorded, limited by ATmega memory size (3 bytes/event) and serial output bandwidth
                            // this cannot be much higher or time-info output overwhelms and crashes ESP8266, 21 is just over 1 byte timing info
                            // info exchange not very clean but using global variables

#define SWS_EVENT_LOOP              0xF0 // stored in sws_event[SWS_MAX - 1] to indicate that <SWS_MAX events were recorded
#define SWS_EVENT_ERR_SB            0xFF
#define SWS_EVENT_ERR_BC            0xFE
#define SWS_EVENT_ERR_PE            0xFD
#define SWS_EVENT_ERR_BE            0xFC
#define SWS_EVENT_ERR_SB_FAKE       0xFB
#define SWS_EVENT_ERR_BC_FAKE       0xFA
#define SWS_EVENT_ERR_PE_FAKE       0xF9
#define SWS_EVENT_ERR_BE_FAKE       0xF8
#define SWS_EVENT_ERR_LOW           0xF7
#define SWS_EVENT_MASK              0xE0

#define SWS_EVENT_SIGNAL_LOW     0x00
#define SWS_EVENT_SIGNAL_HIGH_R  0x80
#define SWS_EVENT_EDGE_FALLING_W 0x40
#define SWS_EVENT_EDGE_FALLING_R 0xC0
#define SWS_EVENT_EDGE_RISING    0x60
#define SWS_EVENT_EDGE_SPIKE     0xA0

#ifdef GENERATE_FAKE_ERRORS
#define errorbuf_t uint16_t
#else /* GENERATE_FAKE_ERRORS */
#define errorbuf_t uint8_t
#endif /* GENERATE_FAKE_ERRORS */

extern volatile uint16_t sws_capture[SWS_MAX];
extern volatile uint8_t sws_event[SWS_MAX];
extern volatile uint8_t sws_cnt;
extern volatile uint8_t sws_overflow;
extern volatile byte sws_block;
extern volatile byte sw_scope;
//extern volatile uint16_t count;
//extern volatile uint16_t capture;

class P1P2Serial
{
public:
	P1P2Serial() { };
	~P1P2Serial() { end(); }
	static void begin(uint32_t baud, bool use_ADC = false, uint8_t ADC_pin0 = 0, uint8_t ADC_pin1 = 1);
	static void end();
	uint8_t read();      // returns next byte in read buffer
        errorbuf_t read_error(); // returns error code or EOP signal for next byte in read buffer, to be called before read()
	uint16_t read_delta(); // returns time difference between next byte in read buffer and previously read byte, to be called before read()
	bool available();
	bool packetavailable();
	static void flushInput();
	static void flushOutput();
	static bool writeready();
	static void write(uint8_t byte);
	static void setDelay(uint16_t t);
	static void setDelayTimeout(uint16_t t);
#ifdef SW_SCOPE
        static void setScope(byte b);
#endif /* SW_SCOPE */
	static void setEcho(uint8_t b);
        static void setAllow(uint8_t b);
        static void setMHI(uint8_t b);
	uint16_t readpacket(uint8_t* readbuf, uint16_t &delta, errorbuf_t* errorbuf, uint8_t maxlen, uint8_t cs_gen = 0);
	void writepacket(uint8_t* writebuf, uint8_t l, uint16_t t, uint8_t cs_gen = 0);
        int32_t uptime_sec(void);
        int32_t uptime_millisec(void);
        void ADC_results(uint16_t &V0_min, uint16_t &V0_max, uint32_t &V0_avg, uint16_t &V1_min, uint16_t &V1_max, uint32_t &V1_avg);
private:
	static void writebyte(uint8_t byte);
	uint8_t readbyte();
};
#endif /* P1P2Serial_h */
