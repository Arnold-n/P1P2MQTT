/* P1P2Serial: Library for reading/writing Daikin/Rotex P1P2 protocol
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
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

#if defined(__arm__) && defined(CORE_TEENSY)
#define ALTSS_BASE_FREQ F_BUS
#else
#define ALTSS_BASE_FREQ F_CPU
#endif

// error messages for readbyte and readpacket
#define MAXDELTA 0xFF00          // 0xFF00=65280; higher values are reserved for error values below
#define DELTA_PE_EOB 0xFFFF
#define DELTA_PE 0xFFFD
#define DELTA_EOB 0xFFFE
#define DELTA_OVERRUN 0xFFFC
#define DELTA_COLLISION 0xFFFB
#define DELTA_CRCE 0xFFFA

class P1P2Serial : public Stream
{
public:
	P1P2Serial() { }
	~P1P2Serial() { end(); }
	static void begin(uint32_t baud) { init((ALTSS_BASE_FREQ + baud / 2) / baud); }
	static void end();
	int peek();
	int read();
	uint16_t read_delta(); // returns time difference between this and previous received byte unless an error was 
	                       // detected, in which case an error value (DELTA_XXX) is returned
	int available();
#if ARDUINO >= 100
	size_t write(uint8_t byte) { writeByte(byte); return 1; }
	void flush() { flushOutput(); }
#else
	void write(uint8_t byte) { writeByte(byte); }
	void flush() { flushInput(); }
#endif
	using Print::write;
	static void flushInput();
	static void flushOutput();
	bool listen() { return false; }
	bool isListening() { return true; }
	bool overflow() { bool r = timing_error; timing_error = false; return r; }
	static int library_version() { return 1; }
	static void enable_timer0(bool enable) { }
	static bool timing_error;
	static void writeByte(uint8_t byte);
	static void setDelay(uint16_t t);
	static void setEcho(uint8_t b);
	uint16_t readpacket(uint8_t* readbuf, uint16_t* deltabuf, uint8_t maxlen, uint8_t crc_gen = 0, uint8_t crc_feed = 0);
	void writepacket(uint8_t* writebuf, uint8_t l, uint16_t t, uint8_t crc_gen = 0, uint8_t crc_feed = 0);
private:
	static void init(uint32_t cycles_per_bit);
};
#endif
