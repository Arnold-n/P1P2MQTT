/* P1P2Serial: Library for reading/writing Daikin/Rotex P1P2 protocol
 *
 * Copyright (c) 2019-2020 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20200109 v0.9.11 allow short pauses between bytes within a packet (for KLIC-DA device, to avoid detecting each byte as individual packet)
 * 20190914 v0.9.10 upon bus collision detection, write buffer is emptied
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

#ifndef P1P2Serial_h
#define P1P2Serial_h

#include <inttypes.h>

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#include "pins_arduino.h"
#endif

#define ALTSS_BASE_FREQ F_CPU

#define ALLOW_PAUSE_BETWEEN_BYTES 0 // If there is a pause between bytes on the bus which is longer than a 1/4 bit time, 
                                    //   P1P2Serial signals en end-of-block.
                                    // Daikin devices do not add a pause between bytes, but some other controllers do, like the KLIC-DA from Zennios.
                                    // To avoid end-of-bus detection due to such a inter-byte pause, ALLOW_PAUSE_BETWEEN_BYTES can be increased
                                    //   to a value between 1 and 11 (but not more than 11 to avoid overflow)
                                    // For KLICDA devices a value of 9 seems to work.

// Signalling of error conditions and end-of-block:
// (changed signalling of error/EOB messages in v0.9.4)
// Use 16 bits for timing information
// Use 8 bits for error code
 
#define ERROR_PE        0x01 // parity error
#define ERROR_OVERRUN   0x02 // read buffer overrun
#define ERROR_READBACK  0x04 // read-back error, likely bus collission
#define ERROR_CRC       0x08 // CRC error
#define ERROR_FLAGS     0x0F // Mask used to avoid that SIGNAL_EOB ends up in errorbuf results of readpacket
#define SIGNAL_EOB      0x10 // signaling end of block

#define RX_BUFFER_SIZE 50  // read buffer overruns may occur if this is too low
#define TX_BUFFER_SIZE 33  // write buffer size

class P1P2Serial
{
public:
	P1P2Serial() { }
	~P1P2Serial() { end(); }
	static void begin(uint32_t baud);
	static void end();
	uint8_t read();      // returns next byte in read buffer
        uint8_t read_error(); // returns error code or EOB signal for next byte in read buffer, to be called before read()
	uint16_t read_delta(); // returns time difference between next byte in read buffer and previously read byte, to be called before read()
	bool available();
	bool packetavailable();
	static void flushInput();
	static void flushOutput();
	static bool writeready();
	static void write(uint8_t byte);
	static void setDelay(uint16_t t);
	static void setDelayTimeout(uint16_t t);
	static void setEcho(uint8_t b);
	uint16_t readpacket(uint8_t* readbuf, uint16_t &delta, uint8_t* errorbuf, uint8_t maxlen, uint8_t crc_gen = 0, uint8_t crc_feed = 0);
	void writepacket(uint8_t* writebuf, uint8_t l, uint16_t t, uint8_t crc_gen = 0, uint8_t crc_feed = 0);
};
#endif
