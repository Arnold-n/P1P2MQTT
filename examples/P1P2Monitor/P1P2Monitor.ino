/* P1P2Monitor: Monitor for reading Daikin/Rotex P1/P2 bus using P1P2Serial library
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20190505 v0.9.3 Changed error handling and corrected deltabuf type in readpacket; added mod12/mod13 counters
 * 20190428 v0.9.2 Added setEcho(b), readpacket() and writepacket(); LCD support added
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
//#define LCD             // show Daikin hybrid data on SPI SSD 1306 128x64 LCD connected to pins 2-6
                        //            (this is not available in RAWMONITOR mode)
                        //            (this requires the installation of the u8g2 library, see
                        //                   https://github.com/olikraus/u8g2/wiki/u8g2install)


#include <P1P2Serial.h>

#ifdef LCD
#include <Arduino.h>
#include <SPI.h>
#include <U8x8lib.h>
U8X8_SSD1306_128X64_NONAME_4W_SW_SPI u8x8(/* clock=*/ 2, /* data=*/ 3, /* cs=*/ 6, /* dc=*/ 5, /* reset=*/ 4);
static int bytecnt=0;
static int packetcntmod12=0; // packetcntmod12 increments upon each package of 13 packets;
static int packetcntmod13=0; // packetcntmod13 as shown on LCD remains constant except (thus signals) additional packets and/or communication errors
#endif // LCD

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
	Serial.println("P1P2Serial monitor v0.9.2");
#ifdef LCD
	init_LCD();
#endif
#endif // not RAWMONITOR
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
#endif // CRC_GEN
#ifdef SHIFTCNT
static int shiftcnt=0;
static int shiftline=0;
static int shiftbytecnt=0;
static int newshift=0;
#endif // SHIFTCNT
#ifdef PRINTDELTA
static int newline=1;
#endif // PRINTDELTA

#ifdef DEBUG_HARDWARE
static uint32_t watchdogcnt=0;
#endif // DEBUG_HARDWARE


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
#endif // not RAWMONITOR
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
#endif // PRINTDELTA
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
#endif // not RAWMONITOR
		int c = P1P2Serial.read();
#ifdef RAWMONITOR
		Serial.write(c);
#else // not RAWMONITOR
#ifdef SHIFTCNT
		++shiftbytecnt;
		if ((shiftbytecnt==3) && (c==0x30)) {
			shiftline=1;
			shiftcnt++;
			if (shiftcnt==241) shiftcnt=0;
		}
		if (shiftline) {
			if ((shiftbytecnt==5) && (c==0x01)) {
				shiftcnt=240;
				newshift=1;
			}
			if ((shiftbytecnt==15) && (c==0x01)) {
				if (shiftcnt != 209) {
					shiftcnt=209; // sync shift pattern
					newshift=1;
				}
			}
		}
#endif // SHIFTCNT
		if ((delta == DELTA_EOB) || (delta == DELTA_PE_EOB)) {
#ifdef LCD
			packetcntmod12++; if (packetcntmod12 == 12) packetcntmod12 = 0;
			packetcntmod13++; if (packetcntmod13 == 13) packetcntmod13 = 0;
			bytecnt=0;
#endif // LCD
#ifdef SHIFTCNT
			shiftbytecnt=0;
#endif // SHIFTCNT
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
#ifdef LCD
			++bytecnt;
			process_for_LCD(bytecnt,c);
#endif
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

#ifdef LCD
#define LCD_char(i)  { u8x8.drawGlyph(col,row,i); }
#define LCD_int1(i)  { LCD_char('0'+(i)%10); }
#define LCD_int2(i)  { LCD_char('0'+((i)%100)/10); col++; LCD_char('0'+(i)%10); }
#define LCD_hex1(i)  { if ((i)<10) LCD_char(('0'+(i))) else LCD_char('A'+((i)-10)); }
#define LCD_hex2(i)  { LCD_hex1(i>>4); col++; LCD_hex1(i&0x0f); }

static int linesrc=0xff;
static int linetype=0xff;
static int linecnt=0xff;

void init_LCD(void) {
	u8x8.begin();
	u8x8.setFont(u8x8_font_chroma48medium8_r);
	uint8_t row,col;
	row=0; col=0;  LCD_int2(20);  // Date/time
	row=0; col=8;  LCD_char(' ');
	row=0; col=11; LCD_char(':');
	row=1; col=0;  u8x8.drawString(col,row,"RWT  MWT  LWT");
	row=2; col=2;  LCD_char('.');
	row=2; col=7;  LCD_char('.');
	row=2; col=12; LCD_char('.');
	row=3; col=0;  u8x8.drawString(col,row,"flow Trm  Tout");
	row=4; col=2;  LCD_char('.'); // flow
	row=4; col=7;  LCD_char('.'); // Troom
	row=4; col=12; LCD_char('.'); // Toutside
	row=5; col=0;  u8x8.drawString(col,row,"Setp Ref1 Ref2");
	row=6; col=2;  LCD_char('.'); // setp
	row=6; col=7;  LCD_char('.'); // Refr1
	row=6; col=12; LCD_char('.'); // Refr2
}

void process_for_LCD(uint8_t n,uint8_t c) {
	uint8_t row,col;
	linecnt=n;
	if (n==1) linesrc=c;
	if (n==3) linetype=c;
	if (n>3)
 
	switch (linetype) {
		case 0x10 :  switch (linesrc) {
			case 0x00 : row=6;col=15;LCD_hex1(packetcntmod12); row=4;col=15;LCD_hex1(packetcntmod13); switch (n) {
				case  4 : row=7; col=8; LCD_hex2(c);                    break; // heating on/off?
				case  5 : row=7; col=10; LCD_hex2(c);                   break; // 0x01/0x81?
				case 11 : /* row=X; col=X; LCD_int2(c); */              break; // target room temp
				case 13 : row=7; col=12; LCD_hex2(c);                   break; // flags 5/6?
				case 14 : row=7; col=14; LCD_hex2(c);                   break; // quiet mode
			} break;
			case 0x40 : switch (n) {
				case  4 : /* row=X; col=X; LCD_hex2(c); */              break; // heating on/off copy ?
				case  7 : row=7; col=4; LCD_hex2(c);                    break; // 0:DHW on/off 4: SHC/tank 3-way valve?
				case 12 : row=7; col=0; LCD_int2(c);                    break; // Troom target
				case 22 : row=7; col=2; LCD_hex2(c);                    break; // 0:compressor 3:pump
			} break;
		} break;
		case 0x11 : switch (linesrc) {
			case 0x00 : switch (n) {
				case  4 : row=4; col=5; LCD_int2(c);                    break; // Troom
				case  5 : row=4; col=8; LCD_char('0'+(c*5)/128);        break; // Troom
			} break;
			case 0x40 : switch (n) {
				case  4 : row=2; col=10; LCD_int2(c);                   break; // LWT
				case  5 : row=2; col=13; LCD_char('0'+(c*5)/128);       break; // LWT
				case  8 : row=4; col=10; LCD_int2(c);                   break; // Toutside
				case  9 : row=4; col=13; LCD_char('0'+(c*5)/128);       break; // Toutside
				case 10 : row=2; col=0; LCD_int2(c);                    break; // RWT
				case 11 : row=2; col=3; LCD_char('0'+(c*5)/128);        break; // RWT
				case 12 : row=2; col=5; LCD_int2(c);                    break; // MWT
				case 13 : row=2; col=8; LCD_char('0'+(c*5)/128);        break; // MWT
				case 14 : row=6; col=5; LCD_int2(c);                    break; // Trefr1 ?
				case 15 : row=6; col=8; LCD_char('0'+(c*5)/128);        break; // Trefr1 ?
			} break;
		} break;
		case 0x12 : switch (linesrc) {
			case 0x00 : switch (n) {
				case  5 : row=0; col=15; LCD_int1(c);                   break; // DayOfWeek
				case  6 : row=0; col=9; LCD_int2(c);                    break; // Hours
				case  7 : row=0; col=12; LCD_int2(c);                   break; // Minutes
				case  8 : row=0; col=2; LCD_int2(c);                    break; // year
				case  9 : row=0; col=4; LCD_int2(c);                    break; // month
				case 10 : row=0; col=6; LCD_int2(c);                    break; // day
			} break;
			case 0x40 : switch (n) {
				case  4 : /* row=X; col=X; LCD_hex2(c); */              break; // Op?
				case  5 : /* row=X; col=X; LCD_hex2(c); */              break; // Op?
				case 15 : /* row=X; col=X; LCD_hex2(c); */              break; // Op?
				case 16 : row=7; col=6; LCD_hex2(c);                    break; // 0:heat pump 6:? 7:DHW
				case 17 : /* row=X; col=X; LCD_hex2(c); */              break; // Op?
			} break;
		} break;
		case 0x13 : switch (linesrc) {
			case 0x00 : switch (n) {
				default : break;
			} break;
			case 0x40 : switch (n) {
				case  4 : /* row=X; col=X; LCD_int2(c); */              break; // DHW
				case 13 : row=4; col=0; LCD_int2(c/10);
				          col=3; LCD_int1(c%10);                        break; // Flow
			} break;
		} break;
		case 0x14 : switch (linesrc) {
			case 0x00 : switch (n) {
				case 12 : /* row=X; col=X; LCD_int2(c); */              break; // Tdelta
				case 13 : /* row=X; col=X; LCD_hex2(c); */              break; // Op?
			} break;
			case 0x40 : switch (n) {
				case 13 : /* row=X; col=X; LCD_hex2(c); */              break; // Op?
				case 19 : row=6; col=0; LCD_int2(c);                    break; // T setpoint1
				case 20 : row=6; col=3; LCD_char('0'+(c*5)/128);        break; // T setpoint1
				case 21 : /* row=X; col=X; LCD_int2(c); */              break; // T setpoint2
				case 22 : /* row=X; col=X; LCD_char('0'+(c*5)/128); */  break; // T setpoint2
			} break;
		} break;
		case 0x15 : switch (linesrc) {
			case 0x00 : switch (n) {
				case  5 : /* row=X; col=X; LCD_hex2(c); */              break; // Op?
				case  6 : /* row=X; col=X; LCD_hex2(c); */              break; // Op?
				case  9 : /* row=X; col=X; LCD_hex2(c); */              break;
			} break;
			case 0x40 : switch (n) {
				case  6 : row=6; col=10; LCD_int2(c);                   break; // Refr2 ?
				case  7 : row=6; col=13; LCD_char('0'+(c*5)/128);       break; // Refr2 ?
			} break;
		} break;
		case 0x30 : break;
	}
}
#endif
