/* P1P2Monitor: monitor and control program for HBS such as P1/P2 bus on many Daikin systems.
 *              Mitsubishi Heavy Industries (MHI) X-Y line protocol: no control yet
 *
 *              Reads the P1/P2 bus using the P1P2MQTT library, and outputs raw hex data and verbose output over serial.
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
 * Copyright (c) 2019-2024 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 * 20240605 v0.9.53 replace KLICDA by configurable counterCycleStealDelay
 * 20240512 v0.9.46 remove unneeded 'G' 'H' and OLD_COMMANDS, adding MHI support, remove verbosity levels, F-series model-suggestions, multiple write-commands
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
 *     and to Paul Stoffregen for publishing the AltSoftSerial library which was the starting point for the P1P2MQTT library.
 *
 * P1P2erial is written and tested for the Arduino Uno and (in the past) Mega.
 * It may or may not work on other hardware using a 16MHz clok.
 * As of P1P2MQTT library 0.9.14, it also runs on 8MHz ATmega328P using MCUdude's MiniCore.
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

#include "P1P2Config.h"
#include <P1P2MQTT.h>

#define SPI_CLK_PIN_VALUE (PINB & 0x20)

#ifdef MHI_SERIES
static byte mhiFormat = INIT_MHI_FORMAT;
#endif /* MHI_SERIES */
static byte brand = INIT_BRAND;
static byte model = INIT_MODEL;
static byte readErrors = 0;
static byte readErrorLast = 0;
static byte writeRefusedBusy = 0;
static byte writeRefusedBudget = 0;
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
#ifdef F_SERIES
static byte modelSuggestion = 0;
#endif /* F_SERIES */

const byte Compile_Options = 0 // multi-line statement
#ifdef MONITORCONTROL
+0x01
#endif
#ifdef PSEUDO_PACKETS
+0x04
#endif
+0x08
#ifdef SERIAL_MAGICSTRING
+0x10
#endif
#ifdef E_SERIES
+0x20
#elif defined F_SERIES
+0x40
#elif defined H_SERIES
+0x60
#elif defined M_SERIES
+0x80
#elif defined MHI_SERIES
+0xA0
#elif defined S_SERIES
+0xC0
#elif defined T_SERIES
+0xE0
#endif
;

#include <EEPROM.h>
void initEEPROM() {
  // Serial.println(F("* EEPROM check"));
  bool sigMatch = 1;
  for (uint8_t i = 0; i < strlen(EEPROM_SIGNATURE); i++) sigMatch &= (EEPROM.read(EEPROM_ADDRESS_SIGNATURE + i) == EEPROM_SIGNATURE[i]);
  // Serial.print(F("* EEPROM sig match"));
  // Serial.println(sigMatch);
  if (!sigMatch) {
    Serial.println(F("* EEPROM init"));
    for (uint8_t i = 0; i < strlen(EEPROM_SIGNATURE); i++) EEPROM.update(EEPROM_ADDRESS_SIGNATURE + i, EEPROM_SIGNATURE[i]); // no '\0', not needed
    EEPROM.update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID_NONE); // Daikin specific
    EEPROM.update(EEPROM_ADDRESS_COUNTER_STATUS, COUNTERREPEATINGREQUEST); // Daikin specific
    EEPROM.update(EEPROM_ADDRESS_BRAND, INIT_BRAND);
    EEPROM.update(EEPROM_ADDRESS_MODEL, INIT_MODEL);
#ifdef MHI_SERIES
    EEPROM.update(EEPROM_ADDRESS_MHI_FORMAT, INIT_MHI_FORMAT);
#endif /* MHI_SERIES */
#if defined MHI_SERIES || defined TH_SERIES
    EEPROM.update(EEPROM_ADDRESS_ALLOW, ALLOW_PAUSE_BETWEEN_BYTES);
#endif /* MHI_SERIES || TH_SERIES */
  }
  if (EEPROM.read(EEPROM_ADDRESS_ERROR_MASK) == 0xFF) EEPROM.update(EEPROM_ADDRESS_ERROR_MASK, ERROR_MASK);
#ifdef E_SERIES
  if (EEPROM.read(EEPROM_ADDRESS_COUNTER_CYCLE_STEAL_DELAY) == 0xFF) EEPROM.update(EEPROM_ADDRESS_COUNTER_CYCLE_STEAL_DELAY, 0);
#endif /* E_SERIES */
  // EEPROM_version 0x00 adds WRITE_BUDGET_PERIOD
  if (EEPROM.read(EEPROM_ADDRESS_VERSION) == 0xFF) {
    EEPROM.update(EEPROM_ADDRESS_VERSION, 0);
#ifdef EF_SERIES
    EEPROM.update(EEPROM_ADDRESS_WRITE_BUDGET_PERIOD, INIT_WRITE_BUDGET_PERIOD);
#endif /* EF_SERIES */
  }
  if (EEPROM.read(EEPROM_ADDRESS_VERSION) == 0) {
    // EEPROM.update(EEPROM_ADDRESS_VERSION, 1);
#ifdef EF_SERIES
    EEPROM.update(EEPROM_ADDRESS_INITIAL_WRITE_BUDGET, INIT_WRITE_BUDGET);
#endif /* EF_SERIES */
  }
  if (EEPROM.read(EEPROM_ADDRESS_VERSION) < 2) {
    EEPROM.update(EEPROM_ADDRESS_VERSION, 2);
#ifdef E_SERIES
    EEPROM.update(EEPROM_ADDRESS_ROOM_TEMPERATURE_LSB, INIT_ROOM_TEMPERATURE_LSB);
    EEPROM.update(EEPROM_ADDRESS_ROOM_TEMPERATURE_MSB, INIT_ROOM_TEMPERATURE_MSB);
#endif /* E_SERIES */
  }
}

P1P2MQTT P1P2MQTT;

static uint16_t sd = INIT_SD;           // delay setting for each packet written upon manual instruction (changed to 50ms which seems to be a bit safer than 0)
static uint16_t sdto = INIT_SDTO;       // time-out delay (applies both to manual instructed writes and controller writes)
static byte echo = INIT_ECHO;           // echo setting (whether written data is read back)
static byte errorMask = ERROR_MASK;
#if defined MHI_SERIES || defined TH_SERIES
static byte allow = ALLOW_PAUSE_BETWEEN_BYTES;
#endif /* MHI || TH_SERIES */
static byte scope = INIT_SCOPE;         // scope setting (to log timing info)
#ifdef EF_SERIES
static uint8_t CONTROL_ID = CONTROL_ID_NONE;
#endif /* EF_SERIES */
#ifdef E_SERIES
static byte counterRequest = 0;
static byte counterRepeatingRequest = 0;
byte insertRoomTemperature = 0;
uint16_t roomTemperature = 200;
#endif /* E_SERIES */

byte save_MCUSR;

#ifdef EF_SERIES
#define WR_MAX 6
uint8_t wr_n = 0;
uint8_t wr_cnt[WR_MAX] = { 0, 0, 0, 0, 0, 0 };
uint8_t wr_pt[WR_MAX] = { 0, 0, 0, 0, 0, 0 };
#endif /* EF_SERIES */
#ifdef E_SERIES
uint16_t wr_nr[WR_MAX] = { 0, 0, 0, 0, 0, 0 };
uint32_t wr_val[WR_MAX] = { 0, 0, 0, 0, 0, 0 };
#endif /* E_SERIES */
#ifdef F_SERIES
uint8_t wr_nr[WR_MAX] = { 0, 0, 0, 0, 0, 0 };
uint8_t wr_val[WR_MAX] = { 0, 0, 0, 0, 0, 0 };
#endif /* F_SERIES */

byte Tmin = 0;
byte Tminprev = 61;
int32_t upt_prev_pseudo = 0;
int32_t upt_prev_write = 0;
int32_t upt_prev_error = 0;
#ifdef EF_SERIES
uint8_t writeBudget = INIT_WRITE_BUDGET;
uint8_t writeBudgetPeriod = INIT_WRITE_BUDGET_PERIOD;
uint8_t errorsPermitted = INIT_ERRORS_PERMITTED;
uint16_t parameterWritesDone = 0;

// FxAbsentCnt[x] counts number of unanswered 00Fx30 messages (only for x=0,1,F (mapped to 0,1,3));
// if -1 than no Fx request or response seen (relevant to detect whether F1 controller is supported or not)
static int8_t FxAbsentCnt[4] = { -1, -1, -1, -1};
#endif /* #F_SERIES */
#ifdef E_SERIES
uint8_t counterCycleStealDelay = 0;
#endif /* E_SERIES */

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

void printWelcomeString(byte ign) {
  if (ign) Serial.println("* (to be ignored)");
  Serial.print("* ");
  Serial.print(F(WELCOMESTRING));
  Serial.print(F(" compiled "));
  Serial.print(F(__DATE__));
  Serial.print(F(" "));
  Serial.print(F(__TIME__));
  Serial.print(F(" brand "));
  Serial.print(brand);
  Serial.print(F(" modelnr "));
  Serial.print(model);
#ifdef MONITORCONTROL
  Serial.print(F(" +control"));
#endif /* MONITORCONTROL */
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
#ifdef H_SERIES
  Serial.print(F(" H-series"));
#endif /* H_SERIES */
#ifdef T_SERIES
  Serial.print(F(" T-series"));
#endif /* T_SERIES */
#ifdef MHI_SERIES
  Serial.print(F(" MHI-series"));
#endif /* T_SERIES */
  Serial.println();
  Serial.print(F("* Reset cause: MCUSR="));
  Serial.print(save_MCUSR);
  if (save_MCUSR & (1 << BORF))  Serial.print(F(" (brown-out)")); // 4
  if (save_MCUSR & (1 << EXTRF)) Serial.print(F(" (ext-reset)")); // 2
  if (save_MCUSR & (1 << PORF)) Serial.print(F(" (power-on-reset)")); // 1
  Serial.println();
  Serial.print(F("* P1P2MQTT bridge ATmegaHwID v1."));
  Serial.println(ATmegaHwID);
#ifdef EF_SERIES
  Serial.print(F("* Control_id=0x"));
  Serial.println(CONTROL_ID, HEX);
#ifdef E_SERIES
  Serial.print(F("* Counter_requests="));
  Serial.println(counterRepeatingRequest);
#endif /* E_SERIES */
#endif /* EF_SERIES */
#ifdef MHI_SERIES
  Serial.print(F("* MHI format="));
  Serial.println(mhiFormat);
#endif /* MHI_SERIES */
#if defined MHI_SERIES || defined TH_SERIES
  Serial.print(F("* Allow="));
  Serial.println(allow);
  Serial.print(F("* Error mask=0x"));
  if (errorMask < 0x10) Serial.print('0');
  Serial.println(errorMask, HEX);
#endif /* MHI || TH_SERIES */
#ifdef E_SERIES
  if (counterCycleStealDelay) {
    Serial.print(F("* Counter cycle steal delay active: "));
    Serial.println(counterCycleStealDelay);
  } else {
    Serial.println(F("* Counter cycle stealing inactive"));
  }
  Serial.print(F("* Room Temperature insertion function "));
  if (insertRoomTemperature) {
    Serial.print(F("on: "));
  } else {
    Serial.print(F("off: "));
  }
  Serial.println(roomTemperature * 0.1);
#endif /* E_SERIES */
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
  initEEPROM();
#ifdef EF_SERIES
  CONTROL_ID = EEPROM.read(EEPROM_ADDRESS_CONTROL_ID);
  writeBudgetPeriod = EEPROM.read(EEPROM_ADDRESS_WRITE_BUDGET_PERIOD);
  writeBudget = EEPROM.read(EEPROM_ADDRESS_INITIAL_WRITE_BUDGET);
#endif /* EF_SERIES */
#ifdef E_SERIES
  counterRepeatingRequest = EEPROM.read(EEPROM_ADDRESS_COUNTER_STATUS);
  counterCycleStealDelay = EEPROM.read(EEPROM_ADDRESS_COUNTER_CYCLE_STEAL_DELAY);
  roomTemperature = ((EEPROM.read(EEPROM_ADDRESS_ROOM_TEMPERATURE_MSB) & 0x7F) << 8) | EEPROM.read(EEPROM_ADDRESS_ROOM_TEMPERATURE_LSB);
  insertRoomTemperature = (EEPROM.read(EEPROM_ADDRESS_ROOM_TEMPERATURE_MSB) & 0x80) >> 7;
#endif /* E_SERIES */
#ifdef MHI_SERIES
  mhiFormat  = EEPROM.read(EEPROM_ADDRESS_MHI_FORMAT);
#endif /* MHI_SERIES */
#if defined MHI_SERIES || defined TH_SERIES
  allow      = EEPROM.read(EEPROM_ADDRESS_ALLOW);
#endif /* MHI || TH_SERIES */
#ifndef E_SERIES
  errorMask = EEPROM.read(EEPROM_ADDRESS_ERROR_MASK);
#endif /* E_SERIES */
  brand = EEPROM.read(EEPROM_ADDRESS_BRAND);
  model = EEPROM.read(EEPROM_ADDRESS_MODEL);
  // write brand/model if not yet known and if defined in P1P2Config.h
  if (brand == 0xFF) {
    EEPROM.update(EEPROM_ADDRESS_BRAND, INIT_BRAND);
    brand = INIT_BRAND;
  }
  if (model == 0xFF) {
    EEPROM.update(EEPROM_ADDRESS_MODEL, INIT_MODEL);
    model = INIT_MODEL;
  }
  printWelcomeString(true);
  // 0 v1.0 no ADC
  // 1 v1.1 use ADC6 and ADC7
  // 2 v1.2 use ADC0 and ADC1, reverse R,W LEDs
  P1P2MQTT.begin(9600, ATmegaHwID ? true : false, (ATmegaHwID == 1) ? 6 : 0, (ATmegaHwID == 1) ? 7 : 1,  (ATmegaHwID == 2));
  P1P2MQTT.setEcho(echo);
#ifdef MHI_SERIES
  P1P2MQTT.setMHI(mhiFormat);
#endif /* MHI_SERIES */
#if defined MHI_SERIES || defined TH_SERIES
  P1P2MQTT.setAllow(allow);
  P1P2MQTT.setErrorMask(errorMask);
#endif /* MHI_SERIES || TH_SERIES */
#ifdef SW_SCOPE
  P1P2MQTT.setScope(scope);
#endif
  P1P2MQTT.setDelayTimeout(sdto);
  P1P2MQTT.ledError(0);
  Serial.println(F("* Ready setup"));
}

// scanint and scanhex are used to save dynamic memory usage in main loop
int scanint(char* s, uint16_t &b) {
  return sscanf(s,"%d",&b);
}

int scanhex(char* s, uint16_t &b) {
  return sscanf(s,"%4x",&b);
}

static byte cs_gen = CS_GEN;

static byte suppressSerial = 0;
#define Serial_read(...) (suppressSerial ? -1 : Serial.read(__VA_ARGS__))
#define Serial_print(...) if (!suppressSerial) Serial.print(__VA_ARGS__)
#define Serial_println(...) if (!suppressSerial) Serial.println(__VA_ARGS__)

void writePseudoPacket(byte* WB, byte rh)
{
  if (!suppressSerial) {
    Serial.print(F("R P         "));
    uint8_t crc_cs = CRC_CS_FEED;
    for (uint8_t i = 0; i < rh; i++) {
      uint8_t c = WB[i];
      if (c <= 0x0F) Serial.print('0');
      Serial.print(c, HEX);
      if (cs_gen != 0) crc_cs += c;
      if (CRC_GEN != 0) for (uint8_t i = 0; i < 8; i++) {
        crc_cs = ((crc_cs ^ c) & 0x01 ? ((crc_cs >> 1) ^ CRC_GEN) : (crc_cs >> 1));
        c >>= 1;
      }
    }
    if (cs_gen || CRC_GEN) {
      if (crc_cs <= 0x0F) Serial.print(F("0"));
      Serial.print(crc_cs, HEX);
    }
    Serial.println();
  }
}

#ifdef EF_SERIES
void stopControlAndCounters(bool stopCycleStealing) {
  if (CONTROL_ID) Serial_println(F("* Switching control functionality off (also after next ATmega reboot)"));
  CONTROL_ID = CONTROL_ID_NONE;
  EEPROM.update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
#ifdef E_SERIES
  if (counterRepeatingRequest) Serial_println(F("* Switching counter request function off (also after next ATmega reboot)"));
  counterRepeatingRequest = 0;
  counterRequest = 0;
  EEPROM.update(EEPROM_ADDRESS_COUNTER_STATUS, counterRepeatingRequest);
  if (stopCycleStealing) {
    Serial_println(F("* Switching counter cycle stealing off (also after next ATmega reboot)"));
    counterCycleStealDelay = 0;
    EEPROM.update(EEPROM_ADDRESS_COUNTER_CYCLE_STEAL_DELAY, 0);
  }
#endif /* E_SERIES */
  for (byte i = 0; i < WR_MAX; i++) wr_cnt[i] = 0;
}
#endif /* EF_SERIES */

#ifdef E_SERIES
#define PARAM_TP_START      0x35
#define PARAM_TP_END        0x3D
#define PARAM_ARR_SZ (PARAM_TP_END - PARAM_TP_START + 1)
const uint32_t nr_params[PARAM_ARR_SZ] = { 0x017C, 0x002F, 0x0002, 0x001F, 0x00F0, 0x006C, 0x00AF, 0x0002, 0x0020 }; // number of parameters observed
//byte packettype                      = {   0x35,   0x36,   0x37,   0x38,   0x39,   0x3A,   0x3B,   0x3C,   0x3D };
#endif /* E_SERIES */

void(* resetFunc) (void) = 0; // declare reset function at address 0

int32_t upt = 0;
static byte pseudo0E = 0;
static byte pseudo0F = 0;

uint8_t scope_budget = 200;

#define SKIP_PACKETS 10
byte skipPackets = SKIP_PACKETS; // skip at most SKIP_PACKETS initial packets with errors; and always skip first packet if delta < 1
#ifdef E_SERIES
int8_t F_prev = -1;
byte F_prevDet = 0;
byte F_max = 0;
byte div4 = 0;
#endif /* E_SERIES */

byte compressor = 0;

#ifdef E_SERIES
bool writeParam(void) {
/* already done via wr_busy
  if (wr_cnt[wr_n]) {
    // previous write still being processed
    Serial_println(F("* Previous parameter write action still busy"));
    return 0;
  }
*/
  if ((wr_pt[wr_n] < PARAM_TP_START) || (wr_pt[wr_n] > PARAM_TP_END)) {
    Serial_print(F("* wr_pt["));
    Serial_print(wr_n);
    Serial_print(F("]: 0x"));
    Serial_print(wr_pt[wr_n], HEX);
    Serial_println(F(" not 0x35-0x3D"));
    return 0;
  }
  if (wr_nr[wr_n] > nr_params[wr_pt[wr_n] - PARAM_TP_START]) {
    Serial_print(F("* wr_nr["));
    Serial_print(wr_n);
    Serial_print(F("] > expected: 0x"));
    Serial_println(wr_nr[wr_n], HEX);
    return 0;
  }
  uint8_t wr_nrb;
  switch (wr_pt[wr_n]) {
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
  if (wr_val[wr_n] >> (wr_nrb << 3)) {
    Serial_print(F("* Parameter value too large for packet type; #bytes is "));
    Serial_print(wr_nrb);
    Serial_print(F(" value is "));
    Serial_println(wr_val[wr_n], HEX);
    return 0;
  }
  if (writeBudget) {
    if (writeBudget != 255) writeBudget--;
    wr_cnt[wr_n] = WR_CNT;
    Serial_print(F("* Initiating parameter write for packet-type 0x"));
    Serial_print(wr_pt[wr_n], HEX);
    Serial_print(F(" parameter nr 0x"));
    Serial_print(wr_nr[wr_n], HEX);
    Serial_print(F(" to value 0x"));
    Serial_print(wr_val[wr_n], HEX);
    Serial_println();
    return 1;
  } else {
    Serial_println(F("* Currently no write budget left"));
    if (writeRefusedBudget < 0xFF) writeRefusedBudget++;
    return 0;
  }
}
#endif /* E_SERIES */

#ifdef F_SERIES
bool writeParam(void) {
  if ((model == 10) || (model == 11)) {
    if (wr_pt[wr_n] != 0x38) {
      Serial_print(F("* wr_pt["));
      Serial_print(wr_n);
      Serial_print(F("]: 0x"));
      Serial_print(wr_pt[wr_n], HEX);
      Serial_println(F(" is not 0x38"));
      return 0;
    }
  } else if (model == 12) {
    if (wr_pt[wr_n] != 0x3B) {
      Serial_print(F("* wr_pt["));
      Serial_print(wr_n);
      Serial_print(F("]: 0x"));
      Serial_print(wr_pt[wr_n], HEX);
      Serial_println(F(" is not 0x3B"));
      return 0;
    }
  } else {
    Serial_print(F("* F cmd not supported yet for model "));
    Serial_println(model);
    return 0;
  }
  // check wr_nr[wr_n]
  if ((wr_nr[wr_n] > 17) || (wr_nr[wr_n] == 3) || (wr_nr[wr_n] == 5) || (wr_nr[wr_n] == 7) || ((wr_nr[wr_n] >= 9) && (wr_nr[wr_n] <= 15)) || ((wr_nr[wr_n] == 16) && (wr_pt[wr_n] == 0x38)) || ((wr_nr[wr_n] == 17) && (wr_pt[wr_n] == 0x38))) {
    Serial_print(F("* wr_nr[wr_n] invalid, should be 0, 1, 2, 4, 6, 8 (or for packet type 0x3B: 16 or 17): "));
    Serial_println(wr_nr[wr_n]);
    return 0;
  }
  if ((wr_nr[wr_n] == 0) && (wr_val[wr_n] > 1)) {
    Serial_println(F("wr_val["));
    Serial_print(wr_n);
    Serial_print(F("] for payload byte 0 (status) must be 0 or 1"));
    return 0;
  }
  if ((wr_nr[wr_n] == 1) && ((wr_val[wr_n] < 0x60) || (wr_val[wr_n] > 0x67))) {
    Serial_println(F("wr_val["));
    Serial_print(wr_n);
    Serial_print(F("] for payload byte 1 (operating-mode) must be in range 0x60-0x67"));
    return 0;
  }
  if (((wr_nr[wr_n] == 2) || (wr_nr[wr_n] == 6)) && ((wr_val[wr_n] < 0x0A) || (wr_val[wr_n] > 0x1E))) {
    Serial_println(F("wr_val["));
    Serial_print(wr_n);
    Serial_print(F("] for payload byte 2/6 (target-temp cooling/heating) must be in range 0x10-0x20"));
    return 0;
  }
  if (((wr_nr[wr_n] == 4) || (wr_nr[wr_n] == 8)) && (wr_val[wr_n] < 0x03)) {
    wr_val[wr_n] = 0x11 + (wr_val[wr_n] << 5); // 0x00 -> 0x11; 0x01 -> 0x31; 0x02 -> 0x51
    return 1;
  } else if (((wr_nr[wr_n] == 4) || (wr_nr[wr_n] == 8)) && ((wr_val[wr_n] < 0x11) || (wr_val[wr_n] > 0x51))) {
    Serial_println(F("wr_val["));
    Serial_print(wr_n);
    Serial_print(F("] for payload byte 4/8 (fan-speed cooling/heating) must be in range 0x11-0x51 or 0x00-0x02"));
    return 0;
  }
  // no limitations for wr_nr[wr_n] == 16
  if ((wr_nr[wr_n] == 17) && (wr_val[wr_n] > 0x03)) {
    Serial_println(F("wr_val["));
    Serial_print(wr_n);
    Serial_print(F("] for payload byte 17 (fan-mode) must be in range 0x00-0x03"));
    return 0;
  }
  if (writeBudget) {
    if (writeBudget != 255) writeBudget--;
    wr_cnt[wr_n] = WR_CNT; // write repetitions, must be 1 for S-series
    Serial_print(F("* Initiating write (part) "));
    Serial_print(wr_n);
    Serial_print(F(" for packet-type 0x"));
    Serial_print(wr_pt[wr_n], HEX);
    Serial_print(F(" payload byte "));
    Serial_print(wr_nr[wr_n]);
    Serial_print(F(" to value 0x"));
    Serial_print(wr_val[wr_n], HEX);
    Serial_println();
  } else {
    Serial_println(F("* Currently no write budget left"));
    if (writeRefusedBudget < 0xFF) writeRefusedBudget++;
    return 0;
  }
}

                                  //   30  31  32  33  34  35  36  37  38  39  3A  3B  3C  3D  3E  3F
const int8_t expectedLength[3][16] ={{ 20, -1, -1, -1, -1, -1, -1, -1, 16, 11, -1, -1, -1, -1, -1, -1 },   // model 10
                                     { 20, -1,  8, -1, -1, 19, 19, -1, 20, 14, 18, -1, -1, -1, -1, -1 },   // model 10
                                     { 20, -1, -1, -1, -1, -1, -1, -2, -1, -1, -1, 20, 12, -1, -1, -1 }};  // model 12
// -1: packet not expected
// -2: packet length unknown, or unsure if packet is expected
#endif /* F_SERIES */




#ifdef EF_SERIES

#define PCKTP_START  0x10
#define PCKTP_END    0x15 // 0x0D-0x15
#define PCKTP_ARR_SZ (2 * (PCKTP_END - PCKTP_START + 1))
const byte nr_bytes[PCKTP_ARR_SZ]     =
// 10,  11,  12,  13,  14,  15,  10,  11,  12,  13,  14,  15,
{  20,   8,  15,   3,  15,   6,  20,  20,  20,  14,  19,   6 };

const byte bytestart[PCKTP_ARR_SZ]     =
{   0,  20,  28,  43,  46,  61,  67,  87, 107, 127, 141, 160 /* , sizeValSeen = 166 */ };
#define sizeValSeen 166
byte payloadByteVal[sizeValSeen]  = { 0 };
byte packetSeen[PCKTP_ARR_SZ] = { 0 };
byte dedup = 0;

bool checkPacketDuplicate(const byte n) {
  bool duplicate = 1;
  byte pti = 0;
  if (!dedup) return 0;
  switch (RB[1]) {
    case 0x00 : break;
    default   : return 0;
  }
  switch (RB[2]) {
    case 0x10 ... 0x15 : pti = RB[2] - 0x10; break;
    default            : return 0;
  }
  switch (RB[0]) {
    case 0x00 : break;
    case 0x40 : pti += 6; break;
    default   : return 0;
  }
  if (n > nr_bytes[pti] + 4) return 0;
  if (packetSeen[pti]) {
    for (byte i = 3; i < n - 1; i++) {
      if (RB[i] != payloadByteVal[bytestart[pti] + i - 3]) {
        duplicate = 0;
        break;
      }
    }
  } else {
    duplicate = 0;
    packetSeen[pti] = 1;
  }
  for (byte i = 3; i < n - 1; i++) payloadByteVal[bytestart[pti] + i - 3] = RB[i];
  return duplicate;
}

void restartData() {
  for (byte i = 0; i < 12; i++) packetSeen[i] = 0;
}

#else

bool checkPacketDuplicate(const byte n) {
  return 0;
}

void restartData() {
}

#endif /* EF_SERIES */

void loop() {
  uint16_t temp;
  uint16_t temphex;
  int wb = 0;
  int n;
  int wbtemp;
  int c;
  static byte ignoreremainder = 2; // ignore first line from serial input to avoid misreading a partial message just after reboot
  static bool reportedTooLong = 0;
  static bool lengthReported[16] = { false };
  static bool wrongLengthReported[16] = { false };
  byte packetDuplicate = 0;

// if GPIO0 = PB4 = L, do nothing, and disable serial input/output, to enable ESP programming
// MISO GPIO0  pin 18 // PB4 // pull-up   // P=Power // white // DS18B20                                            V10/V11
// MOSI GPIO2  pin 17 // PB3 // pull-up   // R=Read  // green // low-during-programming // blue-LED on ESP12F       V12/V13
// CLK  GPIO15 pin 16 // PB5 // pull-down // W=Write // blue                                                        V14/V15
// RST         pin 1  // PC4 // pull-up   // E=Error // red   // reset-switch

  if (ATmegaHwID == 2) switch (suppressSerial) {
    case 0 : if ((PINB & 0x10) == 0) { // if GPIO0 == 0, disable serial, switch off power LED, and wait for GPIO15=0
               suppressSerial = 1;
               P1P2MQTT.ledPower(1);
               P1P2MQTT.ledError(0);
               Serial.println("* Pausing serial");
               Serial.end();
               break;
             }
             if ((PINB & 0x20) == 0) { // if GPIO15=0, go inactive, then wait for GPIO15=1
               suppressSerial = 2;
               P1P2MQTT.ledPower(0);
               P1P2MQTT.ledError(1);
               Serial.println("* Pausing serial");
               delay(10);
               Serial.end();
               break;
             }
             break;
    case 1 : // GPIO=0; wait for GPIO15=0
             // in this mode, power LED is on, error LED is off
             if ((PINB & 0x20) == 0) { // wait for GPIO15=0
               suppressSerial = 2;
               P1P2MQTT.ledError(1);
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
               P1P2MQTT.ledError(0);
               P1P2MQTT.ledPower(1);
               printWelcomeString(true);
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
/*
          Serial_print(F("* Line too long, ignored, ignoring remainder: ->"));
          Serial_print(RS);
          Serial_print(lst);
          Serial_print(c);
          Serial_println(F("<-"));
          // show full line, so not yet setting reportedTooLong = 1 here;
*/
        }
        ignoreremainder = 1;
      } else {
        if (!reportedTooLong) {
          Serial_println(F("* Line too long, terminated"));
/*
          Serial_print(F("* Line too long, terminated, ignored: ->"));
          Serial_print(RS);
          Serial_print(lst);
          Serial_println(F("<-"));
*/
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
        Serial_print(F("* First line received via serial is ignored: ->"));
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
#ifdef SERIAL_MAGICSTRING
        RSp = RS + strlen(SERIAL_MAGICSTRING) + 1;
        if (!strncmp(RS, SERIAL_MAGICSTRING, strlen(SERIAL_MAGICSTRING))) {
#else
        RSp = RS + 1;
        {
#endif
byte scanned = 0;
byte scannedTotal = 0;
byte wr_busy = 0;
          switch (*(RSp - 1)) { // in use by ESP: adhjps (v), in use by ATmega: cefgiklmnoqtuvwx, available: bryz
            case '\0': // Serial_println(F("* Empty line received"));
                      break;
            case '*': // Serial_print(F("* Received: "));
                      // Serial_println(RSp);
                      break;
#ifdef E_SERIES
            case 'e':
            case 'E': if (!CONTROL_ID) {
                        Serial_println(F("* Requires L1"));
                        break;
                      }
                      for (byte i = 0; i < WR_MAX; i++) wr_busy += wr_cnt[i];
                      if (wr_busy) {
                        // previous write still being processed
                        Serial_println(F("* Previous parameter write action(s) still busy"));
                        break;
                      }
                      wr_n = 0;
                      while ((wr_n < WR_MAX) && ((n = sscanf(RSp + scannedTotal, (const char*) "%2hhx%4x%8lx%n", &wr_pt[wr_n], &wr_nr[wr_n], &wr_val[wr_n], &scanned)) > 0)) {
                        if (n == 3) {
                          if (!writeParam()) {
                            // cancel scheduled writes (full or error)
                            wr_n = 0;
                            for (byte i = 0; i < WR_MAX; i++) wr_cnt[i]= 0;
                            break;
                          }
                          wr_n++;
                          scannedTotal += scanned;
                        } else {
                          Serial_print(F("* Ignoring (all) instruction(s), expected 3 arguments, received: "));
                          Serial_print(n);
                          if (n > 0) {
                            Serial_print(F(" pt: 0x"));
                            Serial_print(wr_pt[wr_n], HEX);
                          }
                          if (n > 1) {
                            Serial_print(F(" nr: 0x"));
                            Serial_print(wr_nr[wr_n], HEX);
                          }
                          Serial_println();
                          wr_n = 0;
                          for (byte i = 0; i < WR_MAX; i++) wr_cnt[i] = 0;
                          break;
                        }
                      }
                      break;
            case 'f':
            case 'F': Serial_print(F("* Room Temperature insertion function "));
                      {
                        int l;
                        int n;
                        if ((l = sscanf(RSp, "%hhd%n", &insertRoomTemperature, &n)) > 0) {
                          switch (insertRoomTemperature) {
                            case 0  : Serial_println(F("switched off"));
                                      EEPROM.update(EEPROM_ADDRESS_ROOM_TEMPERATURE_MSB, (roomTemperature >> 8) & 0x7F);
                                      break;
                            default : // fall-through
                            case 1  : Serial_print(F("switched on, temp "));
                                      if ((l = sscanf(RSp + n, "%d", &roomTemperature)) > 0) {
                                        roomTemperature &= 0x7FFF; // ignore most-significant bit, should be 0
                                        Serial_print(F("set to "));
                                        Serial_println(roomTemperature * 0.1);
                                        EEPROM.update(EEPROM_ADDRESS_ROOM_TEMPERATURE_LSB, roomTemperature & 0xFF);
                                      } else {
                                        Serial_print(F(" is "));
                                        Serial_println(roomTemperature * 0.1);
                                      }
                                      EEPROM.update(EEPROM_ADDRESS_ROOM_TEMPERATURE_MSB, (roomTemperature >> 8) /* & 0x7F not needed */ | 0x80);
                                      break;
                          }
                          break;
                        }
                        if (insertRoomTemperature) {
                          Serial_print(F("on: "));
                        } else {
                          Serial_print(F("off: "));
                        }
                        Serial_println(roomTemperature * 0.1);
                      }
                      break;
            case 'q':
            case 'Q': if (CONTROL_ID) {
                        Serial_println(F("* Counter cycle stealing should not be necessary in L1 mode, refusing command, use C2 instead"));
                        break;
                      }
                      {
                        int l;
                        int n;
                        if ((l = sscanf(RSp, "%hhd%n", &counterCycleStealDelay, &n)) > 0) {
                          if (counterCycleStealDelay) {
                            if (counterRepeatingRequest) {
                              Serial_println(F("* Repetitive requesting of counter values already active, will not enable counter cycle stealing"));
                              break;
                            }
                            if (counterRequest && !counterCycleStealDelay) {
                              Serial_println(F("* Previous single counter request not finished yet"));
                              break;
                            }
                            Serial_print(F("* Counter cycle stealing will be active with delay set to (recommended: 9) "));
                            Serial_println(counterCycleStealDelay);
                            Serial_println(F("* Setting errorsPermitted to 2 for safety"));
                            errorsPermitted = 2;
                            counterRequest = 1;
                          } else {
                            Serial_println(F("* Counter cycle stealing will be switched off"));
                            Serial_println(F("* Setting errorsPermitted back to initial value"));
                            errorsPermitted = INIT_ERRORS_PERMITTED;
                            counterRequest = 0;
                          }
                          EEPROM.update(EEPROM_ADDRESS_COUNTER_CYCLE_STEAL_DELAY, counterCycleStealDelay);
                          break;
                        }
                        if (counterCycleStealDelay) {
                          Serial_print(F("* Counter cycle stealing is active with delay "));
                          Serial_println(counterCycleStealDelay);
                        } else {
                          Serial_println(F("* Counter cycle stealing is switched off"));
                        }
                      }
                      break;
#else
            case 'e':
            case 'E': Serial_print(F("* Error mask (Hitachi default 0x3B, PE=+0x04, UC=+0x40, other brands 0x7F) "));
                      if (scanhex(RSp, temp) == 1) {
                        errorMask = temp & 0x7F;
                        P1P2MQTT.setErrorMask(errorMask);
                        Serial_print(F("set to "));
                        EEPROM.update(EEPROM_ADDRESS_ERROR_MASK, errorMask);
                      }
                      Serial_print("0x");
                      if (errorMask < 0x10) Serial_print('0');
                      Serial_println(errorMask, HEX);
                      break;
#endif /* E_SERIES */
#ifdef F_SERIES
            case 'f':
            case 'F': if (!(CONTROL_ID && controlLevel)) {
                        Serial_println(F("* Command requires operation as auxiliary controller (L1)"));
                        break;
                      }
                      for (byte i = 0; i < WR_MAX; i++) wr_busy += wr_cnt[i];
                      if (wr_busy) {
                        // previous write still being processed
                        Serial_println(F("* Previous parameter write action(s) still busy"));
                        break;
                      }
                      wr_n = 0;
                      while ((wr_n < WR_MAX) && ((n = sscanf(RSp + scannedTotal, (const char*) "%2hhx%2hhx%2hhx%n", &wr_pt[wr_n], &wr_nr[wr_n], &wr_val[wr_n], &scanned)) > 0)) {
                        // Valid write parameters
                        // Model 10 BCL FDY  38 0 1 2 4 6 8
                        // Model 11 LPA FXMQ 38 0 1 2 4 6 8
                        // Model 12 M   FDYQ 3B 0 1 2 4 6 8 16 17
                        //
                        // check wr_pt
                        if (n != 3) {
                          Serial_print(F("* Ignoring (all) instruction(s), expected 3 arguments, received: "));
                          Serial_print(n);
                          if (n > 0) {
                            Serial_print(F(" pt: 0x"));
                            Serial_print(wr_pt[wr_n], HEX);
                          }
                          if (n > 1) {
                            Serial_print(F(" nr: 0x"));
                            Serial_print(wr_nr[wr_n], HEX);
                          }
                          Serial_println();
                          wr_n = 0;
                          break;
                        } else {
                          if (!writeParam()) {
                            // cancel scheduled writes (full or error)
                            wr_n = 0;
                            for (byte i = 0; i < WR_MAX; i++) wr_cnt[i]= 0;
                            break;
                          }
                          wr_n++;
                          scannedTotal += scanned;
                        }
                      }
                      break;
#endif /* F_SERIES */
#ifdef MHI_SERIES
            case 'g':
            case 'G': Serial.print(F("* Checksum generation on/off "));
                      if (scanint(RSp, temp) == 1) {
                        cs_gen = temp ? 1 : 0;
                        Serial_print(F("set to "));
                      }
                      Serial.println(cs_gen);
                      break;
            case 'm':
            case 'M': Serial.print(F("* MHI format translation 3-to-1 on/off "));
                      if (scanint(RSp, temp) == 1) {
                        mhiFormat = temp ? 1 : 0;
                        P1P2MQTT.setMHI(mhiFormat);
                        EEPROM.update(EEPROM_ADDRESS_MHI_FORMAT, mhiFormat);
                        Serial.print(F("set to "));
                      }
                      Serial.println(mhiFormat);
                      break;
#endif /* MHI_SERIES */
            case 'v':
            case 'V': printWelcomeString(false);
                      break;
            case 't':
            case 'T': Serial_print(F("* Delay "));
                      if (scanint(RSp, sd) == 1) {
                        if (sd < 2) {
                          sd = 2;
                          Serial_print(F("[use of delay 0 or 1 not recommended, increasing to 2] "));
                        }
                        Serial_print(F("set to "));
                        pseudo0E = 9;
                      }
                      Serial_println(sd);
                      break;
            case 'o':
            case 'O': Serial_print(F("* DelayTimeout "));
                      if (scanint(RSp, sdto) == 1) {
                        P1P2MQTT.setDelayTimeout(sdto);
                        Serial_print(F("set to "));
                        pseudo0E = 9;
                      }
                      Serial_println(sdto);
                      break;
#ifdef SW_SCOPE
            case 'u':
            case 'U': Serial_print(F("* Software-scope "));
                      if (scanint(RSp, temp) == 1) {
                        scope = temp;
                        pseudo0E = 9;
                        if (scope > 1) scope = 1;
                        P1P2MQTT.setScope(scope);
                        Serial_print(F("set to "));
                      }
                      Serial_println(scope);
                      scope_budget = 200;
                      break;
#endif
#if defined MHI_SERIES || defined TH_SERIES
            case 'x':
            case 'X': Serial_print(F("* Allow (bits pause) "));
                      if (scanint(RSp, temp) == 1) {
                        allow = temp;
                        if (allow > 76) allow = 76;
                        P1P2MQTT.setAllow(allow);
                        Serial_print(F("set to "));
                        EEPROM.update(EEPROM_ADDRESS_ALLOW, allow);
                      }
                      Serial_println(allow);
                      break;
#else /* MHI_SERIES || TH_SERIES */
            case 'x':
            case 'X': Serial_print(F("* Echo "));
                      if (scanint(RSp, temp) == 1) {
                        if (temp) temp = 1;
                        echo = temp;
                        P1P2MQTT.setEcho(echo);
                        Serial_print(F("set to "));
                      }
                      Serial_println(echo);
                      break;
#endif /* MHI_SERIES || TH_SERIES */
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
                        Serial_print(F("* Writing next 40Fx30 slot: "));
                        insertMessageLength = 0;
                        restartDaikinReady = 0; // to indicate insertMessage is overwritten and restart cannot be done until new packet received/loaded
                        while ((wb < RB_SIZE) && (wb < WB_SIZE) && (sscanf(RSp, (const char*) "%2x%n", &wbtemp, &n) == 1)) {
                          insertMessage[insertMessageLength++] = wbtemp;
                          insertMessageCnt = 1;
                          RSp += n;
                          {
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
                      // for non-Daikin systems, and for Daikin systems in L0 mode, just schedule a packet write, use with care
                      Serial_print(F("* Scheduling write: "));
                      wb = 0;
                      while ((wb < WB_SIZE) && (sscanf(RSp, (const char*) "%2x%n", &wbtemp, &n) == 1)) {
                        WB[wb++] = wbtemp;
                        RSp += n;
                        {
                          if (wbtemp <= 0x0F) Serial_print("0");
                          Serial_print(wbtemp, HEX);
                        }
                      }
                      Serial_println();
                      if (P1P2MQTT.writeready()) {
                        if (wb) {
#ifdef MHI_SERIES
                          P1P2MQTT.writepacket(WB, wb, sd, cs_gen);
#else /* MHI_SERIES */
                          P1P2MQTT.writepacket(WB, wb, sd, CRC_GEN, CRC_CS_FEED);
#endif /* MHI_SERIES */
                        } else {
                          Serial_println(F("* Refusing to write empty packet"));
                        }
                      } else {
                        Serial_println(F("* Refusing to write packet while previous packet wasn't finished"));
                        if (writeRefusedBusy < 0xFF) writeRefusedBusy++;
                      }
                      break;
            case 'k': // soft-reset ESP
            case 'K': Serial_println(F("* Resetting ATmega ...."));
                      resetFunc(); // call reset
                      break;
#ifdef EF_SERIES
            case 'i':
            case 'I': Serial_print(F("* Write budget increment period "));
                      if (scanint(RSp, temp) == 1) {
                        if (temp > 255) {
                          Serial_print(F("(max 255) "));
                          temp = 255;
                        }
                        if (temp < 15) {
                          Serial_print(F("(min 15) "));
                          temp = 15;
                        }
                        writeBudgetPeriod = temp;
                        Serial_print(F("set to "));
                        EEPROM.update(EEPROM_ADDRESS_WRITE_BUDGET_PERIOD, writeBudgetPeriod);
                        pseudo0F = 9;
                      } else {
                        Serial_print(F("is "));
                      }
                      Serial_print(writeBudgetPeriod);
                      Serial_println(" minutes");
                      break;
            case 'n':
            case 'N': Serial_print(F("* Current write budget "));
                      if (scanint(RSp, temp) == 1) {
                        if (temp > 255) {
                          Serial_print(F("(max 255 = unlimited) "));
                          temp = 255;
                        }
                        writeBudget = temp;
                        Serial_print(F("and initial write budget set to "));
                        EEPROM.update(EEPROM_ADDRESS_INITIAL_WRITE_BUDGET, writeBudget);
                        pseudo0F = 9;
                      } else {
                        Serial_print(F("is "));
                      }
                      if (writeBudget == 255) {
                        Serial_println(F("unlimited"));
                      } else {
                        Serial_println(writeBudget);
                      }
                      break;
#endif /* EF_SERIES */
#ifdef F_SERIES
            case 'm': // set Daikin model type (currently for F-series only)
            case 'M': if (scanint(RSp, temp) == 1) {
                        if (((temp >= 10) && (temp <= 12)) || (temp == 1)) {
                          Serial_print(F("* F_series model will be set to "));
                          model = temp;
                          pseudo0E = 9;
                          EEPROM.update(EEPROM_ADDRESS_MODEL, model);
                          Serial_print(model);
                          switch (model) {
                            case  1 : Serial_println(F(": Unknown F-series model"));
                                      break;
                            case 10 : Serial_println(F(": FDY / version B/C/L"));
                                      break;
                            case 11 : Serial_println(F(": FXMQ / version LA/P/PA/A"));
                                      break;
                            case 12 : Serial_println(F(": FDYQ / version M"));
                                      break;
                            default : Serial_println(F(": unknown/should not happen"));
                                      break;
                           }
                        } else {
                          Serial_println(F("* Model can (only) be set to 1 (F unknown), 10 (FDY B/C/L), 11 (FXMQ L/P/A), or 12 (FDYQ M) (enter M1, M10, M11 or M12 to do so)"));
                        }
                      } else {
                        Serial_println(F("* Model can be set to 1 (F unknown), 10 (FDY B/C/L), 11 (FXMQ L/P/A), or 12 (FDYQ M) (enter M1, M10, M11 or M12 to do so)"));
                      }
                      Serial_print(F("* Brand is "));
                      Serial_print(brand);
                      switch (brand) {
                        case  1 : Serial_println(F(": Daikin"));
                                  break;
                        default : Serial_println(F(": unknown/should not happen"));
                                  break;
                      }
                      Serial_print(F("* Model is "));
                      Serial_print(model);
                      switch (model) {
                        case  1 : Serial_println(F(": Unknown F-series model"));
                                  break;
                        case 10 : Serial_println(F(": FDY / version B/C/L"));
                                  break;
                        case 11 : Serial_println(F(": FXMQ / version L/P/A"));
                                  break;
                        case 12 : Serial_println(F(": FDYQ / version M"));
                                  break;
                        default : Serial_println(F(": unknown/should not happen"));
                                  break;
                      }
                      break;
#endif /* F_SERIES */
#ifdef EF_SERIES
            case 'l': // set auxiliary controller function on/off; CONTROL_ID address is set automatically; Daikin-specific
            case 'L': // 0 controller mode off, and store setting in EEPROM // 1 controller mode on, and store setting in EEPROM
                      // 2 controller mode off, do not store setting in EEPROM
                      // 3 controller mode on, do not store setting in EEPROM
                      // 5 (for experimenting with F-series only) controller responds only to 00F030 message with an empty packet, but not to other 00F03x packets
                      // 99 restart Daikin
                      // other values are treated as 0 and are reserved for further experimentation of control modes
                      if (scanint(RSp, temp) == 1) {
                        switch (temp) {
                          case 96 : Serial_println(F("* dedup off"));
                                    dedup = 0;
                                    break;
                          case 97 : Serial_println(F("* dedup on"));
                                    dedup = 1;
                                    break;
                          case 98 : Serial_println(F("* restart data"));
                                    restartData();
                                    break;
#ifdef ENABLE_INSERT_MESSAGE
                          case 99 : if (compressor) {
                                      Serial_println(F("* restart refused, because compressor is on"));
                                      break;
                                    }
                                    if (insertMessageCnt) {
                                      Serial_println(F("* insertMessage already scheduled, retry later"));
                                      break;
                                    }
                                    if (restartDaikinCnt) {
                                      Serial_println(F("* restartDaikin already scheduled"));
                                      break;
                                    }
                                    if (!restartDaikinReady) {
                                      Serial_println(F("* restartDaikin waiting to receive sample payload, retry later"));
                                      break;
                                    }
                                    restartDaikinCnt = RESTART_NR_MESSAGES;
                                    Serial_println(F("* Scheduling attempt to restart Daikin"));
                                    break;
#endif /* ENABLE_INSERT_MESSAGE */
                          case  0 :
                          case  1 :
                          case  2 :
                          case  3 :
                          case  5 : if (temp & 0x01) {
                                      if (errorsPermitted < MIN_ERRORS_PERMITTED) {
                                        Serial_println(F("* Errorspermitted (error budget) too low; control not enabled"));
                                        break;
                                      }
                                      if (CONTROL_ID) {
                                        Serial_print(F("* CONTROL_ID is 0x"));
                                        Serial_print(CONTROL_ID, HEX);
                                        Serial_print(F(", changing control mode to L"));
                                        Serial_println(temp);
                                        if (temp < 2) EEPROM.update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
                                        controlLevel = (temp == 5) ? 0 : 1; // F-series: special mode L5 answers only to F030
                                        break;
                                      }
                                      if (FxAbsentCnt[0] == F0THRESHOLD) {
                                        Serial_println(F("* Control_id 0xF0 supported and free, starting auxiliary controller"));
                                        CONTROL_ID = 0xF0;
                                        controlLevel = (temp == 5) ? 0 : 1; // F-series: special mode L5 answers only to F030
                                        if (temp < 2) EEPROM.update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID); // L2/L3/L5 for short experiments, doesn't survive ATmega reboot !
#ifdef E_SERIES
                                      } else if (FxAbsentCnt[1] == F0THRESHOLD) {
                                        Serial_println(F("* Control_id 0xF1 supported, no auxiliary controller found for 0xF1, switching control functionality on 0xF1 on"));
                                        CONTROL_ID = 0xF1;
                                        if (temp < 2) EEPROM.update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
                                      } else if (FxAbsentCnt[3] == F0THRESHOLD) {
                                        Serial_println(F("* Control_id 0xFF supported, no auxiliary controller found for 0xFF, but control on 0xFF not supported (yet?)"));
#endif /* E_SERIES */
                                      } else {
                                        Serial_println(F("* No free address for controller found (yet). Control functionality not enabled"));
                                        Serial_println(F("* You may wish to re-try in a few seconds (and perhaps switch other auxiliary controllers off)"));
                                      }
                                    } else {
                                      if (!CONTROL_ID) {
                                        Serial_println(F("* CONTROL_ID is already 0x00"));
                                        if (temp < 2) EEPROM.update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
                                        break;
                                      } else {
                                        CONTROL_ID = 0x00;
                                        for (byte i = 0; i < WR_MAX; i++) wr_cnt[i] = 0;
                                      }
                                      if (temp < 2) EEPROM.update(EEPROM_ADDRESS_CONTROL_ID, CONTROL_ID);
                                    }
                                    Serial_print(F("* Control_id set to 0x"));
                                    if (CONTROL_ID <= 0x0F) Serial_print("0");
                                    Serial_println(CONTROL_ID, HEX);
                                    break;
                          default : Serial_println(F("* Valid arguments: 0 (control mode off, store state in EEPROM), 1 (control mode on, store state in EEPROM),"));
                                    Serial_println(F("*   2 (control mode off, do not store state in EEPROM), 3 (control mode on, do not store state in EEPROM),"));
#ifdef F_SERIES
                                    Serial_println(F("*   5 (partial control mode on, do not store state in EEPROM)"));
                                    Serial_println(F("*   Control mode is limited for model 1 (F generic)"));
#endif /* F_SERIES */
                                    break;
                        }
                      } else {
                        Serial_print(F("* Control_id is 0x"));
                        if (CONTROL_ID <= 0x0F) Serial_print("0");
                        Serial_println(CONTROL_ID, HEX);
                        if (CONTROL_ID) {
                          if (controlLevel) {
                            if (model > 1) {
                              Serial_println(F("* Control mode is on (L1) "));
                            } else {
                              Serial_println(F("* Control mode is on (L1) but limited because model=1, use M command to set Daikin model"));
                            }
                          } else {
                            Serial_println(F("* Control mode is limited (L5 mode), replying only to 00F030*"));
                          }
                        } else {
                          Serial_println(F("* Control mode is off"));
                        }
                        Serial_println(F("* Valid arguments: 0 (control mode off, store state in EEPROM), 1 (control mode on, store state in EEPROM),"));
                        Serial_println(F("*   2 (control mode off, do not store state in EEPROM), 3 (control mode on, do not store state in EEPROM),"));
#ifdef F_SERIES
                        Serial_println(F("*   5 (partial control mode on, do not store state in EEPROM)"));
                        Serial_println(F("*   Control mode is limited for model 1 (F generic)"));
#endif /* F_SERIES */
                      }
                      break;
#endif /* EF_SERIES */
#ifdef E_SERIES
            case 'c': // set counterRequest cycle (once or repetitive)
            case 'C': if (scanint(RSp, temp) == 1) {
                        if (temp) {
                          if ((FxAbsentCnt[0] != F0THRESHOLD) && (FxAbsentCnt[1] != F0THRESHOLD) && (FxAbsentCnt[3] != F0THRESHOLD)) {
                            Serial_println(F("* No free controller slot found (yet). Counter functionality will not enabled"));
                            break;
                          }
                          if (errorsPermitted < MIN_ERRORS_PERMITTED) {
                            Serial_println(F("* Errorspermitted too low; counter functionality will not enabled"));
                            break;
                          }
                          if (counterCycleStealDelay) {
                            Serial_println(F("* Counter cycle steal active, regular counter functionality will not enabled"));
                            break;
                          }
                        }
                        if (temp > 2) {
                          if (counterRepeatingRequest) {
                            Serial_println(F("* Repetitive requesting of counter values already active, no need to trigger request"));
                            break;
                          }
                          if (counterRequest) {
                            Serial_println(F("* Previous single counter request not finished yet"));
                            break;
                          }
                          counterRequest = 1;
                          Serial_println(F("* Single counter request cycle initiated"));
                        } else if (temp == 2) {
                          if (counterRepeatingRequest) {
                            Serial_println(F("* Repetitive requesting of counter values was already active"));
                            break;
                          }
                          counterRepeatingRequest = 1;
                          Serial_println(F("* Repetitive requesting of counter values initiated"));
                          EEPROM.update(EEPROM_ADDRESS_COUNTER_STATUS, counterRepeatingRequest);
                        } else if (temp == 1) {
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
                              Serial_println(F("* Previous single counter request not finished yet"));
                            } else {
                              counterRequest = 1;
                              Serial_println(F("* Single counter request cycle initiated"));
                            }
                          }
                        } else { // temp == 0
                          counterRepeatingRequest = 0;
                          counterRequest = 0;
                          Serial_println(F("* All counter-requests stopped"));
                          EEPROM.update(EEPROM_ADDRESS_COUNTER_STATUS, counterRepeatingRequest);
                        }
                      } else {
                        Serial_print(F("* Counter_requests is "));
                        Serial_println(counterRepeatingRequest);
                      }
                      break;
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
  int32_t upt = P1P2MQTT.uptime_sec();
  if (upt > upt_prev_pseudo) {
    pseudo0E++;
    pseudo0F++;
    upt_prev_pseudo = upt;
  }
#ifdef EF_SERIES
  if (upt >= upt_prev_write + 60 * writeBudgetPeriod) {
    if (writeBudget < MAX_WRITE_BUDGET) writeBudget++;
    upt_prev_write += 60 * writeBudgetPeriod;
  }
  if (upt >= upt_prev_error + TIME_ERRORS_PERMITTED) {
    if (errorsPermitted < MAX_ERRORS_PERMITTED) errorsPermitted++;
    upt_prev_error += TIME_ERRORS_PERMITTED;
  }
#endif /* EF_SERIES */
  while (P1P2MQTT.packetavailable()) {
    uint16_t delta;
    errorbuf_t readError = 0;
#ifdef MHI_SERIES
    int nread = P1P2MQTT.readpacket(RB, delta, EB, RB_SIZE, cs_gen);
#else /* MHI_SERIES */
    int nread = P1P2MQTT.readpacket(RB, delta, EB, RB_SIZE, CRC_GEN, CRC_CS_FEED);
#endif /* MHI_SERIES */
    if (nread > RB_SIZE) {
      Serial_println(F("* Received packet longer than RB_SIZE"));
      nread = RB_SIZE;
      readError = 0xFF;
    }
    for (int i = 0; i < nread; i++) readError |= EB[i];
    if (skipPackets) {
      if ((skipPackets == SKIP_PACKETS) && !delta) {
        Serial_print(F("* Always skipping first packet if delta == 0. readerror = 0x"));
        Serial_println(readError, HEX);
        skipPackets--;
        break;
      } else if (readError) {
        Serial_print(F("* Skipping initial packet with readerror = 0x"));
        Serial_println(readError, HEX);
        skipPackets--;
        break;
      } else {
        skipPackets = 0;
      }
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
      // always keep scope budget for 40F0 and expecially for readErrors
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
    if (readError && upt) { // don't count errors while upt == 0
      if (readErrors < 0xFF) {
        readErrors++;
        readErrorLast = readError;
      }
#ifdef EF_SERIES
      if (errorsPermitted) {
        errorsPermitted--;
        if (!errorsPermitted) {
          Serial_println(F("* WARNING: too many read errors detected"));
          stopControlAndCounters(1);
        }
      }
#endif /* EF_SERIES */
    }
#ifdef MONITORCONTROL
    if (!readError) {
      // message received, no error detected, no buffer overrun
      bool F030forcounter = false;
#ifdef E_SERIES
      if ((nread > 9) && (RB[0] == 0x00) && (RB[1] == 0x00) && (RB[2] == 0x12)) {
        // obtain day-of-week, hour, minute
        Tmin = RB[6];
        if (Tmin != Tminprev) {
          if ((counterRepeatingRequest || counterCycleStealDelay) && !counterRequest) counterRequest = 1;
          Tminprev = Tmin;
        }
      }
      if ((RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == 0x10)) {
        compressor = RB[21] & 0x01;
      }

      if (counterCycleStealDelay) {
        // request one counter per cycle in short pause after first 0012 msg at start of each minute
        if ((nread > 4) && (RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == 0x12)) {
          if (counterRequest) {
            WB[0] = 0x00;
            WB[1] = 0x00;
            WB[2] = 0xB8;
            WB[3] = (counterRequest - 1);
            if (P1P2MQTT.writeready()) {
              // write counterCycleStealDelay ms after 400012 message
              // pause after 400012 is around 47 ms for some systems which is long enough for a 0000B8*/4000B8* counter request/response pair
              // in exceptional cases (1 in 300 in my system) the pause after 400012 is only 27ms,
              //      in which case the 4000B* reply arrives after the 000013* request
              //      (and in thoses cases the 000013* request is ignored)
              //      (NOTE!: if counterCycleStealDelay is chosen incorrectly, such as 5 ms in some example systems, this results in incidental bus collisions)
              P1P2MQTT.writepacket(WB, 4, counterCycleStealDelay, CRC_GEN, CRC_CS_FEED);
            } else {
              Serial_println(F("* Refusing to write counter-request packet while previous packet wasn't finished"));
              if (writeRefusedBusy < 0xFF) writeRefusedBusy++;
            }
            if (++counterRequest == 7) counterRequest = 0; // wait until next new minute; change 0 to 1 to continuously request counters for increased resolution
          }
        }
      } else {
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
          // non-empty 00Fx30 request message received, slot has no other aux controller active, and this is the last 00Fx30 request message in this cycle
          // For CONTROL_ID != 0 and F_max==1, use 1-in-4 mechanism
          if (div4) {
            div4--;
          } else {
            WB[0] = 0x00;
            WB[1] = 0x00;
            WB[2] = 0xB8;
            WB[3] = (counterRequest - 1);
            F030forcounter = true;
            if (P1P2MQTT.writeready()) {
              P1P2MQTT.writepacket(WB, 4, F03XDELAY, CRC_GEN, CRC_CS_FEED);
            } else {
              Serial_println(F("* Refusing to write counter-request packet while previous packet wasn't finished"));
              if (writeRefusedBusy < 0xFF) writeRefusedBusy++;
            }
            if (++counterRequest == 7) counterRequest = 0;
            if (CONTROL_ID && (F_max == 1)) div4 = 3;
          }
        }
      }
#endif /* E_SERIES */
      if ((nread > 4) && (RB[0] == 0x40) && (((RB[1] & 0xFE) == 0xF0) || (RB[1] == 0xFF)) && ((RB[2] & 0x30) == 0x30)) {
        // non-empty 40Fx3x auxiliary controller reply received - note this could be our own (slow, delta=F030DELAY or F03XDELAY) reply so only reset count if delta < F03XDELAY/F030DELAY (- margin)
        // Note for developers using >1 P1P2Monitor-interfaces (=to self): this detection mechanism fails if there are 2 P1P2Monitor programs (and adapters) with same delay settings on the same bus.
        // check if there is any other auxiliary controller on 0x3x:
        if (((RB[2] == 0x30) && (delta < F030DELAY - 2)) || ((RB[2] != 0x30) && (delta < F03XDELAY - 2))) {
          FxAbsentCnt[RB[1] & 0x03] = 0;
          if (RB[1] == CONTROL_ID) {
            // this should only happen if another auxiliary controller is connected if/after CONTROL_ID is set
            Serial_print(F("* Warning: another aux controller answering to address 0x"));
            Serial_print(RB[1], HEX);
            Serial_println(F(" detected"));
            stopControlAndCounters(0);
          }
        }
      } else if ((nread > 4) && (RB[0] == 0x00) && ((RB[1] & 0xFE) == 0xF0) && ((RB[2] & 0x30) == 0x30) && !F030forcounter)  {
        // non-empty 00Fx3x (F0/F1, but not FF, FF is not yet supported) request message received, and we did not use this slot to request counters
        // check if there is no other auxiliary controller
        // TODO should we detect and report empty payload messages?
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
#ifdef E_SERIES
              Serial_println(F(" detected, switching control functionality can be switched on (using L command, L1 for full control)"));
#else /* E_SERIES */
              Serial_println(F(" detected, switching control functionality can be switched on (using L command, L1 for full control, L5 for partial control, subject to model selection (M command))"));
#endif /* E_SERIES */
            }
          }
        }
#ifdef F_SERIES
        // check and report packet length
        if (((RB[2] & 0xF0) == 0x30) && !lengthReported[RB[2] & 0x0F]) {
          if (RB[2] == 0x38) {
            if ((nread == 16 + 4) && (model != 10)) {
              Serial_println(F("* Packet type 0x38 has payload length 16; perhaps this unit is compatible with model 10 (FDY / BCL), you may try command M10"));
              modelSuggestion = 10;
              lengthReported[8] = true;
            } else if ((nread == 20 + 4)  && (model != 11)) {
              Serial_println(F("* Packet type 0x38 has payload length 20; perhaps this unit is compatible with model 11 (FXMQ / LPA), you may try command M11"));
              modelSuggestion = 11;
              lengthReported[8] = true;
            }
          }
          if (RB[2] == 0x3B) {
            if ((nread == 20 + 4) && (model != 12)) {
              Serial_println(F("* Packet type 0x3B has payload length 20; perhaps this unit is compatible with model 12 (FDYQ / M), you may try command M12"));
              modelSuggestion = 12;
              lengthReported[11] = true;
            }
          }
          if (!lengthReported[RB[2] & 0x0F]) {
            Serial_print(F("* Packet type 0x"));
            Serial_print(RB[2], HEX);
            Serial_print(F(" observed with payload length "));
            Serial_println(nread - 4);
            lengthReported[RB[2] & 0x0F] = true;
          }
        }
        if ((model >= 10) && (model <= 12) && ((RB[2] & 0xF0) == 0x30) && !wrongLengthReported[RB[2] & 0x0F]) {
          int8_t expLength = expectedLength[model - 10][RB[2] & 0x0F];
          if (expLength == -1) {
            Serial_print(F("* Packet type 0x"));
            Serial_print(RB[2], HEX);
            Serial_print(F(" not expected for model "));
            Serial_println(model);
            wrongLengthReported[RB[2] & 0x0F] = true;
          }
          if ((expLength >= 0)  && ((nread - 4) != expLength)) {
            Serial_print(F("* Packet length "));
            Serial_print(nread - 4);
            Serial_print(F(" not expected for packet type 0x"));
            Serial_print(RB[2], HEX);
            Serial_print(F(" and model "));
            Serial_println(model);
            wrongLengthReported[RB[2] & 0x0F] = true;
          }
        }
#endif /* F_SERIES */
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
          byte w;
          if (CRC_GEN) n--; // omit CRC from received-byte-counter
          if (n > WB_SIZE) {
            n = WB_SIZE;
            Serial_print(F("* Surprise: received 00Fx3x packet of size "));
            Serial_println(nread);
          }
#ifdef E_SERIES
          switch (RB[2]) {
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
                        // set byte WB[<wr_pt - 0x2E>] to 0x01 to request a F03x message to set wr_nr to wr_val
                        for (byte i = 0; i < wr_n; i++) if (wr_cnt[i]) WB[wr_pt[i] - 0x2E] = 0x01;
                        if (insertRoomTemperature) WB[0x36 - 0x2E] = 0x01;
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
            case 0x35 : // fall-through
            case 0x3A : // in: 21 byte; out 21 byte; 3-byte parameters reply with FF
                        // LAN adapter replies first packet which starts with 930000 (why?)
                        // A parameter consists of 3 bytes: 2 bytes for param nr, and 1 byte for value
                        // parameters in the 00F035 message may indicate status changes in the heat pump (parameters 0144- or 0162- ASCII name of device)
                        // parameters in the 00F03A message may indicate system status (silent, schedule, unit, DST, holiday)
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        for (byte i = 0; i < wr_n; i++) {
                          if (wr_cnt[i] && (wr_pt[i] == RB[2])) {
                             WB[w++] = wr_nr[i] & 0xFF;
                             WB[w++] = wr_nr[i] >> 8;
                             WB[w++] = (wr_val[i] & 0xFF);
                          }
                          if (w >= n) break; // max 6 writes in 0x35/0x3A
                        }
                        wr = 1;
                        break;
            case 0x36 : // in: 23 byte; out 23 byte; 2-byte parameters; reply with FF
                        // A parameter consists of 4 bytes: 2 bytes for param nr, and 2 bytes for value
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;

                        if (insertRoomTemperature) {
                          WB[w++] = 0x02; // param 0x0002
                          WB[w++] = 0x00;
                          WB[w++] =  roomTemperature        & 0xFF;
                          WB[w++] = (roomTemperature >> 8 ) & 0x7F;
                        }
                        for (byte i = 0; i < wr_n; i++) {
                          if ((wr_cnt[i])  && (wr_pt[i]  == RB[2]) && ((!insertRoomTemperature) || (wr_nr[i] != 0x0002))) {
                            WB[w++] = wr_nr[i] & 0xFF;
                            WB[w++] = wr_nr[i] >> 8;
                            WB[w++] = wr_val[i] & 0xFF;
                            WB[w++] = (wr_val[i] >> 8) & 0xFF;
                          }
                          if (w >= n) {
                            if (i + 1 < wr_n) Serial_println(F("* >5 writes in 0x36 not possible, skipping some"));
                            break; // max 5 writes in 0x36/0x3B
                          }
                        }
                        wr = 1;
                        break;
            case 0x3B : // in: 23 byte; out 23 byte; 2-byte parameters; reply with FF
                        // A parameter consists of 4 bytes: 2 bytes for param nr, and 2 bytes for value
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        for (byte i = 0; i < wr_n; i++) {
                          if ((wr_cnt[i])  && (wr_pt[i]  == RB[2])) {
                            WB[w++] = wr_nr[i] & 0xFF;
                             WB[w++] = wr_nr[i] >> 8;
                             WB[w++] = wr_val[i] & 0xFF;
                             WB[w++] = (wr_val[i] >> 8) & 0xFF;
                          }
                          if (w >= n) break; // max 5 writes in 0x36/0x3B
                        }
                        wr = 1;
                        break;
            case 0x37 : // fall-through
            case 0x3C : // in: 23 byte; out 23 byte; 3-byte parameters; reply with FF
                        // A parameter consists of 4 bytes: 2 bytes for param nr, and 3 bytes for value
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        for (byte i = 0; i < wr_n; i++) {
                          if (wr_cnt[i] && (wr_pt[i] == RB[2])) {
                            WB[w++] = wr_nr[i] & 0xFF;
                             WB[w++] = wr_nr[i] >> 8;
                             WB[w++] = wr_val[i] & 0xFF;
                             WB[w++] = (wr_val[i] >> 8) & 0xFF;
                             WB[w++] = (wr_val[i] >> 16) & 0xFF;
                          }
                          if (w >= n) break; // max 4 writes in 0x37/0x3C
                        }
                        wr = 1;
                        break;
            case 0x38 : // fall-through
            case 0x39 : // fall-through
            case 0x3D : // in: 21 byte; out: 21 byte; 4-byte parameters; reply with FF
                        // parameters in the 00F038 range are kwH/hour/start counters
                        // parameters in the 00F039 range are field settings
                        // parameters in the 00F03D range are ?
                        // A parameter consists of 6 bytes: 2 bytes for param nr, and 4 bytes for value
                        for (w = 3; w < n; w++) WB[w] = 0xFF;
                        w = 3;
                        for (byte i = 0; i < wr_n; i++) {
                          if (wr_cnt[i] && (wr_pt[i] == RB[2])) {
                            WB[w++] = wr_nr[i] & 0xFF;
                            WB[w++] = wr_nr[i] >> 8;
                            WB[w++] = wr_val[i] & 0xFF;
                            WB[w++] = (wr_val[i] >> 8) & 0xFF;
                            WB[w++] = (wr_val[i] >> 16) & 0xFF;
                            WB[w++] = (wr_val[i] >> 24) & 0xFF;
                          }
                          if (w >= n) break; // max 3 writes in 0x38/0x39/0x3D
                        }
                        wr = 1;
                        break;
            case 0x3E : // schedule related packet
                        // 0x3E01, 0x3E02, ... in: 23 byte; out: 23 byte; out 40F13E01(even for higher) + 19xFF
                        WB[3] = RB[3];
                        for (w = 4; w < n; w++) WB[w] = 0xFF;
                        wr = 1;
                        break;
            default   : // 0x31/0x33/0x34/0x3F not seen, no response
                        break;
          }
          if (wr) {
            if (P1P2MQTT.writeready()) {
              P1P2MQTT.writepacket(WB, n, d, CRC_GEN, CRC_CS_FEED);
              for (byte i = 0; i < wr_n; i++) {
                if (wr_cnt[i] && (wr_pt[i] == RB[2])) {
                  wr_cnt[i]--;
                  parameterWritesDone ++;
                  Serial_print(F("* Executing E command for packet 0x"));
                  Serial_print(wr_pt[i], HEX);
                  Serial_print(" setting param 0x");
                  Serial_print(wr_nr[i], HEX);
                  Serial_print(" to 0x");
                  Serial_println(wr_val[i], HEX);
                }
              }
            } else {
              Serial_println(F("* Refusing to write packet while previous packet wasn't finished"));
              if (writeRefusedBusy < 0xFF) writeRefusedBusy++;
            }
          }
#endif /* E_SERIES */
#ifdef F_SERIES
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

// writable payload fields
// 10 BCL FDY  38 0 1 2 4 6 8
// 11 LPA FXMQ 38 0 1 2 4 6 8
// 12 M   FDYQ 3B 0 1 2 4 6 8 16 17
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
F 3B 16 ?? (not sure yet, active hvac zones related)
F 3B 17 ?? (not sure yet, target fan mode related)
or the following commands without spaces with extra zeroes in first and second operand:
F38001     to switch on
F38000     to switch off
F380217    to set the setpoint to 0x17 = 23C
F380219    to set the setpoint to 0x19 = 25C
F380411    to set the fanmode to low
etc

For FDYQ-like systems, try using the same commands with packet type 38 replaced by 3B.
*/

          switch (RB[2]) {
            case 0x30 : // all models: polling auxiliary controller, reply with empty payload
              d = F030DELAY;
              wr = 1;
              n = 3;
              break;
            case 0x32 : // incoming message,  occurs only once, 8 bytes, first byte is 0xC0, others 0x00; reply is one byte value 0x01? polling auxiliary controller?, reply with empty payload
              // if (model != 11)  break;
              // FXMQ / version LPA
              wr = controlLevel;
              n = 4;
              WB[3]  = 0x01; // W target status
              break;
            case 0x35 : // FXMQ outside unit name, reply with empty payload
              // if (model != 11)  break;
              // FXMQ / version LPA
              wr = controlLevel;
              n = 3;
              break;
            case 0x36 : // FXMQ indoor unit name, reply with empty payload
              // if (model != 11)  break;
              // FXMQ / version LPA
              wr = controlLevel;
              n = 3;
              break;
            case 0x37 : // FDYQ / version M  zone name packet, reply with empty payload
              // if (model != 12)  break;
              // FDYQ / version M
              wr = controlLevel;
              n = 3;
              break;
            case 0x38 :
              if (model == 10) {
                // FDY / version BCL control message, copy bytes back and change if 'F' command is given
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
                for (byte i = 0; i < wr_n; i++) {
                  if (wr_cnt[i] && (wr_pt[i] == RB[2])) { WB[wr_nr[i] + 3] = wr_val[i]; wr_cnt[i]--; };
                }
                break;
              }
              if (model == 11) {
                // FXMQ / version PCL control message, copy a few bytes back, change bytes if 'F' command is given
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
                for (byte i = 0; i < wr_n; i++) {
                  if (wr_cnt[i] && (wr_pt[i] == RB[2])) {
                    if ((wr_nr[i] == 0) && (WB[wr_nr[i] + 3] == 0x00) && (wr_val[i])) WB[16] |= 0x20; // change payload byte 13 from C0 to E0, only if payload byte 0 is set to 1 here
                    WB[wr_nr[i] + 3] = wr_val[i];
                    wr_cnt[i]--;
                  }
                }
                break;
              }
              break;
            case 0x39 :
              if (model == 10)  {
                // guess that this is filter warning for FDY / version BCL, reply with 4-byte payload
                wr = controlLevel;
                n = 7;
                WB[3] = 0x00;
                WB[4] = 0x00;
                WB[5] = RB[11];
                WB[6] = RB[12];
                for (byte i = 0; i < wr_n; i++) {
                  if (wr_cnt[i] && (wr_pt[i] == RB[2])) { WB[wr_nr[i] + 3] = wr_val[i]; wr_cnt[i]--; };
                }
                break;
              }
              if (model == 11) {
                // FXMQ / LPA reply with 5-byte all-zero payload
                wr = controlLevel;
                n = 8;
                WB[3] = 0x00;
                WB[4] = 0x00;
                WB[5] = 0x00;
                WB[6] = 0x00;
                WB[7] = 0x00;
                // for now, don't support write: for (byte i = 0; i < wr_n; i++) { if (wr_cnt[i] && (wr_pt[i] == RB[2])) { WB[wr_nr[i] + 3] = wr_val[i]; wr_cnt[i]--; };
                break;
              }
              break;
            case 0x3A : // ??, reply with 8-byte all-zero payload
              if (model == 11)  {
                // FXMQ / version LPA
                wr = controlLevel;
                n = 11;
                WB[3] = 0x00;
                WB[4] = 0x00;
                WB[5] = 0x00;
                WB[6] = 0x00;
                WB[7] = 0x00;
                WB[8] = 0x00;
                WB[9] = 0x00;
                // for now, don't support write: for (byte i = 0; i < wr_n; i++) { if (wr_cnt[i] && (wr_pt[i] == RB[2])) { WB[wr_nr[i] + 3] = wr_val[i]; wr_cnt[i]--; };
                break;
              }
              break;
            case 0x3B : // FDYQ /version M control message, copy bytes back and change if 'F' command is given
              if (model == 12)  {
                // FDYQ / version M
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
                for (byte i = 0; i < wr_n; i++) {
                  if (wr_cnt[i] && (wr_pt[i] == RB[2])) { WB[wr_nr[i] + 3] = wr_val[i]; wr_cnt[i]--; };
                }
                break;
              }
              break;
            case 0x3C : // FDYQ filter message, reply with 2-byte zero payload
              if (model == 12)  {
                // FDYQ / version M
                wr = controlLevel;
                n = 5;
                WB[3] = 0x00;
                WB[4] = 0x00;
                for (byte i = 0; i < wr_n; i++) {
                  if (wr_cnt[i] && (wr_pt[i] == RB[2])) { WB[wr_nr[i] + 3] = wr_val[i]; wr_cnt[i]--; };
                }
                break;
              }
              break;
            default   : // 0x31/0x33/0x34/0x3E/0x3F not seen, no response
              break;
          }
          if (wr) {
            if (P1P2MQTT.writeready()) {
              P1P2MQTT.writepacket(WB, n, d, CRC_GEN, CRC_CS_FEED);
            } else {
              Serial_println(F("* Refusing to write packet while previous packet wasn't finished, flushing write action"));
              if (writeRefusedBusy < 0xFF) writeRefusedBusy++;
            }
          }
#endif /* F_SERIES */
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
#ifdef EF_SERIES
    if ((RB[0] == 0x00) && (RB[1] == 0x00) && (RB[2] == 0x10)) pseudo0F = 9; // Insert one pseudo packet 00000F in output serial after 000010
    if ((RB[0] == 0x40) && (RB[1] == 0x00) && (RB[2] == 0x10)) pseudo0E++;   // Insert one pseudo packet 00000E in output serial after every 5th 400010
#endif /* EF_SERIES */
#endif /* PSEUDO_PACKETS */

    // uint8_t cs_hitachi = 0;
    packetDuplicate = 0;
    if (readError) {
      // error, so output data on line starting with E
      Serial_print(F("E "));
    } else if ((nread >= 3) && (packetDuplicate = checkPacketDuplicate(nread))) {
      Serial_print(F("D "));
    } else {
      // no error, so output data on line starting with R
      Serial_print(F("R "));
    }
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
    byte cs = 0;
    for (int i = 0; i < (packetDuplicate ? 3 : nread); i++) {
      if (EB[i] & ERROR_SB) {
        // collision suspicion due to data verification error in reading back written data
        Serial_print(F("-SB:"));
      }
      if (EB[i] & ERROR_BE) { // or BE3 (duplicate code)
        // collision suspicion due to data verification error in reading back written data
        Serial_print(F("-XX:"));
      }
      if (EB[i] & ERROR_BC) {
        // collision suspicion due to 0 during 2nd half bit signal read back
        Serial_print(F("-BC:"));
      }
      if (EB[i] & ERROR_PE) {
        // parity error detected
        Serial_print(F("-PE:"));
      }
#ifdef H_SERIES
      if (EB[i] & SIGNAL_UC) {
        // 11/12 bit uncertainty detected
        Serial_print(F("-UC:"));
      }
#endif /* H_SERIES */
#ifdef MHI_SERIES
      if (EB[i] & ERROR_INCOMPLETE) {
        // 11/12 bit uncertainty detected
        Serial_print(F("-IC:"));
      }
#endif /* H_SERIES */
#ifdef GENERATE_FAKE_ERRORS
      if (EB[i] & (ERROR_SB << 8)) {
        // collision suspicion due to data verification error in reading back written data
        Serial_print(F("-sb:"));
      }
      if (EB[i] & (ERROR_BE << 8)) {
        // collision suspicion due to data verification error in reading back written data
        Serial_print(F("-xx:"));
      }
      if (EB[i] & (ERROR_BC << 8)) {
        // collision suspicion due to 0 during 2nd half bit signal read back
        Serial_print(F("-bc:"));
      }
      if (EB[i] & (ERROR_PE << 8)) {
        // parity error detected
        Serial_print(F("-pe:"));
      }
#endif /* GENERATE_FAKE_ERRORS */
      byte c = RB[i];
      if (c < 0x10) Serial_print('0');
      Serial_print(c, HEX);
      if (EB[i] & ERROR_OR) {
        // buffer overrun detected (overrun is after, not before, the read byte)
        Serial_print(F(":OR-"));
      }
      // if (i) cs_hitachi ^= c;
      if (EB[i] & ERROR_CRC_CS) {
        // CS or CRC error detected in readpacket
#ifdef MHI_SERIES
        Serial.print(F(" CS error"));
#else /* MHI_SERIES */
        Serial_print(F(" CRC error"));
#endif /* MHI_SERIES */
      }
    }
    Serial_println();
  }
#ifdef PSEUDO_PACKETS
  if (pseudo0E > 4) {
    pseudo0E = 0;
    WB[0]  = 0x00;
    WB[1]  = 0x00;
    WB[2]  = 0x0E;
    WB[3]  = SW_MAJOR_VERSION;
    WB[4]  = SW_MINOR_VERSION;
    WB[5]  = SW_PATCH_VERSION;
    WB[6]  = ATmegaHwID;
    WB[7]  = brand;
    WB[8]  = model;
    WB[9]  = scope;
    WB[10]  = Compile_Options;
    WB[11] = (sd >> 8) & 0xFF;
    WB[12] = sd & 0xFF;
    WB[13] = (sdto >> 8) & 0xFF;
    WB[14] = sdto & 0xFF;
    WB[15] = save_MCUSR;
#ifdef F_SERIES
    WB[16] = modelSuggestion;
    writePseudoPacket(WB, 17);
#else
    writePseudoPacket(WB, 16);
#endif /* F_SERIES */
  }
  if (pseudo0F > 4) {
    pseudo0F = 0;
    WB[0]  = 0x00;
    WB[1]  = 0x00;
    WB[2]  = 0x0F;
    WB[3]  = readErrors;
    WB[4]  = readErrorLast;
#if defined MHI_SERIES || defined TH_SERIES
    WB[5] = errorMask;
    WB[6] = allow;
    for (byte i = 7; i < 17; i++) WB[i] = 0x00;
#endif /* MHI_SERIES || TH_SERIES */
#ifdef EF_SERIES
    WB[5]  = writeBudget;
    WB[6]  = writeBudgetPeriod;
    WB[7]  = writeRefusedBusy;
    WB[8]  = writeRefusedBudget;
    WB[9]  = errorsPermitted;
    WB[10] = (parameterWritesDone >> 8) & 0xFF;
    WB[11] = parameterWritesDone & 0xFF;
    WB[12] = controlLevel;
    WB[13] = CONTROL_ID;
    WB[14] = CONTROL_ID ? controlLevel : 0; // auxiliary control mode fully on; is 1 for full control modes L1 and L3, 0 for L5
#endif /* EF_SERIES */
#ifdef E_SERIES
    WB[15] = counterRepeatingRequest;
    WB[16] = F_max;
#else
    WB[15] = 0;
    WB[16] = 0;
#endif /* E_SERIES */
    WB[17]  = (upt >> 24) & 0xFF;
    WB[18]  = (upt >> 16) & 0xFF;
    WB[19]  = (upt >> 8) & 0xFF;
    WB[20]  = upt & 0xFF;
    if (ATmegaHwID) {
      uint16_t V0_min;
      uint16_t V0_max;
      uint32_t V0_avg;
      uint16_t V1_min;
      uint16_t V1_max;
      uint32_t V1_avg;
      P1P2MQTT.ADC_results(V0_min, V0_max, V0_avg, V1_min, V1_max, V1_avg);
      WB[21] = (uint8_t) (V0_avg * (10 * 20.9 / 1023 / (1 << (16 - ADC_CNT_SHIFT)))) + 28;
      WB[22] = (uint8_t) (V1_avg * (10 * 3.773 / 1023 / (1 << (16 - ADC_CNT_SHIFT))));
    }
    writePseudoPacket(WB, 23);
  }
#endif /* PSEUDO_PACKETS */
}
