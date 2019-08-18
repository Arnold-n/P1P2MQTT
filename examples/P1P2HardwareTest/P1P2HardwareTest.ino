/* P1P2HardwareTest: Program to test P1P2Serial adapter in stand-alone mode
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20190817 v0.9.4 Brought in line with library 0.9.4
 *                 (separated from P1P2Monitor)
 *
 * P1P2erial is written and tested for the Arduino Uno and Mega.
 * It may or may not work on other hardware using a 16MHz clok.
 * As it is based on AltSoftSerial, the following pins should then work:
 *
 * Board          Transmit  Receive   PWM Unusable
 * -----          --------  -------   ------------
 * Arduino Uno        9         8         10
 * Arduino Mega      46        48       44, 45
 * Teensy 3.0 & 3.1  21        20         22
 * Teensy 2.0         9        10       (none)
 * Teensy++ 2.0      25         4       26, 27
 * Arduino Leonardo   5        13       (none)
 * Wiring-S           5         6          4
 * Sanguino          13        14         12
 */

#include <P1P2Serial.h>

#define CRC_GEN 0xD9    // Default generator/Feed for CRC check; these values work for the Daikin hybrid
#define CRC_FEED 0x00   // Define CRC_GEN to 0x00 means no CRC is present or added

P1P2Serial P1P2Serial;

#define TDELAY 200     // 200ms bus pause required before writing - to avoid interfering with other writers if accidently not stand-alone
#define WBLEN 23
uint8_t const WB[WBLEN] = {0x40,0x00,0x99,0x01,0x00,0x21,0x01,0x3C,0x00,0x0F,0x00,0x14,0x00,0x1A,0x00,0xFF,0x00,0x55,0xAA,0x00,0x00,0x09,0x00};

void setup() {
  Serial.begin(115200);
  while (!Serial) ; // wait for Arduino Serial Monitor to open
  Serial.println(F("*"));
  Serial.println(F("*P1P2Serial-arduino monitor v0.9.4"));
  Serial.println(F("*"));
  P1P2Serial.begin(9600);
  P1P2Serial.writepacket(WB, WBLEN, TDELAY, CRC_GEN, CRC_FEED);
}

#define RB_SIZE (WBLEN+1)     // buffers to store raw data and error codes read from P1P2bus; 1 extra for reading back CRC byte
static byte RB[RB_SIZE];
static byte EB[RB_SIZE];
static long watchdogcnt=0;

void loop() {
  if (P1P2Serial.available()) {
    watchdogcnt = 0;
    uint16_t delta;
    int n = P1P2Serial.readpacket(RB, delta, EB, RB_SIZE, CRC_GEN, CRC_FEED);
    Serial.print(F("R"));
    if (delta < 10000) Serial.print(F(" "));
    if (delta < 1000) Serial.print(F("0")); else { Serial.print(delta / 1000);delta %= 1000; };
    Serial.print(F("."));
    if (delta < 100) Serial.print(F("0"));
    if (delta < 10) Serial.print(F("0"));
    Serial.print(delta);
    Serial.print(F(": "));
    for (int i = 0;i<n;i++) {
      if (EB[i] & ERROR_READBACK) {
        // collision suspicion due to verification error in reading back written data
        Serial.print(F("-XX:"));
      }
      if (EB[i] & ERROR_PE) {
        // parity error detected
        Serial.print(F("-PE:"));
      }
      byte c = RB[i];
      if (i == n-1) {
        Serial.print(F(" CRC = "));
      }
      if (c < 0x10) Serial.print(F("0"));
      Serial.print(c, HEX);
      if (EB[i] & ERROR_OVERRUN) {
        // buffer overrun detected (overrun is after, not before, the read byte)
        Serial.print(F(":OR-"));
      }
      if (EB[i] & ERROR_CRC) {
        // CRC error detected in readpacket
        Serial.print(F(" CRC error"));
      }
    }
    Serial.println();
    if (RB[2]==0x99) P1P2Serial.writepacket(WB, WBLEN, TDELAY, CRC_GEN, CRC_FEED); // don't start writing if a different message was received
  }
  if (++watchdogcnt > 150000) {
    Serial.println("Watchdog retriggering communication");
    watchdogcnt=0;
    P1P2Serial.writepacket(WB, WBLEN, TDELAY, CRC_GEN, CRC_FEED);
  }
}
