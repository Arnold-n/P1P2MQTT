/* P1P2LCD: Read P1/P2 bus and show elementary values on OLED display.
 *          Currently supports EHYHB* products only (via included <P1P2-Daikin-LCD-EHYHB.h>)
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20191109 v0.9.10 Solved data misalignment
 * 20190817 v0.9.4 Brought in line with library 0.9.4 (API changes)
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

#define CRC_GEN 0xD9    // Default generator/Feed for CRC check; these values work for the Daikin hybrid
#define CRC_FEED 0x00   // Define CRC_GEN to 0x00 means no CRC is present or added
#define MONITORSERIAL   // outputs raw hex data packets on serial

#include <P1P2Serial.h>
#include "P1P2-Daikin-LCD-EHYHB.h"

P1P2Serial P1P2Serial;

void setup() {
  Serial.begin(115200);
  while (!Serial) ; // wait for Arduino Serial Monitor to open
  Serial.println(F("*"));
  Serial.println(F("*P1P2LCD v0.9.10"));
  Serial.println(F("*"));
  P1P2Serial.begin(9600);
  init_LCD();
}

#define RB_SIZE 25       // buffers to store raw data and error codes read from P1P2bus; 1 one more for reading back written package + CRC byte
static byte RB[RB_SIZE];
static byte EB[RB_SIZE];

void loop() {
  if (P1P2Serial.available()) {
    uint16_t delta;
    int nread = P1P2Serial.readpacket(RB, delta, EB, RB_SIZE, CRC_GEN, CRC_FEED);
    process_for_LCD(RB, (nread - 1));
#ifdef MONITORSERIAL
    bool readerror = 0;
    for (int i=0; i < nread; i++) readerror |= EB[i];
    if (readerror) {
      Serial.print(F("*"));
    } else {
      Serial.print(F("R"));
    }
    if (delta < 10000) Serial.print(F(" "));
    if (delta < 1000) Serial.print(F("0")); else { Serial.print(delta / 1000); delta %= 1000; };
    Serial.print(F("."));
    if (delta < 100) Serial.print(F("0"));
    if (delta < 10) Serial.print(F("0"));
    Serial.print(delta);
    Serial.print(F(": "));
    for (int i = 0; i < nread; i++) {
      if ((EB[i] & ERROR_READBACK)) {
        // collision suspicion due to verification error in reading back written data
        Serial.print(F("-XX:"));
      }
      if ((EB[i] & ERROR_PE)) {
        // parity error detected
        Serial.print(F("-PE:"));
      }
      byte c = RB[i];
      if (CRC_GEN && (i == nread - 1)) {
        Serial.print(F(" CRC="));
      }
      if (c < 0x10) Serial.print(F("0"));
      Serial.print(c, HEX);
      if ((EB[i] & ERROR_OVERRUN)) {
        // buffer overrun detected (overrun is after, not before, the read byte)
        Serial.print(F(":OR-"));
      }
      if ((EB[i] & ERROR_CRC)) {
        // CRC error detected in readpacket
        Serial.print(F(" CRC error"));
      }
    }
    Serial.println();
#endif
  }
}
