#ifndef P1P2_Daikin_json_EHYHB
#define P1P2_Daikin_json_EHYHB
// P1P2-Daikin-json product-dependent code for EHYHB models

// history size parameters
#define RBHLEN 263   // max #bytes saved (263 bytes needed for messages 0x10..0x15 and 0xB8 below)
#define RBHNP  18    // #of messages to be saved in historymax 127 given that shi is int8_t
#define KEYLEN 20    // max length of key/topic or value/payload

#include "P1P2_Daikin_ParameterConversion.h" // for Daikin product-independent code

// device dependent code starts here:
// ----------------------------------
//
// to implement json decoding for another product, rename this file and change the following functions:
// -savehistoryindex() to indicate which packets should be saved
// -savehistoryignore() to determine how many bytes of a packet can be ignored (usually 3 or 4), 3 seems to be a safe value in case of doubt
// -the begin switch statement in bytes2keyvalue()

int8_t savehistoryindex(byte *rb) {
// returns whether rb should be saved,
//    and if so, returns index of rbhistoryp to store history data
// returns -1 if this packet is not to be saved
// returns a value in the range of 0..(RBHNP-1) if message is to be saved
// Example below: save messages 0x10..0x15 from sources 0x00 and 0x40
//                save messages 0xB8_subtype 0x00-0x05 from source 0x40
  if ((rb[2] >= 0x10) && (rb[2] <= 0x15) && ((rb[0] & 0xBF) == 0x00)) {
    return ((rb[2] & 0x0F) + ((rb[0] & 0x40) ? 6 : 0));
  } else if ((rb[2] == 0xB8) && (rb[0] == 0x40) && (rb[3] <= 0x05)) {
    return (rb[3]+12);    
  }
  return -1;
}

uint8_t savehistoryignore(byte *rb) {
// indicates how many bytes at start of packet do *not* need to be stored
// usually the first 3 bytes can be ignored as they indicate src/??/packet-type
// However 0xB8 packets have a sub-type, so we can ignore the first 4 bytes
  return ((rb[2]==0xB8) ? 4 : 3);
}

byte bytes2keyvalue(byte* rb, byte i, byte j, char* key, char* value) {
// Generates key/value pair based on rb,i,j
// if j=8, byte-based handling is performed, and i (i>=3) indicates that rb[i] should be considered within the message rb,
// if 0<=j<=7, bit-based handling of bit j of byte rb[i] is performed.
// Upon return, key and value contain the topic/payload for a mqtt message also usable as a key/value for a json message
// Return value:
// 0 no value to return
// 1 (new) value to print/output
// 8 this byte should be treated on bit basis by calling bits2keyvalue() with 0<=j<=7
// 9 as return value indicates the json message should be terminated
//
// 
// the macro's defined below for KEY and VALUE aim to make the compound switch statement somewhat readable,
// for reuse for other heat pumps
//
// Note also that byte counting protocol documentation starts counting bytes at 1, but we start at 0 as customary in C++
// no break statements are needed as all macro's include a return statement
// 
// For each combination of linetype/linesrce/i(/j), the type of parameter is given as macro, where needed preceded by a KEY value describing the parameter 
// The following macro's can be used below:
//
// BITBASIS (for j==8): this byte is to be treated on a bitbasis (by re-calling this function with j=0..7
//                      switch-statement for j is needed as shown below
//                      indicate for j=0..7: "KEY("KnownParameter"); VALUE_flag8" if bit usage is known
//                      indicate for j=0..7: "UNKNWONBIT", key will then be set to "Bit-0x[source]-0x[packetnr]-0x[location]-[bitnr]"
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
  byte ch = 0;
  float P1,P2;      // these variables cannot be declared within the case statement - not sure why not?
  static float P1prev = 0, P2prev = 0;
  static float LWT = 0, RWT = 0, MWT = 0, Flow = 0;

  switch (linetype) { 
    case 0x10 : switch (linesrc) { 
      case 0x00 : switch (i) { 
        case  3 : switch (j) {  
                    case  8 :                         BITBASIS;
                    case  0 : KEY("Heating1");        VALUE_flag8;
                    default :                         UNKNOWNBIT; 
                  }
        case  4 : switch (j) {  
                    case  8 :                         BITBASIS;
                    case  0 : KEY("Gasmode0");        VALUE_flag8;
                    case  7 : KEY("Gasmode7");        VALUE_flag8;
                    default :                         UNKNOWNBIT; 
                  }
        case  5 : switch (j) {  
                    case  8 :                         BITBASIS;
                    case  0 : KEY("DHWpower");        VALUE_flag8;
                    default :                         UNKNOWNBIT; 
                  }
        case 10 : KEY("RoomTarget1");                 VALUE_u8;
        case 12 :                                     // fallthrough
        case 13 :                                     // fallthrough
        case 15 :                                     // fallthrough
        case 18 :                                     BITBASIS_UNKNOWN;
        case 20 : switch (j) {
                    case  8 :                         BITBASIS;
                    case  1 : KEY("DHWbooster");      VALUE_flag8;
                    case  6 : KEY("DHWoperation");    VALUE_flag8;
                    default :                         UNKNOWNBIT; 
                  }
        case 21 : KEY("DHWTarget1");                  VALUE_u8;
        default :                                     UNKNOWNBYTE;
      }
      case 0x40 : switch (i) { 
        case  3 : switch (j) {
                    case  8 :                         BITBASIS;
                    case  0 : KEY("HeatpumpOn");      VALUE_flag8;
                    default :                         UNKNOWNBIT; 
                  }
        case  4 :                                     BITBASIS_UNKNOWN;
        case  5 : switch (j) {
                    case  8 :                         BITBASIS;
                    case  0 : KEY("Heating2");        VALUE_flag8;
                    case  1 : KEY("Cooling");         VALUE_flag8;
                    case  5 : KEY("MainZone");        VALUE_flag8;
                    case  6 : KEY("AddZone");         VALUE_flag8;
                    case  7 : KEY("DHWtank");         VALUE_flag8;
                    default :                         UNKNOWNBIT; 
                  }
        case  6 : switch (j) {
                    case  8 :                         BITBASIS;
                    case  0 : KEY("3WayValve");       VALUE_flag8;
                    case  4 : KEY("SHCtank");         VALUE_flag8;
                    default :                         UNKNOWNBIT; 
                  }
        case  7 : KEY("DHWTarget2");                  VALUE_u8;
        case  9 :                                     BITBASIS_UNKNOWN;
        case 11 : KEY("RoomTarget2");                 VALUE_u8;
        case 14 : switch (j) {
                    case  8 :                         BITBASIS;
                    case  2 : KEY("Quietmode");       VALUE_flag8;
                    default :                         UNKNOWNBIT; 
                  }
        case 21 : switch (j) {
                    case  8 :                         BITBASIS;
                    case  0 : KEY("Compressor");      VALUE_flag8;
                    case  3 : KEY("Pump");            VALUE_flag8;
                    default :                         UNKNOWNBIT; 
                  }
        case 22 : switch (j) {
                    case  8 :                         BITBASIS;
                    case  2 : KEY("DHWmode");         VALUE_flag8;
                    default :                         UNKNOWNBIT; 
                  }
        default :                                     UNKNOWNBYTE;
      }
      default :                                       UNKNOWNBYTE;
    }
    case 0x11 : switch (linesrc) { 
      case 0x00 : switch (i) { 
        case  3 :                                     return 0;      // Room temperature
        case  4 : KEY("RoomTemp1");                   VALUE_f8_8;    //     (2 bytes)
        default :                                     UNKNOWNBYTE; 
      }
      case 0x40 : switch (i) { 
        case  3 :                                     return 0;      
        case  4 : KEY("TempLWT");LWT=FN_f8_8(&rb[i]); VALUE_f8_8;
        case  5 :                                     return 0;
        case  6 : KEY("TempDHWQ");                    VALUE_f8_8; 
        case  7 :                                     return 0;
        case  8 : KEY("TempOut1");                    VALUE_f8_8;
        case  9 :                                     return 0;
        case 10 : KEY("TempRWT");RWT=FN_f8_8(&rb[i]); VALUE_f8_8;
        case 11 :                                     return 0;
        case 12 : KEY("TempMWT");MWT=FN_f8_8(&rb[i]); VALUE_f8_8;
        case 13 :                                     return 0;
        case 14 : KEY("Temprefr1");                   VALUE_f8_8;
        case 15 :                                     return 0;
        case 16 : KEY("Temproom2");                   VALUE_f8_8;
        case 17 :                                     return 0;
        case 18 : KEY("Tempout2");                    VALUE_f8_8;
        default :                                     UNKNOWNBYTE;
      }
      default :                                       UNKNOWNBYTE;
    }
    case 0x12 : switch (linesrc) {
      case 0x00 : switch (i) {
        case  3 :                                     BITBASIS_UNKNOWN;
        case  4 : KEY("DayOfWeek");                   VALUE_u8;
        case  5 : KEY("Hours");                       VALUE_u8;
        case  6 : KEY("Min");                         VALUE_u8;
        case  7 : KEY("Year");                        VALUE_u8_add2k;
        case  8 : KEY("Month");                       VALUE_u8;
        case  9 : KEY("Day");                         VALUE_u8;
        case 15 :                                     // fallthrough
        case 16 :                                     BITBASIS_UNKNOWN;
        default :                                     UNKNOWNBYTE;
      }
      case 0x40 : switch (i) { 
        case 13 : switch (j) {
                    case  8 :                         BITBASIS;
                    case  4 : KEY("PrefMode");        VALUE_flag8;
                    default :                         UNKNOWNBIT; 
                  }
        case 14 : KEY("Restart");                     VALUE_u8;
        case 15 : switch (j) {
                    case  8 :                         BITBASIS;
                    case  0 : KEY("HeatPump1");       VALUE_flag8;
                    case  6 : KEY("Gas");             VALUE_flag8;
                    case  7 : KEY("DHW");             VALUE_flag8;
                    default :                         UNKNOWNBIT; 
                  }
        default :                                     UNKNOWNBYTE;
      }
      default :                                       UNKNOWNBYTE;
    }
    case 0x13 : switch (linesrc) {
      case 0x00 : switch (i) {
        default :                                     UNKNOWNBYTE;
      }
      case 0x40 : switch (i) {
        case  3 : KEY("TempDHW3");                    VALUE_u8;
        case 12 : KEY("Flow");  Flow=rb[i]/10.0;      VALUE_u8div10;
        case 13 :                                     // fallthrough
        case 14 :                                     // fallthrough
        case 15 :                                     return 0;
        case 16 : KEY("SWversion");                   VALUE_H4;           // sw version 4 bytes
        default :                                     UNKNOWNBYTE;
      }
      default :                                       UNKNOWNBYTE;
    }
    case 0x14 : switch (linesrc) {
      case 0x00 : switch (i) {
        case 11 : KEY("Tempdelta1");                  VALUE_u8delta;
        default :                                     UNKNOWNBYTE;
      }
      case 0x40 : switch (i) {
        case 11 : KEY("Tempdelta2");                  VALUE_u8delta;
        case 18 :                                     return 0;                  
        case 19 : KEY("TsetpMainZone");               VALUE_f8s8;
        case 20 :                                     return 0;                 
        case 21 : KEY("TsetpAddZone");                VALUE_f8s8;
        default :                                     UNKNOWNBYTE;
      }
      default :                                       UNKNOWNBYTE;
    }
    case 0x15 : switch (linesrc) {
      case 0x00 : switch (i) {
      default :                                       UNKNOWNBYTE;
    }
      case 0x40 : switch (i) {
        case  5 :                                     return 0;
        case  6 : KEY("Temprefr2");                   VALUE_f8_8;
        default :                                     UNKNOWNBYTE;
      }
      default :                                       UNKNOWNBYTE;
    }
    case 0x30 : switch (i) {
      case  3 :                                                      // bytes in 0x30 message do not seem to contain usable information
                                                                     // as this is the final packet in a package
                                                                     // we (ab)use this to communicate calculated power
                P1 = (LWT - MWT) * Flow * 0.07;                      // power produced by gas boiler
                ch=0; if (P1 != P1prev) ch=1; P1prev=P1;
                KEY("P1");                            VALUE_F(P1,ch);
      case  4 : P2 = (MWT - RWT) * Flow * 0.07;                      // power produced by heat pump boiler
                ch=0; if (P2 != P2prev) ch=1; P2prev=P2;
                KEY("P2");                            VALUE_F(P2,ch);
      case  5 :                                       TERMINATEJSON; // terminate json string at end of package
      default :                                       return 0;      // these bytes change but don't seem to carry real information
                                                                     // so we return 0 to signal they don't need to be output
    }
    default:                                          UNKNOWNBYTE;
    case 0xB8 : switch (linesrc) {
      case 0x00 :                                     return 0;      // no info in 0x00 packets regardless of subtype
      case 0x40 : switch (rb[3]) {
        case 0x00 : switch (i) {                                     // packet B8 subtype 00
          case  3 :                                   return 0;
          case  4 :                                   return 0;
          case  5 :                                   return 0;
          case  6 : KEY("Counter1q");                 VALUE_u24;
          case  7 :                                   return 0;
          case  8 :                                   return 0; 
          case  9 : KEY("Counter2q");                 VALUE_u24;
          case 10 :                                   return 0;
          case 11 :                                   return 0;
          case 12 : KEY("Counter3q");                 VALUE_u24;
          case 13 :                                   return 0;
          case 14 :                                   return 0;
          case 15 : KEY("ConsElec");                  VALUE_u24;
          default :                                   UNKNOWNBYTE;
        }
        case 0x01 : switch (i) {                                     // packet B8 subtype 01
          case  3 :                                   return 0;
          case  4 :                                   return 0;
          case  5 :                                   return 0;
          case  6 : KEY("HeatProduced1");             VALUE_u24;
          case  7 :                                   return 0;
          case  8 :                                   return 0;
          case  9 : KEY("Counter4q");                 VALUE_u24;
          case 10 :                                   return 0;
          case 11 :                                   return 0;
          case 12 : KEY("Counter5q");                 VALUE_u24;
          case 13 :                                   return 0;
          case 14 :                                   return 0;
          case 15 : KEY("Heatproduced2");             VALUE_u24;
          default :                                   UNKNOWNBYTE;
        }
        case 0x02 : switch (i) {                                     // packet B8 subtype 02
          case  3 :                                   return 0;
          case  4 :                                   return 0;
          case  5 :                                   return 0;
          case  6 : KEY("PumpHours");                 VALUE_u24;
          case  7 :                                   return 0;
          case  8 :                                   return 0;
          case  9 : KEY("CompressorHours");           VALUE_u24;
          case 10 :                                   return 0;
          case 11 :                                   return 0;
          case 12 : KEY("Counter6q");                 VALUE_u24;
          case 13 :                                   return 0;
          case 14 :                                   return 0;
          case 15 : KEY("Counter7q");                 VALUE_u24;
          default :                                   UNKNOWNBYTE;
        }
        case 0x03 : switch (i) {                                     // packet B8 subtype 03
          case  3 :                                   return 0;
          default :                                   UNKNOWNBYTE;
        }
        case 0x04 : switch (i) {                                     // packet B8 subtype 04
          case  3 :                                   return 0;
          case 13 :                                   return 0;
          case 14 :                                   return 0;
          case 15 : KEY("NoStartAttempts");           VALUE_u24;
          default :                                   UNKNOWNBYTE;
        }
        case 0x05 : switch (i) {                                     // packet B8 subtype 05
          case  3 :                                   return 0;
          case  4 :                                   return 0;
          case  5 :                                   return 0;
          case  6 : KEY("BoilerHoursHeating");        VALUE_u24;
          case  7 :                                   return 0;
          case  8 :                                   return 0;
          case  9 : KEY("BoilerHoursDHW");            VALUE_u24;
          case 10 :                                   return 0;
          case 11 :                                   return 0;
          case 12 : KEY("Counter8q");                 VALUE_u24;
          case 13 :                                   return 0;
          case 14 :                                   return 0;
          case 15 : KEY("Counter9q");                 VALUE_u24;
          case 16 :                                   return 0;
          case 17 :                                   return 0;
          case 18 : KEY("Counter10q");                VALUE_u24;
          case 19 :                                   return 0;
          case 20 :                                   return 0;
          case 21 : KEY("Counter11q");                VALUE_u24;
          default :                                   UNKNOWNBYTE;
        }
      default :                                       UNKNOWNBYTE;
      }
    default :                                         UNKNOWNBYTE;
    }
  }
}
#endif /* P1P2_Daikin_json_EHYHB */
