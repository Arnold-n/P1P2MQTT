/* P1P2MQTT: Library for reading/writing Daikin/Rotex P1P2 protocol
 *
 * Copyright (c) 2019-2024 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 * 20240512 v0.9.46 Mitsubishi Heavy Industries (MHI) with increased TX_BUFFER_SIZE/RX_BUFFER_SIZE and data-conversion
 * 20230618 v0.9.39 H-link2 fix buf size
 * 20230604 v0.9.38 H-link2 branch merged into main branch
 * 20230604 v0.9.37 Support for V1.2 hardware
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

#ifndef P1P2MQTT_h
#define P1P2MQTT_h

#include <inttypes.h>
#include "Arduino.h"

// Configuration options
//#define MEASURE_LOAD                // measures irq processing time
#define SW_SCOPE                    // records timing info of P1/P2 bus falling edges of start of the packets
#define SWS_FAKE_ERR_CNT 3000       // one fake error generated (per error type) per SWS_FAKE_ERR_CNT checks
#ifdef H_SERIES
#define ALLOW_PAUSE_BETWEEN_BYTES 20 // If there is a pause between bytes on the bus which is longer than a 1/4 bit time,
#else /* H_SERIES */
#define ALLOW_PAUSE_BETWEEN_BYTES 9
#endif /* H_SERIES */
                                    // If there is a pause between bytes on the bus which is longer than a 1/4 bit time,
                                    // P1P2MQTT signals an end-of-packet.
                                    // Daikin devices do not add any pause between bytes, but some other controllers do, like the KLIC-DA from Zennios, and H-link2 devices.
                                    // so maximum pause between bytes is set to ~9 bits which seems to work for KLIC-DA devices or 20 for H-link2 interfaces
                                    // To avoid end-of-packet detection due to such an inter-byte pause, a pause between bytes of at most
                                    // ALLOW_PAUSE_BETWEEN_BYTES bit lengths is accepted; ALLOW_PAUSE_BETWEEN_BYTES should be less than
                                    //  (65536 / Rticks_per_bit) - 2; for 16MHz at most ~37
                                    //  (65536 / Rticks_per_bit) - 2; for  8MHz at most ~76
                                    //   value can be changed with X command on MHI/Toshiba/Hitachi systems
#define S_TIMER                     // support for uptime_sec() in new library, but monopolizes TIMER0, so millis() cannot be used.
                                    // if undefined, TIMER0 is not used, and millis() can be used
                                    // if S_TIMER is undefined, the write budget (and error budget) will not increase over time TODO fix this
// End of configuration options

#define ADC_AVG_SHIFT 4     // sum 16 = 2^ADC_AVG_SHIFT samples before doing min/max check
#define ADC_CNT_SHIFT 4     // sum 16384 = 2^(16-ADC_CNT_SHIFT) samples to V0avg/V1avg

#ifdef H_SERIES
// H-link2 uses larger packets
// increase RX_BUFFER_SIZE from 65 to 85 for two packets xx0029 without pause in between
#define TX_BUFFER_SIZE 85  // write buffer size (1 more than max size needed)
#define RX_BUFFER_SIZE 85  // read buffer (1 more than max size needed), should be <=254
#elif defined MHI_SERIES
#define TX_BUFFER_SIZE 121 // write buffer size (1 more than max size needed)
#define RX_BUFFER_SIZE 121 // read buffer (1 more than max size needed), should be <=254
#elif defined F1F2_SERIES
#define TX_BUFFER_SIZE 81  // write buffer size (1 more than max size needed)
#define RX_BUFFER_SIZE 81  // read buffer (1 more than max size needed), should be <=254
#else /* H_SERIES */
#define TX_BUFFER_SIZE 25  // write buffer size (1 more than max size needed)
#define RX_BUFFER_SIZE 25  // read buffer (1 more than max size needed), should be <=254
#endif /* H_SERIES */
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
#define ERROR_OR                  0x08 // read buffer overrun
#define ERROR_CRC_CS              0x10 // CRC or CS error
#define SIGNAL_EOP                0x80 // signaling end of packet, this is not an error flag

// read errors
#define ERROR_PE                  0x04 // parity error (some PE errors in H-link2 seem intentional)
#define SIGNAL_UC                 0x40 // double start bit uncertainty (only for H-link2 interface)

// MHI incomplete trio
#define ERROR_INCOMPLETE          0x40 // incomplete trio

#ifdef H_SERIES
#define ERROR_MASK                0x3B // Error mask to remove SIGNAL_EOP and SIGNAL_UC and ERROR_PE for H-link2
#else /* H_SERIES */
#define ERROR_MASK                0x7F // Error mask to remove SIGNAL_EOP
#endif /* H_SERIES */

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

#define errorbuf_t uint8_t

extern volatile uint16_t sws_capture[SWS_MAX];
extern volatile uint8_t sws_event[SWS_MAX];
extern volatile uint8_t sws_cnt;
extern volatile uint8_t sws_overflow;
extern volatile byte sws_block;
extern volatile byte sw_scope;
//extern volatile uint16_t count;
//extern volatile uint16_t capture;

class P1P2MQTT
{
public:
	P1P2MQTT() { };
	~P1P2MQTT() { end(); }
	static void begin(uint32_t baud, bool use_ADC = false, uint8_t ADC_pin0 = 0, uint8_t ADC_pin1 = 1, bool ledRW_reverse = false);
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
        static void setErrorMask(uint8_t b);
#ifdef MHI_SERIES
        uint16_t readpacket(uint8_t* readbuf, uint16_t &delta, errorbuf_t* errorbuf, uint8_t maxlen, uint8_t cs_gen = 0);
        void writepacket(uint8_t* writebuf, uint8_t l, uint16_t t, uint8_t cs_gen = 0);
        static void setMHI(uint8_t b);
#else /* MHI_SERIES */
	uint16_t readpacket(uint8_t* readbuf, uint16_t &delta, errorbuf_t* errorbuf, uint8_t maxlen, uint8_t crc_gen = 0, uint8_t crc_feed = 0);
	void writepacket(uint8_t* writebuf, uint8_t l, uint16_t t, uint8_t crc_gen = 0, uint8_t crc_feed = 0);
#endif
        int32_t uptime_sec(void);
        int32_t uptime_millisec(void);
        void ADC_results(uint16_t &V0_min, uint16_t &V0_max, uint32_t &V0_avg, uint16_t &V1_min, uint16_t &V1_max, uint32_t &V1_avg);
        void ledPower(bool ledOn);
        void ledError(bool ledOn);
#ifdef MHI_SERIES
private:
       static void writebyte(uint8_t byte);
       uint8_t readbyte();
#endif /* MHI_SERIES */
};
#endif /* P1P2MQTT_h */
