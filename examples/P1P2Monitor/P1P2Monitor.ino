#define OLD_COMMANDS
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
 * Copyright (c) 2019-2023 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 * 20230604 v0.9.38 H-link2 support added to main branch
 * 20230604 v0.9.37 support for ATmega serial enable/disable via PD4, required for P1P2MQTT bridge v1.2
 * 20230108 v0.9.31 fix nr_param check
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

// TODO rewrite pseudopacket triggering and identification mechanism
// TODO simplify verbosity modes

#include "P1P2Config.h"
#include <P1P2Serial.h>

#define SPI_CLK_PIN_VALUE (PINB & 0x20)

static byte verbose = INIT_VERBOSE;
static byte readErrors = 0;
static byte readErrorLast = 0;
static byte errorsLargePacket = 0;
static byte writeRefused = 0;
#ifdef EF_SERIES
static byte controlLevel = 1; // for F-series L5 mode
#ifdef ENABLE_INSERT_MESSAGE
static byte restartDaikinCnt = 0;
static byte restartDaikinReady = 0;
static byte insertMessageCnt = 0;
static byte insertMessage[RB_SIZE];
static byte insertMessageLength = 0;
#endif /* ENABLE_INSERT_MESSAGE */
#endif /* EF_SERIES */

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
#ifdef TH_SERIES // TODO make Compile_Options 16-bit
+0x80
#endif
;

#ifdef EEPROM_SUPPORT
#include <EEPROM.h>
void initEEPROM() {
  if (verbose) Serial.println(F("* EEPROM check"));
  bool sigMatch = 1;
  for (uint8_t i = 0; i < strlen(EEPROM_SIGNATURE); i++) sigMatch &= (EEPROM.read(EEPROM_ADDRESS_SIGNATURE + i) == EEPROM_SIGNATURE[i]);
  if (verbose) {
     Serial.print(F("* EEPROM sig match"));
     Serial.println(sigMatch);
  }
  if (!sigMatch) {
    if (verbose) Serial.println(F("* EEPROM init"));
    for (uint8_t i = 0; i < strlen(EEPROM_SIGNATURE); i++) EEPROM.update(EEPROM_ADDRESS_SIGNATURE + i, EEPROM_SIGNATURE[i]); // no '\0', not needed
    EEPROM.update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID_NONE); // Daikin specific
    EEPROM.update(EEPROM_ADDRESS_COUNTER_STATUS, COUNTERREPEATINGREQUEST); // Daikin specific
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
static byte allow = ALLOW_PAUSE_BETWEEN_BYTES;
static byte scope = INIT_SCOPE;         // scope setting (to log timing info)
#ifdef EF_SERIES
static uint8_t CONTROL_ID = CONTROL_ID_NONE;
#endif /* EF_SERIES */
static byte counterRequest = 0;
static byte counterRepeatingRequest = 0;

byte save_MCUSR;

#ifdef E_SERIES
#ifdef OLD_COMMANDS
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
#endif /* OLD_COMMANDS */
#endif /* E_SERIES */

#ifdef EF_SERIES
uint8_t wr_cnt = 0;
uint8_t wr_req = 0;
uint8_t wr_pt = 0;
uint16_t wr_nr = 0;
uint32_t wr_val = 0;
#endif /* EF_SERIES */

byte Tmin = 0;
byte Tminprev = 61;
int32_t upt_prev_pseudo = 0;
int32_t upt_prev_write = 0;
int32_t upt_prev_error = 0;
#ifdef EF_SERIES
uint8_t writePermission = INIT_WRITE_PERMISSION;
uint8_t errorsPermitted = INIT_ERRORS_PERMITTED;
uint16_t parameterWritesDone = 0;

// FxAbsentCnt[x] counts number of unanswered 00Fx30 messages (only for x=0,1,F (mapped to 0,1,3));
// if -1 than no Fx request or response seen (relevant to detect whether F1 controller is supported or not)
static int8_t FxAbsentCnt[4] = { -1, -1, -1, -1};
#endif /* #F_SERIES */

static char RS[RS_SIZE];
static byte WB[WB_SIZE];
static byte RB[RB_SIZE];
static errorbuf_t EB[RB_SIZE];

// serial-receive using direct access to serial read buffer RS
// rs is # characters in buffer;
// sr_buffer is pointer (RS+rs)

static uint8_t rs=0;
static char *RSp = RS;
static byte ATmegaHwID = 0;
// ATmegaHwID = 0 for V1.0 or Arduino: no ADC
//              1 for V1.1: ADC on pins 6,7
//              2 for V1.2: ADC on pins 0,1, and PB5 used as serial-output-enabler
// ATMegaHwID is self-generated based on ADC7 value, whether PD3/PD4 is connected (previously: initial PB5 value, no longer valid as test)

uint16_t testADC = 0;

void printWelcomeString(void) {
  Serial.println("*");
  Serial.print(F(WELCOMESTRING));
  Serial.print(F(" compiled "));
  Serial.print(F(__DATE__));
  Serial.print(F(" "));
  Serial.print(F(__TIME__));
#ifdef MONITORCONTROL
  Serial.print(F(" +control"));
#endif /* MONITORCONTROL */
#ifdef KLICDA
  Serial.print(F(" +klicda"));
#endif /* KLICDA */
#ifdef OLDP1P2LIB
  Serial.print(F(" OLDP1P2LIB"));
#else
  Serial.print(F(" NEWP1P2LIB"));
#endif
#ifdef E_SERIES
  Serial.print(F(" E-series"));
#endif /* E_SERIES */
#ifdef F_SERIES
  Serial.print(F(" F-series"));
#endif /* F_SERIES */
#ifdef FDY
  Serial.print(F(" FDY"));
#endif /* FDY */
#ifdef FDYQ
  Serial.print(F(" FDYQ"));
#endif /* FDYQ */
#ifdef FXMQ
  Serial.print(F(" FXMQT"));
#endif /* FXMQ */
#ifdef H_SERIES
  Serial.print(F(" H-series"));
#endif /* H_SERIES */
#ifdef T_SERIES
  Serial.print(F(" T-series"));
#endif /* T_SERIES */
  Serial.println();
  Serial.print(F("* Reset cause: MCUSR="));
  Serial.print(save_MCUSR);
  if (save_MCUSR & (1 << BORF))  Serial.print(F(" (brown-out)")); // 4
  if (save_MCUSR & (1 << EXTRF)) Serial.print(F(" (ext-reset)")); // 2
  if (save_MCUSR & (1 << PORF)) Serial.print(F(" (power-on-reset)")); // 1
  Serial.println();
  Serial.print(F("* P1P2-ESP-interface ATmegaHwID v1."));
  Serial.println(ATmegaHwID);
#ifdef EF_SERIES
  Serial.print(F("* Control_ID=0x"));
  Serial.println(CONTROL_ID, HEX);
  Serial.print(F("* Counterrepeatingrequest="));
  Serial.println(counterRepeatingRequest);
/*
  Serial.print(F("* F030DELAY="));
  Serial.println(F030DELAY);
  Serial.print(F("* F03XDELAY="));
  Serial.println(F03XDELAY);
*/
#endif /* EF_SERIES */
  Serial.print(F("* Verbosity="));
  Serial.println(verbose);
// Initial parameters
/*
  Serial.print(F("* CPU_SPEED="));
  Serial.println(F_CPU);
  Serial.print(F("* SERIALSPEED="));
  Serial.println(SERIALSPEED);
*/
#ifdef KLICDA
#ifdef KLICDA_DELAY
  Serial.print(F("* KLICDA_DELAY="));
  Serial.println(KLICDA_DELAY);
#endif /* KLICDA_DELAY */
#endif /* KLICDA */
}

#define ADC_busy (0x40 & ADCSRA)
#define ADC_VALUE (ADCL + (ADCH << 8))

void setup() {
  save_MCUSR = MCUSR;
  MCUSR = 0;
// test if hardware is v1.2?
#define V12_TESTPIN_IN  PD4
#define V12_TESTPIN_OUT PD3
  pinMode(V12_TESTPIN_IN, INPUT_PULLUP);
  pinMode(V12_TESTPIN_OUT, OUTPUT);
  digitalWrite(V12_TESTPIN_OUT, LOW);
  bool readBackLow = digitalRead(V12_TESTPIN_IN);
  digitalWrite(V12_TESTPIN_OUT, HIGH);
  bool readBackHigh = digitalRead(V12_TESTPIN_IN);
  pinMode(V12_TESTPIN_IN, INPUT);
  pinMode(V12_TESTPIN_OUT, INPUT);
  if ((readBackLow == false) && (readBackHigh == true))  ATmegaHwID = 2;
  if ((readBackLow == false) && (readBackHigh == false))  ATmegaHwID = 0xFF; // should never happen

  if (!ATmegaHwID) {
    // check if we have ADC support
    DIDR0 = 0x03;
    ADCSRB = 0x00; // ACME disabled, free running mode, conversions will be triggered by ADSC
    PORTC |= 0x03; // set pull-up  on ADC1 (used for v1.2, unused for v1.0/v1.1)
    ADMUX = 0xC1; // ADC1
    ADCSRA = 0xD6; // enable ADC, 8MHz/64=125kHz, clear ADC interrupt flag and trigger single ADC conversion, do not trigger interrupt
    while (ADC_busy); // wait for single ADC to finish
    testADC =  ADC_VALUE;
    PORTC &= 0xFC; // repeat without pull-up  on ADC 0/1 (v1.2) ?
    ADMUX = 0xC7;
    ADCSRA = 0xD6; // enable ADC, 8MHz/64=125kHz, clear ADC interrupt flag and trigger single ADC conversion, do not trigger interrupt
    while (ADC_busy); // wait for single ADC to finish
    testADC =  ADC_VALUE;
    PORTC |= 0x00; // repeat with pull-up  on ADC 0/1 (v1.2) ?
    DIDR0 = 0x00;
    if ((testADC < 1000) && (testADC > 800)) ATmegaHwID = 1;
  }

// start Serial
  Serial.begin(SERIALSPEED);
  while (!Serial);      // wait for Arduino Serial Monitor to open
  printWelcomeString();
#ifdef EEPROM_SUPPORT
  initEEPROM();
#ifdef EF_SERIES
  CONTROL_ID = EEPROM.read(EEPROM_ADDRESS_CONTROL_ID);
  counterRepeatingRequest = EEPROM.read(EEPROM_ADDRESS_COUNTER_STATUS);
#endif /* EF_SERIES */
  verbose    = EEPROM.read(EEPROM_ADDRESS_VERBOSITY);
#endif /* EEPROM_SUPPORT */
  // 0 v1.0 no ADC
  // 1 v1.1 use ADC6 and ADC7
  // 2 v1.2 use ADC0 and ADC1, reverse R,W LEDs
  P1P2Serial.begin(9600, ATmegaHwID ? true : false, (ATmegaHwID == 1) ? 6 : 0, (ATmegaHwID == 1) ? 7 : 1,  (ATmegaHwID == 2));
  P1P2Serial.setEcho(echo);
#ifdef SW_SCOPE
  P1P2Serial.setScope(scope);
#endif
  P1P2Serial.setDelayTimeout(sdto);
  P1P2Serial.ledError(0);
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
    if (c <= 0x0F) Serial.print('0');
    Serial.print(c, HEX);
    if (crc_gen != 0) for (uint8_t i = 0; i < 8; i++) {
      crc = ((crc ^ c) & 0x01 ? ((crc >> 1) ^ crc_gen) : (crc >> 1));
      c >>= 1;
    }
  }
  if (crc_gen) {
    if (crc <= 0x0F) Serial.print('0');
    Serial.print(crc, HEX);
  }
  Serial.println();
}

static byte suppressSerial = 0;
#define Serial_read(...) (suppressSerial ? -1 : Serial.read(__VA_ARGS__))
#define Serial_print(...) if (!suppressSerial) Serial.print(__VA_ARGS__)
#define Serial_println(...) if (!suppressSerial) Serial.println(__VA_ARGS__)

#ifdef EF_SERIES
void stopControlAndCounters() {
          if (counterRepeatingRequest) Serial_println(F("* Switching counter request function off (also after ATmega reboot)"));
          if (CONTROL_ID) Serial_println(F("* Switching control functionality off (also after ATmega reboot)"));
          CONTROL_ID = CONTROL_ID_NONE;
          counterRepeatingRequest = 0;
          counterRequest = 0;
          EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
          EEPROM_update(EEPROM_ADDRESS_COUNTER_STATUS, counterRepeatingRequest);
#ifdef E_SERIES
#ifdef OLD_COMMANDS
          setRequest35 = 0;
          setRequest36 = 0;
          setRequest3A = 0;
          setRequestDHW = 0;
#endif /* OLD_COMMANDS */
#endif /* E_SERIES */
          wr_cnt = 0;
}

void Serial_println_ExecutingCommand(char c) {
  Serial_print(F("* Executing "));
  Serial_print(c);
  Serial_println(F(" command"));
}
#endif /* EF_SERIES */

#ifdef E_SERIES
#define PARAM_TP_START      0x35
#define PARAM_TP_END        0x3D
#define PARAM_ARR_SZ (PARAM_TP_END - PARAM_TP_START + 1)
const uint32_t nr_params[PARAM_ARR_SZ] = { 0x014A, 0x002D, 0x0001, 0x001F, 0x00F0, 0x006C, 0x00AF, 0x0002, 0x0020 }; // number of parameters observed
//byte packettype                      = {   0x35,   0x36,   0x37,   0x38,   0x39,   0x3A,   0x3B,   0x3C,   0x3D };
#endif /* E_SERIES */

void(* resetFunc) (void) = 0; // declare reset function at address 0

int32_t upt = 0;
static byte pseudo0D = 0;
static byte pseudo0E = 0;
static byte pseudo0F = 0;

uint8_t scope_budget = 200;


byte skipPackets = 10; // skip at most 10 initial packets with errors
int8_t F_prev = -1;
byte F_prevDet = 0;
byte F_max = 0;
byte div4 = 0;

#define Serial_read(...) (suppressSerial ? -1 : Serial.read(__VA_ARGS__))
#define Serial_print(...) if (!suppressSerial) Serial.print(__VA_ARGS__)
#define Serial_println(...) if (!suppressSerial) Serial.println(__VA_ARGS__)

#ifdef E_SERIES
void writeParam(void) {
/*
  if (wr_cnt) {
    // previous write still being processed
    Serial_println(F("* Previous parameter write action still busy"));
    return;
  }
*/
  if ((wr_pt < PARAM_TP_START) || (wr_pt > PARAM_TP_END)) {
    Serial_print(F("* wr_pt: 0x"));
    Serial_print(wr_pt, HEX);
    Serial_println(F(" not 0x35-0x3D"));
    return;
  }
  if (wr_nr > nr_params[wr_pt - PARAM_TP_START]) {
    Serial_print(F("* wr_nr > expected: 0x"));
    Serial_println(wr_nr, HEX);
    return;
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
    Serial_print(F("* Parameter value too large for packet type; #bytes is "));
    Serial_print(wr_nrb);
    Serial_print(F(" value is "));
    Serial_println(wr_val, HEX);
    return;
  }
#ifdef OLD_COMMANDS
  if ((wr_pt == 0x35) && (wr_nr == PARAM_DHW_ONOFF) && (setRequestDHW)) {
    Serial_println(F("* Ignored DHW35_req"));
    return;
  }
  if ((wr_pt == 0x35) && (wr_nr == setParam35) && (setRequest35)) {
    Serial_println(F("* Ignored 35_req"));
    return;
  }
  if ((wr_pt == 0x36) && (wr_nr == setParam36) && (setRequest36)) {
    Serial_println(F("* Ignored 36_req"));
    return;
  }
  if ((wr_pt == 0x3A) && (wr_nr == setParam3A) && (setRequest3A)) {
    Serial_println(F("* Ignored 3A_req"));
    return;
  }
#endif /* OLD_COMMANDS */
  if (writePermission) {
    if (writePermission != 0xFF) writePermission--;
    wr_cnt = WR_CNT; // write repetitions, 1 should be enough
    Serial_print(F("* Initiating parameter write for packet-type 0x"));
    Serial_print(wr_pt, HEX);
    Serial_print(F(" parameter nr 0x"));
    Serial_print(wr_nr, HEX);
    Serial_print(F(" to value 0x"));
    Serial_print(wr_val, HEX);
    Serial_println();
  } else {
    Serial_println(F("* Currently no write budget left"));
  }
}
#endif /* E_SERIES */

void loop() {
  uint16_t temp;
  uint16_t temphex;
  int wb = 0;
  int n;
  int wbtemp;
  int c;
  static byte ignoreremainder = 2; // ignore first line from serial input to avoid misreading a partial message just after reboot
  static bool reportedTooLong = 0;

// if GPIO0 = PB4 = L, do nothing, and disable serial input/output, to enable ESP programming
// MISO GPIO0  pin 18 // PB4 // pull-up   // P=Power // white // DS18B20                                            V10/V11
// MOSI GPIO2  pin 17 // PB3 // pull-up   // R=Read  // green // low-during-programming // blue-LED on ESP12F       V12/V13
// CLK  GPIO15 pin 16 // PB5 // pull-down // W=Write // blue                                                        V14/V15
// RST         pin 1  // PC4 // pull-up   // E=Error // red   // reset-switch

  if (ATmegaHwID == 2) switch (suppressSerial) {
    case 0 : if ((PINB & 0x10) == 0) { // if GPIO0 == 0, disable serial, switch off power LED, and wait for GPIO15=0
               suppressSerial = 1;
               P1P2Serial.ledPower(1);
               P1P2Serial.ledError(0);
               Serial.println("* Pausing serial");
               Serial.end();
               break;
             }
             if ((PINB & 0x20) == 0) { // if GPIO15=0, go inactive, then wait for GPIO15=1
               suppressSerial = 2;
               P1P2Serial.ledPower(0);
               P1P2Serial.ledError(1);
               Serial.println("* Pausing serial");
               Serial.end();
               break;
             }
             break;
    case 1 : // GPIO=0; wait for GPIO15=0
             // in this mode, power LED is on, error LED is off
             if ((PINB & 0x20) == 0) { // wait for GPIO15=0
               suppressSerial = 2;
               P1P2Serial.ledError(1);
             }
             break;
    case 2 : // GPIO=0; GPIO15=0, wait for GPIO15=1
             // in this mode, error LED is on, power LED is off
             if ((PINB & 0x20) == 0x20) { // wait for GPIO15=1
               suppressSerial = 0;
               RSp = RS;
               rs = 0;
               reportedTooLong = 0;
               ignoreremainder = 2;
               Serial.begin(SERIALSPEED);
               while (!Serial);      // wait for Arduino Serial Monitor to open
               P1P2Serial.ledError(0);
               P1P2Serial.ledPower(1);
               printWelcomeString();
             }
             break;
  }

// Non-blocking serial input
// rs is number of char received and stored in readbuf
// RSp = readbuf + rs
// ignore first line and too-long lines

  while (((c = Serial_read()) >= 0) && (c != '\n') && (rs < RS_SIZE)) {
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
          Serial_println(F("* Line too long, ignored"));
          // Serial_print(F("* Line too long, ignored, ignoring remainder: ->"));
          // Serial_print(RS);
          // Serial_print(lst);
          // Serial_print(c);
          // Serial_println(F("<-"));
          // show full line, so not yet setting reportedTooLong = 1 here;
        }
        ignoreremainder = 1;
      } else {
        if (!reportedTooLong) {
          Serial_println(F("* Line too long, terminated"));
          // Serial_print(F("* Line too long, terminated, ignored: ->"));
          // Serial_print(RS);
          // Serial_print(lst);
          // Serial_println(F("<-"));
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
/*
        Serial_print(F("* First line ignored: ->"));
        if (rs < 10) {
          Serial_print(RS);
        } else {
          RS[9] = '\0';
          Serial_print(RS);
          Serial_print("...");
        }
        Serial_println("<-");
*/
        ignoreremainder = 0;
      } else if (ignoreremainder == 1) {
        if (!reportedTooLong) {
          // Serial_print(F("* Line too long, last part: "));
          // Serial_println(RS);
          reportedTooLong = 1;
        }
        ignoreremainder = 0;
      } else {
#define maxVerbose ((verbose == 1) || (verbose == 4))
        if (maxVerbose) {
          Serial_print(F("* Received: \""));
          Serial_print(RS);
          Serial_println("\"");
        }
#ifdef SERIAL_MAGICSTRING
        RSp = RS + strlen(SERIAL_MAGICSTRING) + 1;
        if (!strncmp(RS, SERIAL_MAGICSTRING, strlen(SERIAL_MAGICSTRING))) {
#else
        RSp = RS + 1;
        {
#endif
          switch (*(RSp - 1)) {
            case '\0':if (maxVerbose) Serial_println(F("* Empty line received")); break;
            case '*': if (maxVerbose) {
                        Serial_print(F("* Received: "));
                        Serial_println(RSp);
                      }
                      break;
#ifdef E_SERIES
            case 'e':
            case 'E': if (!CONTROL_ID) {
                        Serial_println(F("* Requires L1"));
                        break;
                      }
                      if (wr_cnt) {
                        // previous write still being processed
                        Serial_println(F("* Previous parameter write action still busy"));
                        break;
                      }
                      if ( (n = sscanf(RSp, (const char*) "%2x%4x%8lx", &wr_pt, &wr_nr, &wr_val)) == 3) {
                        writeParam();
                      } else {
                        Serial_print(F("* Ignoring instruction, expected 3 arguments, received: "));
                        Serial_print(n);
                        if (n > 0) {
                          Serial_print(F(" pt: 0x"));
                          Serial_print(wr_pt, HEX);
                        }
                        if (n > 1) {
                          Serial_print(F(" nr: 0x"));
                          Serial_print(wr_nr, HEX);
                        }
                        Serial_println();
                      }
                      break;
#endif /* E_SERIES */
#ifdef F_SERIES
            case 'f':
            case 'F': if (!CONTROL_ID) {
                        Serial_println(F("* Command requires operation as auxiliary controller (L1)"));
                        break;
                      }
                      if (wr_cnt) {
                        // previous write still being processed
                        Serial_println(F("* Previous parameter write action still busy"));
                        break;
                      } else if ( (n = sscanf(RSp, (const char*) "%2x%2d%2x", &wr_pt, &wr_nr, &wr_val)) == 3) {
                        // Valid write parameters
                        // FDY  38 0 1 2 4 6 8
                        // FXMQ 38 0 1 2 4 6 8
                        // FDYQ 3B 0 1 2 4 6 8 16 17
                        //
                        // check wr_pt
#if defined FDY || defined FXMQ
                        if (wr_pt != 0x38) {
                          Serial_print(F("* wr_pt: 0x"));
                          Serial_print(wr_pt, HEX);
                          Serial_println(F(" is not 0x38"));
                          break;
                        }
#endif
#ifdef FDYQ
                        if (wr_pt != 0x3B) {
                          Serial_print(F("* wr_pt: 0x"));
                          Serial_print(wr_pt, HEX);
                          Serial_println(F(" is not 0x3B"));
                          break;
                        }
#endif
                        // check wr_nr
                        if ((wr_nr > 17) || (wr_nr == 3) || (wr_nr == 5) || (wr_nr == 7) || ((wr_nr >= 9) && (wr_nr <= 15)) || ((wr_nr == 16) && (wr_pt == 0x38)) || ((wr_nr == 17) && (wr_pt == 0x38))) {
                          Serial_print(F("* wr_nr invalid, should be 0, 1, 2, 4, 6, 8 (or for packet type 0x3B: 16 or 17): "));
                          Serial_println(wr_nr);
                          break;
                        }
                        if (wr_val > 0xFF) {
                          Serial_print(F("* wr_val > 0xFF: "));
                          Serial_println(wr_val);
                          break;
                        }
                        if ((wr_nr == 0) && (wr_val > 1)) {
                          Serial_println(F("wr_val for payload byte 0 (status) must be 0 or 1"));
                          break;
                        }
                        if ((wr_nr == 1) && ((wr_val < 0x60) || (wr_val > 0x67))) {
                          Serial_println(F("wr_val for payload byte 1 (operating-mode) must be in range 0x60-0x67"));
                          break;
                        }
                        if (((wr_nr == 2) || (wr_nr == 6)) && ((wr_val < 0x0A) || (wr_val > 0x1E))) {
                          Serial_println(F("wr_val for payload byte 2/6 (target-temp cooling/heating) must be in range 0x10-0x20"));
                          break;
                        }
                        if (((wr_nr == 4) || (wr_nr == 8)) && (wr_val < 0x03)) {
                          wr_val = 0x11 + (wr_val << 5); // 0x00 -> 0x11; 0x01 -> 0x31; 0x02 -> 0x51
                          break;
                        } else if (((wr_nr == 4) || (wr_nr == 8)) && ((wr_val < 0x11) || (wr_val > 0x51))) {
                          Serial_println(F("wr_val for payload byte 4/8 (fan-speed cooling/heating) must be in range 0x11-0x51 or 0x00-0x02"));
                          break;
                        }
                        // no limitations for wr_nr == 16
                        if ((wr_nr == 17) && (wr_val > 0x03)) {
                          Serial_println(F("wr_val for payload byte 17 (fan-mode) must be in range 0x00-0x03"));
                          break;
                        }
                        if (writePermission) {
                          if (writePermission != 0xFF) writePermission--;
                          wr_cnt = WR_CNT; // write repetitions, 1 should be enough (especially for F-series)
                          Serial_print(F("* Initiating write for packet-type 0x"));
                          Serial_print(wr_pt, HEX);
                          Serial_print(F(" payload byte "));
                          Serial_print(wr_nr);
                          Serial_print(F(" to value 0x"));
                          Serial_print(wr_val, HEX);
                          Serial_println();
                        } else {
                          Serial_println(F("* Currently no write budget left"));
                          break;
                        }
                      } else {
                        Serial_print(F("* Ignoring instruction, expected 3 arguments, received: "));
                        Serial_print(n);
                        if (n > 0) {
                          Serial_print(F(" pt: 0x"));
                          Serial_print(wr_pt, HEX);
                        }
                        if (n > 1) {
                          Serial_print(F(" nr: "));
                          Serial_print(wr_nr);
                        }
                        Serial_println();
                      }
                      break;
#endif /* F_SERIES */
            case 'g':
            case 'G': if (verbose) Serial_print(F("* Crc_gen "));
                      if (scanhex(RSp, temphex) == 1) {
                        crc_gen = temphex;
                        if (!verbose) break;
                        Serial_print(F("set to "));
                      }
                      Serial_print(F("0x"));
                      if (crc_gen <= 0x0F) Serial_print('0');
                      Serial_println(crc_gen, HEX);
                      break;
            case 'h':
            case 'H': if (verbose) Serial_print(F("* Crc_feed "));
                      if (scanhex(RSp, temphex) == 1) {
                        crc_feed = temphex;
                        if (!verbose) break;
                        Serial_print(F("set to "));
                      }
                      Serial_print(F("0x"));
                      if (crc_feed <= 0x0F) Serial_print('0');
                      Serial_println(crc_feed, HEX);
                      break;
            case 'v':
            case 'V': Serial_print(F("* Verbose "));
                      if (scanint(RSp, temp) == 1) {
                        if (temp > 4) temp = 4;
                        Serial_print(F("set to "));
                        verbose = temp;
                        EEPROM_update(EEPROM_ADDRESS_VERBOSITY, verbose);
                      }
                      Serial_println(verbose);
                      printWelcomeString();
                      break;
            case 't':
            case 'T': if (verbose) Serial_print(F("* Delay "));
                      if (scanint(RSp, sd) == 1) {
                        if (sd < 2) {
                          sd = 2;
                          Serial_print(F("[use of delay 0 or 1 not recommended, increasing to 2] "));
                        }
                        if (!verbose) break;
                        Serial_print(F("set to "));
                      }
                      Serial_println(sd);
                      break;
            case 'o':
            case 'O': if (verbose) Serial_print(F("* DelayTimeout "));
                      if (scanint(RSp, sdto) == 1) {
                        P1P2Serial.setDelayTimeout(sdto);
                        if (!verbose) break;
                        Serial_print(F("set to "));
                      }
                      Serial_println(sdto);
                      break;
#ifdef SW_SCOPE
            case 'u':
            case 'U': if (verbose) Serial_print(F("* Software-scope "));
                      if (scanint(RSp, temp) == 1) {
                        scope = temp;
                        if (scope > 1) scope = 1;
                        P1P2Serial.setScope(scope);
                        if (!verbose) break;
                        Serial_print(F("set to "));
                      }
                      Serial_println(scope);
                      scope_budget = 200; // TODO document implicit reset of budget
                      break;
#endif
#ifndef TH_SERIES
            case 'x':
            case 'X': if (verbose) Serial_print(F("* Echo "));
                      if (scanint(RSp, temp) == 1) {
                        if (temp) temp = 1;
                        echo = temp;
                        P1P2Serial.setEcho(echo);
                        if (!verbose) break;
                        Serial_print(F("set to "));
                      }
                      Serial_println(echo);
                      break;
#else /* TH_SERIES */
            case 'x':
            case 'X': if (verbose) Serial_print(F("* Allow (bits pause) "));
                      if (scanint(RSp, temp) == 1) {
                        allow = temp;
                        if (allow > 76) allow = 76;
                        P1P2Serial.setAllow(allow);
                        if (!verbose) break;
                        Serial_print(F("set to "));
                      }
                      Serial_println(allow);
                      break;
#endif /* TH_SERIES */
            case 'w':
            case 'W':
#ifdef EF_SERIES
                      if (CONTROL_ID) {
                        // in L1/L5 mode, insert message in time allocated for 40F030 slot
                        if (insertMessageCnt) {
                          Serial_println(F("* insertMessage already scheduled"));
                          break;
                        }
                        if (restartDaikinCnt) {
                          Serial_println(F("* restartDaikin already scheduled"));
                          break;
                        }
                        if (verbose) Serial_print(F("* Writing next 40Fx30 slot: "));
                        insertMessageLength = 0;
                        restartDaikinReady = 0; // to indicate insertMessage is overwritten and restart cannot be done until new packet received/loaded
                        while ((wb < RB_SIZE) && (wb < WB_SIZE) && (sscanf(RSp, (const char*) "%2x%n", &wbtemp, &n) == 1)) {
                          insertMessage[insertMessageLength++] = wbtemp;
                          insertMessageCnt = 1;
                          RSp += n;
                          if (verbose) {
                            if (wbtemp <= 0x0F) Serial_print("0");
                            Serial_print(wbtemp, HEX);
                          }
                        }
                        if (insertMessageCnt) {
                          Serial_println();
                        } else {
                          Serial_println(F("- valid data missing, write skipped"));
                        }
                        break;
                      }
#endif /* EF_SERIES */
                      // for H-link2/Toshiba, just write packet
                      // in L0 mode, just write packet (TODO: crude, only for test purposes, or improve write timing to 00Fx30 reply time slot (if available and free)?)
                      if (verbose) Serial_print(F("* Writing: "));
                      wb = 0;
                      while ((wb < WB_SIZE) && (sscanf(RSp, (const char*) "%2x%n", &wbtemp, &n) == 1)) {
                        WB[wb++] = wbtemp;
                        RSp += n;
                        if (verbose) {
                          if (wbtemp <= 0x0F) Serial_print("0");
                          Serial_print(wbtemp, HEX);
                        }
                      }
                      if (verbose) Serial_println();
                      if (P1P2Serial.writeready()) {
                        if (wb) {
                          P1P2Serial.writepacket(WB, wb, sd, crc_gen, crc_feed);
                        } else {
                          Serial_println(F("* Refusing to write empty packet"));
                        }
                      } else {
                        Serial_println(F("* Refusing to write packet while previous packet wasn't finished"));
                        if (writeRefused < 0xFF) writeRefused++;
                      }
                      break;
            case 'k': // soft-reset ESP
            case 'K': Serial_println(F("* Resetting ATmega ...."));
                      resetFunc(); // call reset
                      break;
#ifdef EF_SERIES
            case 'l': // set auxiliary controller function on/off; CONTROL_ID address is set automatically; Daikin-specific
            case 'L': // 0 controller mode off, and store setting in EEPROM // 1 controller mode on, and store setting in EEPROM
                      // 2 controller mode off, do not store setting in EEPROM
                      // 3 controller mode on, do not store setting in EEPROM
                      // 5 (for experimenting with F-series only) controller responds only to 00F030 message with an empty packet, but not to other 00F03x packets
                      // 99 restart Daikin
                      // other values are treated as 0 and are reserved for further experimentation of control modes
                      if (scanint(RSp, temp) == 1) {
#ifdef ENABLE_INSERT_MESSAGE
                        if (temp == 99) {
                          if (insertMessageCnt) {
                            Serial_println(F("* insertMessage already scheduled"));
                            break;
                          }
                          if (restartDaikinCnt) {
                            Serial_println(F("* restartDaikin already scheduled"));
                            break;
                          }
                          if (!restartDaikinReady) {
                            Serial_println(F("* restartDaikin waiting to receive sample payload"));
                            break;
                          }
                          restartDaikinCnt = RESTART_NR_MESSAGES;
                          Serial_println(F("* Scheduling attempt to restart Daikin"));
                          break;
                        }
#endif /* ENABLE_INSERT_MESSAGE */
#ifdef E_SERIES
                        if (temp > 3) temp = 0;
#endif /* E_SERIES */
#ifdef F_SERIES
                        if ((temp > 5) || (temp == 4)) temp = 0;
#endif /* F_SERIES */
                        byte setMode = temp & 0x01;
                        if (setMode) {
                          if (errorsPermitted < MIN_ERRORS_PERMITTED) {
                            Serial_println(F("* Errorspermitted (error budget) too low; control not enabled"));
                            break;
                          }
                          if (CONTROL_ID) {
                            Serial_print(F("* CONTROL_ID is already 0x"));
                            Serial_println(CONTROL_ID, HEX);
                            if (temp < 2) EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
                            break;
                          }
                          if (FxAbsentCnt[0] == F0THRESHOLD) {
                            Serial_println(F("* Control_ID 0xF0 supported and free, starting auxiliary controller"));
                            CONTROL_ID = 0xF0;
                            controlLevel = (temp == 5) ? 0 : 1; // F-series: special mode L5 answers only to F030
                            if (temp < 2) EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID); // L2/L3/L5 for short experiments, doesn't survive ATmega reboot !
#ifdef E_SERIES
                          } else if (FxAbsentCnt[1] == F0THRESHOLD) {
                            Serial_println(F("* Control_ID 0xF1 supported, no auxiliary controller found for 0xF1, switching control functionality on 0xF1 on"));
                            CONTROL_ID = 0xF1;
                            if (temp < 2) EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
                          } else if (FxAbsentCnt[3] == F0THRESHOLD) {
                            Serial_println(F("* Control_ID 0xFF supported, no auxiliary controller found for 0xFF, but control on 0xFF not supported (yet?)"));
#endif /* E_SERIES */
                          } else {
                            Serial_println(F("* No free address for controller found (yet). Control functionality not enabled"));
                            Serial_println(F("* You may wish to re-try in a few seconds (and perhaps switch other auxiliary controllers off)"));
                          }
                        } else {
                          if (!CONTROL_ID) {
                            Serial_println(F("* CONTROL_ID is already 0x00"));
                            break;
                          } else {
                            CONTROL_ID = 0x00;
#ifdef E_SERIES
#ifdef OLD_COMMANDS
                            setRequest35 = 0;
                            setRequest36 = 0;
                            setRequest3A = 0;
                            setRequestDHW = 0;
#endif /* OLD_COMMANDS */
#endif /* E_SERIES */
                            wr_cnt = 0;
                          }
                          if (temp < 2) EEPROM_update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
                        }
                        if (!verbose) break;
                        Serial_print(F("* Control_id set to 0x"));
                        if (CONTROL_ID <= 0x0F) Serial_print("0");
                        Serial_println(CONTROL_ID, HEX);
                        break;
                      }
                      if (verbose) Serial_print(F("* Control_id is 0x"));
                      if (CONTROL_ID <= 0x0F) Serial_print("0");
                      Serial_println(CONTROL_ID, HEX);
                      break;
#endif /* EF_SERIES */
#ifdef E_SERIES
            case 'c': // set counterRequest cycle (once or repetitive)
            case 'C': if (scanint(RSp, temp) == 1) {
                        if (temp) {
                          if ((FxAbsentCnt[0] != F0THRESHOLD) && (FxAbsentCnt[1] != F0THRESHOLD) && (FxAbsentCnt[3] != F0THRESHOLD)) {
                            Serial_println(F("* No free controller slot found (yet). Counter functionality cannot enabled"));
                            break;
                          }
                          if (errorsPermitted < MIN_ERRORS_PERMITTED) {
                            Serial_println(F("* Errorspermitted too low; counter functinality not enabled"));
                            break;
                          }
                        }
                        if (temp > 1) {
                          if (counterRepeatingRequest) {
                            Serial_println(F("* Repetitive requesting of counter values was already active"));
                            break;
                          }
                          counterRepeatingRequest = 1;
                          Serial_println(F("* Repetitive requesting of counter values initiated"));
                          EEPROM_update(EEPROM_ADDRESS_COUNTER_STATUS, counterRepeatingRequest);
                        } else if (temp == 1) {
                          if (counterRequest & !counterRepeatingRequest) {
                            Serial_println(F("* Previous single counter request not finished yet"));
                            break;
                          }
                          if (counterRepeatingRequest) {
                            counterRepeatingRequest = 0;
                            Serial_print(F("* Switching repetitive requesting of counters off, "));
                            if (counterRequest) {
                              Serial_println(F("was still active, finish current cycle"));
                            } else {
                              Serial_println(F("and initiate single counter cycle"));
                              counterRequest = 1;
                            }
                          } else {
                            if (counterRequest) {
                              Serial_println(F("* Single Repetitive requesting of counter values was already active"));
                            } else {
                              counterRequest = 1;
                              Serial_println(F("* Single counter request cycle initiated"));
                            }
                          }
                        } else { // temp == 0
                          counterRepeatingRequest = 0;
                          counterRequest = 0;
                          Serial_println(F("* All counter-requests stopped"));
                          EEPROM_update(EEPROM_ADDRESS_COUNTER_STATUS, counterRepeatingRequest);
                        }
                      } else {
                        Serial_print(F("* Counterrepeatingrequest is "));
                        Serial_println(counterRepeatingRequest);
                        Serial_print(F("* Counterrequest is "));
                        Serial_println(counterRequest);
                      }
                      break;
#ifdef OLD_COMMANDS
            case 'p': // select F035-parameter to write in z step below (default PARAM_HC_ONOFF in P1P2Config.h)
            case 'P': if (verbose) Serial_print(F("* Param35-2Write "));
                      if (scanhex(RSp, temphex) == 1) {
                        if (setRequest35) {
                          Serial_println(F("* Cannot change param35-2write while previous request still pending"));
                          break;
                        }
                        setParam35 = temphex;
                        if (!verbose) break;
                        Serial_print(F("set to "));
                      }
                      Serial_print(F("0x"));
                      if (setParam35 <= 0x000F) Serial_print('0');
                      if (setParam35 <= 0x00FF) Serial_print('0');
                      if (setParam35 <= 0x0FFF) Serial_print('0');
                      Serial_println(setParam35, HEX);
                      break;
            case 'q': // select F036-parameter to write in r step below (default PARAM_TEMP in P1P2Config.h)
            case 'Q': if (verbose) Serial_print(F("* Param36-2Write "));
                      if (scanhex(RSp, temphex) == 1) {
                        if (setRequest36) {
                          Serial_println(F("* Cannot change param36-2write while previous request still pending"));
                          break;
                        }
                        setParam36 = temphex;
                        if (!verbose) break;
                        Serial_print(F("set to "));
                      }
                      Serial_print(F("0x"));
                      if (setParam36 <= 0x000F) Serial_print('0');
                      if (setParam36 <= 0x00FF) Serial_print('0');
                      if (setParam36 <= 0x0FFF) Serial_print('0');
                      Serial_println(setParam36, HEX);
                      break;
            case 'm': // select F03A-parameter to write in n step below (default PARAM_SYS in P1P2Config.h)
            case 'M': if (verbose) Serial_print(F("* Param3A-2Write "));
                      if (scanhex(RSp, temphex) == 1) {
                        if (setRequest3A) {
                          Serial_println(F("* Cannot change param3A-2write while previous request still pending"));
                          break;
                        }
                        setParam3A = temphex;
                        if (!verbose) break;
                        Serial_print(F("set to "));
                      }
                      Serial_print(F("0x"));
                      if (setParam3A <= 0x000F) Serial_print('0');
                      if (setParam3A <= 0x00FF) Serial_print('0');
                      if (setParam3A <= 0x0FFF) Serial_print('0');
                      Serial_println(setParam3A, HEX);
                      break;

            case 'z': // Z  report status of packet type 35 write action
            case 'Z': // Zx set value for parameter write (PARAM_HC_ONOFF/'p') in packet type 35 and initiate write action
                      if (!CONTROL_ID) {
                        Serial_println(F("* Command requires operation as auxiliary controller (L1)"));
                        break;
                      }
                      if (verbose) Serial_print(F("* Param35 "));
                      Serial_print(F("0x"));
                      if (setParam35 <= 0x000F) Serial_print('0');
                      if (setParam35 <= 0x00FF) Serial_print('0');
                      if (setParam35 <= 0x0FFF) Serial_print('0');
                      Serial_print(setParam35, HEX);
                      if (scanhex(RSp, temphex) == 1) {
                        if ((wr_pt == 0x35) && (wr_nr == setParam35) && wr_cnt) {
                          Serial_println(F(": write command ignored - E-write-request is pending"));
                          break;
                        }
                        if ((setParam35 == PARAM_DHW_ONOFF) && setRequestDHW) {
                          Serial_println(F(": write command ignored - DHW-write-request is pending"));
                          break;
                        }
                        if (writePermission) {
                          if (writePermission != 0xFF) writePermission--;
                          setRequest35 = 1;
                          setValue35 = temphex;
                          if (!verbose) break;
                          Serial_print(F(" will be set to 0x"));
                          if (setValue35 <= 0x0F) Serial_print('0');
                          Serial_println(setValue35, HEX);
                          break;
                        } else {
                          Serial_println(F("* Currently no write budget left"));
                          break;
                        }
                      } else if (setRequest35) {
                        Serial_print(F(": 35-write-request to value 0x"));
                        if (setValue35 <= 0x0F) Serial_print('0');
                        Serial_print(setValue35, HEX);
                        Serial_println(F(" pending"));
                      } else {
                        Serial_println(F(": no 35-write-request pending"));
                      }
                      break;
            case 'r': // R  report status of packet type 36 write action
            case 'R': // Rx set value for parameter write (PARAM_TEMP/'q') in packet type 36 and initiate write action
                      if (!CONTROL_ID) {
                        Serial_println(F("* Command requires operation as auxiliary controller (L1)"));
                        break;
                      }
                      if (verbose) Serial_print(F("* Param36 "));
                      Serial_print(F("0x"));
                      if (setParam36 <= 0x000F) Serial_print('0');
                      if (setParam36 <= 0x00FF) Serial_print('0');
                      if (setParam36 <= 0x0FFF) Serial_print('0');
                      Serial_println(setParam36, HEX);
                      if (scanhex(RSp, temphex) == 1) {
                        if ((wr_pt == 0x36) && (wr_nr == setParam36) && wr_cnt) {
                          Serial_println(F(" write command ignored - E-write-request is pending"));
                          break;
                        }
                        if (writePermission) {
                          if (writePermission != 0xFF) writePermission--;
                          setRequest36 = 1;
                          setValue36 = temphex;
                          if (!verbose) break;
                          Serial_print(F(" will be set to 0x"));
                          if (setValue36 <= 0x000F) Serial_print('0');
                          if (setValue36 <= 0x00FF) Serial_print('0');
                          if (setValue36 <= 0x0FFF) Serial_print('0');
                          Serial_println(setValue36, HEX);
                          break;
                        } else {
                          Serial_println(F("* Currently no write budget left"));
                          break;
                        }
                      } else if (setRequest36) {
                        Serial_print(F(": 36-write-request to value 0x"));
                        if (setValue36 <= 0x000F) Serial_print('0');
                        if (setValue36 <= 0x00FF) Serial_print('0');
                        if (setValue36 <= 0x0FFF) Serial_print('0');
                        Serial_print(setValue36, HEX);
                        Serial_println(F(" still pending"));
                      } else {
                        Serial_println(F(": no 36-write-request pending"));
                      }
                      break;
            case 'n': // N  report status of packet type 3A write action
            case 'N': // Nx set value for parameter write (PARAMSYS/'m') in packet type 3A and initiate write action
                      if (!CONTROL_ID) {
                        Serial_println(F("* Command requires operation as auxiliary controller (L1)"));
                        break;
                      }
                      if (verbose) Serial_print(F("* Param3A "));
                      Serial_print(F("0x"));
                      if (setParam3A <= 0x000F) Serial_print('0');
                      if (setParam3A <= 0x00FF) Serial_print('0');
                      if (setParam3A <= 0x0FFF) Serial_print('0');
                      Serial_println(setParam3A, HEX);
                      if (scanhex(RSp, temphex) == 1) {
                        if ((wr_pt == 0x3A) && (wr_nr == setParam3A) && wr_cnt) {
                          Serial_println(F(": write command ignored - E-write-request is pending"));
                          break;
                        }
                        if (writePermission) {
                          if (writePermission != 0xFF) writePermission--;
                          setRequest3A = 1;
                          setValue3A = temphex;
                          if (!verbose) break;
                          Serial_print(F(": will be set to 0x"));
                          if (setValue3A <= 0x0F) Serial_print('0');
                          Serial_println(setValue3A, HEX);
                          break;
                        } else {
                          Serial_println(F(": currently no write budget left"));
                          break;
                        }
                      } else if (setRequest3A) {
                        Serial_print(F(": 3A-write-request to value 0x"));
                        if (setValue3A <= 0x0F) Serial_print('0');
                        Serial_print(setValue3A, HEX);
                        Serial_println(F(" still pending"));
                      } else {
                        Serial_println(F(": no 3A-write-request pending"));
                      }
                      break;
            case 'y': // Y  report status of DHW write action (packet type 0x35)
            case 'Y': // Yx set value for DHW parameter write (defined by PARAM_DHW_ONOFF in P1P2Config.h, not reconfigurable) in packet type 35 and initiate write action
                      if (!CONTROL_ID) Serial_println(F("* Command requires operation as auxiliary controller (L1)"));
                      if (verbose) Serial_print(F("* DHWparam35 "));
                      Serial_print(F("0x"));
                      if (PARAM_DHW_ONOFF <= 0x000F) Serial_print('0');
                      if (PARAM_DHW_ONOFF <= 0x00FF) Serial_print('0');
                      if (PARAM_DHW_ONOFF <= 0x0FFF) Serial_print('0');
                      Serial_println(PARAM_DHW_ONOFF, HEX);
                      if (scanint(RSp, temp) == 1) {
                        if ((wr_pt == 0x35) && (wr_nr == PARAM_DHW_ONOFF) && wr_cnt) {
                          Serial_println(F("* Ignored - Writing to this parameter already pending by E-request"));
                          break;
                        }
                        if ((setParam35 == PARAM_DHW_ONOFF) && setRequest35) {
                          Serial_println(F("* Ignored - Writing to this parameter already pending by 35-request"));
                          break;
                        }
                        if (writePermission) {
                          if (writePermission != 0xFF) writePermission--;
                          setRequestDHW = 1;
                          setStatusDHW = temp;
                          if (!verbose) break;
                          Serial_print(F(" will be set to 0x"));
                          if (setStatusDHW <= 0x000F) Serial_print('0');
                          Serial_println(setStatusDHW);
                          break;
                        } else {
                          Serial_println(F("* Currently no write budget left"));
                          break;
                        }
                      } else if (setRequestDHW) {
                        Serial_print(F(": DHW-write-request to value 0x"));
                        if (setStatusDHW <= 0x0F) Serial_print('0');
                        Serial_print(setStatusDHW, HEX);
                        Serial_println(F(" still pending"));
                      } else {
                        Serial_println(F(": no DHW-write-request pending"));
                      }
                      break;
#endif /* OLD_COMMANDS */
#endif /* E_SERIES */
            default:  Serial_print(F("* Command not understood: "));
                      Serial_println(RSp - 1);
                      break;
          }
#ifdef SERIAL_MAGICSTRING
/*
        } else {
          if (!strncmp(RS, "* [ESP]", 7)) {
            Serial_print(F("* Ignoring: "));
          } else {
            Serial_print(F("* Magic String mismatch: "));
          }
          Serial_println(RS);
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
#ifdef EF_SERIES
  if (upt >= upt_prev_write + TIME_WRITE_PERMISSION) {
    if (writePermission < MAX_WRITE_PERMISSION) writePermission++;
    upt_prev_write += TIME_WRITE_PERMISSION;
  }
  if (upt >= upt_prev_error + TIME_ERRORS_PERMITTED) {
    if (errorsPermitted < MAX_ERRORS_PERMITTED) errorsPermitted++;
    upt_prev_error += TIME_ERRORS_PERMITTED;
  }
#endif /* EF_SERIES */
  while (P1P2Serial.packetavailable()) {
    uint16_t delta;
    errorbuf_t readError = 0;
    int nread = P1P2Serial.readpacket(RB, delta, EB, RB_SIZE, crc_gen, crc_feed);
    if (nread > RB_SIZE) {
      Serial_println(F("* Received packet longer than RB_SIZE"));
      nread = RB_SIZE;
      readError = 0xFF;
      if (errorsLargePacket < 0xFF) errorsLargePacket++;
    }
    for (int i = 0; i < nread; i++) readError |= EB[i];
    if (!readError) skipPackets = 0;
    if (skipPackets) {
      skipPackets--;
      break;
    }
#ifdef SW_SCOPE

#if F_CPU > 8000000L
#define FREQ_DIV 4 // 16 MHz
#else
#define FREQ_DIV 3 // 8 MHz
#endif

    if (scope
#ifdef EF_SERIES
              && ((readError && (scope_budget > 5)) || (((RB[0] == 0x40) && (RB[1] == 0xF0)) && (scope_budget > 50)) || (scope_budget > 150))
      // always keep scope write budget for 40F0 and expecially for readErrors
#endif /* EF_SERIES */
                                                                                                                                             ) {
      if (sws_cnt || (sws_event[SWS_MAX - 1] != SWS_EVENT_LOOP)) {
        scope_budget -= 5;
        if (readError) {
          Serial_print(F("C "));
        } else {
          Serial_print(F("c "));
        }
        if (RB[0] < 0x10) Serial_print('0');
        Serial_print(RB[0], HEX);
        if (RB[1] < 0x10) Serial_print('0');
        Serial_print(RB[1], HEX);
        if (RB[2] < 0x10) Serial_print('0');
        Serial_print(RB[2], HEX);
        Serial_print(' ');
        static uint16_t capture_prev;
        int i = 0;
        if (sws_event[SWS_MAX - 1] != SWS_EVENT_LOOP) i = sws_cnt;
        bool skipfirst = true;
        do {
          switch (sws_event[i]) {
            // error events
            case SWS_EVENT_ERR_BE      : Serial_print(F("   BE   ")); break;
            case SWS_EVENT_ERR_BE_FAKE : Serial_print(F("   be   ")); break;
            case SWS_EVENT_ERR_SB      : Serial_print(F("   SB   ")); break;
            case SWS_EVENT_ERR_SB_FAKE : Serial_print(F("   sb   ")); break;
            case SWS_EVENT_ERR_BC      : Serial_print(F("   BC   ")); break;
            case SWS_EVENT_ERR_BC_FAKE : Serial_print(F("   bc   ")); break;
            case SWS_EVENT_ERR_PE      : Serial_print(F("   PE   ")); break;
            case SWS_EVENT_ERR_PE_FAKE : Serial_print(F("   pe   ")); break;
            case SWS_EVENT_ERR_LOW     : Serial_print(F("   lw   ")); break;
            // read/write related events
            default   : byte sws_ev    = sws_event[i] & SWS_EVENT_MASK;
                        byte sws_state = sws_event[i] & 0x1F;
                        if (!skipfirst) {
                          if (sws_ev == SWS_EVENT_EDGE_FALLING_W) Serial_print(' ');
                          uint16_t t_diff = (sws_capture[i] - capture_prev) >> FREQ_DIV;
                          if (t_diff < 10) Serial_print(' ');
                          if (t_diff < 100) Serial_print(' ');
                          Serial_print(t_diff);
                          if (sws_ev != SWS_EVENT_EDGE_FALLING_W) Serial_print(':');
                        }
                        capture_prev = sws_capture[i];
                        skipfirst = false;
                        switch (sws_ev) {
                          case SWS_EVENT_SIGNAL_LOW     : Serial_print('?'); break;
                          case SWS_EVENT_EDGE_FALLING_W : Serial_print(' '); break;
                          case SWS_EVENT_EDGE_RISING    :
                                                          // state = 1 .. 20 (1,2 start bit, 3-18 data bit, 19,20 parity bit),
                                                          switch (sws_state) {
                                                            case 1       :
                                                            case 2       : Serial_print('S'); break; // start bit
                                                            case 3 ... 18: Serial_print((sws_state - 3) >> 1); break; // state=3-18 for data bit 0-7
                                                            case 19      :
                                                            case 20      : Serial_print('P'); break; // parity bit
                                                            default      : Serial_print(' '); break;
                                                          }
                                                          break;
                          case SWS_EVENT_EDGE_SPIKE     :
                          case SWS_EVENT_SIGNAL_HIGH_R  :
                          case SWS_EVENT_EDGE_FALLING_R :
                                                          switch (sws_state) {
                                                            case 11      : Serial_print('E'); break; // stop bit ('end' bit)
                                                            case 10      : Serial_print('P'); break; // parity bit
                                                            case 0       :
                                                            case 1       : Serial_print('S'); break; // parity bit
                                                            case 2 ... 9 : Serial_print(sws_state - 2); break; // state=2-9 for data bit 0-7
                                                            default      : Serial_print(' '); break;
                                                          }
                                                          break;
                          default                       : Serial_print(' ');
                                                          break;
                        }
                        switch (sws_ev) {
                          case SWS_EVENT_EDGE_SPIKE     : Serial_print(F("-X-"));  break;
                          case SWS_EVENT_SIGNAL_HIGH_R  : Serial_print(F("‾‾‾")); break;
                          case SWS_EVENT_SIGNAL_LOW     : Serial_print(F("___")); break;
                          case SWS_EVENT_EDGE_RISING    : Serial_print(F("_/‾")); break;
                          case SWS_EVENT_EDGE_FALLING_W :
                          case SWS_EVENT_EDGE_FALLING_R : Serial_print(F("‾\\_")); break;
                          default                       : Serial_print(F(" ? ")); break;
                        }
          }
          if (++i == SWS_MAX) i = 0;
        } while (i != sws_cnt);
        Serial_println();
      }
    }
    if (++scope_budget > 200) scope_budget = 200;
    sws_block = 0; // release SW_SCOPE for next log operation
#endif /* SW_SCOPE */
#ifdef MEASURE_LOAD
    if (irq_r) {
      uint8_t irq_busy_c1 = irq_busy;
      uint16_t irq_lapsed_r_c = irq_lapsed_r;
      uint16_t irq_r_c = irq_r;
      uint8_t  irq_busy_c2 = irq_busy;
      if (irq_busy_c1 || irq_busy_c2) {
        Serial_println(F("* irq_busy"));
      } else {
        Serial_print(F("* sh_r="));
        Serial_println(100.0 * irq_r_c / irq_lapsed_r_c);
      }
    }
    if (irq_w) {
      uint8_t irq_busy_c1 = irq_busy;
      uint16_t irq_lapsed_w_c = irq_lapsed_w;
      uint16_t irq_w_c = irq_w;
      uint8_t  irq_busy_c2 = irq_busy;
      if (irq_busy_c1 || irq_busy_c2) {
        Serial_println(F("* irq_busy"));
      } else {
        Serial_print(F("* sh_w="));
        Serial_println(100.0 * irq_w_c / irq_lapsed_w_c);
      }
    }
#endif /* MEASURE_LOAD */
    if ((readError & ERROR_COUNT_MASK) && upt) { // don't count errors while upt == 0  // don't count  PE and UC errors for H-link2
      if (readErrors < 0xFF) {
        readErrors++;
        readErrorLast = readError;
      }
#ifdef EF_SERIES
      if (errorsPermitted) {
        errorsPermitted--;
        if (!errorsPermitted) {
          Serial_println(F("* WARNING: too many read errors detected"));
          stopControlAndCounters();
        }
      }
#endif /* EF_SERIES */
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
            Serial_println(F("* Refusing to write counter-request packet while previous packet wasn't finished"));
            if (writeRefused < 0xFF) writeRefused++;
          }
          if (++counterRequest == 7) counterRequest = 0; // wait until next new minute; change 0 to 1 to continuously request counters for increased resolution
        }
      }
#else /* KLICDA */
      // check number of auxiliary controller slots (F_max), and how many already done in this cycle (F_prev)
      if ((RB[0] == 0x00) && ((RB[1] & 0xF0) == 0xF0) && (RB[2] == 0x30)) {
        byte auxCtrl = RB[1] & 0x03;
        if (F_prev < 0) {
          // do nothing, just after reset, or after detection error in this cycle
          // Serial_println(F("* Earlier F_prev error, or ignore first cycle"));
        } else if ((F_prevDet >> auxCtrl) & 0x01) {
          // repeated message, should not happen unless communication error or after Daikin restart, skip this cycle, reset data structures
          // Serial_println(F("* F_prev error"));
          F_prev = -1;
          F_prevDet = 0;
        } else {
          F_prev++;
          F_prevDet |= (1 << auxCtrl);
        }
      }
      if ((RB[0] == 0x00) && ((RB[1] & 0xF0) != 0xF0)) {
        if (F_prev > F_max) {
          F_max = F_prev;
          // Serial_print(F("* F_max set to "));
          // Serial_println(F_max);
        }
        F_prev = 0;
        F_prevDet = 0;
      }
      // insert counterRequest messages at end of each (or each 4th) cycle
      if (counterRequest && (nread > 4) && (RB[0] == 0x00) && ((RB[1] == 0xFF) || ((RB[1] & 0xFE)  == 0xF0)) && (RB[2] == 0x30) && (FxAbsentCnt[RB[1] & 0x03] == F0THRESHOLD) && (F_prev == F_max)) {
        // 00Fx30 request message received, slot has no other aux controller active, and this is the last 00Fx30 request message in this cycle
        // For CONTROL_ID != 0 and F_max==1, use 1-in-4 mechanism
        if (div4) {
          div4--;
        } else {
          WB[0] = 0x00;
          WB[1] = 0x00;
          WB[2] = 0xB8;
          WB[3] = (counterRequest - 1);
          F030forcounter = true;
          if (P1P2Serial.writeready()) {
            P1P2Serial.writepacket(WB, 4, F03XDELAY, crc_gen, crc_feed);
          } else {
            Serial_println(F("* Refusing to write counter-request packet while previous packet wasn't finished"));
            if (writeRefused < 0xFF) writeRefused++;
          }
          if (++counterRequest == 7) counterRequest = 0;
          if (CONTROL_ID && (F_max == 1)) div4 = 3;
        }
      }
#endif /* KLICDA */
      if ((nread > 4) && (RB[0] == 0x40) && (((RB[1] & 0xFE) == 0xF0) || (RB[1] == 0xFF)) && ((RB[2] & 0x30) == 0x30)) {
        // 40Fx3x auxiliary controller reply received - note this could be our own (slow, delta=F030DELAY or F03XDELAY) reply so only reset count if delta < min(F03XDELAY, F030DELAY) (- margin)
        // Note for developers using >1 P1P2Monitor-interfaces (=to self): this detection mechanism fails if there are 2 P1P2Monitor programs (and adapters) with same delay settings on the same bus.
        // check if there is any other auxiliary controller on 0x3x:
        if ((delta < F03XDELAY - 2) && (delta < F030DELAY - 2)) {
          FxAbsentCnt[RB[1] & 0x03] = 0;
          if (RB[1] == CONTROL_ID) {
            // this should only happen if another auxiliary controller is connected if/after CONTROL_ID is set
            Serial_print(F("* Warning: another aux controller answering to address 0x"));
            Serial_print(RB[1], HEX);
            Serial_println(F(" detected"));
            stopControlAndCounters();
          }
        }
      } else if ((nread > 4) && (RB[0] == 0x00) && ((RB[1] & 0xFE) == 0xF0) && ((RB[2] & 0x30) == 0x30) && !F030forcounter) {
        // 00Fx3x (F0/F1, but not FF, FF is not yet supported) request message received, and we did not use this slot to request counters
        // check if there is no other auxiliary controller
        if ((RB[2] == 0x30) && (FxAbsentCnt[RB[1] & 0x03] < F0THRESHOLD)) {
          FxAbsentCnt[RB[1] & 0x03]++;
          if (FxAbsentCnt[RB[1] & 0x03] == F0THRESHOLD) {
            Serial_print(F("* No auxiliary controller answering to address 0x"));
            Serial_print(RB[1], HEX);
            if (CONTROL_ID == RB[1]) {
              Serial_println(F(" detected, control functionality will restart"));
              insertMessageCnt = 0;  // avoid delayed insertMessage/restartDaikin
              restartDaikinCnt = 0;
            } else {
              Serial_println(F(" detected, switching control functionality can be switched on (using L1)"));
            }
          }
        }
        // act as auxiliary controller:
        if ((CONTROL_ID && (FxAbsentCnt[CONTROL_ID & 0x03] == F0THRESHOLD) && (RB[1] == CONTROL_ID))
#ifdef ENABLE_INSERT_MESSAGE
            || ((insertMessageCnt || restartDaikinCnt) && (FxAbsentCnt[CONTROL_ID & 0x03] == F0THRESHOLD) && (RB[0] == 0x00) && (RB[1] == CONTROL_ID)
#ifndef ENABLE_INSERT_MESSAGE_3x
                                                                                             && (RB[2] == 0x30)
#endif /* ENABLE_INSERT_MESSAGE_3x */
                                                                                                               )
#endif /* ENABLE_INSERT_MESSAGE */
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
            Serial_print(F("* Surprise: received 00Fx3x packet of size "));
            Serial_println(nread); }
          switch (RB[2]) {
#ifdef E_SERIES
            case 0x30 :
#ifdef ENABLE_INSERT_MESSAGE
                        if (insertMessageCnt || restartDaikinCnt) {
                          for (int i = 0; i < insertMessageLength; i++) WB[i] = insertMessage[i];
                          if (insertMessageCnt) {
                            Serial_println(F("* Insert user-specified message"));
                            insertMessageCnt--;
                          }
                          if (restartDaikinCnt) {
                            Serial_println(F("* Attempt to restart Daikin"));
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
#ifdef OLD_COMMANDS
                        // set byte WB[7] to 0x01 to request a F035 message to set Value35 and/or DHWstatus
                        if (setRequestDHW || setRequest35) WB[7] = 0x01;
                        // set byte WB[8] to 0x01 to request a F036 message to set setParam36 to Value36
                        if (setRequest36) WB[8] = 0x01;
                        // set byte WB[12] to 0x01 to request a F03A message to set setParam3A to Value3A
                        if (setRequest3A) WB[12] = 0x01;
#endif /* OLD_COMMANDS */
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
                        // on one system, response is all-zero, so consider to change to all-zero response:
                        // for (w = 3; w < n; w++) WB[w] = 0x00;
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
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = (wr_val & 0xFF); wr_cnt--; wr_req = 1; Serial_println_ExecutingCommand('E'); }
#ifdef OLD_COMMANDS
                        if (setRequestDHW) { WB[w++] = PARAM_DHW_ONOFF & 0xff; WB[w++] = PARAM_DHW_ONOFF >> 8; WB[w++] = setStatusDHW; setRequestDHW = 0; Serial_println_ExecutingCommand('Y'); }
                        if (setRequest35)  { WB[w++] = setParam35  & 0xff; WB[w++] = setParam35  >> 8; WB[w++] = setValue35;   setRequest35 = 0; Serial_println_ExecutingCommand('Z'); }
#endif /* OLD_COMMANDS */
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
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; wr_cnt--; wr_req = 1; Serial_println_ExecutingCommand('E'); }
#ifdef OLD_COMMANDS
                        if (setRequest36) { WB[w++] = setParam36 & 0xff; WB[w++] = (setParam36 >> 8) & 0xff; WB[w++] = setValue36 & 0xff; WB[w++] = (setValue36 >> 8) & 0xff; setRequest36 = 0; Serial_println_ExecutingCommand('R'); }
#endif /* OLD_COMMANDS */
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
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; WB[w++] = (wr_val >> 16) & 0xFF; wr_cnt--; wr_req = 1; Serial_println_ExecutingCommand('E'); }
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
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; WB[w++] = (wr_val >> 16) & 0xFF; WB[w++] = (wr_val >> 24) & 0xFF; wr_cnt--; wr_req = 1; Serial_println_ExecutingCommand('E'); };
                        wr = 1;
                        break;
            case 0x39 : // in: 21 byte; out 21 byte; 4-byte parameters; reply with FF
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; WB[w++] = (wr_val >> 16) & 0xFF; WB[w++] = (wr_val >> 24) & 0xFF; wr_cnt--; wr_req = 1; Serial_println_ExecutingCommand('E'); }
                        wr = 1;
                        break;
            case 0x3A : // in: 21 byte; out 21 byte; 1-byte parameters reply with FF
                        // A parameter consists of 3 bytes: 2 bytes for param nr, and 1 byte for value
                        // parameters in the 00F03A message may indicate system status (silent, schedule, unit, DST, holiday)
                        // change bytes for triggering 3Arequest
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = (wr_val & 0xFF); wr_cnt--; wr_req = 1; Serial_println_ExecutingCommand('E'); }
#ifdef OLD_COMMANDS
                        if (setRequest3A)  { WB[w++] = setParam3A  & 0xff; WB[w++] = setParam3A  >> 8; WB[w++] = setValue3A;   setRequest3A = 0; Serial_println_ExecutingCommand('N'); }
#endif /* OLD_COMMANDS */
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
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; wr_cnt--; wr_req = 1; Serial_println_ExecutingCommand('E'); }
                        wr = 1;
                        break;
            case 0x3C : // in: 23 byte; out 23 byte; 3-byte parameters; reply with FF
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; WB[w++] = (wr_val >> 16) & 0xFF; wr_cnt--; wr_req = 1; Serial_println_ExecutingCommand('E'); }
                        wr = 1;
                        break;
            case 0x3D : // in: 21 byte; out: 21 byte; 4-byte parameters; reply with FF
                        // parameter range 0000-001F; kwH/hour counters?
                        // not seen in EHVX08S23D6V
                        // seen in EHVX08S26CB9W
                        // seen in EHYHBX08AAV3
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        if (wr_cnt && (wr_pt == RB[2])) { WB[w++] = wr_nr & 0xff; WB[w++] = wr_nr >> 8; WB[w++] = wr_val & 0xFF; WB[w++] = (wr_val >> 8) & 0xFF; WB[w++] = (wr_val >> 16) & 0xFF; WB[w++] = (wr_val >> 24) & 0xFF; wr_cnt--; wr_req = 1; Serial_println_ExecutingCommand('E'); }
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

// Messages and payload lengths:
// ?   EKHBRD*ADV   0x30   /               0x34  5/0                                                                  0x38 20/14
// BCL FDY          0x30 20/0                                                                                         0x38 16/15 / 0x39 11/4
// LPA FXMQ         0x30 20/0   0x32 8/1              0x35 19/0   0x36 19/0                                           0x38 20/17   0x39 14/5   0x3A 18/8
// M   FDYQ         0x30 20/0                                                 0x3700-0x3706 14/0 0x3707-0x3713 16/0                                       0x3B 20/19 / 0x3C 12/2

// Compatible systems:
// ?   EKHBRD*ADV: ?
// BCL FDY:  FDY125LV1, 2002 FBQ*B*, ADEQ100B2VEB, perhaps also: FBQ35B7V1
// LPA FXMQ: FXMQ200PWM and/or FXMQ100PAVE ?, FDYQN160LAV1, perhaps also: FBA60A9
// M   FDYQ: FDYQ180MV1

/*
You can use the following commands with spaces:

For FDY/FXMQ-like systems, try using
F 38 0 1     to switch on
F 38 0 0     to switch off
F 38 1 61    to set heating mode
F 38 1 62    to set cooling mode
F 38 1 63    to set auto mode
F 38 1 67    to set dry mode
F 38 1 60    to set fan-only mode
F 38 2 17    to set the setpoint to 0x17 = 23C (cooling mode)
F 38 2 19    to set the setpoint to 0x19 = 25C
F 38 4 11    to set the fanmode to low         (cooling mode)
F 38 4 31    to set the fanmode to medium
F 38 4 51    to set the fanmode to high
F 38 6 17    to set the setpoint to 0x17 = 23C (heating mode)
F 38 6 19    to set the setpoint to 0x19 = 25C
F 38 8 11    to set the fanmode to low         (heating mode)
F 38 8 31    to set the fanmode to medium
F 38 8 51    to set the fanmode to high
F 38 16 ?? (not sure yet, active hvac zones related)
F 38 17 ?? (not sure yet, target fan mode related)
or the following commands without spaces with extra zeroes in first and second operand:
F38001     to switch on
F38000     to switch off
F380217    to set the setpoint to 0x17 = 23C
F380219    to set the setpoint to 0x19 = 25C
F380411    to set the fanmode to low
etc

For FDYQ-like systems, try using the same commands with packet type 38 replaced by 3B.
*/

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
              WB[3]  = RB[3] & 0x01;           // W target status
              WB[4]  = RB[5];                  // W target operating mode
              WB[5]  = RB[7];                  // W target temperature cooling (can be changed by reply with different value)
              WB[6]  = 0x00;
              WB[7]  = RB[9];                  // W target fan speed cooling/fan (11 / 31 / 51) (can be changed by reply with different value)
              WB[8]  = 0x00;
              WB[9]  = RB[11];                 // W target temperature heating (can be changed by reply with different value)
              WB[10] = 0x00;
              WB[11] = RB[13];                 // W target fan speed heating/fan (11 / 31 / 51)
              WB[12] = RB[14];                 // no flag?
              WB[13] = 0x00;
              WB[14] = 0x00;
              WB[15] = 0x00;
              WB[16] = RB[18];                 // C0, E0 when payload byte 0 set to 1
              WB[17] = 0x00;
              WB[18] = 0;                      // puzzle: initially 0, then 2, then 1 ????
              WB[19] = 0x00;
              if (wr_cnt && (wr_pt == RB[2])) {
                if ((wr_nr == 0) && (WB[wr_nr + 3] == 0x00) && (wr_val)) WB[16] |= 0x20; // change payload byte 13 from C0 to E0, only if payload byte 0 is set to 1 here
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
              Serial_println(F("* Refusing to write packet while previous packet wasn't finished, flushing write action"));
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
      for (int i = 0; i < nread - 1; i++) insertMessage[i] = RB[i];
      insertMessageLength = nread - 1;
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

#ifdef H_SERIES
// for H-link2, split reporting errors (if any) and data (always)
    uint16_t delta2 = delta;
#endif /* H_SERIES */
    if (readError) {
      // error, so output data on line starting with E
      Serial_print(F("E "));
#ifndef H_SERIES
    } else {
      // no error, so output data on line starting with R, unless H_SERIES, which does this in separate code segment below
      if (verbose && (verbose < 4)) Serial_print(F("R "));
    }
    if (((verbose & 0x01) == 1) || readError) {
#endif /* H_SERIES */

      // 3nd-12th characters show length of bus pause (max "R T 65.535: ")
      Serial_print(F("T "));
      if (delta < 10000) Serial_print(' ');
      if (delta < 1000) {
        Serial_print('0');
      } else {
        Serial_print(delta / 1000); delta %= 1000;
      };
      Serial_print(F("."));
      if (delta < 100) Serial_print('0');
      if (delta < 10) Serial_print('0');
      Serial_print(delta);
      Serial_print(F(": "));
#ifndef H_SERIES
    }
    if ((verbose < 4) || readError) {
#else
      byte cs = 0;
#endif /* H_SERIES */
      for (int i = 0; i < nread; i++) {
        if (verbose && (EB[i] & ERROR_SB)) {
          // collision suspicion due to data verification error in reading back written data
          Serial_print(F("-SB:"));
        }
        if (verbose && (EB[i] & ERROR_BE)) { // or BE3 (duplicate code)
          // collision suspicion due to data verification error in reading back written data
          Serial_print(F("-XX:"));
        }
        if (verbose && (EB[i] & ERROR_BC)) {
          // collision suspicion due to 0 during 2nd half bit signal read back
          Serial_print(F("-BC:"));
        }
        if (verbose && (EB[i] & ERROR_PE)) {
          // parity error detected
          Serial_print(F("-PE:"));
        }
#ifdef H_SERIES
        if (verbose && (EB[i] & SIGNAL_UC)) {
          // 11/12 bit uncertainty detected
          Serial_print(F("-UC:"));
        }
#endif /* H_SERIES */
#ifdef GENERATE_FAKE_ERRORS
        if (verbose && (EB[i] & (ERROR_SB << 8))) {
          // collision suspicion due to data verification error in reading back written data
          Serial_print(F("-sb:"));
        }
        if (verbose && (EB[i] & (ERROR_BE << 8))) {
          // collision suspicion due to data verification error in reading back written data
          Serial_print(F("-xx:"));
        }
        if (verbose && (EB[i] & (ERROR_BC << 8))) {
          // collision suspicion due to 0 during 2nd half bit signal read back
          Serial_print(F("-bc:"));
        }
        if (verbose && (EB[i] & (ERROR_PE << 8))) {
          // parity error detected
          Serial_print(F("-pe:"));
        }
#endif /* GENERATE_FAKE_ERRORS */
        byte c = RB[i];
        if (crc_gen && (verbose == 1) && (i == nread - 1)) {
          Serial_print(F(" CRC="));
        }
        if (c < 0x10) Serial_print('0');
        Serial_print(c, HEX);
        if (verbose && (EB[i] & ERROR_OR)) {
          // buffer overrun detected (overrun is after, not before, the read byte)
          Serial_print(F(":OR-"));
        }
#ifdef H_SERIES
        if (i) cs ^= c;
#endif
        if (verbose && (EB[i] & ERROR_CRC)) {
          // CRC error detected in readpacket
          Serial_print(F(" CRC error"));
        }
      }
#ifdef H_SERIES
      Serial_print(F(" CS1=")); // checksum starting at byte 1 (which is usually 0)
      if (cs < 0x10) Serial_print(F("0"));
      Serial_print(cs, HEX);
      cs ^= RB[1];
      Serial_print(F(" CS2=")); // checksum starting at byte 2
      if (cs < 0x10) Serial_print(F("0"));
      Serial_print(cs, HEX);
#endif
      if (readError
#ifdef H_SERIES
                    & 0xBB
#endif /* H_SERIES */
                          ) {
        Serial_print(F(" readError=0x"));
        if (readError < 0x10) Serial_print('0');
        if (readError < 0x100) Serial_print('0');
        if (readError < 0x1000) Serial_print('0');
        Serial_print(readError, HEX);
      }
      Serial_println();
    }

#ifdef H_SERIES
    delta = delta2;
    if (!(readError & 0xBB)) {
      if (verbose && (verbose < 4)) Serial_print(F("R "));
      if ((verbose & 0x01) == 1)  {
        // 3nd-12th characters show length of bus pause (max "R T 65.535: ")
        Serial_print(F("T "));
        if (delta < 10000) Serial_print(' ');
        if (delta < 1000) {
          Serial_print(F("0"));
        } else {
          Serial_print(delta / 1000);
          delta %= 1000;
        };
        Serial_print(F("."));
        if (delta < 100) Serial_print(F("0"));
        if (delta < 10) Serial_print(F("0"));
        Serial_print(delta);
        Serial_print(F(": "));
      }
      if (verbose < 4) {
        for (int i = 0; i < nread; i++) {
          byte c = RB[i];
          if (crc_gen && (verbose == 1) && (i == nread - 1)) {
            Serial_print(F(" CRC="));
          }
          if (c < 0x10) Serial_print(F("0"));
          Serial_print(c, HEX);
        }
        Serial_println();
      }

    }
#endif /* H_SERIES */
  }
#ifdef PSEUDO_PACKETS
  for (byte i = 0; i < 23; i++) WB[i] = 0x00;
  if (pseudo0D > 4) {
    // WB[0]  = 0x00;
    // WB[1]  = 0x00;
    pseudo0D = 0;
    WB[2]  = 0x0D;
    if (ATmegaHwID) {
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
    }
#ifdef EF_SERIES
    WB[19] = controlLevel;
    WB[20] = CONTROL_ID ? controlLevel : 0; // auxiliary control mode fully on; is 1 for control modes 1 and 3
    WB[21] = F_max;
#endif /* EF_SERIES */
    if (!suppressSerial & (verbose < 4)) writePseudoPacket(WB, 23);
  }
  if (pseudo0E > 4) {
    pseudo0E = 0;
    // WB[0]  = 0x00;
    // WB[1]  = 0x00;
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
#ifdef EF_SERIES
    WB[15] = wr_cnt;
    WB[16] = wr_pt;
    WB[17] = wr_nr >> 8;
    WB[18] = wr_nr & 0xFF;
    WB[19] = wr_val >> 24;
    WB[20] = 0xFF & (wr_val >> 16);
    WB[21] = 0xFF & (wr_val >> 8);
    WB[22] = 0xFF & wr_val;
#endif /* EF_SERIES */
    if (!suppressSerial & (verbose < 4)) writePseudoPacket(WB, 23);
  }
  if (pseudo0F > 4) {
    bool sigMatch = 1;
    pseudo0F = 0;
    // WB[0]  = 0x00;
    // WB[1]  = 0x00;
    WB[2]  = 0x0F;
    WB[3]  = Compile_Options;
    WB[4]  = verbose;
    WB[5]  = save_MCUSR;
#ifdef EF_SERIES
    WB[6]  = writePermission;
    WB[7]  = errorsPermitted;
    WB[8]  = (parameterWritesDone >> 8) & 0xFF;
    WB[9]  = parameterWritesDone & 0xFF;
#endif /* EF_SERIES */
    WB[10] = (sd >> 8) & 0xFF;
    WB[11] = sd & 0xFF;
    WB[12] = (sdto >> 8) & 0xFF;
    WB[13] = sdto & 0xFF;
    WB[14] = ATmegaHwID;
#ifdef EEPROM_SUPPORT
#ifdef EF_SERIES
    WB[15] = EEPROM.read(EEPROM_ADDRESS_CONTROL_ID);
#endif /* EF_SERIES */
    WB[16] = EEPROM.read(EEPROM_ADDRESS_VERBOSITY);
#ifdef EF_SERIES
    WB[17] = EEPROM.read(EEPROM_ADDRESS_COUNTER_STATUS);
#endif /* EF_SERIES */
    for (uint8_t i = 0; i < strlen(EEPROM_SIGNATURE); i++) sigMatch &= (EEPROM.read(EEPROM_ADDRESS_SIGNATURE + i) == EEPROM_SIGNATURE[i]);
    WB[18] = sigMatch;
#endif /* EEPROM_SUPPORT */
    WB[19] = scope;
    WB[20] = readErrors;
    WB[21] = readErrorLast;
#ifdef EF_SERIES
    WB[22] = CONTROL_ID;
#endif /* EF_SERIES */
    if (!suppressSerial & (verbose < 4)) writePseudoPacket(WB, 23);
  }
#endif /* PSEUDO_PACKETS */
}
