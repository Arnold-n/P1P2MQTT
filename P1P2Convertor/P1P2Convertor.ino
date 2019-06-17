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
// #define SENDUDP         // Sends JSON over UDP. Currently does not work on Uno due to lack of memory.

#include <P1P2Serial.h>
#include <ArduinoJson.h>

// Choose only one of the following libraries according to your model
// #include "src/EHYHB.ino"   // EHYHB(H/X)-AV3 and EHV(H/X)-CB protocol data format

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

#ifdef SENDUDP
#include <Ethernet.h>
#include <SPI.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0};
unsigned int localPort = 10000;                                    // sending port
unsigned int remotePort = 10009;                                     // remote port
IPAddress ip(192, 168, 1, 81);                                       // IP address of your Arduino
// IPAddress gateway(192, 168, 1, 80);
// IPAddress subnet(255, 255, 255, 0);
// IPAddress remoteIp(192, 168, 1, 108);
IPAddress remoteIp(255, 255, 255, 255);

EthernetUDP udp;

#endif // SENDUDP

P1P2Serial P1P2Serial;

const size_t capacity = JSON_OBJECT_SIZE(30);
DynamicJsonDocument json(capacity);


void setup() {
  Serial.begin(115200);
  while (!Serial) ; // wait for Arduino Serial Monitor to open
  Serial.println("");
  Serial.println(F("P1P2Serial convertor v0.0.1"));
  P1P2Serial.begin(9600);

#ifdef SENDUDP
  // Initialize Ethernet libary
  if (!Ethernet.begin(mac)) {
    Serial.println(F("Failed to initialize Ethernet library"));
    return;
  }
  // Enable UDP
  udp.begin(localPort);
#endif // SENDUDP
}

#ifdef CRC_GEN
static int crc = 0;
#endif // CRC_GEN



byte typeCnt = 0;
byte dataCnt = 0;

#define typeSize 3
#define dataSize 20

typedef struct {
  union {
    byte type[typeSize];
    long typeLong;
  };
  byte data[dataSize];
} packet;

packet currentPacket;

#define numberofStoredPackets 5              // <=== Attention, memory-hungry array! Ajust if necessary.
packet storedPackets[numberofStoredPackets];

byte freeSlot = 0;

bool storeCompare = false;

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
      for (byte i = 0; i < typeCnt; i++) {
        if (currentPacket.type[i] < 0x10) Serial.print("0");
        Serial.print(currentPacket.type[i], HEX);
      }
      for (byte i = 0; i < dataCnt; i++) {
        if (currentPacket.data[i] < 0x10) Serial.print("0");
        Serial.print(currentPacket.data[i], HEX);
      }
      Serial.println();
#else // PRINTHEX
      processPacket();
#endif // PRINTHEX

      typeCnt = 0;
      dataCnt = 0;

    } else {


      if (typeCnt < typeSize) {
        currentPacket.type[typeCnt] = c;
        typeCnt++;
      } else if (dataCnt < dataSize) {
        currentPacket.data[dataCnt] = c;
        dataCnt++;
      }




#ifdef CRC_GEN
      for (byte i = 0; i < 8; i++) {
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


bool flag8(int i, int b) {
  bool result = bitRead(currentPacket.data[i], b);
  return result;
}


float f8_8(int i) {
  int result;
  if (currentPacket.data[i] > 127) {
    result = (currentPacket.data[i] - 256) * 100 + currentPacket.data[i + 1] * 100 / 256;
  } else {
    result = currentPacket.data[i] * 100 + currentPacket.data[i + 1] * 100 / 256;
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

void unknownBit(byte i, byte b) {
  String packetTypeStr = "";
  for (byte j = 0; j < typeSize; j++) {
    packetTypeStr += String(currentPacket.type[j], HEX);
  }
  String byteNumber = "";
  if (i < 10) {
    byteNumber = "0" + String(i + 4);
  } else {
    byteNumber = String(i + 4);
  }
  if (bitRead(currentPacket.data[i], b) == 1) {
    json[packetTypeStr + "-" + byteNumber + "-" + String(b)] = flag8(i, b);          // first data byte number is 4 to match our existing tables)
  }
}

void unknownByte(byte i) {
  String packetTypeStr = "";
  for (byte j = 0; j < typeSize; j++) {
    packetTypeStr += String(currentPacket.type[j], HEX);
  }
  String byteNumber = "";
  if (i < 10) {
    byteNumber = "0" + String(i + 4);
  } else {
    byteNumber = String(i + 4);
  }
  String leadingZero = "";
  if (currentPacket.data[i] < 0x10) leadingZero = "0";
  json[packetTypeStr + "-" + byteNumber] = "0x" + leadingZero + String(currentPacket.data[i], HEX);       // first data byte number is 4 to match our existing tables)
}

long packetTypeInt() {
  long bignum = 0;
  for (int i = 0; i < typeSize; i++) {
    bignum = bignum * 256 + currentPacket.type[i];

  }
  return bignum;
}

byte findPacket() {
  for (int i = 0; i < freeSlot; i++) {
    if (storedPackets[i].typeLong == currentPacket.typeLong) {
      return i;
    }
  }
  if (freeSlot < numberofStoredPackets) {
    for (byte i = 0; i < typeSize; i++) {
      storedPackets[freeSlot].type[i] = currentPacket.type[i];
    }
    freeSlot++;
  } else {
    // TODO error
  }
  return freeSlot;
}

void storePacket() {
  byte index = findPacket();
  if (index < numberofStoredPackets) {
    for (byte i = 0; i < typeSize; i++) {
      storedPackets[index].type[i] = currentPacket.type[i];
    }
    for (byte i = 0; i < dataSize; i++) {
      storedPackets[index].data[i] = currentPacket.data[i];
    }
  }
}

void processPacket() {
  json.clear();


  convertPacket();
  storeCompare = false;

  if (!json.isNull()) {
    serializeJson(json, Serial);
    Serial.println();

#ifdef SENDUDP
    udp.beginPacket(remoteIp, remotePort);
    serializeJson(json, udp);
    udp.println();
    udp.endPacket();
#endif // SENDUDP
  }
}
