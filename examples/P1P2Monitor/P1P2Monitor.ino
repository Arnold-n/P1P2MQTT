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
 * Copyright (c) 2019-2022 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 * 20221224 v0.9.30 expand F series write possibilities
 * 20221211 v0.9.29 add control_ID in bool in pseudopacket, fix 3+4-byte writes, FXMQ support
 * 20221129 v0.9.28 option to insert message in 40F030 time slot for restart or user-defined write message
 * 20221102 v0.9.24 suppress repeated "too long" warnings
 * 20221029 v0.9.23 ADC code, fix 'W' command, misc
 * 20220918 v0.9.22 scopemode for writes and focused on actual errors, fake error generation for test purposes, control for FDY/FDYQ (#define F_SERIES), L2/L3/L5 mode
 * 20220903 v0.9.19 minor change in serial output
 * 20220830 v0.9.18 version alignment for firmware image release
 * 20220819 v0.9.17-fix1 fixed non-functioning 'E'/'n' write commands
 * 20220817 v0.9.17 scopemode, fixed 'W' command handling magic string prefix, SERIALSPEED/OLDP1P2LIB in P1P2Config.h now depends on F_CPU/COMBIBOARD selection, ..
 * 20220810 v0.9.15 Improved uptime handling
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
 * Wishlist:
 * -schedule writes
 * -continue control operating (L1) immediately after reset/brown-out, but not immediately after power-up reset
 */

#include "P1P2Config.h"
#include <P1P2Serial.h>

#define SPI_CLK_PIN_VALUE (PINB & 0x20)

static byte verbose = INIT_VERBOSE;
static byte readErrors = 0;
static byte readErrorLast = 0;
static byte writeRefused = 0;
static byte errorsLargePacket = 0;
static byte controlLevel = 1; // for F-series L5 mode
#ifdef ENABLE_INSERT_MESSAGE
static byte restartDaikinCnt = 0;
static byte restartDaikinReady = 0;
static byte insertMessageCnt = 0;
static byte insertMessage[RB_SIZE];
static byte insertMessageLength = 0;
#endif

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
#ifdef E_SERIES
+0x20
#endif
#ifdef F_SERIES
+0x40
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
static byte echo = INIT_ECHO;           // echo setting (whether written data is read back)
static byte scope = INIT_SCOPE;         // scope setting (to log timing info)
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
int32_t upt_prev_pseudo = 0;
int32_t upt_prev_write = 0;
int32_t upt_prev_error = 0;
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
static errorbuf_t EB[RB_SIZE];

// serial-receive using direct access to serial read buffer RS
// rs is # characters in buffer;
// sr_buffer is pointer (RS+rs)

static uint8_t rs=0;
static char *RSp = RS;
static byte hwID = 0;

void setup() {
  save_MCUSR = MCUSR;
  MCUSR = 0;
  hwID = SPI_CLK_PIN_VALUE ? 0 : 1;
  Serial.begin(SERIALSPEED);
  while (!Serial);      // wait for Arduino Serial Monitor to open
  Serial.println(F("*"));
  Serial.println(F(WELCOMESTRING));
  Serial.print(F("* Compiled "));
  Serial.print(F(__DATE__));
  Serial.print(F(" "));
  Serial.print(F(__TIME__));
#ifdef MONITORCONTROL
  Serial.print(F(" +control"));
#endif /* MONITORCONTROL */
#ifdef KLICDA
  Serial.print(F(" +klicda"));
#endif /* KLICDA */
#ifdef E_SERIES
  Serial.print(F(" E-series"));
#endif /* E_SERIES */
#ifdef F_SERIES
  Serial.print(F(" F-series"));
#endif /* F_SERIES */
  Serial.println();
  Serial.print(F("* P1P2-ESP-interface hwID "));
  Serial.println(hwID);
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
    if (save_MCUSR & (1 << EXTRF)) Serial.print(F(" (ext-reset)")); // 2
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
#ifdef OLDP1P2LIB
    Serial.println(F("* OLDP1P2LIB"));
#else
    Serial.println(F("* NEWP1P2LIB"));
#endif
  }
  P1P2Serial.begin(9600, hwID ? true : false, 6, 7); // if hwID = 1, use ADC6 and ADC7
  P1P2Serial.setEcho(echo);
#ifdef SW_SCOPE
  P1P2Serial.setScope(scope);
#endif
  P1P2Serial.setDelayTimeout(sdto);
  Serial.println(F("* Ready setup"));
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
const uint32_t PROGMEM nr_params[PARAM_ARR_SZ] = { 0x014A, 0x002D, 0x0001, 0x001F, 0x00F0, 0x006C, 0x00AF, 0x0002, 0x0020 }; // number of parameters observed
//byte packettype                              = {   0x35,   0x36,   0x37,   0x38,   0x39,   0x3A,   0x3B,   0x3C,   0x3D };

void(* resetFunc) (void) = 0; // declare reset function at address 0

static byte pseudo0D = 0;
static byte pseudo0E = 0;
static byte pseudo0F = 0;

uint8_t scope_budget = 200;

void loop() {
  uint16_t temp;
  uint16_t temphex;
  int wb = 0;
  int n;
  int wbtemp;
  int c;
  static byte ignoreremainder = 2; // ignore first line from serial input to avoid misreading a partial message just after reboot
  static bool reportedTooLong = 0;

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
      //  (c != '\n' ||  rs == RS_SIZE)
      char lst = *(RSp - 1);
      *(RSp - 1) = '\0';
      if (c != '\n') {
        if (!reportedTooLong) {
          Serial.print(F("* Line too long, ignored, ignoring remainder: ->"));
          Serial.print(RS);
          Serial.print(lst);
          Serial.print(c);
          Serial.println(F("<-"));
          // show full line, so not yet setting reportedTooLong = 1 here;
        }
        ignoreremainder = 1;
      } else {
        if (!reportedTooLong) {
          Serial.print(F("* Line too long, terminated, ignored: ->"));
          Serial.print(RS);
          Serial.print(lst);
          Serial.println(F("<-"));
          reportedTooLong = 1;
        }
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
        Serial.print(F("* First line ignored: ->")); // do not copy - usually contains boot-related nonsense
        if (rs < 10) {
          Serial.print(RS);
        } else {
          RS[9] = '\0';
          Serial.print(RS);
          Serial.print("...");
        }
        Serial.println("<-");
        ignoreremainder = 0;
      } else if (ignoreremainder == 1) {
        if (!reportedTooLong) {
          Serial.print(F("* Line too long, last part: "));
          Serial.println(RS);
          reportedTooLong = 1;
        }
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
#ifdef E_SERIES
            case 'e':
            case 'E':
                      if (wr_cnt) {
                        // previous write still being processed
                        Serial.println(F("* Previous parameter write action still busy"));
                        break;
                      } else if ( (n = sscanf(RSp, (const char*) "%2x%4x%8lx", &wr_pt, &wr_nr, &wr_val)) == 3) {
                        if ((wr_pt < PARAM_TP_START) || (wr_pt > PARAM_TP_END)) {
                          Serial.print(F("* wr_pt: 0x"));
                          Serial.print(wr_pt, HEX);
                          Serial.println(F(" out of range 0x35-0x3D"));
                          break;
                        }
                        if (wr_nr > nr_params[wr_pt - PARAM_TP_START]) {
                          Serial.print(F("* wr_nr > expected: 0x"));
                          Serial.println(wr_nr, HEX);
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
                          if (writePermission != 0xFF) writePermission--;
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
#endif /* E_SERIES */
#ifdef F_SERIES
            case 'f':
            case 'F': if (wr_cnt) {
                        // previous write still being processed
                        Serial.println(F("* Previous parameter write action still busy"));
                        break;
                      } else if ( (n = sscanf(RSp, (const char*) "%2x%2d%2x", &wr_pt, &wr_nr, &wr_val)) == 3) {
                        if ((wr_pt != 0x38) && (wr_pt != 0x3B)) {
                          Serial.print(F("* wr_pt: 0x"));
                          Serial.print(wr_pt, HEX);
                          Serial.println(F(" is not 0x38 or 0x3B"));
                          break;
                        }
                        // check valid write parameters
                        if ((wr_nr > 17) || (wr_nr == 3) || (wr_nr == 5) || (wr_nr == 7) || ((wr_nr >= 9) && (wr_nr <= 15)) || ((wr_nr == 16) && (wr_pt == 0x38)) || ((wr_nr == 17) && (wr_pt == 0x38))) {
                          Serial.print(F("* wr_nr invalid, should be 0, 1, 2, 4, 6, 8 (or for packet type 0x3B: 16 or 17): "));
                          Serial.println(wr_nr);
                          break;
                        }
                        if (wr_val > 0xFF) {
                          Serial.print(F("* wr_val > 0xFF: "));
                          Serial.println(wr_val);
                          break;
                        }
                        if ((wr_nr == 0) && (wr_val > 1)) {
                          Serial.println(F("wr_val for payload byte 0 (status) must be 0 or 1"));
                          break;
                        }
                        if ((wr_nr == 1) && ((wr_val < 0x60) || (wr_val > 0x67))) {
                          Serial.println(F("wr_val for payload byte 1 (operating-mode) must be in range 0x60-0x67"));
                          break;
                        }
                        if (((wr_nr == 2) || (wr_nr == 6)) && ((wr_val < 0x0A) || (wr_val > 0x1E))) {
                          Serial.println(F("wr_val for payload byte 2/6 (target-temp cooling/heating) must be in range 0x10-0x20"));
                          break;
                        }
                        if (((wr_nr == 4) || (wr_nr == 8)) && ((wr_val < 0x11) || (wr_val > 0x51))) {
                          Serial.println(F("wr_val for payload byte 4/8 (fan-speed cooling/heating) must be in range 0x11-0x51"));
                          break;
                        }
                        // no limitations for wr_nr == 16
                        if ((wr_nr == 17) && (wr_val > 0x03)) {
                          Serial.println(F("wr_val for payload byte 17 (fan-mode) must be in range 0x00-0x03"));
                          break;
                        }
                        if (writePermission) {
                          if (writePermission != 0xFF) writePermission--;
                          wr_cnt = WR_CNT; // write repetitions, 1 should be enough (especially for F-series)
                          Serial.print(F("* Initiating write for packet-type 0x"));
                          Serial.print(wr_pt, HEX);
                          Serial.print(F(" payload byte "));
                          Serial.print(wr_nr);
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
                          Serial.print(F(" nr: "));
                          Serial.print(wr_nr);
                        }
                        Serial.println();
                      }
                      break;
#endif /* F_SERIES */
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
                      Serial.print(F(__DATE__));
                      Serial.print(F(" "));
                      Serial.println(F(__TIME__));
#ifdef OLDP1P2LIB
                      Serial.print(F("* OLDP1P2LIB"));
#else
                      Serial.print(F("* NEWP1P2LIB"));
#endif
#ifdef E_SERIES
                      Serial.println(F(" E-series"));
#endif /* E_SERIES */
#ifdef F_SERIES
                      Serial.println(F(" F-series"));
#endif /* F_SERIES */
                      Serial.print(F("* Reset cause: MCUSR="));
                      Serial.print(save_MCUSR);
                      if (save_MCUSR & (1 << BORF))  Serial.print(F(" (brown-out-detected)")); // 4
                      if (save_MCUSR & (1 << EXTRF)) Serial.print(F(" (ext-reset)")); // 2
                      if (save_MCUSR & (1 << PORF)) Serial.print(F(" (power-on-reset)")); // 1
                      Serial.println();
                      Serial.print(F("* P1P2-ESP-Interface hwID "));
                      Serial.println(hwID);
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
#ifdef SW_SCOPE
            case 'u':
            case 'U': if (verbose) Serial.print(F("* Software-scope "));
                      if (scanint(RSp, temp) == 1) {
                        scope = temp;
                        if (scope > 1) scope = 1;
                        P1P2Serial.setScope(scope);
                        if (!verbose) break;
                        Serial.print(F("set to "));
                      }
                      Serial.println(scope);
                      scope_budget = 200;
                      break;
#endif
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
                        // insert message in time for 40F030 slot in L1 mode
                        if (insertMessageCnt || restartDaikinCnt) {
                          Serial.println(F("* insertMessage (or restartDaikin) already scheduled"));
                          break;
                        }
                        if (verbose) Serial.print(F("* Writing next 40F030 slot: "));
                        insertMessageLength = 0;
                        restartDaikinReady = 0; // indicate the insertMessage is overwritten and restart cannot be done until new packet received/loaded
                        while ((wb < RB_SIZE) && (wb < WB_SIZE) && (sscanf(RSp, (const char*) "%2x%n", &wbtemp, &n) == 1)) {
                          insertMessage[insertMessageLength++] = wbtemp;
                          insertMessageCnt = 1;
                          RSp += n;
                          if (verbose) {
                            if (wbtemp <= 0x0F) Serial.print("0");
                            Serial.print(wbtemp, HEX);
                          }
                        }
                        if (insertMessageCnt) {
                          Serial.println();
                        } else {
                          Serial.println(F("- valid data missing, write skipped"));
                        }
                        break;
                      }
                      if (verbose) Serial.print(F("* Writing: "));
                      wb = 0;
                      while ((wb < WB_SIZE) && (sscanf(RSp, (const char*) "%2x%n", &wbtemp, &n) == 1)) {
                        WB[wb++] = wbtemp;
                        RSp += n;
                        if (verbose) {
                          if (wbtemp <= 0x0F) Serial.print("0");
                          Serial.print(wbtemp, HEX);
                        }
                      }
                      if (verbose) Serial.println();
                      if (P1P2Serial.writeready()) {
                        if (wb) {
                          P1P2Serial.writepacket(WB, wb, sd, crc_gen, crc_feed);
                        } else {
                          Serial.println(F("* Refusing to write empty packet"));
                        }
                      } else {
                        Serial.println(F("* Refusing to write packet while previous packet wasn't finished"));
                        if (writeRefused < 0xFF) writeRefused++;
                      }
                      break;
            case 'k': // reset ATmega
            case 'K': Serial.println(F("* Resetting ...."));
                      resetFunc(); //call reset
            case 'l': // set auxiliary controller function on/off; CONTROL_ID address is set automatically
            case 'L': // 0 controller mode off, and store setting in EEPROM
                      // 1 controller mode on, and store setting in EEPROM
                      // 2 controller mode off, do not store setting in EEPROM
                      // 3 controller mode on, do not store setting in EEPROM
                      // 5 (for experimenting with F-series only) controller responds only to 00F030 message withan empty packet, but not to other 00F03x packets
                      // other values are treated as 0 and are reserved for further experimentation of control modes
                      if (scanint(RSp, temp) == 1) {
#ifdef ENABLE_INSERT_MESSAGE
                        if (temp == 99) {
                          if (insertMessageCnt) {
                            Serial.println(F("* insertMessage already scheduled"));
                            break;
                          }
                          if (restartDaikinCnt) {
                            Serial.println(F("* restartDaikin already scheduled"));
                            break;
                          }
                          if (!restartDaikinReady) {
                            Serial.println(F("* restartDaikin waiting to receive sample payload"));
                            break;
                          }
                          restartDaikinCnt = RESTART_NR_MESSAGES;
                          Serial.println(F("* Scheduling attempt to restart Daikin"));
                          break;
                        }
#endif
#ifdef E_SERIES
                        if (temp > 3) temp = 0;
#endif
#ifdef F_SERIES
                        if ((temp > 5) || (temp == 4)) temp = 0;
#endif
                        byte setMode = temp & 0x01;
                        if (setMode) {
                          if (errorsPermitted < MIN_ERRORS_PERMITTED) {
                            Serial.println(F("* Errorspermitted (error budget) too low; control functionality cannot enabled"));
                            break;
                          }
                          if (CONTROL_ID) {
                            Serial.print(F("* CONTROL_ID is already 0x"));
                            Serial.println(CONTROL_ID, HEX);
                            if (temp < 2) EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
                            break;
                          }
                          if (FxAbsentCnt[0] == F0THRESHOLD) {
                            Serial.println(F("* Control_ID 0xF0 supported, no auxiliary controller found for 0xF0, switching control functionality on 0xF0 on"));
                            CONTROL_ID = 0xF0;
                            controlLevel = (temp == 5) ? 0 : 1; // F-series: special mode L5 answers only to F030
                            if (temp < 2) EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID); // L2/L3/L5 for short experiments, doesn't survive ATmega reboot !
#ifdef E_SERIES
                          } else if (FxAbsentCnt[1] == F0THRESHOLD) {
                            Serial.println(F("* Control_ID 0xF1 supported, no auxiliary controller found for 0xF1, switching control functionality on 0xF1 on"));
                            CONTROL_ID = 0xF1;
                            if (temp < 2) EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
#endif
                          } else {
                            Serial.println(F("* No free address for controller found (yet). Control functionality not enabled"));
                            Serial.println(F("* You may wish to re-try in a few seconds (and perhaps switch auxiliary controllers off)"));
                          }
                        } else {
                          if (!CONTROL_ID) {
                            Serial.print(F("* CONTROL_ID is already 0x00"));
                            break;
                          } else {
                            CONTROL_ID = 0x00;
                          }
                          if (temp < 2) EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
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
#ifdef E_SERIES
            case 'c': // set counterRequest cycle (once or repetitive)
            case 'C': if (scanint(RSp, temp) == 1) {
                        if (temp > 1) {
                          if (errorsPermitted < MIN_ERRORS_PERMITTED) {
                            Serial.println(F("* Errorspermitted too low; control functinality not enabled"));
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
                          if (writePermission != 0xFF) writePermission--;
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
                          if (writePermission != 0xFF) writePermission--;
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
                          if (writePermission != 0xFF) writePermission--;
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
                          if (writePermission != 0xFF) writePermission--;
                          setRequestDHW = 1;
                          setStatusDHW = temp;
                          if (!verbose) break;
                          Serial.print(F(" will be set to 0x"));
                          if (setStatusDHW <= 0x000F) Serial.print(F("0"));
                          Serial.println(setStatusDHW);
                          break;
                        } else {
                          Serial.println(F("* Currently no write budget left"));
                          break;
                        }
                      } else if (setRequest35) {
                        Serial.println(F(": DHW-write-request to value 0x"));
                        Serial.println(setValue35, HEX);
                        Serial.println(F(" still pending"));
                      } else {
                        Serial.println(F(": no DHW-write-request pending"));
                      }
                      break;
#endif /* E_SERIES */
            default:  Serial.print(F("* Command not understood: "));
                      Serial.println(RSp - 1);
                      break;
          }
#ifdef SERIAL_MAGICSTRING
/*
        } else {
          if (!strncmp(RS, "* [ESP]", 7)) {
            Serial.print(F("* Ignoring: "));
          } else {
            Serial.print(F("* Magic String mismatch: "));
          }
          Serial.println(RS);
*/
#endif
        }
      }
    }
    RSp = RS;
    rs = 0;
  }
  int32_t upt = P1P2Serial.uptime_sec();
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
    errorbuf_t readError = 0;
    int nread = P1P2Serial.readpacket(RB, delta, EB, RB_SIZE, crc_gen, crc_feed);
    if (nread > RB_SIZE) {
      Serial.println(F("* Received packet longer than RB_SIZE"));
      nread = RB_SIZE;
      readError = 0xFF;
      if (errorsLargePacket < 0xFF) errorsLargePacket++;
    }
    for (int i = 0; i < nread; i++) readError |= EB[i];
#ifdef SW_SCOPE

#if F_CPU > 8000000L
#define FREQ_DIV 4 // 16 MHz
#else
#define FREQ_DIV 3 // 8 MHz
#endif

    if (scope && ((readError && (scope_budget > 5)) || (((RB[0] == 0x40) && (RB[1] == 0xF0)) && (scope_budget > 50)) || (scope_budget > 150))) {
      // always keep scope write budget for 40F0 and expecially for readErrors
      if (sws_cnt || (sws_event[SWS_MAX - 1] != SWS_EVENT_LOOP)) {
        scope_budget -= 5;
        if (readError) {
          Serial.print(F("C "));
        } else {
          Serial.print(F("c "));
        }
        if (RB[0] < 0x10) Serial.print('0');
        Serial.print(RB[0], HEX);
        if (RB[1] < 0x10) Serial.print('0');
        Serial.print(RB[1], HEX);
        if (RB[2] < 0x10) Serial.print('0');
        Serial.print(RB[2], HEX);
        Serial.print(' ');
        static uint16_t capture_prev;
        int i = 0;
        if (sws_event[SWS_MAX - 1] != SWS_EVENT_LOOP) i = sws_cnt;
        bool skipfirst = true;
        do {
          switch (sws_event[i]) {
            // error events
            case SWS_EVENT_ERR_BE      : Serial.print(F("   BE   ")); break;
            case SWS_EVENT_ERR_BE_FAKE : Serial.print(F("   be   ")); break;
            case SWS_EVENT_ERR_SB      : Serial.print(F("   SB   ")); break;
            case SWS_EVENT_ERR_SB_FAKE : Serial.print(F("   sb   ")); break;
            case SWS_EVENT_ERR_BC      : Serial.print(F("   BC   ")); break;
            case SWS_EVENT_ERR_BC_FAKE : Serial.print(F("   bc   ")); break;
            case SWS_EVENT_ERR_PE      : Serial.print(F("   PE   ")); break;
            case SWS_EVENT_ERR_PE_FAKE : Serial.print(F("   pe   ")); break;
            case SWS_EVENT_ERR_LOW     : Serial.print(F("   lw   ")); break;
            // read/write related events
            default   : byte sws_ev    = sws_event[i] & SWS_EVENT_MASK;
                        byte sws_state = sws_event[i] & 0x1F;
                        if (!skipfirst) {
                          if (sws_ev == SWS_EVENT_EDGE_FALLING_W) Serial.print(' ');
                          uint16_t t_diff = (sws_capture[i] - capture_prev) >> FREQ_DIV;
                          if (t_diff < 10) Serial.print(' ');
                          if (t_diff < 100) Serial.print(' ');
                          Serial.print(t_diff);
                          if (sws_ev != SWS_EVENT_EDGE_FALLING_W) Serial.print(':');
                        }
                        capture_prev = sws_capture[i];
                        skipfirst = false;
                        switch (sws_ev) {
                          case SWS_EVENT_SIGNAL_LOW     : Serial.print('?'); break;
                          case SWS_EVENT_EDGE_FALLING_W : Serial.print(' '); break;
                          case SWS_EVENT_EDGE_RISING    :
                                                          // state = 1 .. 20 (1,2 start bit, 3-18 data bit, 19,20 parity bit),
                                                          switch (sws_state) {
                                                            case 1       :
                                                            case 2       : Serial.print('S'); break; // start bit
                                                            case 3 ... 18: Serial.print((sws_state - 3) >> 1); break; // state=3-18 for data bit 0-7
                                                            case 19      :
                                                            case 20      : Serial.print('P'); break; // parity bit
                                                            default      : Serial.print(' '); break;
                                                          }
                                                          break;
                          case SWS_EVENT_EDGE_SPIKE     :
                          case SWS_EVENT_SIGNAL_HIGH_R  :
                          case SWS_EVENT_EDGE_FALLING_R :
                                                          switch (sws_state) {
                                                            case 11      : Serial.print('E'); break; // stop bit ('end' bit)
                                                            case 10      : Serial.print('P'); break; // parity bit
                                                            case 0       :
                                                            case 1       : Serial.print('S'); break; // parity bit
                                                            case 2 ... 9 : Serial.print(sws_state - 2); break; // state=2-9 for data bit 0-7
                                                            default      : Serial.print(' '); break;
                                                          }
                                                          break;
                          default                       : Serial.print(' ');
                                                          break;
                        }
                        switch (sws_ev) {
                          case SWS_EVENT_EDGE_SPIKE     : Serial.print(F("-X-"));  break;
                          case SWS_EVENT_SIGNAL_HIGH_R  : Serial.print(F("‾‾‾")); break;
                          case SWS_EVENT_SIGNAL_LOW     : Serial.print(F("___")); break;
                          case SWS_EVENT_EDGE_RISING    : Serial.print(F("_/‾")); break;
                          case SWS_EVENT_EDGE_FALLING_W :
                          case SWS_EVENT_EDGE_FALLING_R : Serial.print(F("‾\\_")); break;
                          default                       : Serial.print(F(" ? ")); break;
                        }
          }
          if (++i == SWS_MAX) i = 0;
        } while (i != sws_cnt);
        Serial.println();
      }
    }
    if (++scope_budget > 200) scope_budget = 200;
    sws_block = 0; // release SW_SCOPE for next log operation
#endif
#ifdef MEASURE_LOAD
    if (irq_r) {
      uint8_t irq_busy_c1 = irq_busy;
      uint16_t irq_lapsed_r_c = irq_lapsed_r;
      uint16_t irq_r_c = irq_r;
      uint8_t  irq_busy_c2 = irq_busy;
      if (irq_busy_c1 || irq_busy_c2) {
        Serial.println(F("* irq_busy"));
      } else {
        Serial.print(F("* sh_r="));
        Serial.println(100.0 * irq_r_c / irq_lapsed_r_c);
      }
    }
    if (irq_w) {
      uint8_t irq_busy_c1 = irq_busy;
      uint16_t irq_lapsed_w_c = irq_lapsed_w;
      uint16_t irq_w_c = irq_w;
      uint8_t  irq_busy_c2 = irq_busy;
      if (irq_busy_c1 || irq_busy_c2) {
        Serial.println(F("* irq_busy"));
      } else {
        Serial.print(F("* sh_w="));
        Serial.println(100.0 * irq_w_c / irq_lapsed_w_c);
      }
    }
#endif
    if ((readError & ERROR_REAL_MASK) && upt) { // don't count errors while upt == 0
      if (readErrors < 0xFF) {
        readErrors++;
        readErrorLast = readError;
      }
      if (errorsPermitted) {
        errorsPermitted--;
        if (!errorsPermitted) {
          Serial.println(F("* WARNING: too many read errors detected"));
          if (counterRepeatingRequest) Serial.println(F("* Switching counter request function off"));
          if (CONTROL_ID) Serial.println(F("* Switching control functionality off"));
          CONTROL_ID = CONTROL_ID_NONE;
          counterRepeatingRequest = 0;
          counterRequest = 0;
          Serial.println(F("* Warning: Upon ATmega restart auxiliary controller functionality and counter request functionality will remain switched off"));
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
        // but only if auxiliary F0 controller has not been detected (so check on FxAbsentCnt[0])
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
        // 40Fx3x auxiliary controller reply received - note this could be our own (slow, delta=F030DELAY or F03XDELAY) reply so only reset count if delta < min(F03XDELAY, F030DELAY) (- margin)
        // Note for developers using >1 P1P2Monitor-interfaces (=to self): this detection mechanism fails if there are 2 P1P2Monitor programs (and adapters) with same delay settings on the same bus.
        // check if there is any auxiliary controller on 0x30 (including P1P2Monitor self, requires echo)
        if (RB[2] == 0x30) FxAbsentCntInclOwn[RB[1] & 0x01] = 0;
        Fx30ReplyDelay[RB[1] & 0x01] = (delta & 0xFF00) ? 0xFF : (delta & 0xFF);
        // check if there is any other auxiliary controller on 0x3x
        if ((delta < F03XDELAY - 2) && (delta < F030DELAY - 2)) {
          FxAbsentCnt[RB[1] & 0x01] = 0;
          if (RB[1] == CONTROL_ID) {
            // this should only happen if an auxiliary controller is connected if/after CONTROL_ID is set, either because
            //    -CONTROL_ID_DEFAULT is set and conflicts with auxiliary controller, or
            //    -because an auxiliary controller has been connected after CONTROL_ID has been manually set
            Serial.print(F("* Warning: another auxiliary controller is answering to address 0x"));
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
        // check if there is no other auxiliary controller
        if ((RB[2] == 0x30) && (FxAbsentCnt[RB[1] & 0x01] < F0THRESHOLD)) {
          FxAbsentCnt[RB[1] & 0x01]++;
          if (FxAbsentCnt[RB[1] & 0x01] == F0THRESHOLD) {
            Serial.print(F("* No auxiliary controller answering to address 0x"));
            Serial.print(RB[1], HEX);
            if (CONTROL_ID == RB[1]) {
              Serial.println(F(" detected, control functionality will restart"));
              insertMessageCnt = 0;  // avoid delayed insertMessage/restartDaikin
              restartDaikinCnt = 0;
            } else {
              Serial.println(F(" detected, switching control functionality can be switched on (using L1)"));
            }
          }
        }
        // act as auxiliary controller:
        if ((CONTROL_ID && (FxAbsentCnt[CONTROL_ID & 0x01] == F0THRESHOLD) && (RB[1] == CONTROL_ID))
#ifdef ENABLE_INSERT_MESSAGE
            || ((insertMessageCnt || restartDaikinCnt) && (RB[0] == 0x00) && (RB[1] == 0xF0) && (RB[2] == 0x30))
#endif
                                                                                                     ) {
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
            Serial.println(nread); }
          switch (RB[2]) {
#ifdef E_SERIES
            case 0x30 :
#ifdef ENABLE_INSERT_MESSAGE
                        if (insertMessageCnt || restartDaikinCnt) {
                          for (int i = 0; i < insertMessageLength; i++) WB[i] = insertMessage[i];
                          if (insertMessageCnt) {
                            Serial.println(F("* Insert user-specified message"));
                            insertMessageCnt--;
                          }
                          if (restartDaikinCnt) {
                            Serial.println(F("* Attempt to restart Daikin"));
                            WB[RESTART_PACKET_PAYLOAD_BYTE + 3] |= RESTART_PACKET_BYTE;
                            restartDaikinCnt--;
                          }
                          d = F030DELAY_INSERT;
                          n = insertMessageLength;
                          wr = 1;
                          break;
                        }
#endif
                        // in: 17 byte; out: 17 byte; answer WB[7] should contain a 01 if we want to communicate a new setting in packet type 35
                        for (w = 3; w < n; w++) WB[w] = 0x00;
                        // set byte WB[7] to 0x01 to request a F035 message to set Value35 and/or DHWstatus
                        if (setRequestDHW || setRequest35) WB[7] = 0x01;
                        // set byte WB[8] to 0x01 to request a F036 message to set setParam36 to Value36
                        if (setRequest36) WB[8] = 0x01;
                        // set byte WB[12] to 0x01 to request a F03A message to set setParam3A to Value3A
                        if (setRequest3A) WB[12] = 0x01;
                        // set byte WB[<wr_pt - 0x2E>] to 0x01 to request a F03x message to set wr_nr to wr_val
                        if (wr_cnt) WB[wr_pt - 0x2E] = 0x01;
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
#endif /* E_SERIES */
#ifdef F_SERIES
            case 0x30 : // all models: polling auxiliary controller, reply with empty payload
              d = F030DELAY;
              wr = 1;
              n = 3;
              break;
#ifdef FDY
            case 0x38 : // FDY control message, copy bytes back and change if 'F' command is given
              wr = controlLevel;
              n = 18;
              for (w = 13; w <= 15; w++) WB[w] = 0x00;
              WB[3]  = RB[3] & 0x01;           // W target status
              WB[4]  = (RB[5] & 0x07) | 0x60;  // W target operating mode
              WB[5]  = RB[7];                  // W target temperature cooling
              WB[6]  = 0x00;                   //   clear change flag 80 from input (alternative:? WB[6] = RB[8] & 0x7F)
              WB[7]  = (RB[9] & 0x60) | 0x11;  // W target fan speed cooling/fan    (alternative: WB[7] = RB[9] & 0x7F might work too)
              WB[8]  = 0x00;                   //   clear change flag 80 from input
              WB[9]  = RB[11];                 // W target temperature heating
              WB[10] = 0x00;                   //   clear change flag from input    (alternative:? WB[10] = RB[12] & 0x7F)
              WB[11] = (RB[13] & 0x60) | 0x11; // W target fan speed heating;       (alternative: WB[11] = RB[13] & 0x7F might work too)
              WB[12] = RB[14] & 0x7F;          //   clear change flag from input
              WB[16] = RB[18];                 //   target fan mode ?? & 0x03 ?
              WB[17] = 0x00;                   //   ? (change flag, & 0x7F ?)
              if (wr_cnt && (wr_pt == RB[2])) { WB[wr_nr + 3] = wr_val; wr_cnt--; };
              break;
            case 0x39 : // guess that this is filter for FDY, reply with 4-byte payload
              wr = controlLevel;
              n = 7;
              WB[3] = 0x00;
              WB[4] = 0x00;
              WB[5] = RB[11];
              WB[6] = RB[12];
              if (wr_cnt && (wr_pt == RB[2])) { WB[wr_nr + 3] = wr_val; wr_cnt--; };
              break;
#endif
#ifdef FDYQ
            case 0x37 : // FDYQ zone name packet, reply with empty payload
              wr = controlLevel;
              n = 3;
              break;
            case 0x3B : // FDYQ control message, copy bytes back and change if 'F' command is given
              wr = controlLevel;
              n = 22;
              for (w = 13; w <= 18; w++) WB[w] = 0x00;
              WB[3]  = RB[3] & 0x01;           // W target status
              WB[4]  = (RB[5] & 0x07) | 0x60;  // W target operating mode
              WB[5]  = RB[7];                  // W target temperature cooling
              WB[6]  = 0x00;                   //   clear change flag 80 from input (alternative:? WB[6] = RB[8] & 0x7F)
              WB[7]  = (RB[9] & 0x60) | 0x11;  // W target fan speed cooling/fan    (alternative: WB[7] = RB[9] & 0x7F might work too)
              WB[8]  = 0x00;                   //   clear change flag 80 from input
              WB[9]  = RB[11];                 // W target temperature heating
              WB[10] = 0x00;                   //   clear change flag from input    (alternative:? WB[10] = RB[12] & 0x7F)
              WB[11] = (RB[13] & 0x60) | 0x11; // W target fan speed heating        (alternative: WB[11] = RB[13] & 0x7F might work too)
              WB[12] = RB[14] & 0x7F;          //   clear change flag from input
              WB[19] = RB[20];                 // W active hvac zones
              WB[20] = RB[21] & 0x03;          // W target fan mode
              WB[21] = 0x00;                   //   ? (change flag, & 0x7F ?)
              if (wr_cnt && (wr_pt == RB[2])) { WB[wr_nr + 3] = wr_val; wr_cnt--; };
              break;
            case 0x3C : // FDYQ filter message, reply with 2-byte zero payload
              wr = controlLevel;
              n = 5;
              WB[3] = 0x00;
              WB[4] = 0x00;
              if (wr_cnt && (wr_pt == RB[2])) { WB[wr_nr + 3] = wr_val; wr_cnt--; };
              break;
#endif
#ifdef FXMQ
            case 0x32 : // incoming message,  occurs only once, 8 bytes, first byte is 0xC0, others 0x00; reply is one byte value 0x01? polling auxiliary controller?, reply with empty payload
              d = F030DELAY;
              wr = controlLevel;
              n = 4;
              WB[3]  = 0x01; // W target status
              break;
            case 0x35 : // FXMQ outside unit name, reply with empty payload
              wr = controlLevel;
              n = 3;
              break;
            case 0x36 : // FXMQ indoor unit name, reply with empty payload
              wr = controlLevel;
              n = 3;
              break;
            case 0x38 : // FXMQ control message, copy a few bytes back, change bytes if 'F' command is given
              wr = controlLevel;
              n = 20;
              for (w = 3; w <= 20; w++) WB[w] = 0x00;
              WB[3]  = RB[3] & 0x01;           // W target status
              WB[4]  = RB[5];                  // W target operating mode
              WB[5]  = RB[7];                  // W target temperature cooling (can be changed by reply with different value)
              WB[7]  = RB[9];                  // W target fan speed cooling/fan (11 / 31 / 51) (can be changed by reply with different value)
              WB[9]  = RB[11];                 // W target temperature heating (can be changed by reply with different value)
              WB[11] = RB[13];                 // W target fan speed heating/fan (11 / 31 / 51)
              WB[12] = RB[14];                 // no flag?
              WB[16] = RB[18];                 // C0, E0 when payload byte 0 set to 1
              WB[18] = 0;                      // puzzle: initially 0, then 2, then 1 ????
              if (wr_cnt && (wr_pt == RB[2])) {
                if ((wr_nr == 0) && (WB[wr_nr + 3] == 0x00) && (wr_val)) WB[16] |= 0x20; // chnage payload byte 13 from C0 to E0, only if payload byte 0 is set to 1 here
                WB[wr_nr + 3] = wr_val;
                wr_cnt--;
              }
              break;
            case 0x39 : // ??, reply with 5-byte all-zero payload
              wr = controlLevel;
              n = 8;
              WB[3] = 0x00;
              WB[4] = 0x00;
              WB[5] = 0x00;
              WB[6] = 0x00;
              WB[7] = 0x00;
              // for now, don't support write: if (wr_cnt && (wr_pt == RB[2])) { WB[wr_nr + 3] = wr_val; wr_cnt--; };
              break;
            case 0x3A : // ??, reply with 8-byte all-zero payload
              wr = controlLevel;
              n = 11;
              WB[3] = 0x00;
              WB[4] = 0x00;
              WB[5] = 0x00;
              WB[6] = 0x00;
              WB[7] = 0x00;
              WB[8] = 0x00;
              WB[9] = 0x00;
              // for now, don't support write: if (wr_cnt && (wr_pt == RB[2])) { WB[wr_nr + 3] = wr_val; wr_cnt--; };
              break;
#endif
#endif /* F_SERIES */
            default   : // not seen, no response
              break;
          }
          if (wr) {
            if (P1P2Serial.writeready()) {
              P1P2Serial.writepacket(WB, n, d, crc_gen, crc_feed);
              parameterWritesDone += wr_req;
              wr_req = 0;
            } else {
              Serial.println(F("* Refusing to write packet while previous packet wasn't finished, flushing write action"));
              if (writeRefused < 0xFF) writeRefused++;
              wr_req = 0;
            }
          }
        }
      }
    }
#endif /* MONITORCONTROL */
#ifdef ENABLE_INSERT_MESSAGE
    if ((insertMessageCnt == 0) && (RB[0] == 0x00) && (RB[1] == 0x00) && (RB[2] == RESTART_PACKET_TYPE)) {
      for (int i = 0; i < nread; i++) insertMessage[i] = RB[i];
      insertMessageLength = nread;
      restartDaikinReady = 1;
    }
#endif
#ifdef PSEUDO_PACKETS
#ifdef E_SERIES
    if ((RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == 0x10)) pseudo0D = 5; // Insert one pseudo packet 00000D in output serial after 400010
    if ((RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == 0x11)) pseudo0E = 5; // Insert one pseudo packet 00000E in output serial after 400011
    if ((RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == 0x12)) pseudo0F = 5; // Insert one pseudo packet 00000F in output serial after 400012
#endif /* E_SERIES */
#ifdef F_SERIES
    if ((RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == 0x10)) pseudo0D = 5; // Insert one pseudo packet 00000D in output serial after 400010
    if ((RB[0] == 0x00) && (RB[1] == 0x00) && (RB[2] == 0x1F)) pseudo0E = 5; // Insert one pseudo packet 00000E in output serial after 00001F
    if ((RB[0] == 0x80) && (RB[1] == 0x00) && (RB[2] == 0x18)) pseudo0F = 5; // Insert one pseudo packet 00000F in output serial after 800018
#endif /* F_SERIES */
#endif /* PSEUDO_PACKETS */
    if (readError) {
      Serial.print(F("E "));
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
        if (verbose && (EB[i] & ERROR_SB)) {
          // collision suspicion due to data verification error in reading back written data
          Serial.print(F("-SB:"));
        }
        if (verbose && (EB[i] & ERROR_BE)) { // or BE3 (duplicate code)
          // collision suspicion due to data verification error in reading back written data
          Serial.print(F("-XX:"));
        }
        if (verbose && (EB[i] & ERROR_BC)) {
          // collision suspicion due to 0 during 2nd half bit signal read back
          Serial.print(F("-BC:"));
        }
        if (verbose && (EB[i] & ERROR_PE)) {
          // parity error detected
          Serial.print(F("-PE:"));
        }
#ifdef GENERATE_FAKE_ERRORS
        if (verbose && (EB[i] & (ERROR_SB << 8))) {
          // collision suspicion due to data verification error in reading back written data
          Serial.print(F("-sb:"));
        }
        if (verbose && (EB[i] & (ERROR_BE << 8))) {
          // collision suspicion due to data verification error in reading back written data
          Serial.print(F("-xx:"));
        }
        if (verbose && (EB[i] & (ERROR_BC << 8))) {
          // collision suspicion due to 0 during 2nd half bit signal read back
          Serial.print(F("-bc:"));
        }
        if (verbose && (EB[i] & (ERROR_PE << 8))) {
          // parity error detected
          Serial.print(F("-pe:"));
        }
#endif
        byte c = RB[i];
        if (crc_gen && (verbose == 1) && (i == nread - 1)) {
          Serial.print(F(" CRC="));
        }
        if (c < 0x10) Serial.print(F("0"));
        Serial.print(c, HEX);
        if (verbose && (EB[i] & ERROR_OR)) {
          // buffer overrun detected (overrun is after, not before, the read byte)
          Serial.print(F(":OR-"));
        }
        if (verbose && (EB[i] & ERROR_CRC)) {
          // CRC error detected in readpacket
          Serial.print(F(" CRC error"));
        }
      }
      if (readError) {
        Serial.print(F(" readError=0x"));
        if (readError < 0x10) Serial.print('0');
        if (readError < 0x100) Serial.print('0');
        if (readError < 0x1000) Serial.print('0');
        Serial.print(readError, HEX);
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
    if (hwID) {
      uint16_t V0_min;
      uint16_t V0_max;
      uint32_t V0_avg;
      uint16_t V1_min;
      uint16_t V1_max;
      uint32_t V1_avg;
      P1P2Serial.ADC_results(V0_min, V0_max, V0_avg, V1_min, V1_max, V1_avg);
      WB[3]  = (V0_min >> 8) & 0xFF;
      WB[4]  = V0_min & 0xFF;
      WB[5]  = (V0_max >> 8) & 0xFF;
      WB[6]  = V0_max & 0xFF;
      WB[7]  = (V0_avg >> 24) & 0xFF;
      WB[8]  = (V0_avg >> 16) & 0xFF;
      WB[9]  = (V0_avg >> 8) & 0xFF;
      WB[10] = V0_avg & 0xFF;
      WB[11]  = (V1_min >> 8) & 0xFF;
      WB[12] = V1_min & 0xFF;
      WB[13] = (V1_max >> 8) & 0xFF;
      WB[14] = V1_max & 0xFF;
      WB[15] = (V1_avg >> 24) & 0xFF;
      WB[16] = (V1_avg >> 16) & 0xFF;
      WB[17] = (V1_avg >> 8) & 0xFF;
      WB[18] = V1_avg & 0xFF;
      WB[19] = controlLevel;
      WB[20] = CONTROL_ID ? controlLevel : 0; // auxiliary control mode fully on; is 1 for control modes 1 and 3
      WB[21] = 0x00;
      WB[22] = 0x00;
    } else {
      for (int i = 3; i <= 22; i++) WB[i]  = 0x00;
    }
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
    WB[14] = hwID;
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
    WB[19] = scope;
    WB[20] = readErrors;
    WB[21] = readErrorLast;
    WB[22] = CONTROL_ID;
    if (verbose < 4) writePseudoPacket(WB, 23);
  }
#endif /* PSEUDO_PACKETS */
}
