/* P1P2_Daikin_ParameterConversion_EHYHB.h product-dependent code for EHYHBX08AAV3 and perhaps other EHYHB models
 *
 * Copyright (c) 2019 Arnold Niessen, arnold.niessen -at- gmail-dot-com  - licensed under GPL v2.0 (see LICENSE)
 *
 */

#ifndef P1P2_Daikin_json_EHYHB
#define P1P2_Daikin_json_EHYHB

// history size parameters (only relevant if SAVEHISTORY is defined)
#define RBHLEN 500   // max #bytes saved to store packet payloads
#define PARAM35LEN 512 // max nr of 00F035/00F135/40F035/40F135 params to be saved (per packet, so mem use is *4)
#define PARAM36LEN  48 // max nr of 00F036/00F136/40F036/40F136 params to be saved (per packet, so mem use is *4)
#define RBHNP  38    // #of packet types of messages to be saved in historymax 127 given that shi is int8_t
#define PARAM35START 0 // first param nr saved
#define PARAM36START 0 // first param nr saved
#define NHYST 30     // nr of values for hysteresis comparison
#ifdef __AVR_ATmega328P__
// Reduce memory usage if the code is to be run on an Arduino Uno:
#define RBHLEN   166   // should be >= 1, 166 is sufficient for 0x10..0x16 packets
#define PARAM35LEN 1     // should be >= 1
#define RBHNP   14     // should be >= 1, 14 is sufficient for 0x10..0x16 packets
#define NHYST   15     // nr of values for hysteresis comparison (15 for this model)
#endif /* __AVR_ATmega328P__ */

#define KEYLEN 40    // max length of key/topic or value/payload

#include "P1P2_Daikin_ParameterConversion.h" // for Daikin product-independent code

char TimeString[17]="2000-00-00 00:00";
char TimeString2[20]="2000-00-00 00:00:00";
byte SelectTimeString2=0;


// device dependent code starts here:
// ----------------------------------
//
// to implement json decoding for another product, rename this file and change the following functions:
// -savehistoryindex() to indicate which packets should be saved
// -savehistoryignore() to determine how many bytes of a packet can be ignored (usually 3 or 4), 3 seems to be a safe value in case of doubt
// -the big switch statement in bytes2keyvalue()
//
// Some of the code below might turn out to be device-independent, such as savehistoryindex() and savehistoryignore()

int8_t savehistoryindex(byte *rb) {
// returns whether rb should be saved,
//    and if so, returns index of rbhistoryp to store history data
// returns -1 if this packet is not to be saved
// returns -0x35 if packet is a 00F035/00F135/40F035/40F135 packet to be handled separately, parameters in packet will be saved by saveparam()
// returns -0x36 if packet is a 00F036/00F136/40F036/40F136 packet to be handled separately, parameters in packet will be saved by saveparam()
// returns a value in the range of 0..(RBHNP-1) if message is to be saved as a packet
// Example below: save messages 0x10..0x15 from sources 0x00 and 0x40
//                save messages 0xB8_subtype 0x00-0x05 from source 0x40
  int8_t rv;
  if (((rb[0] & 0xBF) == 0x00) && ((rb[1] & 0xFE) == 0xF0) && (rb[2] == 0x35)) return -0x35; // for now only slave addresses 0xF0 and 0xF1
  if (((rb[0] & 0xBF) == 0x00) && ((rb[1] & 0xFE) == 0xF0) && (rb[2] == 0x36)) return -0x36; // for now only slave addresses 0xF0 and 0xF1
  if ((rb[2] >= 0x10) && (rb[2] <= 0x16) && ((rb[0] & 0xBF) == 0x00)) {
    rv = ((rb[2] & 0x0F) + ((rb[0] & 0x40) ? 7 : 0));
    if (rv < RBHNP) return rv; else return -1;
  } else if ((rb[2] == 0xB8) && (rb[0] == 0x40) && (rb[3] <= 0x05)) {
    rv = (rb[3] + 14);
    if (rv < RBHNP) return rv; else return -1;
  } else if (((rb[2] & 0xF0) == 0x30) && (rb[2] != 0x30) && (rb[1] == 0xF0) && ((rb[0] & 0xBF) == 0x00)) { // only 0xF0 0x31-0x3F
    rv = ((rb[2] - 0x31) + 20);
    if (rv < RBHNP) return rv; else return -1;
  }
  return -1;
}

uint8_t savehistoryignore(byte *rb) {
// indicates how many bytes at start of packet do *not* need to be stored
// usually the first 3 bytes can be ignored as they indicate src/??/packet-type
// However 0xB8 packets have a sub-type, so we can ignore the first 4 bytes
  return ((rb[2] == 0xB8) ? 4 : 3);
}

byte bytes2keyvalue(byte* rb, byte i, byte j, char* key, char* value, byte &cat) {
// Generates key/value pair based on rb, i, j
// if j=8, byte-based handling is performed, and i (i>=3) indicates that rb[i] should be considered within the message rb,
// if 0<=j<=7, bit-based handling of bit j of byte rb[i] is performed.
// Upon return, key and value contain the topic/payload for a mqtt message also usable as a key/value for a json message
// Return value:
// 0 no value to return
// 1 (new) value to print/output
// 8 this byte should be treated on bit basis by calling bits2keyvalue() with 0<=j<=7
// 9 as return value indicates the json message should be terminated
//
// the macro's defined below for KEY and VALUE aim to make the compound switch statement somewhat readable,
// for reuse for other heat pumps
// the keyword SETTING, MEASUREMENT, or PARAM3x determine which mqtt subtopic will be used to communicate the results.
//
// no break statements are needed as the macro's include a return statement
//
// For each combination of linetype/linesrce/i(/j), the type of parameter is given as macro, where needed preceded by a KEY value describing the parameter
// The following macro's can be used below:
//
// BITBASIS (for j==8): this byte is to be treated on a bitbasis (by re-calling this function with j=0..7
//                      switch-statement for j is needed as shown below
//                      indicate for j=0..7: "KEY("KnownParameter"); VALUE_flag8" if bit usage is known
//                      indicate for j=0..7: "UNKNOWNBIT", key will then be set to "Bit-0x[source]-0x[packetnr]-0x[location]-[bitnr]"
// BITBASIS_UNKNOWN:    shortcut for entire bit-switch statement if bit usage of all bits is unknown
// UNKNOWNBYTE:         parameter function is unknown, key will be "Byte-0x[source]-0x[packetnr]-0x[location]"
// VALUE_u8:            1-byte unsigned integer value at current location rb[i]
// VALUE_u24:           3-byte unsigned integer value at location rb[i-2]..rb[i]
// return 0:            use this for all bytes (except for the last byte) which are part of a multi-byte parameter
// VALUE_u8_add2k:      for 1-byte value (2000+rb[i]) (for year value)
// VALUE_u8delta:       for 1-byte value -10..10 where bit 4 is sign and bit 0..3 is value
// VALUE_f8_8           for 2-byte float in f8_8 format (see protocol description)
// VALUE_f8s8           for 2-byte float in f8s8 format (see protocol description)
// VALUE_u8div10        for 1-byte integer to be divided by 10 (used for flow in 0.1m/s)
// VALUE_F(value, ch)   for self-calculated float parameter value, uses additional parameter ch to signal change
// VALUE_H4             for 4-byte hex value (used for sw version)
// TERMINATEJSON        returns 9 to signal end of package, signal for json string termination

  byte linetype = rb[2];
  byte linesrc = rb[0];
  float P1, P2;      // these variables cannot be declared within the case statement - not sure why not?
  static float LWT = 0, RWT = 0, MWT = 0, Flow = 0;

  switch (linetype) {
    case 0x10 : switch (linesrc) {
      case 0x00 : switch (i) {
        case  3 : switch (j) {
                    case  8 : BITBASIS;
                    case  0 : KEY("HeatingOn1");                SETTING;     VALUE_flag8;
                    default : UNKNOWNBIT;
                  }
        case  4 : switch (j) {
                    case  8 : BITBASIS;
                    case  0 : KEY("Gasmode0");                  SETTING;     VALUE_flag8;
                    case  7 : KEY("Gasmode7");                  SETTING;     VALUE_flag8;
                    default : UNKNOWNBIT;
                  }
        case  5 : switch (j) {
                    case  8 : BITBASIS;
                    case  0 : KEY("DHWon");                     SETTING;     VALUE_flag8;
                    default : UNKNOWNBIT;
                  }
        case 10 :             KEY("RoomTarget1");                            VALUE_u8;
        case 12 :             // fallthrough
        case 13 : switch (j) {
                    case  8 : BITBASIS;
                    case  2 : KEY("QuietMode1");                SETTING;     VALUE_flag8;
                    default : UNKNOWNBIT;
                  }
        case 15 :             // fallthrough
        case 18 :             BITBASIS_UNKNOWN;
        case 20 : switch (j) {
                    case  8 : BITBASIS;
                    case  1 : KEY("DHWboosterActive");          SETTING;     VALUE_flag8;
                    case  6 : KEY("DHWactive");                 SETTING;     VALUE_flag8;
                    default : UNKNOWNBIT;
                  }
        case 21 :             KEY("DHWcomfort1");               SETTING;     VALUE_u8;
        default :             UNKNOWNBYTE;
      }
      case 0x40 : switch (i) {
        case  3 : switch (j) {
                    case  8 : BITBASIS;
                    case  0 : KEY("HeatingOn2");                SETTING;     VALUE_flag8;
                    default : UNKNOWNBIT;
                  }
        case  4 :             BITBASIS_UNKNOWN;
        case  5 : switch (j) {
                    case  8 : BITBASIS;
                    case  0 : KEY("HeatingValve");              SETTING;     VALUE_flag8;
                    case  1 : KEY("CoolingValve");              SETTING;     VALUE_flag8;
                    case  5 : KEY("MainZoneValve");             SETTING;     VALUE_flag8;
                    case  6 : KEY("AddZoneValve");              SETTING;     VALUE_flag8;
                    case  7 : KEY("DHWtankValve");              SETTING;     VALUE_flag8;
                    default :                                   UNKNOWNBIT;
                  }
        case  6 : switch (j) {
                    case  8 : BITBASIS;
                    case  0 : KEY("3WayValve");                 SETTING;     VALUE_flag8;
                    case  4 : KEY("SHCtank");                   SETTING;     VALUE_flag8;
                    default : UNKNOWNBIT;
                  }
        case  7 :             KEY("DHWcomfort2");                             VALUE_u8;
        case  9 :             BITBASIS_UNKNOWN;
        case 11 :             KEY("RoomTarget2");                            VALUE_u8;
        case 14 : switch (j) {
                    case  8 : BITBASIS;
                    case  2 : KEY("QuietMode2");                SETTING;     VALUE_flag8;
                    default : UNKNOWNBIT;
                  }
        case 21 : switch (j) {
                    case  8 : BITBASIS;
                    case  0 : KEY("Compressor");                SETTING;     VALUE_flag8;
                    case  3 : KEY("CircPump");                  SETTING;     VALUE_flag8;
                    default : UNKNOWNBIT;
                  }
        case 22 : switch (j) {
                    case  8 : BITBASIS;
                    case  2 : KEY("DHWmode");                   SETTING;     VALUE_flag8;
//                    case  1 : KEY("DHWactive1");                SETTING;     VALUE_flag8;
                    default : UNKNOWNBIT;
                  }
        default :             UNKNOWNBYTE;
      }
      default :               UNKNOWNBYTE;
    }
    case 0x11 : switch (linesrc) {
      case 0x00 : switch (i) {
        case  3 :             return 0;                         // Room temperature
        case  4 :             KEY("TempRoom1");                 TEMPFLOWP;   VALUE_f8_8(0, 0.4, 0.6);
        default :             UNKNOWNBYTE;
      }
      case 0x40 : switch (i) {
        case  3 :             return 0;
        case  4 :             KEY("TempLWT");                   // Leaving water temperature
                              LWT = FN_f8_8(&rb[i]);            TEMPFLOWP;   VALUE_f8_8(1, 0.09, 0.15);
        case  5 :             return 0;                         // Domestic hot water temperature (on some models)
        case  6 :             KEY("TempDHW");                   TEMPFLOWP;   VALUE_f8_8(2, 0.09, 0.15);   // unconnected on EHYHBX08AAV3?, then reading -40
        case  7 :             return 0;                         // Outside air temperature (low res)
        case  8 :             KEY("TempOut1");                  TEMPFLOWP;   VALUE_f8_8(3, 0.4, 0.6);
        case  9 :             return 0;
        case 10 :             KEY("TempRWT");                   // Return water temperature
                              RWT = FN_f8_8(&rb[i]);            TEMPFLOWP;   VALUE_f8_8(4, 0.09, 0.15);
        case 11 :             return 0;
        case 12 :             KEY("TempMWT");                   // Leaving water temperature on the heat exchanger
                              MWT = FN_f8_8(&rb[i]);            TEMPFLOWP;   VALUE_f8_8(5, 0.09, 0.15);
        case 13 :             return 0;                         // Refrigerant temperature
        case 14 :             KEY("TempRefr1");                 TEMPFLOWP;   VALUE_f8_8(6, 0.09, 0.15);
        case 15 :             return 0;                         // Room temperature
        case 16 :             KEY("TempRoom2");                 TEMPFLOWP;   VALUE_f8_8(7, 0.4, 0.6);
        case 17 :             return 0;                         // Outside air temperature
        case 18 :             KEY("TempOut2");                  TEMPFLOWP;   VALUE_f8_8(8, 0.4, 0.6);
        default :             UNKNOWNBYTE;
      }
      default :               UNKNOWNBYTE;
    }
    case 0x12 : switch (linesrc) {
      case 0x00 : switch (i) {
        case  3 :             BITBASIS_UNKNOWN;
        case  4 :             KEY("DayOfWeek");                 MEASUREMENT; VALUE_u8;    // Monday: 0
        case  5 :             if (SelectTimeString2) SelectTimeString2--;
                              // Switch to TimeString if we have not seen a 00Fx31 packet for a few cycli (>6 in view of counter request blocking 6 cycli)
                              TimeString[11] = '0' + (rb[i] / 10);
                              TimeString[12] = '0' + (rb[i] % 10);
                              KEY("Hours");                     MEASUREMENT; VALUE_u8;
        case  6 :             
                              TimeString[14] = '0' + (rb[i] / 10);
                              TimeString[15] = '0' + (rb[i] % 10);
                              KEY("Min");                       MEASUREMENT; VALUE_u8;
        case  7 :             
                              TimeString[2] = '0' + (rb[i] / 10);
                              TimeString[3] = '0' + (rb[i] % 10);
                              KEY("Year");                      MEASUREMENT; VALUE_u8_add2k;
        case  8 :             
                              TimeString[5] = '0' + (rb[i] / 10);
                              TimeString[6] = '0' + (rb[i] % 10);
                              KEY("Month");                     MEASUREMENT; VALUE_u8;
        case  9 :             
                              TimeString[8] = '0' + (rb[i] / 10);
                              TimeString[9] = '0' + (rb[i] % 10);
                              KEY("Day");                       MEASUREMENT; VALUE_u8;
        case 15 :             // fallthrough
        case 16 :             BITBASIS_UNKNOWN;
        default :             UNKNOWNBYTE;
      }
      case 0x40 : switch (i) {
        case 13 : switch (j) {
                    case  8 : BITBASIS;
                    case  4 : KEY("PrefMode");                  SETTING;     VALUE_flag8;
                    default : UNKNOWNBIT;
                  }
        case 14 :             KEY("Restart");                   SETTING;     VALUE_u8;
        case 15 : switch (j) {
                    case  8 : BITBASIS;
                    case  0 : KEY("HeatPump1");                 SETTING;     VALUE_flag8;
                    case  6 : KEY("Gas");                       SETTING;     VALUE_flag8;
//                    case  7 : KEY("DHWactive2");                SETTING;     VALUE_flag8;
                    default : UNKNOWNBIT;
                  }
        default :             UNKNOWNBYTE;
      }
      default :               UNKNOWNBYTE;
    }
    case 0x13 : switch (linesrc) {
      case 0x00 : switch (i) {
        default :             UNKNOWNBYTE;
      }
      case 0x40 : switch (i) {
        case  3 :             KEY("TargetDHW");                 SETTING;     VALUE_u8;
        case 11 :             return 0;                                                          // 0xFF = No flow
        case 12 :             KEY("Flow");
                              Flow = FN_u8div10(&rb[i]);        TEMPFLOWP;   VALUE_u8div10(9, 0.09, 0.15);
        case 13 :             // fallthrough
        case 14 :             // fallthrough
        case 15 :             return 0;
        case 16 :             KEY("SWversion");                 SETTING;     VALUE_H4;           // sw version 4 bytes
        default :             UNKNOWNBYTE;
      }
      default :               UNKNOWNBYTE;
    }
    case 0x14 : switch (linesrc) {
      case 0x00 : switch (i) {
        case 11 :             KEY("TargetDeltaMain1");          SETTING;     VALUE_u8delta;
        case 13 :             KEY("TargetDeltaAdd1");           SETTING;     VALUE_u8delta;
        default :             UNKNOWNBYTE;
      }
      case 0x40 : switch (i) {
        case 11 :             KEY("TargetDeltaMain2");          SETTING;     VALUE_u8delta;
        case 13 :             KEY("TargetDeltaAdd2");           SETTING;     VALUE_u8delta;
        case 18 :             return 0;
        case 19 :             KEY("TargetMainZone");            SETTING;     VALUE_f8s8(10, 0.2, 0.6);
        case 20 :             return 0;
        case 21 :             KEY("TargetAddZone");             SETTING;     VALUE_f8s8(11, 0.2, 0.6);
        default :             UNKNOWNBYTE;
      }
      default :               UNKNOWNBYTE;
    }
    case 0x15 : switch (linesrc) {
      case 0x00 : switch (i) {
      default :               UNKNOWNBYTE;
    }
      case 0x40 : switch (i) {
        case  5 :             return 0;
        case  6 :             KEY("Temprefr2");                 TEMPFLOWP;   VALUE_f8_8(12, 0.4, 0.6);
        default :             UNKNOWNBYTE;
      }
      default :               UNKNOWNBYTE;
    }
    case 0x30 : switch (i) {
      case  3 :                                                 // bytes in 0x30 message do not seem to contain usable information
                                                                // as this is the final packet in a package
                                                                // we (ab)use this to communicate calculated power
                              P1 = (LWT - MWT) * Flow * 0.07;   // power produced by gas boiler
                              KEY("P1");                        TEMPFLOWP;   VALUE_F(P1, 13, 0.09, 0.15);
      case  4 :               P2 = (MWT - RWT) * Flow * 0.07;   // power produced by heat pump
                              KEY("P2");                        TEMPFLOWP;   VALUE_F(P2, 14, 0.09, 0.15);
      case  5 :               TERMINATEJSON;                    // terminate json string at end of package
      default :               return 0;                         // these bytes change but don't carry real information
                                                                // they are used to request or announce further 3x packets
                                                                // so we return 0 to signal they don't need to be output
    }
    case 0xB8 : switch (linesrc) {
      case 0x00 :             return 0;                         // no info in 0x00 packets regardless of subtype
      case 0x40 : switch (rb[3]) {
        case 0x00 : switch (i) {                                // packet B8 subtype 00
          case  3 :           return 0;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 :           KEY("ConsElecHeatBackup");        MEASUREMENT; VALUE_u24; // electricity used for room heating, backup heater
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :           KEY("ConsElecDHWBackup");         MEASUREMENT; VALUE_u24; // electricity used for DHW, backup heater
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 :           KEY("ConsElecHeatCompr");         MEASUREMENT; VALUE_u24; // electricity used for room heating, compressor
          case 13 :           return 0;
          case 14 :           return 0;
          case 15 :           KEY("ConsElecCoolCompr");         MEASUREMENT; VALUE_u24; // electricity used for cooling, compressor
          case 16 :           return 0;
          case 17 :           return 0;
          case 18 :           KEY("ConsElecDHWCompr");          MEASUREMENT; VALUE_u24; // eletricity used for DHW, compressor
          case 19 :           return 0;
          case 20 :           return 0;
          case 21 :           KEY("ConsElecTotal");             MEASUREMENT; VALUE_u24; // electricity used, total
          default :           UNKNOWNBYTE;
        }
        case 0x01 : switch (i) {                                // packet B8 subtype 01
          case  3 :           return 0;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 :           KEY("EnergyProdHeat");           MEASUREMENT; VALUE_u24; // energy produced for room heating
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :           KEY("EnergyProdCool");           MEASUREMENT; VALUE_u24; // energy produced when cooling
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 :           KEY("EnergyProdDHW");            MEASUREMENT; VALUE_u24; // energy produced for DHW
          case 13 :           return 0;
          case 14 :           return 0;
          case 15 :           KEY("EnergyProdTotal");          MEASUREMENT; VALUE_u24; // energy produced total
          default :           UNKNOWNBYTE;
        }
        case 0x02 : switch (i) {                                // packet B8 subtype 02
          case  3 :           return 0;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 :           KEY("PumpHours");                 MEASUREMENT; VALUE_u24;
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :           KEY("CompressorHoursHeat");       MEASUREMENT; VALUE_u24;
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 :           KEY("CompressorHoursCool");       MEASUREMENT; VALUE_u24;
          case 13 :           return 0;
          case 14 :           return 0;
          case 15 :           KEY("CompressorHoursDHW");        MEASUREMENT; VALUE_u24;
          default :           UNKNOWNBYTE;
        }
        case 0x03 : switch (i) {                                // packet B8 subtype 03
          case  3 :           return 0;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 :           KEY("HoursHeatBackup1");          MEASUREMENT; VALUE_u24;
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :           KEY("HoursDHWBackup1");           MEASUREMENT; VALUE_u24;
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 :           KEY("HoursHeatBackup2");          MEASUREMENT; VALUE_u24;
          case 13 :           return 0;
          case 14 :           return 0;
          case 15 :           KEY("HoursDHWBackup2");           MEASUREMENT; VALUE_u24;
          case 16 :           return 0;
          case 17 :           return 0;
          case 18 :           KEY("Hours0q");                   MEASUREMENT; VALUE_u24; // ?
          case 19 :           return 0;
          case 20 :           return 0;
          case 21 :           KEY("Hours1q");                   MEASUREMENT; VALUE_u24; // reports 0?
          default :           UNKNOWNBYTE;
        }
        case 0x04 : switch (i) {                                // packet B8 subtype 04
          case  3 :           return 0;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 :           KEY("Counter40q");                MEASUREMENT; VALUE_u24;
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :           KEY("Counter41q");                MEASUREMENT; VALUE_u24;
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 :           KEY("Counter42q");                MEASUREMENT; VALUE_u24;
          case 13 :           return 0;
          case 14 :           return 0;
          case 15 :           KEY("CompressorStarts");          MEASUREMENT; VALUE_u24;
          default :           UNKNOWNBYTE;
        }
        case 0x05 : switch (i) {                                // packet B8 subtype 05
          case  3 :           return 0;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 :           KEY("BoilerHoursHeating");        MEASUREMENT; VALUE_u24;
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :           KEY("BoilerHoursDHW");            MEASUREMENT; VALUE_u24;
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 :           KEY("Counter8q");                 MEASUREMENT; VALUE_u24; // reports 0?
          case 13 :           return 0;
          case 14 :           return 0;
          case 15 :           KEY("Counter9q");                 MEASUREMENT; VALUE_u24; // reports 0?
          case 16 :           return 0;
          case 17 :           return 0;
          case 18 :           KEY("BoilerStarts");              MEASUREMENT; VALUE_u24; // ?
          case 19 :           return 0;
          case 20 :           return 0;
          case 21 :           KEY("Counter11q");                MEASUREMENT; VALUE_u24; // reports 0?
          default :           UNKNOWNBYTE;
        }
        default :             UNKNOWNBYTE;
      }
      default :               UNKNOWNBYTE;
    }
    case 0x31 : switch (linesrc) {
      case 0x00 : switch (i) {                                  // param packet from main controller to external controller
        case  3 :             BITBASIS_UNKNOWN;
        case  4 :             KEY("F031-4");                      MEASUREMENT; VALUE_u8hex;
        case  5 :             KEY("F031-5");                      MEASUREMENT; VALUE_u8hex;
        case  6 :             KEY("F031-6");                      MEASUREMENT; VALUE_u8hex;
        case  7 :             KEY("F031-7");                      MEASUREMENT; VALUE_u8hex;
        case  8 :             KEY("F031-8");                      MEASUREMENT; VALUE_u8hex;
        case  9 :             SelectTimeString2=10;
                              // Keep on TimeString2 unless we have not seen a 00Fx31 packet for a few cycli (>6 in view of counter request blocking 6 cycli)
                              TimeString2[2] = '0' + (rb[i] / 10);
                              TimeString2[3] = '0' + (rb[i] % 10);
                              KEY("YearExt");                      MEASUREMENT; VALUE_u8_add2k;
        case 10 :             
                              TimeString2[5] = '0' + (rb[i] / 10);
                              TimeString2[6] = '0' + (rb[i] % 10);
                              KEY("MonthExt");                     MEASUREMENT; VALUE_u8;
        case 11 :             
                              TimeString2[8] = '0' + (rb[i] / 10);
                              TimeString2[9] = '0' + (rb[i] % 10);
                              KEY("DayExt");                       MEASUREMENT; VALUE_u8;
        case 12 :             
                              TimeString2[11] = '0' + (rb[i] / 10);
                              TimeString2[12] = '0' + (rb[i] % 10);
                              KEY("HoursExt");                     MEASUREMENT; VALUE_u8;
        case 13 :             
                              TimeString2[14] = '0' + (rb[i] / 10);
                              TimeString2[15] = '0' + (rb[i] % 10);
                              KEY("MinExt");                       MEASUREMENT; VALUE_u8;
        case 14 :             
                              TimeString2[17] = '0' + (rb[i] / 10);
                              TimeString2[18] = '0' + (rb[i] % 10);
                              KEY("SecExt");                       MEASUREMENT; VALUE_u8;
        default:              return 0;
      }
      case 0x40 : switch (i) {                                  // param packet from main controller to external controller
        default:              return 0;
      }
    }
    case 0x35 : switch (linesrc) {
      case 0x00 : switch (i) {                                  // param packet from main controller to external controller
        case 5 :              // fallthrough
        case 8 :              // fallthrough
        case 11:              // fallthrough
        case 14:              // fallthrough
        case 17:              // fallthrough
        case 20:                                                SETTING;     HANDLEPARAM;
        default:              return 0;
      }
      case 0x40 : switch (i) {                                  // param packet from main controller to external controller
        case 5 :              // fallthrough
        case 8 :              // fallthrough
        case 11:              // fallthrough
        case 14:              // fallthrough
        case 17:              // fallthrough
        case 20:                                                SETTING;     HANDLEPARAM;
        default:              return 0;
      }
    }
    case 0x36 : switch (linesrc) {
      case 0x00 : switch (i) {                                  // param packet from main controller to external controller
        case 6 :              // fallthrough
        case 10:              // fallthrough
        case 14:              // fallthrough
        case 18:              // fallthrough
        case 22:                                                SETTING;     HANDLEPARAM;
        default:              return 0;
      }
      case 0x40 : switch (i) {                                  // param packet from main controller to external controller
        case 6 :              // fallthrough
        case 10:              // fallthrough
        case 14:              // fallthrough
        case 18:              // fallthrough
        case 22:                                                SETTING;     HANDLEPARAM;
        default:              return 0;
      }
    }
    default :                 UNKNOWNBYTE;
  }
}

/* WIP: 
byte paramnr2key(byte* rb, uint16_t paramnr, uint16_t paramval, byte j, char* key, char* value) {
// Generates key based on rb, paramnr, j
// if j=8, byte-based handling is performed
// if 0<=j<=7, bit-based handling of bit j of byte rb[i] is performed.
// Upon return, key and value contain the topic/payload for a mqtt message also usable as a key/value for a json message
// Return value:
// 0 no value to return
// 1 (new) value to print/output
// 8 this byte should be treated on bit basis by calling bits2keyvalue() with 0<=j<=7
// 9 as return value indicates the json message should be terminated
//

  byte linetype = rb[2];
  byte linesrc = rb[0];

  // TODO how to handle bit basis?
  switch (paramnr) {
    case 0xA2 : return 0;
    case 0x2E :; //KEY3("Heating1");            SETTING; VALUE_FLAG8;
    default:   ;// UNKNOWNBYTE3;;
  }
}
*/
#endif /* P1P2_Daikin_json_EHYHB */
