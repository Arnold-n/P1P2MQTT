/* P1P2Monitor: monitor program for reading Daikin/Rotex P1/P2 bus using P1P2Serial library
 *               * includes rudimentary code to control DHW on/off and cooling/heating on/off
 *               * the adapter behaves as an external controller like the BRP* products
 *               * Control has only been tested on EHYHBX08AAV3 and EHVX08S23D6V
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20190824 v0.9.6 Added limited control functionality
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
 *
 * P1P2Monitor Configuration is done by sending one of the following lines over serial 
 *
 *    (L,W,Z are new and are for controlling DHW on/off and heating/cooling on/off:)
 * Lx sets controller functionality off (x=0), on with address 0xF0 (x=1), on with address 0xF1 (x=2)
 * Wx sets DHW on/off
 * Zx sets cooling on/off
 * Px sets the parameter number in packet type 35 (0x00..0xFF) to use for heating/cooling on/off (default 0x31 forEHVX08S23D6V, use 0x2F for EHYHBX08AAV3)
 * L reports controller status
 * W reports DHW status as reported by main controller
 * Z reports cooling status as reported by main controller
 * P reports parameter number in packet type 35 used for heating/cooling on/off
 *
 * W<hex data> Write packet (max 32 bytes) (no 0x prefix should be used for the hex bytes; hex bytes may be concatenated or separated by white space)
 * Vx Sets reading mode verbose off/on
 * Tx sets new delay value, to be used for future packets (default 0)
 * Ox sets new delay timeout value, used immediately (default 2500)
 * Xx calls P1PsSerial.SetEcho(x)    sets Echo on/off
 * Gx Sets crc_gen (defaults to CRC_GEN=0xD9)
 * Hx Sets crc_feed (defaults to CRC_FEED=0x00)
 * V  Display current verbose mode
 * T  Display current delay value
 * O  Display current delay timeout value
 * X  Display current echo status
 * G  Display current crc_gen value
 * H  Display current crc_feed value
 * * comment lines starting with an asterisk are ignored (or copied in verbose mode)
 * These commands are case-insensitive
 * Maximum line length is 99 bytes (allowing "W 00 00 [..] 00[\r]\n" format)
 *
 *     Thanks to Krakra for providing coding suggestions, the hints and references to the HBS and MM1192 documents on
 *     https://community.openenergymonitor.org/t/hack-my-heat-pump-and-publish-data-onto-emoncms, 
 *     to Luis Lamich Arocas for sharing logfiles and testing the new controlling functions for the EHVX08S23D6V,
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
byte setParam = 0x31;   // Parameter in packet type 35 for switching cooling/heating on/off
                        // 0x31 works on EHVX08S23D6V
                        // 0x2F works on EHYHBX08AAV3

P1P2Serial P1P2Serial;

void setup() {
  Serial.begin(115200);
  while (!Serial);      // wait for Arduino Serial Monitor to open
  Serial.println(F("*"));
  Serial.println(F("*P1P2Monitor v0.9.6 (limited controller functionality)"));
  Serial.println(F("*"));
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

#define CONTROL_ID0 0xF0     // first external controller
#define CONTROL_ID1 0xF1     // second external controller
byte CONTROL_ID=0x00;

byte setDHW = 0;
byte setDHWstatus = 0;
byte DHWstatus = 0;
byte setcooling = 0;
byte setcoolingstatus = 0;
byte coolingstatus = 0;

void loop() {
  static byte crc_gen = CRC_GEN;
  static byte crc_feed = CRC_FEED;
  int temp;
  int wb = 0; int wbp = 1; int n; int wbtemp;
  if (Serial.available()) {
    byte rs = Serial.readBytesUntil('\n', RS, RS_SIZE - 1);
    if ((rs > 0) && (RS[rs-1] == '\r')) rs--;
    RS[rs] = '\0';
    if (rs) switch (RS[0]) {
      case '\0': if (verbose) Serial.println("* Empty line received");
                break;
      case '*': if (verbose) Serial.println(RS); 
                break;
      case 'g':
      case 'G': if (verbose) Serial.print(F("* Crc_gen "));
                if (scanhex(&RS[1],temp) == 1) { 
                  crc_gen = temp; 
                  if (!verbose) break; 
                  Serial.print(F("set to ")); crc_gen = temp; 
                }
                Serial.print("0x"); 
                if (crc_gen <= 0x0F) Serial.print(F("0")); 
                Serial.println(crc_gen, HEX);
                break;
      case 'h':
      case 'H': if (verbose) Serial.print(F("* Crc_feed "));
                if (scanhex(&RS[1],temp) == 1) { 
                  crc_feed = temp; 
                  if (!verbose) break; 
                  Serial.print(F("set to ")); 
                }
                Serial.print("0x"); 
                if (crc_feed <= 0x0F) Serial.print(F("0")); 
                Serial.println(crc_feed, HEX);
                break;
      case 'v':
      case 'V': Serial.print(F("* Verbose "));
                if (scanint(&RS[1], temp) == 1) {
                  if (temp) temp = 1;
                  Serial.print(F("set to "));
                  verbose = temp;
                }
                Serial.println(verbose);
                break;
      case 't':
      case 'T': if (verbose) Serial.print(F("* Delay "));
                if (scanint(&RS[1], sd) == 1) { 
                  if (!verbose) break; 
                  Serial.print(F("set to "));
                }
                Serial.println(sd);
                break;
      case 'o':
      case 'O': if (verbose) Serial.print(F("* DelayTimeout "));
                if (scanint(&RS[1], sdto) == 1) { 
                  P1P2Serial.setDelayTimeout(sdto); 
                  if (!verbose) break; 
                  Serial.print(F("set to ")); 
                }
                Serial.println(sdto);
                break;
      case 'x':
      case 'X': if (verbose) Serial.print(F("* Echo "));
                if (scanint(&RS[1], temp) == 1) {
                  if (temp) temp = 1;
                  echo = temp;
                  P1P2Serial.setEcho(echo);
                  if (!verbose) break;
                  Serial.print(F("set to "));
                }
                Serial.println(echo);
                break;
      case 'w':
      case 'W': if (CONTROL_ID) {
                  if (verbose) Serial.println(F("* Write refused; controller is on")); 
                  break;
                } // don't write while controller is on
                if (verbose) Serial.print(F("* Writing: "));
                while ((wb < WB_SIZE) && (sscanf(&RS[wbp], "%2x%n", &wbtemp, &n) == 1)) {
                  WB[wb++] = wbtemp;
                  wbp += n;
                  if (verbose) { 
                    if (wbtemp <= 0x0F) Serial.print("0"); 
                    Serial.print(wbtemp, HEX);
                  }
                }
                if (verbose) Serial.println();
                P1P2Serial.writepacket(WB, wb, sd, crc_gen, crc_feed);
                break;
      case 'l': // set external controller answering function on/off and set CONTROL_ID address
      case 'L': if (verbose) Serial.print(F("* Control_id ")); 
                if (scanint(&RS[1], temp) == 1) {
                  if (temp > 2) temp = 2; 
                  switch (temp) {
                    case 0 : CONTROL_ID = 0x00; break;
                    case 1 : CONTROL_ID = CONTROL_ID0; break;
                    case 2 : CONTROL_ID = CONTROL_ID1; break;
                    default: break;
                  }
                  if (!verbose) break;
                  Serial.print(F("set to 0x")); 
                }
                Serial.print("0x");
                if (CONTROL_ID <= 0x0F) Serial.print(F("0")); 
                Serial.println(CONTROL_ID, HEX);
                break;
                
      case 'y': // set DHW on/off
      case 'Y': if (verbose) Serial.print(F("* DHW ")); 
                if (scanint(&RS[1], temp) == 1) {
                  setDHW = 1;
                  setDHWstatus = temp;
                  if (!verbose) break;
                  Serial.print(F("set to ")); 
                  Serial.println(setDHWstatus);
                  break;
                }
                Serial.println(DHWstatus);
                break;
      case 'p': // select parameter to write in z step below
      case 'P': if (verbose) Serial.print(F("* Param2Write ")); 
                if (scanhex(&RS[1], temp) == 1) {
                  setParam = temp;
                  if (!verbose) break;
                  Serial.print(F("set to 0x")); 
                  Serial.println(setParam, HEX);
                  break;
                }
                Serial.println(setParam, HEX);
                break;
      case 'z' :// set heating/cooling mode on/off
      case 'Z': if (verbose) Serial.print(F("* Cooling/heating ")); 
                if (scanint(&RS[1], temp) == 1) {
                  setcooling = 1;
                  setcoolingstatus = temp;
                  if (!verbose) break;
                  Serial.print(F("set to ")); 
                  Serial.println(setcoolingstatus);
                  break;
                }
                Serial.println(coolingstatus);
                break;
      case 'u':
      case 'U':
      case 's':
      case 'S': // u and x: reserved for esp configuration
      default : Serial.print(F("* Command not understood: "));
                Serial.println(RS);
                break;
    }
  }
  if (P1P2Serial.packetavailable()) {
    uint16_t delta;
    int n = P1P2Serial.readpacket(RB, delta, EB, RB_SIZE, crc_gen, crc_feed);
    // perhaps we need to remember which packets are announced in the 00F030 packet,
    // though I think that is not necessary.
    // if ((RB[0] == 0x00) && (RB[1] == CONTROL_ID) && (RB[2] == 0x30)) {
    //   for (byte i = 3; i < n; i++) writeFx[i] = RB[i];
    // }
    byte w;
    if ((n > 4) && (RB[0] == 0x00) && (RB[1] == CONTROL_ID) && ((RB[2] & 0x30) == 0x30)) {
      WB[0] = 0x40;
      WB[1] = RB[1];
      WB[2] = RB[2];
      if (crc_gen) n--; // omit CRC from received-byte-counter
      switch (RB[2]) {
        case 0x30 : // in: 17 byte; out: 17 byte; out pattern WB[7] should contain a 01 if we want to communicate a new setting
                    for (w = 3; w < n; w++) WB[w] = 0x00;
                    // set byte WB[7] to 0x01 for triggering F035 to 0x01 if setcooling/setDHW needs to be changed to request a F035 message
                    if (setDHW || setcooling) WB[7] = 0x01;
                    P1P2Serial.writepacket(WB, n, 25, crc_gen, crc_feed);
                    break;
        case 0x31 : // in: 15 byte; out: 15 byte; out pattern is copy of in pattern except for 2 bytes RB[7] RB[8]; function unknown.. copy primary controller bytes for now
                    for (w = 3; w < n; w++) WB[w] = RB[w];
                    WB[7] = 0xB4;
                    WB[8] = 0x10; // could be part of serial nr? If so, would it matter that we use the same nr as the BRP?
                    P1P2Serial.writepacket(WB, n, 25, crc_gen, crc_feed);
                    break;
        case 0x32 : // in: 19 byte: out 19 byte, out is copy in
                    for (w = 3; w < n; w++) WB[w] = RB[w];
                    P1P2Serial.writepacket(WB, n, 25, crc_gen, crc_feed);
                    break;
        case 0x33 : // not seen
                    break;
        case 0x34 : // not seen
                    break;
        case 0x35 : // in: 21 byte; out 21 byte; in is parameters; out is FF except for first packet which starts with 930000 (why?)
                    // parameters in the 00F035 message may indicate status changes in the heat pump
                    // A parameter consists of 3 bytes: 1 byte nr, and (likely) 2 byte value
                    // for now we only check for the following parameters: 
                    // DHW 400000
                    // cooling 310000
                    // change bytes for triggering cooling on/off
                    for (w = 3; w < n; w++) WB[w] = 0xFF;
                    w = 3;
                    // if (cooling/DHW)  WB[3..5 (6..8)] = 31000/40000
                    if (setDHW) { WB[w++] = 0x40; WB[w++] = 0x00; WB[w++] = setDHWstatus; setDHW = 0; }
                    if (setcooling) { WB[w++] = setParam; WB[w++] = 0x00; WB[w++] = setcoolingstatus; setcooling = 0; }
                    P1P2Serial.writepacket(WB, n, 25, crc_gen, crc_feed);
                    for (w = 3; w < n; w++) if ((RB[w] == setParam) && (RB[w+1] == 0x00)) coolingstatus = RB[w+2];
                    for (w = 3; w < n; w++) if ((RB[w] == 0x40) && (RB[w+1] == 0x00)) DHWstatus = RB[w+2];
                    break;
        case 0x36 : // in: 23 byte; out 23 byte; in is parameters??; out is FF
                    // fallthrough
        case 0x37 : // not seen, but we still reply with a packet with the same length and only FF as payload for devices that do request this type of message
                    // fallthrough
        case 0x38 : // not seen on EHVX08S23D6VXX, seen on hybrid, we don't know what the reply should be, 
                    // but let's also reply with a packet with the same length and only FF as payload
                    // fallthrough
        case 0x39 : // in: 21 byte; out 21 byte; in is parameters??; out is FF
                    // fallthrough
        case 0x3A : // in: 21 byte; out 21 byte; in is parameters??; out is FF
                    // fallthrough
        case 0x3B : // not seen on EHVX08S23D6VXX, seen on hybrid, we don't know what the reply should be, 
                    // but let's also reply with a packet with the same length and only FF as payload
                    // fallthrough
        case 0x3C : // in: 23 byte; out 23 byte; in is parameters??; out is FF
                    // fallthrough
        case 0x3D : // not seen on EHVX08S23D6VXX, seen on hybrid, we don't know what the reply should be, 
                    // but let's also reply with a packet with the same length and only FF as payload
                    for (w = 3; w < n - 1; w++) WB[w] = 0xFF;
                    P1P2Serial.writepacket(WB, n, 25, crc_gen, crc_feed);
                    break;
        case 0x3E : if (RB[3]) {
                      // 0x3E01, 0x3E02, ... in: 23 byte; out: 23 byte; out 40F13E01(even for higher) + 19xFF
                      WB[3] = 0x01;
                      for (w = 4; w < n; w++) WB[w] = 0xFF;
                      P1P2Serial.writepacket(WB, n, 25, crc_gen, crc_feed);
                    } else {
                      // 0x3E00 in: 8 byte out: 8 byte; 40F13E00 (4x FF)
                      WB[3] = 0x00;
                      for (w = 4; w < n; w++) WB[w] = 0xFF;
                      P1P2Serial.writepacket(WB, n, 25, crc_gen, crc_feed);
                    }
                    break;
        default   : // not seen
                    break;
      }
      if (crc_gen) n++; // reverse CRC exclusion - include CRC in serial output
    }
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
    for (int i = 0;i < n;i++) {
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
