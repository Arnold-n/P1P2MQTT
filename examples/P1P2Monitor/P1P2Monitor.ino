/* P1P2Monitor: Monitor for reading Daikin/Rotex P1/P2 bus using P1P2Serial library
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20190409 v0.9.1 Improved setDelay()
 * 20190407 v0.9.0 Improved reading, writing, and meta-data; added support for timed writings and collision detection; added stand-alone hardware-debug mode
 * 20190303 v0.0.1 initial release; support for raw monitoring and hex monitoring
 *
 *     Thanks to Krakra for providing the hints and references to the HBS and MM1192 documents on
 *     https://community.openenergymonitor.org/t/hack-my-heat-pump-and-publish-data-onto-emoncms,
 *     to Bart Ellast for providing explanations and sample output from his heat pump, and
 *     to Paul Stoffregen for publishing the AltSoftSerial library.
 *
 * This program is based on the public domain Echo program from the
 *     Alternative Software Serial Library, v1.2
 *     Copyright (c) 2014 PJRC.COM, LLC, Paul Stoffregen, paul@pjrc.com
 *     (AltSoftSerial itself is available under the MIT license, see
 *      http://www.pjrc.com/teensy/td_libs_AltSoftSerial.html,
 *      but please note that P1P2Monitor and P1P2Serial are licensed under the GPL v2.0)
 *
 */

//#define DEBUG_HARDWARE  // uncomment this to verify correct operation of stand-alone read/write interface
                        // DEBUG_HARDWARE does not work in RAWMONITOR mode
//#define RAWMONITOR      // uncomment this for receiving and forwarding raw serial data
                        // or keep this undefined for printing messages in hexadecimal
                        //            with one line per received block
                        //            and optionally reporting of timing between data blocks
#define PRINTDELTA      // to print time between data blocks, and start each block on new line
                        //            (this is not available in RAWMONITOR mode)
#define CRC_GEN 0xD9    // perform CRC check; these values work for the Daikin hybrid
#define CRC_FEED 0x00   //
#define PRINTCRC        // prints CRC byte at end of line (otherwise it will not be printed)
#define PRINTERRORS     // prints overrun error and parity errors as byte prefix ("-OR:" and "-PE:")
#define SHIFTCNT        // prints package counter related to shifting bit in final packet

#include <P1P2Serial.h>

// P1P2erial is written and tested for the Arduino Uno.
// It may work on other hardware.
// As it is based on AltSoftSerial, the following pins should then work:
//
// Board          Transmit  Receive   PWM Unusable
// -----          --------  -------   ------------
// Teensy 3.0 & 3.1  21        20         22
// Teensy 2.0         9        10       (none)
// Teensy++ 2.0      25         4       26, 27
// Arduino Uno        9         8         10
// Arduino Leonardo   5        13       (none)
// Arduino Mega      46        48       44, 45
// Wiring-S           5         6          4
// Sanguino          13        14         12

P1P2Serial P1P2Serial;

void setup() {
	Serial.begin(115200);
	while (!Serial) ; // wait for Arduino Serial Monitor to open
#ifndef RAWMONITOR
  Serial.println("");
	Serial.println("P1P2Serial monitor v0.9.1");
#endif // RAWMONITOR
	P1P2Serial.begin(9600);
#ifdef DEBUG_HARDWARE
	P1P2Serial.write((uint8_t) 0x00);
	P1P2Serial.write((uint8_t) 0x00);
	P1P2Serial.write((uint8_t) 0x13);
	P1P2Serial.write((uint8_t) 0x00);
	P1P2Serial.write((uint8_t) 0x00);
	P1P2Serial.write((uint8_t) 0xD0);
	P1P2Serial.write((uint8_t) 0xDD);
#endif // DEBUG_HARDWARE
}

#ifdef CRC_GEN
static int crc=0;
#endif
#ifdef SHIFTCNT
static int shiftcnt=0;
static int shiftline=0;
static int bytecnt=0;
static int newshift=0;
#endif
#ifdef PRINTDELTA
static int newline=1;
#endif // PRINTDELTA

#ifdef DEBUG_HARDWARE
static uint32_t watchdogcnt=0;
#endif

void loop() {
#ifdef RAWMONITOR
	// direct write-through from serial to P1P2, raw mode
	if (Serial.available()) {
		uint8_t c = Serial.read();
		P1P2Serial.write(c);
	}
#else // RAWMONITOR
#ifdef DEBUG_HARDWARE
	if (++watchdogcnt > 50000) {
		// re-trigger write/read events after long silence
		// watchdog time-out is approximately 200ms on Uno
		Serial.println("Watchdog retriggering communication");
		watchdogcnt=0;
		P1P2Serial.write((uint8_t) 0x00);
		P1P2Serial.write((uint8_t) 0x00);
		P1P2Serial.write((uint8_t) 0x13);
		P1P2Serial.write((uint8_t) 0x00);
		P1P2Serial.write((uint8_t) 0x00);
		P1P2Serial.write((uint8_t) 0xD0);
		P1P2Serial.write((uint8_t) 0xDD);
	}
#endif // DEBUG_HARDWARE
#endif // RAWMONITOR
	if (P1P2Serial.available()) {
#ifndef RAWMONITOR
		uint16_t delta = P1P2Serial.read_delta();
#ifdef PRINTDELTA
		if (newline) {
			if (delta <= MAXDELTA) {
				if (delta < 1000) Serial.print("0"); else { Serial.print(delta/1000);delta%=1000; };
				Serial.print(".");
				if (delta < 100) Serial.print("0");
				if (delta < 10) Serial.print("0");
				Serial.print(delta);
				Serial.print(": ");
			} else {
				Serial.print("....: ");
			}
			newline=0;
		}
#ifdef SHIFTCNT
#ifdef PRINTERRORS
		if (delta == DELTA_COLLISION) {
			// collision suspicion due to verification error in reading back written data
			Serial.print("-XX:");
		}
		if (delta == DELTA_OVERRUN) {
			// buffer overrun
			Serial.print("-OR:");
		}
		if ((delta == DELTA_PE) || (delta == DELTA_PE_EOB)) {
			// parity error detected
			Serial.print("-PE:");
		}
#endif // PRINTERRORS
			bytecnt=0;
#endif // not RAWMONITOR
#endif // PRINTDELTA
#endif // RAWMONITOR
		int c = P1P2Serial.read();
#ifdef RAWMONITOR
		Serial.write(c);
#else // RAWMONITOR
#ifdef SHIFTCNT
		++bytecnt;
		if ((bytecnt==3) && (c==0x30)) {
			shiftline=1;
			shiftcnt++;
			if (shiftcnt==241) shiftcnt=0;
		}
		if (shiftline) {
			if ((bytecnt==5) && (c==0x01)) {
				shiftcnt=240;
				newshift=1;
			}
			if ((bytecnt==15) && (c==0x01)) {
				if (shiftcnt != 209) {
					shiftcnt=209; // sync shift pattern
					newshift=1;
				}
			}
		}
#endif // SHIFTCNT
		if ((delta == DELTA_EOB) || (delta == DELTA_PE_EOB)) {
#ifdef CRC_GEN
#ifdef PRINTCRC
			Serial.print(" CRC=");
			if (c < 0x10) Serial.print("0");
			Serial.print(c,HEX);
#ifdef PRINTERRORS
			if (c != crc) Serial.print(" CRC error");
#endif // PRINTERRORS
#endif // PRINTCRC
			crc = CRC_FEED;
#endif // CRC_GEN
#ifdef SHIFTCNT
			if (shiftline) {
				Serial.print(" LC=");
				if (shiftcnt < 0x10) Serial.print("0");
				Serial.print(shiftcnt,HEX);
				shiftline=0;
			}
#ifdef PRINTERRORS
			if (newshift) Serial.print(" newshift");
			newshift=0;
#endif // PRINTERRORS
#endif // SHIFTCNT
#ifdef PRINTDELTA
			Serial.println();
			newline=1;
#endif
#ifdef DEBUG_HARDWARE
			// As any write triggers a new byte to be read back,
			// a standa-lone read circuit can be tested.
			// Each received block triggers a new write of
			// an example block, with correct CRC at end.
			// But first wait for 100ms silence on bus before writing
			P1P2Serial.setDelay(100);
			P1P2Serial.write((uint8_t) 0x00);
			P1P2Serial.write((uint8_t) 0x00);
			P1P2Serial.write((uint8_t) 0x13);
			P1P2Serial.write((uint8_t) 0x00);
			P1P2Serial.write((uint8_t) 0x00);
			P1P2Serial.write((uint8_t) 0xD0);
			P1P2Serial.write((uint8_t) 0xDD);
			watchdogcnt = 0;
#endif // DEBUG_HARDWARE
		} else {
			if (c < 0x10) Serial.print("0");
			Serial.print(c,HEX);
#ifdef CRC_GEN
			for (int i=0;i<8;i++) {
				if ((crc^c) & 0x01) {
					crc>>=1; crc ^= CRC_GEN;
				} else {
					crc>>=1;
				}
				c>>=1;
			}
#endif // CRC_GEN
		}
#endif // not RAWMONITOR
	}
}
