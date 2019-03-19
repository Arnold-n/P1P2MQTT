/* P1P2Monitor: Monitor for reading
 * Uses P1P2Serial library for reading/writing Daikin/Rotex P1P2 protocol
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Status:
 * 20190303 v0.01 initial release; support for raw monitoring and hex monitoring
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

//#define RAWMONITOR    // uncomment this for receiving and forwarding raw serial data
			// or keep this undefined for printing messages in hexadecimal
			//            with one line per received block
			//            and optionally reporting of timing between data blocks
#define MINDELTA 3	// minimum delta time needed to generate line break
#define PRINTDELTA	// to print time between data blocks for each line
			//            (this is not available in RAWMONITOR mode)
//#define CRC_GEN 0xD9    // perform CRC check; these values work for the Daikin hybrid
#define CRC_FEED 0x00  // 

#include "P1P2Serial.h"

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
	Serial.println("P1P2Serial monitor");
#endif
	P1P2Serial.begin(9600);
}

#ifdef CRC_GEN
static int crc=-1;
static int lastc=0;
static int lastcrc=0;
#endif

void loop() {
	if (P1P2Serial.available()) {
#ifndef RAWMONITOR
		int delta = P1P2Serial.read_delta();
		if (delta == 0) {
			// parity error detected
			Serial.print("-PE:");
		}
#endif
		int c = P1P2Serial.read();
#ifdef RAWMONITOR
		Serial.write(c);
#else
		if (delta > MINDELTA) {
#ifdef CRC_GEN
			if (lastc != lastcrc) Serial.print(" CRC error");
			crc = CRC_FEED;
#endif
			Serial.println();
#ifdef PRINTDELTA
			if (delta < 0x10) Serial.print("0");
			Serial.print(delta,HEX);
			Serial.print(" ");
#endif
		}
		if (c < 0x10) Serial.print("0");
		Serial.print(c,HEX);
#ifdef CRC_GEN
		lastcrc = crc;
		lastc = c;
		for (int i=0;i<8;i++) {
			if ((crc^c) & 0x01) {
				crc>>=1; crc ^= CRC_GEN;
			} else {
				crc>>=1;
			}
			c>>=1;
		}
#endif
#endif
	}
	/* direct write to P1P2, disabled for now
	if (Serial.available()) {
		c = Serial.read();
		P1P2Serial.print(c);
	}
	*/
}
