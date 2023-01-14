/* P1P2Monitor: monitor program for HBS (including raw data write) such as Mitsubishi Heavy Industries (MHI) X-Y line protocol
 *              A few additional parameters are added in raw hex data as so-called pseudo-packets (packet types 0x08-0x0F).
 *
 *              The P1P2Monitor command format is described in P1P2Monitor-commands.md,
 *
 * Copyright (c) 2019-2023 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 *
 * Mitsubishi Heavy Industries version, increased buffer size
 *
 * 20230113 v0.9.30 MHI branch started
 *
 *     Thanks to Krakra for providing coding suggestions and UDP support, the hints and references to the HBS and MM1192 documents on
 *     https://community.openenergymonitor.org/t/hack-my-heat-pump-and-publish-data-onto-emoncms,
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
 */

#include "P1P2Config.h"
#include <P1P2Serial.h>

#define SPI_CLK_PIN_VALUE (PINB & 0x20)

static byte mhiFormat = INIT_MHI_FORMAT;
static byte allow = ALLOW_PAUSE_BETWEEN_BYTES;
static byte verbose = INIT_VERBOSE;
static byte readErrors = 0;
static byte readErrorLast = 0;
static byte errorsLargePacket = 0;

const byte Compile_Options = 0 // multi-line statement
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
    EEPROM.update(EEPROM_ADDRESS_MHI_FORMAT, INIT_MHI_FORMAT);
    EEPROM.update(EEPROM_ADDRESS_ALLOW, ALLOW_PAUSE_BETWEEN_BYTES);
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
static byte counterRequest = 0;
static byte counterRepeatingRequest = 0;
static byte writeRefused = 0;

byte save_MCUSR;

byte Tmin = 0;
byte Tminprev = 61;
int32_t upt_prev_pseudo = 0;
int32_t upt_prev_write = 0;
int32_t upt_prev_error = 0;

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
  Serial.println();
  Serial.print(F("* P1P2-ESP-interface hwID "));
  Serial.println(hwID);
#ifdef EEPROM_SUPPORT
  initEEPROM();
  mhiFormat  = EEPROM.read(EEPROM_ADDRESS_MHI_FORMAT);
  allow      = EEPROM.read(EEPROM_ADDRESS_ALLOW);
  verbose    = EEPROM.read(EEPROM_ADDRESS_VERBOSITY);
#endif /* EEPROM_SUPPORT */
  if (verbose) {
    Serial.print(F("* Verbosity="));
    Serial.println(verbose);
    Serial.print(F("* MHI format="));
    Serial.println(mhiFormat);
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
    Serial.println();
#ifdef OLDP1P2LIB
    Serial.println(F("* OLDP1P2LIB"));
#else
    Serial.println(F("* NEWP1P2LIB"));
#endif
  }
  P1P2Serial.begin(9600, hwID ? true : false, 6, 7); // if hwID = 1, use ADC6 and ADC7
  P1P2Serial.setEcho(echo);
  P1P2Serial.setMHI(mhiFormat);
  P1P2Serial.setAllow(allow);
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

static byte cs_gen = CS_GEN;

void writePseudoPacket(byte* WB, byte rh)
{
  if (verbose) Serial.print(F("R "));
  if (verbose & 0x01) Serial.print(F("P         "));
  uint8_t cs = 0;
  for (uint8_t i = 0; i < rh; i++) {
    uint8_t c = WB[i];
    if (c <= 0x0F) Serial.print(F("0"));
    Serial.print(c, HEX);
    if (cs_gen != 0) cs += c;
  }
  if (cs_gen) {
    if (cs <= 0x0F) Serial.print(F("0"));
    Serial.print(cs, HEX);
  }
  Serial.println();
}

void(* resetFunc) (void) = 0; // declare reset function at address 0

static byte pseudo0D = 0;
static byte pseudo0E = 0;
static byte pseudo0F = 0;

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
        Serial.print(F("* First line ignored: ->"));
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
            case 'g':
            case 'G': if (verbose) Serial.print(F("* Check sum generation "));
                      if (scanint(RSp, temp) == 1) {
                        cs_gen = temp ? 1 : 0;
                        if (!verbose) break;
                        Serial.print(F("set to "));
                      }
                      Serial.println(cs_gen);
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
                      break;
#endif
            case 'm':
            case 'M': if (verbose) Serial.print(F("* MHI format "));
                      if (scanint(RSp, temp) == 1) {
                        mhiFormat = temp ? 1 : 0;
                        P1P2Serial.setMHI(mhiFormat);
                        EEPROM_update(EEPROM_ADDRESS_MHI_FORMAT, mhiFormat);
                        if (!verbose) break;
                        Serial.print(F("set to "));
                      }
                      Serial.println(mhiFormat);
                      break;
            case 'x':
            case 'X': if (verbose) Serial.print(F("* Allow (bits pause) "));
                      if (scanint(RSp, temp) == 1) {
                        allow = temp;
                        if (allow > 76) allow = 76;
                        P1P2Serial.setAllow(allow);
                        EEPROM_update(EEPROM_ADDRESS_ALLOW, allow);
                        if (!verbose) break;
                        Serial.print(F("set to "));
                      }
                      Serial.println(allow);
                      break;
            case 'w':
            case 'W': if (verbose) Serial.print(F("* Writing: "));
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
                          P1P2Serial.writepacket(WB, wb, sd, cs_gen);
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
  while (P1P2Serial.packetavailable()) {
    uint16_t delta;
    errorbuf_t readError = 0;
    int nread = P1P2Serial.readpacket(RB, delta, EB, RB_SIZE, cs_gen);
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

    if (scope) {
      if (sws_cnt || (sws_event[SWS_MAX - 1] != SWS_EVENT_LOOP)) {
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
    }
// for MHI initial version, split reporting errors (if any) and data (always)
    uint16_t delta2 = delta;
    if (readError) {
      Serial.print(F("E "));
      // 3nd-12th characters show length of bus pause (max "R T 65.535: ")
      Serial.print(F("T "));
      if (delta < 10000) Serial.print(F(" "));
      if (delta < 1000) Serial.print(F("0")); else { Serial.print(delta / 1000); delta %= 1000; };
      Serial.print(F("."));
      if (delta < 100) Serial.print(F("0"));
      if (delta < 10) Serial.print(F("0"));
      Serial.print(delta);
      Serial.print(F(": "));
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
        if (cs_gen && (verbose == 1) && (i == nread - 1)) {
          Serial.print(F(" CS="));
        }
        if (c < 0x10) Serial.print(F("0"));
        Serial.print(c, HEX);
        if (verbose && (EB[i] & ERROR_OR)) {
          // buffer overrun detected (overrun is after, not before, the read byte)
          Serial.print(F(":OR-"));
        }
        if (verbose && (EB[i] & ERROR_CS)) {
          // CS error detected in readpacket
          Serial.print(F(" CS error"));
        }
      }
      if (readError & 0xBB) {
        Serial.print(F(" readError=0x"));
        if (readError < 0x10) Serial.print('0');
        if (readError < 0x100) Serial.print('0');
        if (readError < 0x1000) Serial.print('0');
        Serial.print(readError, HEX);
      }
      Serial.println();
    }
    delta = delta2;
    if (!(readError & 0xBB)) {
      if (verbose && (verbose < 4)) Serial.print(F("R "));
      if ((verbose & 0x01) == 1)  {
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
      if (verbose < 4) {
        for (int i = 0; i < nread; i++) {
          byte c = RB[i];
          if (cs_gen && (verbose == 1) && (i == nread - 1)) {
            Serial.print(F(" CS="));
          }
          if (c < 0x10) Serial.print(F("0"));
          Serial.print(c, HEX);
        }
        Serial.println();
      }
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
      WB[19] = 0x00;
      WB[20] = 0x00;
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
    WB[15] = 0x00;
    WB[16] = 0x00;
    WB[17] = 0x00;
    WB[18] = 0x00;
    WB[19] = 0x00;
    WB[20] = 0x00;
    WB[21] = 0x00;
    WB[22] = 0x00;
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
    WB[6]  = 0x00;
    WB[7]  = 0x00;
    WB[8]  = 0x00;
    WB[9]  = 0x00;
    WB[10] = (sd >> 8) & 0xFF;
    WB[11] = sd & 0xFF;
    WB[12] = (sdto >> 8) & 0xFF;
    WB[13] = sdto & 0xFF;
    WB[14] = hwID;
#ifdef EEPROM_SUPPORT
    WB[15] = 0x00;
    WB[16] = EEPROM.read(EEPROM_ADDRESS_VERBOSITY);
    WB[17] = 0x00;
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
    WB[22] = 0x00;
    if (verbose < 4) writePseudoPacket(WB, 23);
  }
#endif /* PSEUDO_PACKETS */
}
