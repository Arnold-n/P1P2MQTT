/* P1P2json-mega: Monitor for reading Daikin/Rotex P1/P2 bus using P1P2Serial library, output in json format
 *           This does not support writing as the Arduino Uno is memory-limited,
 *           also no error reporting done in this version.
 *           Does not fit on Arduino Uno; if needed change PARAMLEN to 2 in device-dependent header file; a low memory warning remains to stability may be an issue
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20190827 v0.9.6 Brought in line with 0.9.6 P1P2-bridge-esp8266
 * 20190817 v0.9.4 Brought in line with library 0.9.4 (API changes), print last byte if crc_gen==0, added json support
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
#include <Ethernet.h>
#include <EthernetUdp.h>

// choices needed to be made before including header file:
#define outputunknown 0   // whether json parameters include parameters even if functionality is unknown
#define changeonly    1   // whether json parameters are repeated if unchanged
                          //   (only for parameters for which savehistory() is active,
                          //   and excluding parameters as indicated in ignorechange())
#define udpMessages   1   // send json as an UDP message (W5500 compatible ethernet shield is required)                          

#include "P1P2_Daikin_ParameterConversion_EHYHB.h"

#define CRC_GEN 0xD9    // Default generator/Feed for CRC check; these values work for the Daikin hybrid
#define CRC_FEED 0x00   // Define CRC_GEN to 0x00 means no CRC is present or added
#define RB_SIZE 24      // buffers to store raw data and error codes read from P1P2bus; 1 one more for reading back written package + CRC byte

P1P2Serial P1P2Serial;

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
unsigned int listenPort = 10003;                                  // listening port
unsigned int sendPort = 10000;                                    // sending port
unsigned int remPort = 10002;                                     // remote port
IPAddress ip(192, 168, 1, 33);                                       // IP address
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress sendIpAddress(255, 255, 255, 255);                      // remote IP or 255, 255, 255, 255 for broadcast (faster)

#define inputPacketBufferSize UDP_TX_PACKET_MAX_SIZE
char inputPacketBuffer[UDP_TX_PACKET_MAX_SIZE];
#define outputPacketBufferSize 100
char outputPacketBuffer[outputPacketBufferSize];

EthernetUDP udpRecv;                                              // for future functionality....
EthernetUDP udpSend;

void setup() {
  Serial.begin(115200);
  while (!Serial) ; // wait for Arduino Serial Monitor to open
  Serial.println(F("*"));
  Serial.println(F("*P1P2json-mega v0.9.6"));
  Serial.println(F("*"));
  P1P2Serial.begin(9600);
  if (udpMessages) {
    Ethernet.begin(mac, ip);
    udpRecv.begin(listenPort);
    udpSend.begin(sendPort);
  }
}

static byte RB[RB_SIZE];

static byte jsonterm = 1; // indicates whether json string has been terminated or not

void process_for_mqtt_json(byte* rb, int n) {
  char value[KEYLEN];
  char key[KEYLEN];
  byte cat; // used for esp not for Arduino
  for (byte i = 3; i < n; i++) {
    int kvrbyte = bytes2keyvalue(rb, i, 8, key, value, cat);
    // bytes2keyvalue returns 0 if byte does not trigger any output
    //                        1 if a new value should be output
    //                        8 if byte should be treated per bit
    //                        9 if json string should be terminated
    if (kvrbyte == 9) {
      // only terminate json string if there is any parameter writte
      if (jsonterm == 0) {
        jsonterm = 1;
        Serial.println(F("}"));
        if (udpMessages) {
          udpSend.print(F("}"));
          udpSend.endPacket();
        }
      }
    } else {
      for (byte j = 0; j < kvrbyte; j++) {
        int kvr = ((kvrbyte == 8) ? bytes2keyvalue(rb, i, j, key, value, cat) : kvrbyte);
        if (kvr) {
          if (jsonterm) {
            Serial.print(F("J {\"")); 
            if (udpMessages) {
              udpSend.beginPacket(sendIpAddress, remPort);
              udpSend.print(F("{\""));
            }
            jsonterm = 0;
          } else {
            Serial.print(F(",\""));
            if (udpMessages) udpSend.print(F(",\""));
          }
          Serial.print(key);
          Serial.print(F("\":"));
          Serial.print(value);
          if (udpMessages) {
            udpSend.print(key);
            udpSend.print(F("\":"));
            udpSend.print(value);
          }
        }
      }
    }
  }
  savehistory(rb, n);
}

void loop() {
  if (P1P2Serial.available()) {
    uint16_t delta;
    int n = P1P2Serial.readpacket(RB, delta, NULL, RB_SIZE, CRC_GEN, CRC_FEED);
    process_for_mqtt_json(RB, n - 1);
  }
}
