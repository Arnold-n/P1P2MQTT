/* P1P2Monitor: Monitor for reading Daikin/Rotex P1/P2 bus using P1P2Serial library
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20190820 v0.9.5 Changed delay behaviour, timeout added
 * 20190817 v0.9.4 Brought in line with library 0.9.4 (API changes), print last byte if crc_gen==0, removed LCD support due to performance concerns, added config over serial
                   See comments below for description of serial protocol
 * 20190505 v0.9.3 Changed error handling and corrected deltabuf type in readpacket; added mod12/mod13 counters
 * 20190428 v0.9.2 Added setEcho(b), readpacket() and writepacket(); LCD support added
 * 20190409 v0.9.1 Improved setDelay()
 * 20190407 v0.9.0 Improved reading, writing, and meta-data; added support for timed writings and collision detection; added stand-alone hardware-debug mode
 * 20190303 v0.0.1 initial release; support for raw monitoring and hex monitoring
 *
 * Serial protocol from/to Arduino Uno/Mega:
 * -----------------------------------------
 * Configuration is done by sending one of the following lines over serial 
 * W<hex data> Write packet (max 32 bytes) (no 0x prefix should be used for the hex bytes; hex bytes may be concatenated or separated by white space)
 * Vx Sets reading mode verbose off/on
 * Tx sets new delay value, to be used for future packets (default 0)
 * Ox sets new delay timeout value, used immediately (default 2500)
 * Xx calls P1PsSerial.SetEcho(x)    sets Echo on/off
 * Gx Sets crc_gen (defaults to CRC_GEN=0xD9)
 * Hx Sets crc_feed (defaults to CRC_FEED=0x00)
 * V  Display current verbose mode
 * G  Display current crc_gen value
 * H  Display current crc_feed value
 * X  Display current echo status
 * T  Display current delay value
 * T  Display current delay timeout value
 * * comment lines starting with an asterisk are ignored (or copied in verbose mode)
 * These commands are case-insensitive
 * Maximum line length is 99 bytes (allowing "W 00 00 [..] 00[\r]\n" format)
 *
 *     Thanks to Krakra for providing coding suggestions, the hints and references to the HBS and MM1192 documents on
 *     https://community.openenergymonitor.org/t/hack-my-heat-pump-and-publish-data-onto-emoncms, 
 *     and to Paul Stoffregen for publishing the AltSoftSerial library.
 *
 * This program is based on the public domain Echo program from the
 *     Alternative Software Serial Library, v1.2
 *     Copyright (c) 2014 PJRC.COM, LLC, Paul Stoffregen, paul@pjrc.com
 *     (AltSoftSerial itself is available under the MIT license, see
 *      http://www.pjrc.com/teensy/td_libs_AltSoftSerial.html, 
 *      but please note that P1P2Monitor and P1P2Serial are licensed under the GPL v2.0)
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
                        // These values can be changed via serial port

P1P2Serial P1P2Serial;

void setup() {
  Serial.begin(115200);
  while (!Serial) ; // wait for Arduino Serial Monitor to open
  Serial.println(F("*"));
  Serial.println(F("*P1P2Serial-arduino monitor v0.9.5"));
  Serial.println(F("*"));
  delay (5000); // give ESP time to boot, it doesn't like serial input while initiating wifi
  P1P2Serial.begin(9600);
}

#define RS_SIZE 99      // buffer to store data read from serial port, max line length on serial input is 99 (3 characters per byte, plus 'W" and '\r\n')
static char RS[RS_SIZE];
#define WB_SIZE 32       // buffer to store raw data for writing to P1P2bus, max packet size is 32 (have not seen anytyhing over 24 (23+CRC))
static byte WB[WB_SIZE];
#define RB_SIZE 33       // buffers to store raw data and error codes read from P1P2bus; 1 extra for reading back CRC byte
static byte RB[RB_SIZE];
static byte EB[RB_SIZE];

static byte verbose = 1;      // By default include timing and error information in output
static int sd = 0;            // for storing delay setting for each packet written
static int sdto = 2500;       // for storing delay timeout setting
static byte echo = 0;         // echo setting (whether written data is read back)

// next 2 functions are used to save on dynamic memory usage in main loop
int scanint(char* s, int &b) {
  return sscanf(s,"%d",&b);
}

int scanhex(char* s, int &b) {
  return sscanf(s,"%2x",&b);
}

void loop() {
  static byte crc_gen = CRC_GEN;
  static byte crc_feed = CRC_FEED;
  int temp;
  int wb = 0; int wbp = 1; int n; int wbtemp;
  if (Serial.available()) {
    byte rs = Serial.readBytesUntil('\n', RS, RS_SIZE-1);
    if (RS[rs-1] == '\r') rs--;
    RS[rs] = '\0';
    if (rs) switch (RS[0]) {
      case '*': if (verbose) Serial.println(RS); 
                break;
      case 'g' :
      case 'G': if (verbose) Serial.print(F("* Crc_gen ")); 
                if (scanhex(&RS[1],temp) == 1) { if (verbose) Serial.print(F("set to ")); crc_gen = temp; }
                if (verbose) { Serial.print("0x"); if (crc_gen <= 9) Serial.print(F("0")); Serial.println(crc_gen,HEX); }
                break;
      case 'h' :
      case 'H': if (verbose) Serial.print(F("* Crc_feed ")); 
                if (scanhex(&RS[1],temp) == 1) { if (verbose) Serial.print(F("set to ")); crc_feed = temp; }
                if (verbose) { Serial.print("0x"); if (crc_feed <= 9) Serial.print(F("0")); Serial.println(crc_feed,HEX); }
                break;
      case 'v' :
      case 'V': Serial.print(F("* Verbose "));
                if (scanint(&RS[1], temp) == 1) {
                  if (temp) temp = 1; 
                  Serial.print(F("set to "));
                  verbose = temp;
                } 
                Serial.println(verbose);
                break;
      case 't' :
      case 'T': if (verbose) Serial.print(F("* Delay ")); 
                if (scanint(&RS[1], sd) == 1) if (verbose) Serial.print(F("set to "));
                if (verbose) Serial.println(sd);
                break;
      case 'o' :
      case 'O': if (verbose) Serial.print(F("* DelayTimeout ")); 
                if (scanint(&RS[1], sdto) == 1) { if (verbose) Serial.print(F("set to ")); P1P2Serial.setDelayTimeout(sdto); }
                if (verbose) Serial.println(sdto);
                break;
      case 'x' :
      case 'X': if (verbose) Serial.print(F("* Echo ")); 
                if (scanint(&RS[1], temp) == 1) {
                  if (temp) temp = 1; 
                  echo = temp;
                  P1P2Serial.setEcho(echo);
                  if (verbose) Serial.print(F("set to "));
                }
                Serial.println(echo);
                break;
      case 'w' :
      case 'W': if (verbose) Serial.print(F("* Writing: "));
                while ((wb < WB_SIZE) && (sscanf(&RS[wbp], "%2x%n", &wbtemp, &n) == 1)) {
                  WB[wb++] = wbtemp;
                  wbp += n;
                  if (verbose) { if (wbtemp <= 0x0F) Serial.print("0"); Serial.print(wbtemp, HEX);}
                }
                if (verbose) Serial.println();
                P1P2Serial.writepacket(WB, wb, sd, crc_gen, crc_feed);
                break;
      default : Serial.print(F("* Command not understood: "));
                Serial.println(RS);
                break;
    }
  }
  if (P1P2Serial.available()) {
    uint16_t delta;
    int n = P1P2Serial.readpacket(RB, delta, EB, RB_SIZE, crc_gen, crc_feed);
    if (verbose) {
      Serial.print(F("R"));
      if (delta < 10000) Serial.print(F(" "));
      if (delta < 1000) Serial.print(F("0")); else { Serial.print(delta / 1000);delta %= 1000; };
      Serial.print(F("."));
      if (delta < 100) Serial.print(F("0"));
      if (delta < 10) Serial.print(F("0"));
      Serial.print(delta);
      Serial.print(F(": "));
    }
    for (int i = 0;i<n;i++) {
      if (verbose && (EB[i] & ERROR_READBACK)) {
        // collision suspicion due to verification error in reading back written data
        Serial.print(F("-XX:"));
      }
      if (verbose && (EB[i] & ERROR_PE)) {
        // parity error detected
        Serial.print(F("-PE:"));
      }
      byte c = RB[i];
      if (crc_gen && verbose && (i == n-1)) {
        Serial.print(F(" CRC="));
      }
      if (c < 0x10) Serial.print(F("0"));
      Serial.print(c, HEX);
      if (verbose && (EB[i] & ERROR_OVERRUN)) {
        // buffer overrun detected (overrun is after, not before, the read byte)
        Serial.print(F(":OR-"));
      }
      if (verbose && (EB[i] & ERROR_CRC)) {
        // CRC error detected in readpacket
        Serial.print(F(" CRC error"));
      }
    }
    Serial.println();
  }
}
