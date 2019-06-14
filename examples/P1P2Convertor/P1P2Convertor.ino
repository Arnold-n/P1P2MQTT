/* P1P2Convertor: Converts Daikin/Rotex P1/P2 bus data to JSON using P1P2Serial library and model-specific conversion libraries

   Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)

   Version history
   20190614 v0.0.1 initial release; support for hex monitoring and conversion to JSON

       Thanks to Krakra for providing the hints and references to the HBS and MM1192 documents on
       https://community.openenergymonitor.org/t/hack-my-heat-pump-and-publish-data-onto-emoncms,
       to Bart Ellast for providing explanations and sample output from his heat pump, and
       to Paul Stoffregen for publishing the AltSoftSerial library.

   This program is based on the public domain Echo program from the
       Alternative Software Serial Library, v1.2
       Copyright (c) 2014 PJRC.COM, LLC, Paul Stoffregen, paul@pjrc.com
       (AltSoftSerial itself is available under the MIT license, see
        http://www.pjrc.com/teensy/td_libs_AltSoftSerial.html,
        but please note that P1P2Monitor and P1P2Serial are licensed under the GPL v2.0)

*/

#define CRC_GEN 0xD9    // perform CRC check; these values work for the Daikin hybrid
#define CRC_FEED 0x00   //
#define PRINTERRORS     // prints overrun error and parity errors as byte prefix ("-OR:" and "-PE:")
// #define PRINTHEX        // prints output in HEX instead of JSON

#include <P1P2Serial.h>
#include <ArduinoJson.h>

// Choose only one of the following libraries according to your model
#include "P1P2_EHYHB.h"   // EHYHB(H/X)-AV3 and EHV(H/X)-CB protocol data format

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
  Serial.println("");
  Serial.println(F("P1P2Serial convertor v0.0.1"));
  P1P2Serial.begin(9600);

}

#ifdef CRC_GEN
static int crc = 0;
#endif // CRC_GEN


byte packetType[3];
#define packetSize 20                     // max packet size
byte packet[packetSize];           // store data packet

int byteTypecnt = 0;
int bytecnt = 0;




void loop() {
  if (P1P2Serial.available()) {
    uint16_t delta = P1P2Serial.read_delta();
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

    int c = P1P2Serial.read();

    if ((delta == DELTA_EOB) || (delta == DELTA_PE_EOB)) {
#ifdef CRC_GEN
#ifdef PRINTERRORS
      if (c != crc) Serial.print(" CRC error");
#endif // PRINTERRORS
      crc = CRC_FEED;
#endif // CRC_GEN

#ifdef PRINTHEX
      for (int i = 0; i < byteTypecnt; i++) {
        if (packet[i] < 0x10) Serial.print("0");
        Serial.print(packetType[i], HEX);
      }
      for (int i = 0; i < bytecnt; i++) {
        if (packet[i] < 0x10) Serial.print("0");
        Serial.print(packet[i], HEX);
      }
      Serial.println();
#else // PRINTHEX
      processPacket();
#endif // PRINTHEX
      for (int i = 0; i < packetSize; i++) {
        packet[i] = 0;
      }
      byteTypecnt = 0;
      bytecnt = 0;

    } else {


      if (byteTypecnt < 3) {
        packetType[byteTypecnt] = c;
        byteTypecnt++;
      } else if (bytecnt < packetSize) {
        packet[bytecnt] = c;
        bytecnt++;
      }



#ifdef CRC_GEN
      for (int i = 0; i < 8; i++) {
        if ((crc ^ c) & 0x01) {
          crc >>= 1; crc ^= CRC_GEN;
        } else {
          crc >>= 1;
        }
        c >>= 1;
      }
#endif // CRC_GEN
    }
  }
}

String packetTypeToString(byte type[]) {
  String s = "";
  for (int i = 0; i < 3; i++) {
    s += String(type[i], HEX);
  }
  return s;
}

bool flag8(int i, int b) {
  bool result = bitRead(packet[i], b);
  return result;
}


float f8_8(int i) {
  int result;
  if (packet[i] > 127) {
    result = (packet[i] - 256) * 100 + packet[i + 1] * 100 / 256;
  } else {
    result = packet[i] * 100 + packet[i + 1] * 100 / 256;
  }
  float f = (float)result / 100;
  return f;
}


/*                                    // version without float
  String f8_8(byte byte1, byte byte2) {
  int result;
  if (byte1 > 127) {
    result = (byte1 - 256) * 100 + byte2 * 100 / 256;
  } else {
    result = byte1 * 100 + byte2 * 100 / 256;
  }
  String s = String(result / 100) + "." + String(abs(result % 100));
  return s;
  }
*/
