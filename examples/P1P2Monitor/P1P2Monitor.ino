/* P1P2Monitor: monitor and control program for HBS such as P1/P2 bus on many Daikin systems.
 *              Reads the P1/P2 bus using the P1P2Serial library, and outputs raw hex data and verbose output over serial.
 *              A few additional parameters are added in raw hex data as so-called pseudo-packets (packet type 0x08 and 0x09).
 *
 *              P1P2Monitor can act as a 2nd (external/auxiliary) controller to the main controller (like an EKRUCBL)
 *               and can use the P1P2 protocol to set various parameters in the main controller, as if these were set manually.
 *              such as heating on/off, DHW on/off, DHW temperature, temperature deviation, etcetera.
 *              It can receive instructions over serial (from an ESP or via USB).
 *              Control over UDP is not yet supported here, but is supported by https://github.com/budulinek/Daikin-P1P2---UDP-Gateway
 *              It can regularly request the system counters to better monitor efficiency and operation.
 *              Control has been tested on EHYHBX08AAV3 and EHVX08S23D6V, and it likely works on various other Altherma models.
 *              The P1P2Monitor command format is described in P1P2Monitor-commands.md,
 *              but just as an example, I can switch my heat pump off with the following commands
 *
 *              *            < one empty line (only needed immediately after reboot, to flush the input buffer) >
 *              L1           < switch control mode on >
 *              Z0           < switch heating off >
 *              Z1           < switch heating on >
 *
 *              That is all!
 *
 *              If you know the parameter number for a certain function
 *              (see https://github.com/budulinek/Daikin-P1P2---UDP-Gateway/blob/main/Payload-data-write.md),
 *              you can set that parameter by:
 *
 *              Q03         < select writing to parameter 0x03 in F036 block, used for DHW temperature setting >
 *              R0208       < set parameter 0x0003 to 0x0208, which is 520, which means 52.0 degrees Celcius >
 *
 *              P40         < select writing to parameter 0x40 in F035 block, used for DHW on/off setting >
 *              Z01         < set parameter 0x0040 to 0x01, DHW status on >
 *              P2F         < select writing back to parameter 0x2F in F035 block, used for heating(/cooloing) on/off >
 *              Z01         < set parameter 0x002F to 0x01, heating status on >
 *
 *              Support for parameter setting in P1P2Monitor is rather simple. There is no buffering, so enough time (a few seconds)
 *              is needed in between commands.
 *
 * Copyright (c) 2019-2022 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 * Version history
 * 20220802 v0.9.14 New parameter-write method 'e' (param write packet type 35-3D), error and write throttling, verbosity mode 2, EEPROM state, MCUSR reporting,
 *                  pseudo-packets, error counters, ...
 *                  Simplified code by removing packet interpretation, json, MQTT, UDP (all off-loaded to P1P2MQTT)
 * 20220511 v0.9.13 Removed dependance on millis(), reducing blocking due to serial input, correcting 'Z' handling to hexadecimal, ...
 * 20220225 v0.9.12 Added parameter support for F036 parameters
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
 *     Thanks to Krakra for providing coding suggestions and UDP support, the hints and references to the HBS and MM1192 documents on
 *     https://community.openenergymonitor.org/t/hack-my-heat-pump-and-publish-data-onto-emoncms,
 *     to Luis Lamich Arocas for sharing logfiles and testing the new controlling functions for the EHVX08S23D6V,
 *     and to Paul Stoffregen for publishing the AltSoftSerial library which was the starting point for the P1P2Serial library.
 *
 * P1P2erial is written and tested for the Arduino Uno and (in the past) Mega.
 * It may or may not work on other hardware using a 16MHz clok.
 * As of P1P2Serial library 0.9.14, it also runs on 8MHz ATmega328P using MCUdude's MiniCore.
 * Information on MCUdude's MiniCore: 
 * Install in Arduino IDE (File/preferences, additional board manager URL: https://mcudude.github.io/MiniCore/package_MCUdude_MiniCore_index.json.
 *
 * Board          Transmit  Receive   Unusable PWM pins
 * -----          --------  -------   ------------
 * Arduino Uno        9         8         10
 * Arduino Mega      46        48       44, 45
 * ATmega328P        PB1      PB0
 *
 * Wishlist: schedule writes
 */

#include "P1P2Config.h"
#include <P1P2Serial.h>

static byte verbose = INIT_VERBOSE;
static byte readErrors = 0;
static byte readErrorLast = 0;
static byte writeRefused = 0;
static byte errorsLargePacket = 0;

const byte Compile_Options = 0 // multi-line statement
#ifdef MONITORCONTROL
+0x01
#endif
#ifdef KLICDA
+0x02
#endif
#ifdef PSEUDO_PACKETS
+0x04
#endif
#ifdef EEPROM_SUPPORT
+0x08
#endif
#ifdef SERIAL_MAGICSTRING
+0x10
#endif
;

#ifdef EEPROM_SUPPORT
#include <EEPROM.h>
void initEEPROM() {
  if (verbose) Serial.println(F("* checking EEPROM"));
  bool sigMatch = 1;
  for (uint8_t i = 0; i < strlen(EEPROM_SIGNATURE); i++) sigMatch &= (EEPROM.read(EEPROM_ADDRESS_SIGNATURE + i) == EEPROM_SIGNATURE[i]);
  if (verbose) {
     Serial.print(F("* EEPROM sig match"));
     Serial.println(sigMatch);
  }
  if (!sigMatch) {
    if (verbose) Serial.println(F("* EEPROM sig mismatch, initializing EEPROM"));
    for (uint8_t i = 0; i < strlen(EEPROM_SIGNATURE); i++) EEPROM.update(EEPROM_ADDRESS_SIGNATURE + i, EEPROM_SIGNATURE[i]); // no '\0', not needed
    EEPROM.update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID_DEFAULT);
    EEPROM.update(EEPROM_ADDRESS_COUNTER_STATUS, COUNTERREPEATINGREQUEST);
    EEPROM.update(EEPROM_ADDRESS_VERBOSITY, INIT_VERBOSE);
  }
}
#define EEPROM_update(x, y) { EEPROM.update(x, y); };
#else /* EEPROM_SUPPORT */
#define EEPROM_update(x, y) {}; // dummy function to avoid cluttering code with #ifdef EEPROM_SUPPORT
#endif /* EEPROM_SUPPORT */

P1P2Serial P1P2Serial;

static uint16_t sd = INIT_SD;           // delay setting for each packet written upon manual instruction (changed to 50ms which seems to be a bit safer than 0)
static uint16_t sdto = INIT_SDTO;       // time-out delay (applies both to manual instructed writes and controller writes)
static byte echo = INIT_ECHO;         // echo setting (whether written data is read back)
static uint8_t CONTROL_ID = CONTROL_ID_DEFAULT;
static byte counterRequest = 0;
static byte counterRepeatingRequest = 0;

byte save_MCUSR;

byte setRequestDHW = 0;
byte setStatusDHW = 0;

uint16_t setParam35 = PARAM_HC_ONOFF;
byte setRequest35 = 0;
uint16_t setValue35 = 0;

uint16_t setParam36 = PARAM_TEMP;
byte setRequest36 = 0;
uint16_t setValue36 = 0;

uint16_t setParam3A = PARAM_SYS;
byte setRequest3A = 0;
byte setValue3A = 0;

uint8_t wr_cnt = 0;
uint8_t wr_req = 0;
uint8_t wr_pt = 0;
uint16_t wr_nr = 0;
uint32_t wr_val = 0;

byte Tmin = 0;
byte Tminprev = 61;
uint32_t upt_prev_pseudo = 0;
uint32_t upt_prev_write = 0;
uint32_t upt_prev_error = 0;
uint8_t writePermission = INIT_WRITE_PERMISSION;
uint8_t errorsPermitted = INIT_ERRORS_PERMITTED;
uint16_t parameterWritesDone = 0;

// FxAbsentCnt[x] counts number of unanswered 00Fx30 messages;
// if -1 than no Fx request or response seen (relevant to detect whether F1 controller is supported or not)
static int8_t FxAbsentCnt[2] = { -1, -1};
static int8_t FxAbsentCntInclOwn[2] = { -1, -1};
static byte Fx30ReplyDelay[2] = { 0, 0 };

static char RS[RS_SIZE];
static byte WB[WB_SIZE];
static byte RB[RB_SIZE];
static byte EB[RB_SIZE];

// serial-receive using direct access to serial read buffer RS
// rs is # characters in buffer;
// sr_buffer is pointer (RS+rs)

static uint8_t rs=0;
static char *RSp = RS;

void setup() {
  save_MCUSR = MCUSR;
  MCUSR = 0;
  Serial.begin(SERIALSPEED);
  while (!Serial);      // wait for Arduino Serial Monitor to open
  Serial.println(F("*"));
  Serial.println(F(WELCOMESTRING));
  Serial.print(F("* Compiled "));
  Serial.print(__DATE__);
  Serial.print(F(" "));
  Serial.print(__TIME__);
#ifdef MONITORCONTROL
  Serial.print(F(" +control"));
#endif /* MONITORCONTROL */
#ifdef KLICDA
  Serial.print(F(" +klicda"));
#endif /* KLICDA */
  Serial.println();
#ifdef EEPROM_SUPPORT
  initEEPROM();
  CONTROL_ID = EEPROM.read(EEPROM_ADDRESS_CONTROL_ID);
  verbose    = EEPROM.read(EEPROM_ADDRESS_VERBOSITY);
  counterRepeatingRequest = EEPROM.read(EEPROM_ADDRESS_COUNTER_STATUS);
#endif /* EEPROM_SUPPORT */
  if (verbose) {
    Serial.print(F("* Control_ID=0x"));
    Serial.println(CONTROL_ID, HEX);
    Serial.print(F("* Verbosity="));
    Serial.println(verbose);
    Serial.print(F("* Counterrepeatingrequest="));
    Serial.println(counterRepeatingRequest);
    Serial.print(F("* Reset cause: MCUSR="));
    Serial.print(save_MCUSR);
    if (save_MCUSR & (1 << BORF))  Serial.print(F(" (brown-out-detected)")); // 4
    if (save_MCUSR & (1 << EXTRF)) Serial.print(F(" (external-reset)")); // 2
    if (save_MCUSR & (1 << PORF)) Serial.print(F(" (power-on-reset)")); // 1
    Serial.println();
// Initial parameters
    Serial.print(F("* CPU_SPEED="));
    Serial.println(F_CPU);
    Serial.print(F("* SERIALSPEED="));
    Serial.println(SERIALSPEED);
#ifdef KLICDA
#ifdef KLICDA_DELAY
    Serial.print(F("* KLICDA_DELAY="));
    Serial.println(KLICDA_DELAY);
#endif /* KLICDA_DELAY */
#endif /* KLICDA */
    Serial.print(F("* CONTROL_ID_DEFAULT=0x"));
    Serial.println(CONTROL_ID_DEFAULT, HEX);
    Serial.print(F("* F030DELAY="));
    Serial.println(F030DELAY);
    Serial.print(F("* F03XDELAY="));
    Serial.print(F03XDELAY);
    Serial.println();
  }
  P1P2Serial.begin(9600);
}

// scanint and scanhex are used to save dynamic memory usage in main loop
int scanint(char* s, uint16_t &b) {
  return sscanf(s,"%d",&b);
}

int scanhex(char* s, uint16_t &b) {
  return sscanf(s,"%4x",&b);
}

static byte crc_gen = CRC_GEN;
static byte crc_feed = CRC_FEED;

void writePseudoPacket(byte* WB, byte rh)
{
  if (verbose) Serial.print(F("R "));
  if (verbose & 0x01) Serial.print(F("P         "));
  uint8_t crc = crc_feed;
  for (uint8_t i = 0; i < rh; i++) {
    uint8_t c = WB[i];
    if (c <= 0x0F) Serial.print(F("0"));
    Serial.print(c, HEX);
    if (crc_gen != 0) for (uint8_t i = 0; i < 8; i++) {
      crc = ((crc ^ c) & 0x01 ? ((crc >> 1) ^ crc_gen) : (crc >> 1));
      c >>= 1;
    }
  }
  if (crc_gen) {
    if (crc <= 0x0F) Serial.print(F("0"));
    Serial.print(crc, HEX);
  }
  Serial.println();
}

#define PARAM_TP_START      0x35
#define PARAM_TP_END        0x3D
#define PARAM_ARR_SZ (PARAM_TP_END - PARAM_TP_START + 1)
const uint16_t  nr_params[PARAM_ARR_SZ] = { 0x014A, 0x002D, 0x0001, 0x001F, 0x00F0, 0x006C, 0x00AF, 0x0002, 0x0020 }; // number of parameters observed
//byte packettype                       = {   0x35,   0x36,   0x37,   0x38,   0x39,   0x3A,   0x3B,   0x3C,   0x3D };

void(* resetFunc) (void) = 0; // declare reset function at address 0

static byte pseudo0D = 0;
static byte pseudo0E = 0;
static byte pseudo0F = 0;

void loop() {
  uint16_t temp;
  uint16_t temphex;
  int wb = 0;
  int wbp = 7;
  int n;
  int wbtemp;
  int c;
  static byte ignoreremainder = 2; // ignore first line from serial input to avoid misreading a partial message just after reboot

// Non-blocking serial input
// rs is number of char received and stored in readbuf
// RSp = readbuf + rs
// ignore first line and too-long lines
  while (((c = Serial.read()) >= 0) && (c != '\n') && (rs < RS_SIZE)) {
    *RSp++ = (char) c;
    rs++;
  }
  // ((c == '\n' and rs > 0))  and/or  rs == RS_SIZE)  or  c == -1
  if (c >= 0) {
    if ((c != '\n') || (rs == RS_SIZE)) {
      //  (c != '\n' ||  rb == RB)
      char lst = *(RSp - 1);
      *(RSp - 1) = '\0';
      if (c != '\n') {
        Serial.print("* Line too long, ignored, ignoring remainder: ->");
        Serial.print(RS);
        Serial.print(lst);
        Serial.print(c);
        Serial.println("<-");
        ignoreremainder = 1;
      } else {
        Serial.print("* Line too long, terminated, ignored: ->");
        Serial.print(RS);
        Serial.print(lst);
        Serial.println("<-");
        ignoreremainder = 0;
      }
    } else {
      // c == '\n'
      *RSp = '\0';
      // Remove \r before \n in DOS input
      if ((rs > 0) && (RS[rs - 1] == '\r')) {
        rs--;
        *(--RSp) = '\0';
      }
      if (ignoreremainder == 2) {
        Serial.print(F("* First line ignored: "));
        Serial.println(RS);
        ignoreremainder = 0;
      } else if (ignoreremainder == 1) {
        Serial.print(F("* Line too long, last part: "));
        Serial.println(RS);
        ignoreremainder = 0;
      } else {
#define maxVerbose ((verbose == 1) || (verbose == 4))
        if (maxVerbose) {
          Serial.print(F("* Received: \""));
          Serial.print(RS);
          Serial.println("\"");
        }
#ifdef SERIAL_MAGICSTRING
        RSp = RS + strlen(SERIAL_MAGICSTRING) + 1;
        if (!strncmp(RS, SERIAL_MAGICSTRING, strlen(SERIAL_MAGICSTRING))) {
        
#else
        RSp = RS + 1;
        {
#endif
          switch (*(RSp - 1)) {
            case '\0':if (maxVerbose) Serial.println(F("* Empty line received")); break;
            case '*': if (maxVerbose) {
                        Serial.print(F("* Received: "));
                        Serial.println(RSp);
                      }
                      break;
            case 'e':
            case 'E':
                      if (wr_cnt) {
                        // previous write still being processed
                        Serial.println(F("* Previous parameter write action still busy"));
                        break;
                      } else if ( (n = sscanf(RSp, (const char*) "%2x%4x%8x", &wr_pt, &wr_nr, &wr_val)) == 3) {
                        if ((wr_pt < PARAM_TP_START) || (wr_pt > PARAM_TP_END)) {
                          Serial.print(F("* wr_pt: 0x"));
                          Serial.print(wr_pt, HEX);
                          Serial.println(F(" out of range 0x35-0x3D"));
                          break;
                        }
                        if (wr_nr > nr_params[wr_pt - PARAM_TP_START]) {
                          Serial.print(F("* wr_nr > expected: 0x"));
                          Serial.println(wr_nr);
                          break;
                        }
                        uint8_t wr_nrb;
                        switch (wr_pt) {
                          case 0x35 : // fallthrough
                          case 0x3A : wr_nrb = 1; break;
                          case 0x36 : // fallthrough
                          case 0x3B : wr_nrb = 2; break;
                          case 0x37 : // fallthrough
                          case 0x3C : wr_nrb = 3; break;
                          case 0x38 : // fallthrough
                          case 0x39 : // fallthrough
                          case 0x3D : wr_nrb = 4; break;
                        }
                        if (wr_val >> (wr_nrb << 3)) {
                          Serial.print(F("* Parameter value too large for packet type; #bytes is "));
                          Serial.print(wr_nrb);
                          Serial.print(F(" value is "));
                          Serial.println(wr_val, HEX);
                          break;
                        }
                        if ((wr_pt == 0x35) && (wr_nr == PARAM_DHW_ONOFF) && (setRequestDHW)) {
                          Serial.println(F("* Ignored - Writing to this parameter already pending by DHW35_request"));
                          break;
                        }
                        if ((wr_pt == 0x35) && (wr_nr == setParam35) && (setRequest35)) {
                          Serial.println(F("* Ignored - Writing to this parameter already pending by 35_request"));
                          break;
                        }
                        if ((wr_pt == 0x36) && (wr_nr == setParam36) && (setRequest36)) {
                          Serial.println(F("* Ignored - Writing to this parameter already pending by 36_request"));
                          break;
                        }
                        if ((wr_pt == 0x3A) && (wr_nr == setParam3A) && (setRequest3A)) {
                          Serial.println(F("* Ignored - Writing to this parameter already pending by 3A_request"));
                          break;
                        }
                        if (writePermission) {
                          writePermission--;
                          wr_cnt = WR_CNT; // write repetitions, 1 should be enough
                          Serial.print(F("* Initiating parameter write for packet-type 0x"));
                          Serial.print(wr_pt, HEX);
                          Serial.print(F(" parameter nr 0x"));
                          Serial.print(wr_nr, HEX);
                          Serial.print(F(" to value 0x"));
                          Serial.print(wr_val, HEX);
                          Serial.println();
                        } else {
                          Serial.println(F("* Currently no write budget left"));
                          break;
                        }
                      } else {
                        Serial.print(F("* Ignoring instruction, expected 3 arguments, received: "));
                        Serial.print(n);
                        if (n > 0) {
                          Serial.print(F(" pt: 0x"));
                          Serial.print(wr_pt, HEX);
                        }
                        if (n > 1) {
                          Serial.print(F(" nr: 0x"));
                          Serial.print(wr_nr, HEX);
                        }
                        Serial.println();
                      }
                      break;
            case 'g':
            case 'G': if (verbose) Serial.print(F("* Crc_gen "));
                      if (scanhex(RSp, temphex) == 1) {
                        crc_gen = temphex;
                        if (!verbose) break;
                        Serial.print(F("set to "));
                      }
                      Serial.print(F("0x"));
                      if (crc_gen <= 0x0F) Serial.print(F("0"));
                      Serial.println(crc_gen, HEX);
                      break;
            case 'h':
            case 'H': if (verbose) Serial.print(F("* Crc_feed "));
                      if (scanhex(RSp, temphex) == 1) {
                        crc_feed = temphex;
                        if (!verbose) break;
                        Serial.print(F("set to "));
                      }
                      Serial.print(F("0x"));
                      if (crc_feed <= 0x0F) Serial.print(F("0"));
                      Serial.println(crc_feed, HEX);
                      break;
            case 'v':
            case 'V': Serial.print(F("* Verbose "));
                      if (scanint(RSp, temp) == 1) {
                        if (temp > 4) temp = 4;
                        Serial.print(F("set to "));
                        verbose = temp;
                        EEPROM_update(EEPROM_ADDRESS_VERBOSITY, verbose);
                      }
                      Serial.println(verbose);
                      Serial.println(F(WELCOMESTRING));
                      Serial.print(F("* Compiled "));
                      Serial.print(__DATE__);
                      Serial.print(F(" "));
                      Serial.println(__TIME__);
                      break;
            case 't':
            case 'T': if (verbose) Serial.print(F("* Delay "));
                      if (scanint(RSp, sd) == 1) {
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
                      if (scanint(RSp, sdto) == 1) {
                        P1P2Serial.setDelayTimeout(sdto);
                        if (!verbose) break;
                        Serial.print(F("set to "));
                      }
                      Serial.println(sdto);
                      break;
            case 'x':
            case 'X': if (verbose) Serial.print(F("* Echo "));
                      if (scanint(RSp, temp) == 1) {
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
                      while ((wb < WB_SIZE) && (sscanf(&RS[wbp], (const char*) F("%2x%n"), &wbtemp, &n) == 1)) {
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
                        if (writeRefused < 0xFF) writeRefused++;
                      }
                      break;
            case 'k': // reset ATmega
            case 'K': Serial.println(F("* Resetting ...."));
                      resetFunc(); //call reset
            case 'l': // set auxiliary controller function on/off; CONTROL_ID address is set automatically
            case 'L': if (scanint(RSp, temp) == 1) {
                        if (temp > 1) temp = 1;
                        if (temp) {
                          if (errorsPermitted < MIN_ERRORS_PERMITTED) {
                            Serial.println(F("* Errorspermitted too low; control functinality not enabled."));
                            break;
                          }
                          if (CONTROL_ID && temp) {
                            Serial.print(F("* CONTROL_ID is already 0x"));
                            Serial.println(CONTROL_ID, HEX);
                            break;
                          }
                          if (!CONTROL_ID && !temp) {
                            Serial.print(F("* CONTROL_ID is already 0x00"));
                            break;
                          }
                          if (FxAbsentCnt[0] == F0THRESHOLD) {
                            Serial.println(F("* Control_ID 0xF0 supported, no external controller found for 0xF0, switching control functionality on 0xF0 on"));
                            CONTROL_ID = 0xF0;
                            EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
                          } else if (FxAbsentCnt[1] == F0THRESHOLD) {
                            Serial.println(F("* Control_ID 0xF1 supported, no external controller found for 0xF1, switching control functionality on 0xF1 on"));
                            CONTROL_ID = 0xF1;
                            EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
                          } else {
                            Serial.println(F("* No free address for controller found (yet). Control functionality not enabled. You may wish to re-try in a few seconds (and perhaps switch external controllers off)"));
                            CONTROL_ID = 0x00;
                            EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
                          }
                        } else {
                          CONTROL_ID = 0x00;
                          EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
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
            case 'c': // set counterRequest cycle (once or repetitive)
            case 'C': if (scanint(RSp, temp) == 1) {
                        if (temp > 1) {
                          if (errorsPermitted < MIN_ERRORS_PERMITTED) {
                            Serial.println(F("* Errorspermitted too low; control functinality not enabled."));
                            break;
                          }
                          if (counterRepeatingRequest) {
                            Serial.println(F("* Repetitive requesting of counter values was already active"));
                            break;
                          }
                          counterRepeatingRequest = 1;
                          Serial.println(F("* Repetitive requesting of counter values initiated"));
                          EEPROM_update(EEPROM_ADDRESS_COUNTER_STATUS, counterRepeatingRequest);
                        } else if (temp == 1) {
                          if (counterRequest & !counterRepeatingRequest) {
                            Serial.println(F("* Previous single counter request not finished yet"));
                            break;
                          }
                          if (counterRepeatingRequest) {
                            counterRepeatingRequest = 0;
                            if (counterRequest) {
                              Serial.println(F("* Switching repetitive requesting of counters off, was still active, no new cycle iniated"));
                            } else {
                              Serial.println(F("* Switching repetitive requesting of counters off, single counter cycle iniated"));
                              counterRequest = 1;
                            }
                          } else {
                            if (counterRequest) {
                              Serial.println(F("* Single Repetitive requesting of counter values was already active"));
                            } else {
                              counterRequest = 1;
                              Serial.println(F("* Single counter request cycle initiated"));
                            }
                          }
                        } else { // temp == 0
                          counterRepeatingRequest = 0;
                          counterRequest = 0;
                          Serial.println(F("* All counter-requests stopped"));
                          EEPROM_update(EEPROM_ADDRESS_COUNTER_STATUS, counterRepeatingRequest);
                        }
                      } else {
                        Serial.print(F("* Counterrepeatingrequest is "));
                        Serial.println(counterRepeatingRequest);
                        Serial.print(F("* Counterrequest is "));
                        Serial.println(counterRequest);
                      }
                      break;
            case 'p': // select F035-parameter to write in z step below (default PARAM_HC_ONOFF in P1P2Config.h)
            case 'P': if (verbose) Serial.print(F("* Param35-2Write "));
                      if (scanhex(RSp, temphex) == 1) {
                        if (setRequest35) {
                          Serial.println(F("* Cannot change param35-2write while previous request still pending"));
                          break;
                        }
                        setParam35 = temphex;
                        if (!verbose) break;
                        Serial.print(F("set to "));
                      }
                      Serial.print(F("0x"));
                      if (setParam35 <= 0x000F) Serial.print(F("0"));
                      if (setParam35 <= 0x00FF) Serial.print(F("0"));
                      if (setParam35 <= 0x0FFF) Serial.print(F("0"));
                      Serial.println(setParam35, HEX);
                      break;
            case 'q': // select F036-parameter to write in r step below (default PARAM_TEMP in P1P2Config.h)
            case 'Q': if (verbose) Serial.print(F("* Param36-2Write "));
                      if (scanhex(RSp, temphex) == 1) {
                        if (setRequest36) {
                          Serial.println(F("* Cannot change param36-2write while previous request still pending"));
                          break;
                        }
                        setParam36 = temphex;
                        if (!verbose) break;
                        Serial.print(F("set to "));
                      }
                      Serial.print(F("0x"));
                      if (setParam36 <= 0x000F) Serial.print(F("0"));
                      if (setParam36 <= 0x00FF) Serial.print(F("0"));
                      if (setParam36 <= 0x0FFF) Serial.print(F("0"));
                      Serial.println(setParam36, HEX);
                      break;
            case 'm': // select F03A-parameter to write in n step below (default PARAM_SYS in P1P2Config.h)
            case 'M': if (verbose) Serial.print(F("* Param3A-2Write "));
                      if (scanhex(RSp, temphex) == 1) {
                        if (setRequest3A) {
                          Serial.println(F("* Cannot change param3A-2write while previous request still pending"));
                          break;
                        }
                        setParam3A = temphex;
                        if (!verbose) break;
                        Serial.print(F("set to "));
                      }
                      Serial.print(F("0x"));
                      if (setParam3A <= 0x000F) Serial.print(F("0"));
                      if (setParam3A <= 0x00FF) Serial.print(F("0"));
                      if (setParam3A <= 0x0FFF) Serial.print(F("0"));
                      Serial.println(setParam3A, HEX);
                      break;

            case 'z': // Z  report status of packet type 35 write action
            case 'Z': // Zx set value for parameter write (PARAM_HC_ONOFF/'p') in packet type 35 and initiate write action
                      if (verbose) Serial.print(F("* Param35 "));
                      Serial.print(F("0x"));
                      if (setParam35 <= 0x000F) Serial.print(F("0"));
                      if (setParam35 <= 0x00FF) Serial.print(F("0"));
                      if (setParam35 <= 0x0FFF) Serial.print(F("0"));
                      Serial.println(setParam35, HEX);
                      if (scanhex(RSp, temphex) == 1) {
                        if ((wr_pt == 0x35) && (wr_nr == setParam35) && wr_cnt) {
                          Serial.println(F(": write command ignored - E-write-request is pending"));
                          break;
                        }
                        if ((setParam35 == PARAM_DHW_ONOFF) && setRequestDHW) {
                          Serial.println(F(": write command ignored - DHW-write-request is pending"));
                          break;
                        }
                        if (writePermission) {
                          writePermission--;
                          setRequest35 = 1;
                          setValue35 = temphex;
                          if (!verbose) break;
                          Serial.print(F(" will be set to 0x"));
                          if (setValue35 <= 0x000F) Serial.print(F("0"));
                          Serial.println(setValue35, HEX);
                          break;
                        } else {
                          Serial.println(F("* Currently no write budget left"));
                          break;
                        }
                      } else if (setRequest35) {
                        Serial.println(F(": 35-write-request to value 0x"));
                        Serial.println(setValue35, HEX);
                        Serial.println(F(" pending"));
                      } else {
                        Serial.println(F(": no 35-write-request pending"));
                      }
                      break;
            case 'r': // R  report status of packet type 36 write action
            case 'R': // Rx set value for parameter write (PARAM_TEMP/'q') in packet type 36 and initiate write action
                      if (verbose) Serial.print(F("* Param36 "));
                      Serial.print(F("0x"));
                      if (setParam36 <= 0x000F) Serial.print(F("0"));
                      if (setParam36 <= 0x00FF) Serial.print(F("0"));
                      if (setParam36 <= 0x0FFF) Serial.print(F("0"));
                      Serial.println(setParam36, HEX);
                      if (scanhex(RSp, temphex) == 1) {
                        if ((wr_pt == 0x36) && (wr_nr == setParam36) && wr_cnt) {
                          Serial.println(F(" write command ignored - E-write-request is pending"));
                          break;
                        }
                        if (writePermission) {
                          writePermission--;
                          setRequest36 = 1;
                          setValue36 = temphex;
                          if (!verbose) break;
                          Serial.print(F(" will be set to 0x"));
                          if (setValue36 <= 0x000F) Serial.print(F("0"));
                          if (setValue36 <= 0x00FF) Serial.print(F("0"));
                          if (setValue36 <= 0x0FFF) Serial.print(F("0"));
                          Serial.println(setValue36, HEX);
                          break;
                        } else {
                          Serial.println(F("* Currently no write budget left"));
                          break;
                        }
                      } else if (setRequest36) {
                        Serial.println(F(": 36-write-request to value 0x"));
                        Serial.println(setValue36, HEX);
                        Serial.println(F(" still pending"));
                      } else {
                        Serial.println(F(": no 36-write-request pending"));
                      }
                      break;
            case 'n': // N  report status of packet type 3A write action
            case 'N': // Nx set value for parameter write (PARAMSYS/'m') in packet type 3A and initiate write action
                      if (verbose) Serial.print(F("* Param3A "));
                      Serial.print(F("0x"));
                      if (setParam3A <= 0x000F) Serial.print(F("0"));
                      if (setParam3A <= 0x00FF) Serial.print(F("0"));
                      if (setParam3A <= 0x0FFF) Serial.print(F("0"));
                      Serial.println(setParam3A, HEX);
                      if (scanhex(RSp, temphex) == 1) {
                        if ((wr_pt == 0x3A) && (wr_nr == setParam3A) && wr_cnt) {
                          Serial.println(F(": write command ignored - E-write-request is pending"));
                          break;
                        }
                        if (writePermission) {
                          writePermission--;
                          setRequest3A = 1;
                          setValue3A = temphex;
                          if (!verbose) break;
                          Serial.print(F(": will be set to 0x"));
                          if (setValue3A <= 0x000F) Serial.print(F("0"));
                          Serial.println(setValue3A, HEX);
                          break;
                        } else {
                          Serial.println(F(": currently no write budget left"));
                          break;
                        }
                      } else if (setRequest3A) {
                        Serial.println(F(": 3A-write-request to value 0x"));
                        Serial.println(setValue3A, HEX);
                        Serial.println(F(" still pending"));
                      } else {
                        Serial.println(F(": no 3A-write-request pending"));
                      }
                      break;
            case 'y': // Y  report status of DHW write action (packet type 0x35)
            case 'Y': // Yx set value for DHW parameter write (defined by PARAM_DHW_ONOFF in P1P2Config.h, not reconfigurable) in packet type 35 and initiate write action
                      if (verbose) Serial.print(F("* DHWparam35 "));
                      Serial.print(F("0x"));
                      if (PARAM_DHW_ONOFF <= 0x000F) Serial.print(F("0"));
                      if (PARAM_DHW_ONOFF <= 0x00FF) Serial.print(F("0"));
                      if (PARAM_DHW_ONOFF <= 0x0FFF) Serial.print(F("0"));
                      Serial.println(PARAM_DHW_ONOFF, HEX);
                      if (scanint(RSp, temp) == 1) {
                        if ((wr_pt == 0x35) && (wr_nr == PARAM_DHW_ONOFF) && wr_cnt) {
                          Serial.println(F("* Ignored - Writing to this parameter already pending by E-request"));
                          break;
                        }
                        if ((setParam35 == PARAM_DHW_ONOFF) && setRequest35) {
                          Serial.println(F("* Ignored - Writing to this parameter already pending by 35-request"));
                          break;
                        }
                        if (writePermission) {
                          writePermission--;
                          setRequestDHW = 1;
                          setStatusDHW = temp;
                          if (!verbose) break;
                          Serial.print(F(" will be set to 0x"));
                          if (setValue3A <= 0x000F) Serial.print(F("0"));
                          Serial.println(setStatusDHW);
                          break;
                        } else {
                          Serial.println(F("* Currently no write budget left"));
                          break;
                        }
                      } else if (setRequest3A) {
                        Serial.println(F(": DHW-write-request to value 0x"));
                        Serial.println(setValue3A, HEX);
                        Serial.println(F(" still pending"));
                      } else {
                        Serial.println(F(": no DHW-write-request pending"));
                      }
                      break;
            default:  Serial.print(F("* Command not understood: "));
                      Serial.println(RS);
                      break;
          }
#ifdef SERIAL_MAGICSTRING
        } else {
          Serial.print(F("* Magic String mismatch: "));
          Serial.println(RS);
#endif
        }
      }
    }
    RSp = RS;
    rs = 0;
  }
  uint32_t upt = P1P2Serial.uptime_sec();
  if (upt > upt_prev_pseudo) {
    pseudo0D++;
    pseudo0E++;
    pseudo0F++;
    upt_prev_pseudo = upt;
  }
  if (upt >= upt_prev_write + TIME_WRITE_PERMISSION) {
    if (writePermission < MAX_WRITE_PERMISSION) writePermission++;
    upt_prev_write += TIME_WRITE_PERMISSION;
  }
  if (upt >= upt_prev_error + TIME_ERRORS_PERMITTED) {
    if (errorsPermitted < MAX_ERRORS_PERMITTED) errorsPermitted++;
    upt_prev_error += TIME_ERRORS_PERMITTED;
  }
  while (P1P2Serial.packetavailable()) {
    uint16_t delta;
    byte readError = 0;

    int nread = P1P2Serial.readpacket(RB, delta, EB, RB_SIZE, crc_gen, crc_feed);

    if (nread > RB_SIZE) {
      Serial.println(F("* Received packet longer than RB_SIZE"));
      nread = RB_SIZE;
      readError = 1;
      if (errorsLargePacket < 0xFF) errorsLargePacket++;
    }
    for (int i = 0; i < nread; i++) readError |= EB[i];
    if (readError && upt) { // ignore errors while upt == 0
       if (readErrors < 0xFF) { 
         readErrors++; 
         readErrorLast = readError; 
      }
      if (errorsPermitted) {
        errorsPermitted--;
        if (!errorsPermitted) {
          Serial.println(F("* WARNING: too many errors detected."));
          if (counterRepeatingRequest) Serial.println(F("* Switching counter request function off."));
          if (CONTROL_ID) Serial.println(F("* Switching control functionality off."));
          CONTROL_ID = CONTROL_ID_NONE;
          counterRepeatingRequest = 0;
          counterRequest = 0;
          Serial.println(F("* Warning: Upon ATmega restart auxiliary controller functionality and counter request functionality will be disabled."));
          EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
          EEPROM_update(EEPROM_ADDRESS_COUNTER_STATUS, counterRepeatingRequest);
        }
      }
    }
#ifdef MONITORCONTROL
    if (!readError) {
      // message received, no error detected, no buffer overrun
      byte w;
      if ((nread > 9) && (RB[0] == 0x00) && (RB[1] == 0x00) && (RB[2] == 0x12)) {
        // obtain day-of-week, hour, minute
        Tmin = RB[6];
        if (Tmin != Tminprev) {
          if (counterRepeatingRequest && !counterRequest) counterRequest = 1;
          Tminprev = Tmin;
        }
      }
      bool F030forcounter = false;
#ifdef KLICDA
      // request one counter per cycle in short pause after first 0012 msg at start of each minute
      if ((nread > 4) && (RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == 0x12)) {
        if (counterRequest) {
          WB[0] = 0x00;
          WB[1] = 0x00;
          WB[2] = 0xB8;
          WB[3] = (counterRequest - 1);
          if (P1P2Serial.writeready()) {
            // write KLICDA_DELAY ms after 400012 message
            // pause after 400012 is around 47 ms for some systems which is long enough for a 0000B8*/4000B8* counter request/response pair
            // in exceptional cases (1 in 300 in my system) the pause after 400012 is only 27ms,
            //      in which case the 4000B* reply arrives after the 000013* request
            //      (and in thoses cases the 000013* request is ignored)
            //      (NOTE!: if KLICDA_DELAY is chosen incorrectly, such as 5 ms in some example systems, this results in incidental bus collisions)
            P1P2Serial.writepacket(WB, 4, KLICDA_DELAY, crc_gen, crc_feed);
          } else {
            Serial.println(F("* Refusing to write counter-request packet while previous packet wasn't finished"));
            if (writeRefused < 0xFF) writeRefused++;
          }
          if (++counterRequest == 7) counterRequest = 0; // wait until next new minute; change 0 to 1 to continuously request counters for increased resolution
        }
      }
#else /* KLICDA */
      if ((FxAbsentCnt[0] == F0THRESHOLD) && counterRequest && (nread > 4) && (RB[0] == 0x00) && (RB[1] == 0xF0) && (RB[2] == 0x30)) {
        // 00F030 request message received; counterRequest > 0 so hijack every 4th time slot to request counters
        // but only if external F0 controller has not been detected (so check on FxAbsentCnt[0])
        // This works only if there is no other auxiliary controller responding to 0xF0, TODO: in that case extend to hijack 0xF1 timeslot
        if ((counterRequest & 0x03) == 0x01) {
          WB[0] = 0x00;
          WB[1] = 0x00;
          WB[2] = 0xB8;
          WB[3] = counterRequest >> 2;
          F030forcounter = true;
          if (P1P2Serial.writeready()) {
            P1P2Serial.writepacket(WB, 4, F03XDELAY, crc_gen, crc_feed);
          } else {
            Serial.println(F("* Refusing to write counter-request packet while previous packet wasn't finished"));
            if (writeRefused < 0xFF) writeRefused++;
          }
        }
        if (++counterRequest == 22) counterRequest = 0;
      }
#endif /* KLICDA */
      if ((nread > 4) && (RB[0] == 0x40) && ((RB[1] & 0xFE) == 0xF0) && ((RB[2] & 0x30) == 0x30)) {
        // 40Fx3x external controller reply received - note this could be our own (slow, delta=F030DELAY or F03XDELAY) reply so only reset count if delta < min(F03XDELAY, F030DELAY) (- margin)
        // Note for developers using >1 P1P2Monitor-interfaces (=to self): this detection mechanism fails if there are 2 P1P2Monitor programs (and adapters) with same delay settings on the same bus.
        // check if there is any auxiliary controller on 0x30 (including P1P2Monitor self, requires echo)
        if (RB[2] == 0x30) FxAbsentCntInclOwn[RB[1] & 0x01] = 0;
        Fx30ReplyDelay[RB[1] & 0x01] = (delta & 0xFF00) ? 0xFF : (delta & 0xFF);
        // check if there is any other auxiliary controller on 0x3x
        if ((delta < F03XDELAY - 2) && (delta < F030DELAY - 2)) {
          FxAbsentCnt[RB[1] & 0x01] = 0;
          if (RB[1] == CONTROL_ID) {
            // this should only happen if an external controller is connected if/after CONTROL_ID is set, either because
            //    -CONTROL_ID_DEFAULT is set and conflicts with external controller, or
            //    -because an external controller has been connected after CONTROL_ID has been manually set
            Serial.print(F("* Warning: external controller answering to address 0x"));
            Serial.print(RB[1], HEX);
            Serial.println(F(" detected"));
            CONTROL_ID = CONTROL_ID_NONE;
            EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
          }
        }
      } else if ((nread > 4) && (RB[0] == 0x00) && ((RB[1] & 0xFE) == 0xF0) && ((RB[2] & 0x30) == 0x30) && !F030forcounter) {
        // 00Fx3x request message received, and we did not use this slot to request counters
        // check if there is any controller on 0x30 (including P1P2Monitor self, requires echo)
        if (RB[2] == 0x30) {
          if (FxAbsentCntInclOwn[RB[1] & 0x01] > 1) {
            Fx30ReplyDelay[RB[1] & 0x01] = 0;
          } else {
            FxAbsentCntInclOwn[RB[1] & 0x01]++;
          }
        }
        // check if there is no other external controller
        if ((RB[2] == 0x30) && (FxAbsentCnt[RB[1] & 0x01] < F0THRESHOLD)) {
          FxAbsentCnt[RB[1] & 0x01]++;
          if (FxAbsentCnt[RB[1] & 0x01] == F0THRESHOLD) {
            Serial.print(F("* No external controller answering to address 0x"));
            Serial.print(RB[1], HEX);
            if (CONTROL_ID == RB[1]) {
              Serial.println(F(" detected, control functionality will restart"));
            } else {
              Serial.println(F(" detected, switching control functionality can be switched on (using L1)"));
            }
          }
        }
        // act as external controller:
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
            case 0x30 : // in: 17 byte; out: 17 byte; answer WB[7] should contain a 01 if we want to communicate a new setting in packet type 35
                        for (w = 3; w < n; w++) WB[w] = 0x00;
                        // set byte WB[7] to 0x01 to request a F035 message to set Value35 and/or DHWstatus
                        if (setRequestDHW || setRequest35) WB[7] = 0x01;
                        // set byte WB[8] to 0x01 to request a F036 message to set setParam36 to Value36
                        if (setRequest36) WB[8] = 0x01;
                        // set byte WB[12] to 0x01 to request a F03A message to set setParam3A to Value3A
                        if (setRequest3A) WB[8] = 0x01;
                        // set byte WB[<wr_pt - 34>2] to 0x01 to request a F03x message to set wr_nr to wr_val
                        if (wr_cnt) WB[wr_pt - 0x34] = 0x01;
                        d = F030DELAY;
                        wr = 1;
                        break;
            case 0x31 : // in: 15 byte; out: 15 byte; out pattern is copy of in pattern except for 2 bytes RB[7] RB[8]; function partly date/time, partly unknown
                        // RB[7] RB[8] seem to identify the auxiliary controller type;
                        // Do pretend to be a LAN adapter (even though this may trigger "data not in sync" upon restart?)
                        // If we don't set address, installer mode in main thermostat may become inaccessible
                        for (w = 3; w < n; w++) WB[w] = RB[w];
#ifdef CTRL_ID_1
                        WB[7] = CTRL_ID_1;
#endif
#ifdef CTRL_ID_2
                        WB[8] = CTRL_ID_2;
#endif
                        wr = 1;
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
                        // DHW parameter PARAM_DHW_ONOFF
                        // EHVX08S26CB9W DHWbooster parameter 0x48 (EHVX08S26CB9W)
                        // 35requester SetParam
                        // parameters 0144- or 0162- ASCII name of device
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        // change bytes for triggering 35request
                        w = 3;
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = (wr_val & 0xFF); wr_cnt--; wr_req = 1; }
                        if (setRequestDHW) { WB[w++] = PARAM_DHW_ONOFF & 0xff; WB[w++] = PARAM_DHW_ONOFF >> 8; WB[w++] = setStatusDHW; setRequestDHW = 0; }
                        if (setRequest35)  { WB[w++] = setParam35  & 0xff; WB[w++] = setParam35  >> 8; WB[w++] = setValue35;   setRequest35 = 0; }
                        // feedback no longer supported:
                        // for (w = 3; w < n; w+=3) if ((RB[w] | (RB[w+1] << 8)) == setParam35) Value35 = RB[w+2];
                        // for (w = 3; w < n; w+=3) if ((RB[w] | (RB[w+1] << 8)) == PARAM_DHW_ONOFF) DHWstatus = RB[w+2];
                        wr = 1;
                        break;
            case 0x36 : // in: 23 byte; out 23 byte; 2-byte parameters; reply with FF
                        // A parameter consists of 4 bytes: 2 bytes for param nr, and 2 bytes for value
                        // write bytes for parameter setParam36 to value set36status if setRequest36
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; wr_cnt--; wr_req = 1; }
                        if (setRequest36) { WB[w++] = setParam36 & 0xff; WB[w++] = (setParam36 >> 8) & 0xff; WB[w++] = setValue36 & 0xff; WB[w++] = (setValue36 >> 8) & 0xff; setRequest36 = 0; }
                        // check if set36status has been changed by main controller; removed this part as it is not the most reliable confirmation method
                        // for (w = 3; w < n; w+=4) if (((RB[w+1] << 8) | RB[w]) == setParam36) Value36 = RB[w+2] | (RB[w+3] << 8);
                        wr = 1;
                        break;
            case 0x37 : // in: 23 byte; out 23 byte; 3-byte parameters; reply with FF
                        // not seen in EHVX08S23D6V
                        // seen in EHVX08S26CB9W (value: 00001001010100001001)
                        // seen in EHYHBX08AAV3
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; WB[w++] = (wr_val >> 16) & 0xFF; wr_cnt--; wr_req = 1; }
                        wr = 1;
                        break;
            case 0x38 : // in: 21 byte; out 21 byte; 4-byte parameters; reply with FF
                        // parameter range 0000-001E; kwH/hour counters?
                        // not seen in EHVX08S23D6V
                        // seen in EHVX08S26CB9W
                        // seen in EHYHBX08AAV3
                        // A parameter consists of 6 bytes: 2 bytes for param nr, and 4 bytes for value
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; WB[w++] = (wr_val >> 16) & 0xFF; WB[w++] = (wr_val >> 24) & 0xFF; wr_cnt--; wr_req = 1; };
                        wr = 1;
                        break;
            case 0x39 : // in: 21 byte; out 21 byte; 4-byte parameters; reply with FF
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; WB[w++] = (wr_val >> 16) & 0xFF; WB[w++] = (wr_val >> 24) & 0xFF; wr_cnt--; wr_req = 1;}
                        wr = 1;
                        break;
            case 0x3A : // in: 21 byte; out 21 byte; 1-byte parameters reply with FF
                        // A parameter consists of 3 bytes: 2 bytes for param nr, and 1 byte for value
                        // parameters in the 00F03A message may indicate system status (silent, schedule, unit, DST, holiday)
                        // change bytes for triggering 3Arequest
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = (wr_val & 0xFF); wr_cnt--; wr_req = 1;}
                        if (setRequest3A)  { WB[w++] = setParam3A  & 0xff; WB[w++] = setParam3A  >> 8; WB[w++] = setValue3A;   setRequest3A = 0; }
                        // feedback no longer supported:
                        // for (w = 3; w < n; w+=3) if ((RB[w] | (RB[w+1] << 8)) == setParam3A) Value3A = RB[w+2];
                        wr = 1;
                        break;
            case 0x3B : // in: 23 byte; out 23 byte; 2-byte parameters; reply with FF
                        // not seen in EHVX08S23D6V
                        // seen in EHVX08S26CB9W
                        // seen in EHYHBX08AAV3
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; wr_cnt--; wr_req = 1; }
                        wr = 1;
                        break;
            case 0x3C : // in: 23 byte; out 23 byte; 3-byte parameters; reply with FF
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; WB[w++] = (wr_val >> 16) & 0xFF; wr_cnt--; wr_req = 1; }
                        wr = 1;
                        break;
            case 0x3D : // in: 21 byte; out: 21 byte; 4-byte parameters; reply with FF
                        // parameter range 0000-001F; kwH/hour counters?
                        // not seen in EHVX08S23D6V
                        // seen in EHVX08S26CB9W
                        // seen in EHYHBX08AAV3
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; WB[w++] = (wr_val >> 16) & 0xFF; WB[w++] = (wr_val >> 24) & 0xFF; wr_cnt--; wr_req = 1; }
                        wr = 1;
                        break;
            case 0x3E : // schedule related packet
                        // 0x3E01, 0x3E02, ... in: 23 byte; out: 23 byte; out 40F13E01(even for higher) + 19xFF
                        WB[3] = RB[3];
                        for (w = 4; w < n; w++) WB[w] = 0xFF;
                        wr = 1;
                        break;
            default   : // not seen, no response
                        break;
          }
          if (wr) {
            if (P1P2Serial.writeready()) {
              P1P2Serial.writepacket(WB, n, d, crc_gen, crc_feed);
              parameterWritesDone += wr_req; 
              wr_req = 0;
// TODO wr_req seems unneeded, just increment parameterWritesDone when wr_cnt-- above ?
            } else {
              Serial.println(F("* Refusing to write packet while previous packet wasn't finished"));
              if (writeRefused < 0xFF) writeRefused++;
              wr_req = 0;
            }
          }
        }
      }
    }
#endif /* MONITORCONTROL */
#ifdef PSEUDO_PACKETS
    if ((RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == 0x10)) pseudo0D = 5; // Insert one pseudo packet 00000D in output serial after 400010
    if ((RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == 0x11)) pseudo0E = 5; // Insert one pseudo packet 00000E in output serial after 400011
    if ((RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == 0x12)) pseudo0F = 5; // Insert one pseudo packet 00000F in output serial after 400012
#endif /* PSEUDO_PACKETS */
    if (readError) {
      Serial.print(F("* "));
    } else {
      if (verbose && (verbose < 4)) Serial.print(F("R "));
    }
    if (((verbose & 0x01) == 1) || readError) {
      // 3nd-12th characters show length of bus pause (max "R T 65.535: ")
      Serial.print(F("T "));
      if (delta < 10000) Serial.print(F(" "));
      if (delta < 1000) Serial.print(F("0")); else { Serial.print(delta / 1000); delta %= 1000; };
      Serial.print(F("."));
      if (delta < 100) Serial.print(F("0"));
      if (delta < 10) Serial.print(F("0"));
      Serial.print(delta);
      Serial.print(F(": "));
    }
    if ((verbose < 4) || readError) {
      for (int i = 0; i < nread; i++) {
        if (verbose && (EB[i] & ERROR_READBACK)) {
          // collision suspicion due to data verification error in reading back written data
          Serial.print(F("-XX:"));
        }
        if (verbose && (EB[i] & ERROR_BUSCOLLISION)) {
          // collision suspicion due to 0 during 2nd half bit signal read back
          Serial.print(F("-BC:"));
        }
        if (verbose && (EB[i] & ERROR_PE)) {
          // parity error detected
          Serial.print(F("-PE:"));
        }
        byte c = RB[i];
        if (crc_gen && (verbose == 1) && (i == nread - 1)) {
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
#ifdef PSEUDO_PACKETS
  if (pseudo0D > 4) {
    pseudo0D = 0;
    WB[0]  = 0x00;
    WB[1]  = 0x00;
    WB[2]  = 0x0D;
    for (int i = 3; i <= 22; i++) WB[i]  = 0x00; // reserved for future use
    if (verbose < 4) writePseudoPacket(WB, 23);
  }
  if (pseudo0E > 4) {
    pseudo0E = 0;
    WB[0]  = 0x00;
    WB[1]  = 0x00;
    WB[2]  = 0x0E;
    WB[3]  = (upt >> 24) & 0xFF;
    WB[4]  = (upt >> 16) & 0xFF;
    WB[5]  = (upt >> 8) & 0xFF;
    WB[6]  = upt & 0xFF;
    WB[7]  = (F_CPU >> 24) & 0xFF;
    WB[8]  = (F_CPU >> 16) & 0xFF;
    WB[9]  = (F_CPU >> 8) & 0xFF;
    WB[10] = F_CPU & 0xFF;
    WB[11] = writeRefused;
    WB[12] = counterRepeatingRequest;
    WB[13] = counterRequest;
    WB[14] = errorsLargePacket;
    WB[15] = wr_cnt;
    WB[16] = wr_pt;
    WB[17] = wr_nr >> 8;
    WB[18] = wr_nr & 0xFF;
    WB[19] = wr_val >> 24;
    WB[20] = 0xFF & (wr_val >> 16);
    WB[21] = 0xFF & (wr_val >> 8);
    WB[22] = 0xFF & wr_val;
    if (verbose < 4) writePseudoPacket(WB, 23);
  }
  if (pseudo0F > 4) {
    bool sigMatch = 1;
    pseudo0F = 0;
    WB[0]  = 0x00;
    WB[1]  = 0x00;
    WB[2]  = 0x0F;
    WB[3]  = Compile_Options;
    WB[4]  = verbose;
    WB[5]  = save_MCUSR;
    WB[6]  = writePermission;
    WB[7]  = errorsPermitted;
    WB[8]  = (parameterWritesDone >> 8) & 0xFF;
    WB[9]  = parameterWritesDone & 0xFF;
    WB[10] = (sd >> 8) & 0xFF;
    WB[11] = sd & 0xFF;
    WB[12] = (sdto >> 8) & 0xFF;
    WB[13] = sdto & 0xFF;
    WB[14] = 0x00;
#ifdef EEPROM_SUPPORT
    WB[15] = EEPROM.read(EEPROM_ADDRESS_CONTROL_ID);
    WB[16] = EEPROM.read(EEPROM_ADDRESS_VERBOSITY);
    WB[17] = EEPROM.read(EEPROM_ADDRESS_COUNTER_STATUS);
    for (uint8_t i = 0; i < strlen(EEPROM_SIGNATURE); i++) sigMatch &= (EEPROM.read(EEPROM_ADDRESS_SIGNATURE + i) == EEPROM_SIGNATURE[i]);
    WB[18] = sigMatch;
#else
    WB[15] = 0x00;
    WB[16] = 0x00;
    WB[17] = 0x00;
    WB[18] = 0x00;
#endif
    WB[19] = 0x00;
    WB[20] = readErrors;
    WB[21] = readErrorLast;
    WB[22] = CONTROL_ID;
    if (verbose < 4) writePseudoPacket(WB, 23);
  }
#endif /* PSEUDO_PACKETS */
}
