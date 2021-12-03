/* P1P2Monitor: Monitor for reading Daikin/Rotex P1/P2 bus using P1P2Serial library, output in json format, over UDP,
 *           and limited control (DHW on/off, cooling/heating on/off) for some models.
 *           Control has only been tested on EHYHBX08AAV3 and EHVX08S23D6V
 *           If too many options are selected (SAVEHISTORY, JSON, MQTTTOPICS, OUTPUTUDP, OUTPUTSERIAL, MONITOR, MONITORCONTROL, MONITORSERIAL) this program
 *           will not fit in an Arduino Uno, but it will fit in an Arduino Mega.
 *
 * Copyright (c) 2019-2020 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20200109 v0.9.11 Added option to insert counter request after 000012 packet (for KLIC-DA)
 * 20191018 v0.9.10 Added MQTT topic like output over serial/udp, and parameter support for EHVX08S26CA9W (both thanks to jarosolanic)
 * 20190914 v0.9.9 Controller/write safety related improvements; automatic controller ID detection
 * 20190908 v0.9.8 Minor improvements: error handling, hysteresis, improved output (a.o. x0Fx36)
 * 20190829 v0.9.7 Improved Serial input handling, added B8 counter request
 * 20190828        Fused P1P2Monitor and P1P2json-mega (with udp support provided by Budulinek)
 * 20190827 v0.9.6 Added limited control functionality, json conversion files merged with esp8266 files
 * 20190820 v0.9.5 Changed delay behaviour, timeout added
 * 20190817 v0.9.4 Brought in line with library 0.9.4 (API changes), print last byte if crc_gen==0, added json support
 *                 See comments below for description of serial protocol
 * 20190505 v0.9.3 Changed error handling and corrected deltabuf type in readpacket; added mod12/mod13 counters
 * 20190428 v0.9.2 Added setEcho(b), readpacket() and writepacket(); LCD support added
 * 20190409 v0.9.1 Improved setDelay()
 * 20190407 v0.9.0 Improved reading, writing, and meta-data; added support for timed writings and collision detection; added stand-alone hardware-debug mode
 * 20190303 v0.0.1 initial release; support for raw monitoring and hex monitoring
 *
 * Serial protocol from/to Arduino Uno/Mega:
 * -----------------------------------------
 *
 *
 *
 * P1P2Monitor Configuration is done by sending one of the following lines over serial 
 *
 *    (L, W, Z, P are new and are for controlling DHW on/off and heating/cooling on/off:)
 * Lx sets controller functionality off (x=0), on (x=1), address (0xF0/0xF1) autodetected
 * Yx sets DHW on/off
 * Zx sets cooling on/off
 * Px sets the parameter number in packet type 35 (0x00..0xFF) to use for heating/cooling on/off (default 0x31 forEHVX08S23D6V, use 0x2F for EHYHBX08AAV3)
 * L reports controller status
 * Y reports DHW status as reported by main controller
 * Z reports cooling/heating status as reported by main controller
 * P reports parameter number in packet type 35 used for cooling/heating on/off
 *
 * Ux Sets mode whether to include (in json format) unknown bits and bytes off/on (if JSON is defined) (default off)
 * Sx Sets mode whether to include (in json format) only changed parameters (if JSON is defined) (default on)
 * U  Display display-unknown mode (if JSON is defined)
 * S  Display changed-only mode (if JSON is defined)
 *
 * W<hex data> Write packet (max 32 bytes) (no 0x prefix should be used for the hex bytes; hex bytes may be concatenated or separated by white space)
 * Cx counterrequest triggers (x=1:) single cycle or (x=2:) repetitive cycles of 6 B8 packets to request counters from heat pump; temporarily blocks controller function
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
 *     Thanks to Krakra for providing coding suggestions and UDP support, the hints and references to the HBS and MM1192 documents on
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

#include "P1P2Config.h"
#include <P1P2Serial.h>

P1P2Serial P1P2Serial;

#if defined JSON || defined MQTTTOPICS
// these variables need to be set before including header file:
static bool outputunknown = OUTPUTUNKNOWN; // whether json parameters include parameters even if functionality is unknown
static bool changeonly =  CHANGEONLY;      // whether json parameters are included only if changed
                                           //   (only for parameters for which savehistory() is active)
#include "P1P2_Daikin_ParameterConversion_EHYHB.h"

#ifdef OUTPUTUDP
// send json as an UDP message (W5500 compatible ethernet shield is required)                          
//    Connections W5500: Arduino Mega pins 50 (MISO), 51 (MOSI), 52 (SCK), and 10 (SS) (https://www.arduino.cc/en/Reference/Ethernet)
//                       Pin 53 (hardware SS) is not used but must be kept as output. In addition, connect RST, GND, and 5V.
#include <Ethernet.h>
#include <EthernetUdp.h>
#include "NetworkParams.h"

//EthernetUDP udpRecv;                                              // for future functionality....
EthernetUDP udpSend;
#endif /* OUTPUTUDP */
#endif /* JSON || MQTTTOPICS */

// copied from Stream.cpp, adapted to have readBytesUntil() include the terminator character:
// with the regular Serial.readBytesUntil(), we can't distinguish a time-out from a terminated string which is not too long
int timedRead()
{
  static unsigned long startMillis;
  int c;
  startMillis = millis();
  do {
    c = Serial.read();
    if (c >= 0) return c;
  } while(millis() - startMillis < 5); // timeout 5 ms
  return -1;     // -1 indicates timeout
}

size_t readBytesUntilIncluding(char terminator, char *buffer, size_t length)
{
  if (length < 1) return 0;
  size_t index = 0;
  while (index < length) {
    int c = timedRead();
    if (c < 0) break;
    *buffer++ = (char)c;
    index++;
    if (c == terminator) break;
  }
  return index; // return number of characters, not including null terminator
}

void setup() {
  Serial.begin(115200);
  while (!Serial);      // wait for Arduino Serial Monitor to open
  // Serial.setTimeout(5); // hardcoded in timedRead above
  Serial.println(F("*"));
  Serial.print(F("* P1P2"));
#ifdef MONITOR
  Serial.print(F("Monitor"));
#ifdef MONITORSERIAL
  Serial.print(F("+serial"));
#endif /* MONITORSERIAL */
#ifdef MONITORCONTROL
  Serial.print(F("+control"));
#endif /* MONITORCONTROL */
#endif /* MONITOR */
#ifdef JSON
  Serial.print(F("+json"));
#endif /* JSON */
#ifdef MQTTTOPICS
  Serial.print(F("+mqtttopics"));
#endif /* MQTTTOPICS */
#ifdef OUTPUTUDP
  Serial.print(F("+udp"));
#endif /* OUTPUTUDP */
#ifdef OUTPUTSERIAL
  Serial.print(F("+serial"));
#endif /* OUTPUTSERIAL */
#ifdef SAVEHISTORY
  Serial.print(F("+savehist"));
#endif
#ifdef KLICDA
  Serial.print(F("+klicda"));
#endif /* KLICDA */
  Serial.println();
  Serial.println("*");
  // Initial parameters
  Serial.print(F("* outputunknown="));
  Serial.println(OUTPUTUNKNOWN);
  Serial.print(F("* changeonly="));
  Serial.println(CHANGEONLY);
  Serial.print(F("* counterrepeatingrequest="));
  Serial.println(COUNTERREPEATINGREQUEST);
#ifdef KLICDA
#ifdef KLICDA_delay
  Serial.print(F("* klicda_delay="));
  Serial.println(KLICDA_delay);
#endif /* KLICDA_delay */
#endif /* KLICDA */
  Serial.print(F("* CONTROL_ID_DEFAULT=0x"));
  Serial.println(CONTROL_ID_DEFAULT, HEX);
  Serial.print(F("* F030DELAY="));
  Serial.println(F030DELAY);
  Serial.print(F("* F03XDELAY="));
  Serial.println(F03XDELAY);
  Serial.println(F("* v0.9.11 20200108"));
  Serial.println(F("*"));
  P1P2Serial.begin(9600);
#if defined JSON || defined MQTTTOPICS
#ifdef OUTPUTUDP
  Ethernet.begin(mac, ip);
//  udpRecv.begin(listenPort);                                      // for future functionality...
  udpSend.begin(sendPort);
#endif /* OUTPUTUDP */
#endif /* JSON || MQTTTOPICS */
}

int8_t FxAbsentCnt[2]={-1, -1}; // FxAbsentCnt[x] counts number of unanswered 00Fx30 messages; 
                        // if -1 than no Fx request or response seen (relevant to detect whether F1 controller is supported or not)
byte setParam = PARAM_HC_ONOFF;
byte setParamDHW = PARAM_DHW_ONOFF; 

#define RS_SIZE 99      // buffer to store data read from serial port, max line length on serial input is 99 (3 characters per byte, plus 'W" and '\r\n')
static char RS[RS_SIZE];
#define WB_SIZE 32       // buffer to store raw data for writing to P1P2bus, max packet size is 32 (have not seen anytyhing over 24 (23+CRC))
static byte WB[WB_SIZE];
#define RB_SIZE 33       // buffers to store raw data and error codes read from P1P2bus; 1 extra for reading back CRC byte
static byte RB[RB_SIZE];
static byte EB[RB_SIZE];

static byte verbose = 1;      // By default include timing and error information in output
static int sd = 50;           // for storing delay setting for each packet written (changed to 50ms which seems to be a bit safer than 0)
static int sdto = 2500;       // for storing delay timeout setting
static byte echo = 0;         // echo setting (whether written data is read back)

#ifdef JSON
static byte jsonterm = 1; // indicates whether json string has been terminated or not
#endif /* JSON */

#if defined JSON || defined MQTTTOPICS
void process_packet(byte* rb, int n) {
  char key[KEYLEN + 9]; // 9 character prefix for topic
  char value[KEYLEN];
  byte cat; // used for esp but not for Aduino
  strcpy(key,"P1P2/P/U/");
  for (byte i = 3; i < n; i++) {
    int kvrbyte = bytes2keyvalue(rb, i, 8, key + 9, value, cat);
    // returns 0 if byte does not trigger any output
    // returns 1 if a new value should be output
    // returns 8 if byte should be treated per bit
    // returns 9 if json string should be terminated (intended for ESP8266 only), on Arduino
    //           we terminate json string after each packet read so we can ignore kvrbyte=9 signal
    if (kvrbyte != 9) {
      for (byte j = 0; j < kvrbyte; j++) {
        int kvr = (kvrbyte == 8) ? bytes2keyvalue(rb, i, j, key + 9, value, cat) : kvrbyte;
        if (kvr) {
#ifdef MQTTTOPICS
          switch (cat) {
            case 1 : key[7] = 'P'; // parameter settings
                     break;
            case 2 : key[7] = 'M'; // measurements, time, date
                     break;
            case 3 : key[7] = 'F'; // F related params
                     break;
            case 4 : key[7] = 'T'; // Temp Flow Power measurements
                     break;
            case 0 : // fallthrough
            default: key[7] = 'U'; // unknown
          }
#ifdef OUTPUTSERIAL
          Serial.print(F("T "));
          Serial.print(key);
          Serial.print(F(":"));
          Serial.println(value);
#endif /* OUTPUTSERIAL */
#ifdef OUTPUTUDP
          udpSend.beginPacket(sendIpAddress, remPort);
          udpSend.print(key);
          udpSend.print(F(":"));
          udpSend.print(value);
          udpSend.endPacket();
#endif /* OUTPUTUDP */
#endif /* MQTTTOPICS */
#ifdef JSON
          if (jsonterm) {
#ifdef OUTPUTSERIAL
            Serial.print(F("J {\"")); 
#endif /* OUTPUTSERIAL */
#ifdef OUTPUTUDP
            udpSend.beginPacket(sendIpAddress, remPort);
            udpSend.print(F("{\""));
#endif /* OUTPUTUDP */
            jsonterm = 0;
          } else {
#ifdef OUTPUTSERIAL
            Serial.print(F(",\""));
#endif /* OUTPUTSERIAL */
#ifdef OUTPUTUDP
            udpSend.print(F(",\""));
#endif /* OUTPUTUDP */
          }
#ifdef OUTPUTSERIAL
          Serial.print(key + 9);
          Serial.print(F("\":"));
          Serial.print(value);
#ifdef MQTTTOPICS
          // terminate each json parameter to allow mqtt topics to be printed on separate lines
          jsonterm = 1;
          Serial.println(F("}"));
#endif /* MQTTTOPICS */
#endif /* OUTPUTSERIAL */
#ifdef OUTPUTUDP
          udpSend.print(key + 9);
          udpSend.print(F("\":"));
          udpSend.print(value);
#endif /* OUTPUTUDP */
#endif /* JSON */
        }
      }
    }
  }
#ifdef JSON
  if (jsonterm == 0) {
    // only terminate json string if any parameter was written
    jsonterm = 1;
#ifdef OUTPUTSERIAL
    Serial.println(F("}"));
#endif /* OUTPUTSERIAL */
#ifdef OUTPUTUDP
    udpSend.print(F("}"));
    udpSend.endPacket();
#endif /* OUTPUTUDP */
  }
#endif /* JSON */
#ifdef SAVEHISTORY
  savehistory(rb, n);
#endif /* SAVEHISTORY */
}
#endif /* JSON || MQTTTOPICS */

// next 2 functions are used to save on dynamic memory usage in main loop
int scanint(char* s, int &b) {
  return sscanf(s,"%d",&b);
}

int scanhex(char* s, int &b) {
  return sscanf(s,"%2x",&b);
}

byte CONTROL_ID = CONTROL_ID_DEFAULT;

byte setDHWrequest = 0;
byte setDHWstatus = 0;
byte DHWstatus = 0;
byte setcoolingheatingrequest = 0;
byte setcoolingheatingstatus = 0;
byte coolingheatingstatus = 0;
byte counterrequest = 0;
byte counterrepeatingrequest = COUNTERREPEATINGREQUEST;
byte Tmin = 61;
byte Tminprev = 61;
byte Thour = 26;
byte TDoW = 9;

void loop() {
  static byte crc_gen = CRC_GEN;
  static byte crc_feed = CRC_FEED;
  int temp;
  int wb = 0; int wbp = 1; int n; int wbtemp;
  static byte ignoreremainder = 1; // 1 is more robust to avoid misreading a partial message upon reboot, but be aware the first line received will be ignored.
#ifdef MONITOR
  if (Serial.available()) {
    byte rs = readBytesUntilIncluding('\n', RS, RS_SIZE - 1);
    if (rs > 0) {
      if (ignoreremainder) { 
        // we should ignore this serial input block except for this check:
        // if this message ends with a '\n', we should act on next serial input
        if (RS[rs - 1] == '\n') ignoreremainder = 0;
      } else if (RS[rs - 1] != '\n') {
        // message not terminated with '\n', too long or interrupted, so ignore
        ignoreremainder = 1; // message without \n, ignore next input(s)
        if (verbose) {
          Serial.println(F("* Input line too long, interrupted or not EOL-terminated, ignored"));
        }
      } else {
        if ((--rs > 0) && (RS[rs - 1] == '\r')) rs--;
        RS[rs] = '\0';
        switch (RS[0]) {
          case '\0': if (verbose) Serial.println(F("* Empty line received"));
                    break;
          case '*': if (verbose) Serial.println(RS); 
                    break;
          case 'g':
          case 'G': if (verbose) Serial.print(F("* Crc_gen "));
                    if (scanhex(&RS[1], temp) == 1) { 
                      crc_gen = temp; 
                      if (!verbose) break; 
                      Serial.print(F("set to "));
                    }
                    Serial.print("0x"); 
                    if (crc_gen <= 0x0F) Serial.print(F("0")); 
                    Serial.println(crc_gen, HEX);
                    break;
          case 'h':
          case 'H': if (verbose) Serial.print(F("* Crc_feed "));
                    if (scanhex(&RS[1], temp) == 1) { 
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
                      if (sd < 2) {
                        sd = 2;
                        Serial.print(F("[use of delay 0 or 1 not recommended, increasing to 2] "));
                      }
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
                    if (P1P2Serial.writeready()) {
                      P1P2Serial.writepacket(WB, wb, sd, crc_gen, crc_feed);
                    } else {
                      Serial.println(F("* Refusing to write packet while previous packet wasn't finished"));
                    }
                    break;
          case 'l': // set external controller function on/off; CONTROL_ID address is set automatically
          case 'L': if (scanint(&RS[1], temp) == 1) {
                      if (temp > 1) temp = 1; 
                      if (temp) {
                        if (FxAbsentCnt[0] == F0THRESHOLD) {
                          Serial.println(F("* Control_ID 0xF0 supported, no external controller found for 0xF0, switching control functionality on 0xF0 on"));
                          CONTROL_ID = 0xF0;
                        } else if (FxAbsentCnt[1] == F0THRESHOLD) {
                          Serial.println(F("* Control_ID 0xF1 supported, no external controller found for 0xF1, switching control functionality on 0xF1 on"));
                          CONTROL_ID = 0xF1;
                        } else {
                          Serial.println(F("* No free slave address for controller found. Control functionality not enabled. You may wish to re-try later (and perhaps switch external controllers off)"));
                          CONTROL_ID = 0x00;
                        }
                      } else {
                        CONTROL_ID = 0x00;
                      }
                      if (!verbose) break;
                      Serial.print(F("* Control_id set to 0x")); 
                      if (CONTROL_ID <= 0x0F) Serial.print("0"); 
                      Serial.println(CONTROL_ID, HEX);
                      break;
                    }
                    if (verbose) Serial.print(F("* Control_id is 0x")); 
                    if (CONTROL_ID <= 0x0F) Serial.print("0"); 
                    Serial.println(CONTROL_ID, HEX);
                    break;
          case 'y': // set DHW on/off
          case 'Y': if (verbose) Serial.print(F("* DHW ")); 
                    if (scanint(&RS[1], temp) == 1) {
                      setDHWrequest = 1;
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
                      Serial.print(F("set to ")); 
                    }
                    Serial.print(F("0x")); 
                    if (setParam <= 0x0F) Serial.print(F("0")); 
                    Serial.println(setParam, HEX);
                    break;
          case 'c': // set counterrequest cycle (once or repetitive)
          case 'C': if (scanint(&RS[1], temp) == 2) {
                      counterrepeatingrequest = 1;
                      Serial.println(F("* Repetitive requesting of counter values initiated")); 
                    } else if (scanint(&RS[1], temp) == 1) {
                      if (!counterrequest) counterrequest = 1;
                      Serial.println(F("* Single counter request cycle initiated")); 
                    } else {
                      counterrepeatingrequest = 0;
                      counterrequest = 0;
                      Serial.println(F("* Counter-requests stopped"));
                    }
                    break;
          case 'z': // set heating/cooling mode on/off
          case 'Z': if (verbose) Serial.print(F("* Cooling/heating ")); 
                    if (scanint(&RS[1], temp) == 1) {
                      setcoolingheatingrequest = 1;
                      setcoolingheatingstatus = temp;
                      if (!verbose) break;
                      Serial.print(F("set to ")); 
                      Serial.println(setcoolingheatingstatus);
                      break;
                    }
                    Serial.println(coolingheatingstatus);
                    break;
#if defined JSON || defined MQTTTOPICS
          case 'u':
          case 'U': if (verbose) Serial.print(F("* OutputUnknown "));
                    if (scanint(&RS[1], temp) == 1) {
                      if (temp) temp = 1;
                      outputunknown = temp;
                      if (!verbose) break;
                      Serial.print(F("set to "));
                    }
                    Serial.println(outputunknown);
                    break;
          case 's':
          case 'S': if (verbose) Serial.print(F("* ChangeOnly "));
                    if (scanint(&RS[1], temp) == 1) {
                      if (temp) temp = 1;
                      changeonly = temp;
                      if (!verbose) break;
                      Serial.print(F("set to"));
                    }
                    Serial.println(changeonly);
                    break;
#endif /* JSON || MQTTTOPICS */
          default:  Serial.print(F("* Command not understood: "));
                    Serial.println(RS);
                    break;
        }
      }
    }
  }
#endif /* MONITOR */
  while (P1P2Serial.packetavailable()) {
    uint16_t delta;
    int nread = P1P2Serial.readpacket(RB, delta, EB, RB_SIZE, crc_gen, crc_feed);
#ifdef MONITOR
#ifdef MONITORCONTROL
    if (!(EB[nread - 1] & ERROR_CRC)) {
      // message received, no error detected
      byte w;
      if ((nread > 9) && (RB[0] == 0x00) && (RB[1] == 0x00) && (RB[2] == 0x12)) {
        // obtain day-of-week, hour, minute
        Tmin = RB[6];
        Thour = RB[5];
        TDoW = RB[4];
        if (Tmin != Tminprev) {
          if (counterrepeatingrequest && !counterrequest) counterrequest = 1;
          Tminprev = Tmin;
/* draft code to switch heat pump on/off 
          if ((Tmin == T_MIN_ON) && (Thour == T_HOUR_ON)) {
            setcoolingheatingrequest = 1;
            setcoolingheatingstatus = 1;
          }
          if ((Tmin == T_MIN_OFF) && (Thour == T_HOUR_OFF)) {
            setcoolingheatingrequest = 1;
            setcoolingheatingstatus = 0;
          }
*/
        }
      }
#ifdef KLICDA
      if ((nread > 4) && (RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == 0x12)) {
        if (counterrequest) {
          // request one counter per cycle
          WB[0] = 0x00;
          WB[1] = 0x00;
          WB[2] = 0xB8;
          WB[3] = (counterrequest - 1);
          if (P1P2Serial.writeready()) {
            // write KLICDA_delay ms after 400012 message
            // pause after 400012 is around 47 ms for some systems which is long enough for a 0000B8*/4000B8* counter request/response pair
            // in exceptional cases (1 in 300 in my system) the pause after 400012 is only 27ms, 
            //      in which case the 4000B* reply arrives after the 000013* request
            //      (and in thoses cases the 000013* request is ignored)
            //      (NOTE!: if KLICDA_delay is chosen incorrectly, such as 5 ms in some example systems, this results in incidental bus collisions)
            P1P2Serial.writepacket(WB, 4, KLICDA_delay, crc_gen, crc_feed); 
          } else {
            Serial.println(F("* Refusing to write counter-request packet while previous packet wasn't finished"));
          }
          if (++counterrequest == 7) counterrequest = 0; // wait until next new minute; change 0 to 1 to continuously request counters for increased resolution
        }
      }
#else /* KLICDA */
      if ((FxAbsentCnt[0] == F0THRESHOLD) && counterrequest && (nread > 4) && (RB[0] == 0x00) && (RB[1] == 0xF0) && (RB[2] == 0x30)) {
        // 00F030 request message received; counterrequest > 0 so hijack this time slot to request counters
        // but only if external F0 controller has not been detected (so check on FxAbsentCnt[0])
        // TODO extend to 00F130 ?
        WB[0] = 0x00;
        WB[1] = 0x00;
        WB[2] = 0xB8;
        WB[3] = (counterrequest - 1);
        if (P1P2Serial.writeready()) {
          P1P2Serial.writepacket(WB, 4, F03XDELAY, crc_gen, crc_feed);
        } else {
          Serial.println(F("* Refusing to write counter-request packet while previous packet wasn't finished"));
        }
        if (++counterrequest == 7) counterrequest = 0;
      } else 
#endif /* KLICDA */
             if ((nread > 4) && (RB[0] == 0x40) && ((RB[1] & 0xFE) == 0xF0) && ((RB[2] & 0x30) == 0x30)) {
        // 40Fx30 external controller reply received - note this could be our own (slow, delta=F030DELAY or F03XDELAY) reply so only reset count if delta < min(F03XDELAY, F030DELAY) (- margin)
        if ((delta < F03XDELAY - 2) && (delta < F030DELAY - 2)) {
          FxAbsentCnt[RB[1] & 0x01] = 0;
          if (RB[1] == CONTROL_ID) {
            // this should only happen if an external controller is connected if/after CONTROL_ID is set (either because
            //    -CONTROL_ID_DEFAULT conflicts with external controller, or
            //    -because an external controller has been connected after CONTROL_ID has been manually set
            Serial.print(F("* Warning: external controller answering to address 0x"));
            Serial.print(RB[1], HEX);
            Serial.println(F(" detected"));
          }
        }
      } else if ((nread > 4) && (RB[0] == 0x00) && ((RB[1] & 0xFE) == 0xF0) && ((RB[2] & 0x30) == 0x30)) {
        // 00Fx30 request message received, and we did not use this slot to request counters
        // check if there is no other external controller
        if ((RB[2] == 0x30) && (FxAbsentCnt[RB[1] & 0x01] < F0THRESHOLD)) {
          FxAbsentCnt[RB[1] & 0x01]++;
          if (FxAbsentCnt[RB[1] & 0x01] == F0THRESHOLD) {
            Serial.print(F("* No external controller answering to address 0x"));
            Serial.print(RB[1], HEX);
            Serial.println(F(" detected, switching control functionality can be switched on (using L1)"));
          }
        }
        // act as external controller
        if (CONTROL_ID && (FxAbsentCnt[CONTROL_ID & 0x01] == F0THRESHOLD) && (RB[1] == CONTROL_ID)) {
          WB[0] = 0x40;
          WB[1] = RB[1];
          WB[2] = RB[2];
          int n = nread;
          int d = F03XDELAY;
          bool wr = 0;
          if (crc_gen) n--; // omit CRC from received-byte-counter
          if (n > WB_SIZE) { 
            n = WB_SIZE; 
            Serial.print(F("* Surprise: received 00Fx3x packet of size ")); 
            Serial.println(nread);
          }
          switch (RB[2]) {
            case 0x30 : // in: 17 byte; out: 17 byte; out pattern WB[7] should contain a 01 if we want to communicate a new setting
                        for (w = 3; w < n; w++) WB[w] = 0x00;
                        // set byte WB[7] to 0x01 for triggering F035 to 0x01 if setcoolingheatingrequest/setDHWrequest needs to be changed to request a F035 message
                        if (setDHWrequest || setcoolingheatingrequest) WB[7] = 0x01;
                        d = F030DELAY;
                        wr = 1;
                        break;
            case 0x31 : // in: 15 byte; out: 15 byte; out pattern is copy of in pattern except for 2 bytes RB[7] RB[8]; function partly date/time, partly unknown
                        for (w = 3; w < n; w++) WB[w] = RB[w];
                        WB[7] = 0xB4; // 180 ?? product type for LAN adapter?
                        WB[8] = 0x10; //  16 ??
                        wr = 1;
                        // same 0xB4 0x10 values also seen at EHVX08S26CB9W so enable this change
                        break;
            case 0x32 : // in: 19 byte: out 19 byte, out is copy in
                        for (w = 3; w < n; w++) WB[w] = RB[w];
                        wr = 1;
                        break;
            case 0x33 : // not seen, no response
                        break;
            case 0x34 : // not seen, no response
                        break;
            case 0x35 : // in: 21 byte; out 21 byte; 3-byte parameters reply with FF  
                        // LAN adapter replies first packet which starts with 930000 (why?)
                        // A parameter consists of 3 bytes: 2 bytes for param nr, and 1 byte for value
                        // parameters in the 00F035 message may indicate status changes in the heat pump
                        // for now we only check for the following parameters: 
                        // DHW parameter setParamDHW
                        // EHVX08S26CB9W DHWbooster parameter 0x48 (EHVX08S26CB9W)
                        // coolingheating parameter SetParam
                        // parameters 0144- or 0162- ASCII name of device
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        // change bytes for triggering coolingheating on/off
                        w = 3;
                        if (setDHWrequest) { WB[w++] = setParamDHW; WB[w++] = 0x00; WB[w++] = setDHWstatus; setDHWrequest = 0; }
                        if (setcoolingheatingrequest) { WB[w++] = setParam; WB[w++] = 0x00; WB[w++] = setcoolingheatingstatus; setcoolingheatingrequest = 0; }
                        for (w = 3; w < n; w++) if ((RB[w] == setParam) && (RB[w+1] == 0x00)) coolingheatingstatus = RB[w+2];
                        for (w = 3; w < n; w++) if ((RB[w] == setParamDHW) && (RB[w+1] == 0x00)) DHWstatus = RB[w+2];
                        wr = 1;
                        break;
            case 0x36 : // in: 23 byte; out 23 byte; 4-byte parameters; reply with FF
                        // A parameter consists of 4 bytes: 2 bytes for param nr, and 2 bytes for value
                        // fallthrough
            case 0x37 : // in: 23 byte; out 23 byte; 5-byte parameters; reply with FF
                        // not seen in EHVX08S23D6V
                        // seen in EHVX08S26CB9W (value: 00001001010100001001)
                        // seen in EHYHBX08AAV3 (could it be date?: 000013081F = 31 aug 2019)
                        // fallthrough
            case 0x38 : // in: 21 byte; out 21 byte; 6-byte parameters; reply with FF
                        // parameter range 0000-001E; kwH/hour counters?
                        // not seen in EHVX08S23D6V
                        // seen in EHVX08S26CB9W
                        // seen in EHYHBX08AAV3
                        // A parameter consists of 6 bytes: ?? bytes for param nr, and ?? bytes for value/??
                        // fallthrough
            case 0x39 : // in: 21 byte; out 21 byte; 6-byte parameters; reply with FF
                        // fallthrough
            case 0x3A : // in: 21 byte; out 21 byte; 3-byte parameters; reply with FF
                        // fallthrough
            case 0x3B : // in: 23 byte; out 23 byte; 4-byte parameters; reply with FF
                        // not seen in EHVX08S23D6V
                        // seen in EHVX08S26CB9W
                        // seen in EHYHBX08AAV3
                        // fallthrough
            case 0x3C : // in: 23 byte; out 23 byte; 5 or 10-byte parameters; reply with FF
                        // fallthrough
            case 0x3D : // in: 21 byte; out: 21 byte; 6-byte parameters; reply with FF
                        // parameter range 0000-001F; kwH/hour counters?
                        // not seen in EHVX08S23D6V
                        // seen in EHVX08S26CB9W
                        // seen in EHYHBX08AAV3
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        wr = 1;
                        break;
            case 0x3E : // schedule related packet
                        if (RB[3]) {
                          // 0x3E01, 0x3E02, ... in: 23 byte; out: 23 byte; out 40F13E01(even for higher) + 19xFF
                          // schedule-related
                          WB[3] = 0x01;
                        } else {
                          // 0x3E00 in: 8 byte out: 8 byte; 40F13E00 (4x FF)
                          WB[3] = 0x00;
                        }
                        for (w = 4; w < n; w++) WB[w] = 0xFF;
                        wr = 1;
                        break;
            default   : // not seen, no response
                        break;
          }
          if (wr) {
            if (P1P2Serial.writeready()) {
              P1P2Serial.writepacket(WB, n, d, crc_gen, crc_feed);
            } else {
              Serial.println(F("* Refusing to write packet while previous packet wasn't finished"));
            }
          }
        }
      }
    }
#endif /* MONITORCONTROL */
#ifdef MONITORSERIAL
    if (verbose) {
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
    }
    for (int i = 0; i < nread; i++) {
      if (verbose && (EB[i] & ERROR_READBACK)) {
        // collision suspicion due to verification error in reading back written data
        Serial.print(F("-XX:"));
      }
      if (verbose && (EB[i] & ERROR_PE)) {
        // parity error detected
        Serial.print(F("-PE:"));
      }
      byte c = RB[i];
      if (crc_gen && verbose && (i == nread - 1)) {
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
#endif /* MONITORSERIAL */
#endif /* MONITOR */
#if defined JSON || defined MQTTTOPICS
    if (!(EB[nread - 1] & ERROR_CRC)) process_packet(RB, nread - 1);
#endif /* JSON || MQTTTOPICS */
  }
}
