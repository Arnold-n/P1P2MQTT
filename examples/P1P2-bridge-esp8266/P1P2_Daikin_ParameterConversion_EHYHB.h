/* P1P2_Daikin_ParameterConversion_EHYHB.h product-dependent code for EHYHBX08AAV3 and perhaps other EHYHB models
 *
 * Copyright (c) 2019-2022 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * WARNING: P1P2-bridge-esp8266 is end-of-life, and will be replaced by P1P2MQTT
 *
 * Version history
 * 20231019 v0.9.43 solves resetDataStructure bug (counter in HA not visible, introduced in v0.9.41)
 * 20230604 v0.9.37 add heating/cooling/auto setting report
 * 20230526 v0.9.36 threshold
 * 20230423 v0.9.35 (skipped)
 * 20230212 v0.9.33a LWT setpoints added/renamed
 * 20230108 v0.9.31 sensor prefix, +2 valves in HA, fix bit history for 0x30/0x31, +pseudo controlLevel
 * 20221211 v0.9.29 defrost, DHW->gasboiler
 * 20221112 v0.9.27 fix to get Power_* also in HA
 * 20221102 v0.9.24 minor DHW/gas changes in reporting
 * 20221029 v0.9.23 ISPAVR over BB SPI, ADC, misc
 * 20220918 v0.9.22 degree symbol, hwID, 32-bit outputMode
 * 20220821 v0.9.17-fix1 corrected negative deviation temperature reporting, more temp settings reported
 * 20220817 v0.9.17 license change
 * 20220808 v0.9.15 extended verbosity command, unique OTA hostname, minor fixes
 * 20220802 v0.9.14 AVRISP, wifimanager, mqtt settings, EEPROM, telnet, outputMode, outputFilter, ...
 * ..
 *
 */

// This file translates data from the P1/P2 bus to (mqtt_key,mqtt_value) pairs, code here is highly device-dependent,
// in order to implement parameter decoding for another product, rename this file and make any necessary changes
// (in particular in the big switch statements below).
//
// The main conversion function here is bytes2keyvalue(..),
// which is called for every individual byte of every payload.
// It aims to calculate a <mqtt_key,mqtt_value> pair for json/mqtt output.
//
// Its return value determines what to do with the result <mqtt_key,mqtt_value>:
//
// Return value:
// 0 no value to return
// 1 indicates that a new (mqtt_key,mqtt_value) pair can be output via serial, json, and/or mqtt
// 8 this byte should be treated on an individual bit basis by calling bits2keyvalue(.., bitNr) 8 times with 0 <= bitNr <= 7
// 9 no value to return, but json message (if not empty) can be terminated and transmitted
//
// The core of the translation is formed by large nested switch statements.
// Many of the macro's include break statements and make the core more readable.

#ifndef P1P2_Daikin_ParameterConversion_EHYHB
#define P1P2_Daikin_ParameterConversion_EHYHB

#include "P1P2_Config.h"
#include "P1P2Serial_ADC.h"

#define CAT_SETTING      { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'S'); }  // system settings
#define CAT_TEMP         { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'T'); maxOutputFilter = 1; }  // TEMP
#define CAT_MEASUREMENT  { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'M'); maxOutputFilter = 1; }  // non-TEMP measurements
#define CAT_FIELDSETTING { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'F'); } // field settings
#define CAT_COUNTER      { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'C'); } // kWh/hour counters
#define CAT_PSEUDO2      { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'B'); } // 2nd ESP operation
#define CAT_PSEUDO       { if (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] != 'B') mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'A'; } // ATmega/ESP operation
// unused #define CAT_TARGET       { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'D'); } // D desired
// unused #define CAT_DAILYSTATS   { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'R'); } // Results per day (tbd)
#define CAT_SCHEDULE     { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'E'); } // Schedule
#define CAT_UNKNOWN      { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'U'); }

#define SRC(x)           { (mqtt_key[MQTT_KEY_PREFIXSRC - MQTT_KEY_PREFIXLEN] = '0' + x); }

#ifdef __AVR__

// ATmega328 works on 16MHz Arduino Uno, not on Minicore...., use strlcpy_PF, F()
#define KEY(K)        strlcpy_PF(mqtt_key, F(K), MQTT_KEY_LEN - 1)
#define PARAM_KEY(K)  strlcpy_PF(mqtt_key, F(K), MQTT_KEY_LEN - 1)
#define FIELDKEY(K)   { CAT_FIELDSETTING; strlcpy_PF(mqtt_key, F(K), MQTT_KEY_LEN - 1); break; }

#elif defined ESP8266

// ESP8266, use strncpy_P, PSTR()

#include <pgmspace.h>
#define FIELDKEY(K) { CAT_FIELDSETTING; strncpy_P(mqtt_key, PSTR(K), MQTT_KEY_LEN - 1); mqtt_key[MQTT_KEY_LEN - 1] = '\0'; break; }
#ifdef REVERSE_ENGINEER
#define KEY(K) { byte l = snprintf(mqtt_key, MQTT_KEY_LEN, "0x%02X_0x%02X_%i_", packetType, packetSrc, payloadIndex); strncpy_P(mqtt_key + l, PSTR(K), MQTT_KEY_LEN - l); mqtt_key[MQTT_KEY_LEN - 1] = '\0'; };
#define PARAM_KEY(K) { byte l = snprintf(mqtt_key, MQTT_KEY_LEN, "0x%02X_0x%02X_0x%04X_", paramPacketType, paramSrc, paramNr); strncpy_P(mqtt_key + l, PSTR(K), MQTT_KEY_LEN - l); mqtt_key[MQTT_KEY_LEN - 1] = '\0'; };
#else
#define KEY(K) { strncpy_P(mqtt_key, PSTR(K), MQTT_KEY_LEN); mqtt_key[MQTT_KEY_LEN - 1] = '\0'; };
#define PARAM_KEY(K) { strncpy_P(mqtt_key, PSTR(K), MQTT_KEY_LEN); mqtt_key[MQTT_KEY_LEN - 1] = '\0'; };
#endif

#else /* not ESP8266, not AVR, so RPI/Ubuntu/.., use strncpy  */

#define FIELDKEY(K) { CAT_FIELDSETTING; strncpy(mqtt_key, K, MQTT_KEY_LEN - 1); mqtt_key[MQTT_KEY_LEN - 1] = '\0'; break; }
#define KEY(K) { strncpy(mqtt_key, K, MQTT_KEY_LEN - 1); mqtt_key[MQTT_KEY_LEN - 1] = '\0'; };
#define PARAM_KEY(K) { strncpy(mqtt_key, K, MQTT_KEY_LEN); mqtt_key[MQTT_KEY_LEN - 1] = '\0'; };

#endif /* __AVR__ / ESP8266 */

char TimeString1[20] = "Mo 2000-00-00 00:00";    // reads time from packet type 0x12
char TimeString2[23] = "Mo 2000-00-00 00:00:00"; // reads time from packet type 0x31 (+weekday from packet 12)
byte SelectTimeString2 = 0; // to check whether TimeString2 was recently seen
// int TimeStringCnt = 0;  // reset when TimeString is changed, for use in P1P2TimeStamp
byte maxOutputFilter = 0;

#define PARAM_TP_START      0x35
#define PARAM_TP_END        0x3D
#define PARAM_ARR_SZ (PARAM_TP_END - PARAM_TP_START + 1)
const PROGMEM uint32_t  nr_params[PARAM_ARR_SZ] = { 0x014A, 0x002D, 0x0001, 0x001F, 0x00F0, 0x006C, 0x00AF, 0x0002, 0x0020 }; // number of parameters observed
#ifdef SAVEPARAMS
//byte packettype                                    = {   0x35,   0x36,   0x37,   0x38,   0x39,   0x3A,   0x3B,   0x3C,   0x3D };
const PROGMEM uint32_t  parnr_bytes [PARAM_ARR_SZ]   = {      1,      2,      3,      4,      1,      1,      2,      3,      4 }; // byte per parameter // was 8-bit
const PROGMEM uint32_t   valstart[PARAM_ARR_SZ] = { 0x0000, 0x014A, 0x01A4, 0x01A7, 0x0223, 0x0313, 0x037F, 0x04DD, 0x04E3 /* , 0x0563 */ }; // valstart = sum  (parnr_bytes * nr_params)
const PROGMEM uint32_t  seenstart[PARAM_ARR_SZ] = { 0x0000, 0x014A, 0x0177, 0x0178, 0x0197, 0x0287, 0x02FF, 0x03A2, 0x03A4 /* , 0x03C4 */ }; // seenstart = sum (parnr_bytes)
byte paramVal [2][0x0563] = { 0 }; // 2 * 2094 = 4188 bytes 2 * 0x0563
byte paramSeen[2][0x03C4] = { 0 }; // 2 *  959 = 1918 bytes

#endif /* SAVEPARAMS */

#ifdef SAVESCHEDULE
#define SCHEDULE_MEM_START 0x0250
#define SCHEDULE_MEM_SIZE 0x048E          // this model uses only 0x0250 - 0x070D
uint16_t scheduleMemLoc[2];
uint8_t scheduleLength[2];
uint8_t scheduleSeq[2];
byte scheduleMem[2][SCHEDULE_MEM_SIZE] = {};     // 2 * 1166 = 2332 bytes
bool scheduleMemSeen[2][SCHEDULE_MEM_SIZE] = {}; // 2 * 1166 = 2332 bytes
#endif /* SAVESCHEDULE */

#ifdef SAVEPACKETS
#ifdef PSEUDO_PACKETS
#define PCKTP_START  0x08
#define PCKTP_END    0x15 // 0x0D-0x15 and 0x31 to 0x16 0x20 0x21 0x60-0x9F mapped to 0x17-0x58
#define PCKTP_ARR_SZ (PCKTP_END - PCKTP_START + 3 + 18 + 48)
//byte packetsrc                                      = { {00                                                                 }, {  40                                                                       }}
//byte packettype                                     =

// 08,  09,  0A,  0B,  0C,  0D,  0E,  0F,  10,  11,  12,  13,  14,  15,  30,  31,  20,  21,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  6A,  6B,  6C,  6D,  6E,  6F,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  7A,  7B,  7C,  7D,  7E,  7F,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  8A,  8B,  8C,  8D,  8E,  8F,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  9A,  9B,  9C,  9D,  9E,  9F
const PROGMEM uint32_t nr_bytes[2] [PCKTP_ARR_SZ]     =
{{  0,   0,   0,   0,   0,  20,  20,  20,  20,   8,  15,   3,  15,   6,   2,   6,   0,   0,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20 } ,
{   0,  20,  20,  20,   0,  20,  20,  20,  20,  20,  20,  14,  19,   6,   0,   0,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20 }};

const PROGMEM uint32_t bytestart[2][PCKTP_ARR_SZ]     =
{{  0,   0,   0,   0,   0,   0,  20,  40,  60,  80,  88, 103, 106, 121, 127, 129, 135, 155, 175, 195, 215, 235, 255, 275, 295, 315, 335, 355, 375, 395, 415, 435, 455, 475, 495, 515, 535, 555, 575, 595, 615, 635, 655, 675, 695, 715, 735, 755, 775, 795, 815, 835, 855, 875, 895, 915, 935, 955, 975, 995,1015,1035,1055,1075,1095,1115,1135,1155,1175,1195,1215,1235,1255,1275,1295,1315,1335,1355,1375,1395,1415,1435 } ,
{1455,1455,1475,1495,1515,1515,1535,1555,1575,1595,1615,1635,1649,1668,1674,1674,1674,1694,1714,1734,1754,1774,1794,1814,1834,1854,1874,1894,1914,1934,1954,1974,1994,2014,2034,2054,2074,2094,2114,2134,2154,2174,2194,2214,2234,2254,2274,2294,2314,2334,2354,2374,2394,2414,2434,2454,2474,2494,2514,2534,2554,2574,2594,2614,2634,2654,2674,2694,2714,2734,2754,2774,2794,2814,2834,2854,2874,2894,2914,2934,2954,2974
 /* ,sizeValSeen=2994 */ }};
#define sizeValSeen 2994
byte payloadByteVal[sizeValSeen]  = { 0 };
byte payloadByteSeen[sizeValSeen] = { 0 };
#else /* PSEUDO_PACKETS */
#define PCKTP_START  0x10
#define PCKTP_END    0x15 // 0x10-0x15 and 0x31 mapped to 0x16
#define PCKTP_ARR_SZ (PCKTP_END - PCKTP_START + 2)
//byte packetsrc                                  = { {00, 00, 00, 00, 00, 00, 00 }, {40, 40,  40,  40,  40,  40,  40 }}
//byte packettype                                 = { {10, 11, 12, 13, 14, 15, 31 }, {10, 11,  12,  13,  14,  15,  31 }}
const PROGMEM uint32_t nr_bytes[2] [PCKTP_ARR_SZ] = { {20,  8, 15,  3, 15, 6,   6 }, {20, 20,  20,  14,  19,   6,   0 }};
const PROGMEM uint32_t bytestart[2][PCKTP_ARR_SZ] = { { 0, 20, 28, 43, 46, 61, 67 }, {73, 93, 113, 133, 147, 166, 172 /*, sizeValSeen=172 */}};
#define sizeValSeen 172
byte payloadByteVal[sizeValSeen]  = { 0 };
byte payloadByteSeen[sizeValSeen] = { 0 };
#endif /* PSEUDO_PACKETS */
#endif /* SAVEPACKETS */

// for 0xB8 counters we store 36 bytes; 30 should suffice but not worth the extra code

#define EMPTY_PAYLOAD 0xFF

#ifdef SAVEPACKETS
byte cntByte [36] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
#endif /* SAVEPACKETS */

char ha_mqttKey[HA_KEY_LEN];
char ha_mqttValue[HA_VALUE_LEN];
static byte uom = 0;
static byte stateclass = 0;

#define HYSTERESIS_TYPE_F8_8 1
#define HYSTERESIS_TYPE_F8S8 2
#define HYSTERESIS_TYPE_U16DIV10 3
#define HYSTERESIS_TYPE_S16DIV10 4
// #define HYSTERESIS_TYPE_S_F8_8 5 // F-series
#define HYSTERESIS_TYPE_U16 5
#define HYSTERESIS_TYPE_U32 6

#define HYSTERESIS_SIZE_F8_8 20     // 25/256 ~ 0.1 // TODO do better
#define HYSTERESIS_SIZE_F8S8 1      //  1/10  = 0.1
#define HYSTERESIS_SIZE_U16DIV10 1  //  1/10  = 0.1
#define HYSTERESIS_SIZE_S16DIV10 1  //  1/10  = 0.1
// #define HYSTERESIS_TYPE_S_F_S8 2 // F-series


uint16_t FN_u16_LE(uint8_t *b)          { return (                (((uint16_t) b[-1]) << 8) | (b[0]));}
uint32_t FN_u24_LE(uint8_t *b)          { return (((uint32_t) b[-2] << 16) | (((uint32_t) b[-1]) << 8) | (b[0]));}
uint32_t FN_u32_LE(uint8_t *b)          { return (((uint32_t) b[-3] << 24) | ((uint32_t) b[-2] << 16) | (((uint32_t) b[-1]) << 8) | (b[0]));}

void resetDataStructures(void) {
// warning: called in onMqttMessage callback, do not output via mqtt or telnet in this function
  byte i;
  uint16 j;
#ifdef SAVEPARAMS
  for (i = 0; i < 2; i++) {
    for (j = 0; j < 0x0563; j++) paramVal[i][j] = 0;
    for (j = 0; j < 0x03C4; j++) paramSeen[i][j] = 0;
  }
#endif /* SAVEPARAMS */
#ifdef SAVESCHEDULE
  for (i = 0; i < 2; i++) {
    for (j = 0; j < SCHEDULE_MEM_SIZE; j++) scheduleMem[i][j] = 0;
    for (j = 0; j < SCHEDULE_MEM_SIZE; j++) scheduleMemSeen[i][j] = false;
  }
#endif /* SAVESCHEDULE */
#ifdef SAVEPACKETS
  for (j = 0; j < sizeValSeen; j++) {
    payloadByteVal[j]  = 0;
    payloadByteSeen[j] = 0;
  }
  for (j = 0; j < 36; j++) cntByte[j] = 0xFF;
#endif /* SAVEPACKETS */
}

bool newPayloadBytesVal(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, byte haConfig, byte length, bool saveSeen, byte applyHysteresisType = 0, uint16_t applyHysteresis = 0) {
// returns true if a packet parameter is observed for the first time ((and publishes it for homeassistant if haConfig==true)
// if (outputFilter <= maxOutputFilter), it detects if a parameter has changed, and returns true if changed (or if outputFilter==0)
// hysteresis: if (applyHysteresisType), returns true if changed beyond applyHysteresis (assumes f8_8/f8s8/u16div10/s16div10 data format), and if hysteresis difference is not met, does not save new value
// Also, if saveSeen==true, the new value is saved
// In BITBASIS calls, saveSeen should be false such that data can be handled again on bit-basis
//
#ifdef SAVEPACKETS
  bool newByte = (outputFilter == 0);
  byte pts = (packetSrc >> 6) & 0x01;
  byte pti = 0xFF;
  switch (packetType) {
    case 0x08 ... 0x15 : pti = packetType - PCKTP_START;
                break;
    case 0x20 : pti = (PCKTP_END - PCKTP_START) + 3;
                break;
    case 0x21 : pti = (PCKTP_END - PCKTP_START) + 4;
                break;
    case 0x30 : pti = (PCKTP_END - PCKTP_START) + 1;
                break;
    case 0x31 : pti = (PCKTP_END - PCKTP_START) + 2;
                break;
    case 0x60 ... 0x9F : pti = (PCKTP_END - PCKTP_START) + 5 + (packetType - 0x60);
                break;
    default   : /* pti = 0xFF; */ break;
  }
  if (payloadIndex == EMPTY_PAYLOAD) {
    newByte = 0;
  } else if (pti == 0xFF) {
    if (packetType == 0xB8) {
      // Handle 0xB8 history
      uint16_t pi2 = payloadIndex;
      if (pi2 > 13) pi2 += 2;
      pi2 >>= 2; // if payloadIndex is 3, 6, 9, 12, 15, 18, pi2 is now 0..5
      pi2 += 6 * payload[0];
      if (haConfig && (cntByte[pi2] & 0x80)) {
        // MQTT discovery
        // HA key
        HA_KEY
        // HA value
        HA_VALUE
        // publish key,value
        clientPublishMqtt(ha_mqttKey, ha_mqttValue);
      }
      if (cntByte[pi2] != (payload[payloadIndex] & 0x7F) ) { // it's a 3-byte (slow) counter, so sufficient if we look at the 7 least significant bits LSB byte (LE); bit 7 = "!seen"
        newByte = (outputFilter <= maxOutputFilter) || (cntByte[pi2] == 0xFF);
        cntByte[pi2] = payload[payloadIndex] & 0x7F;
      }
      return (haConfig || (outputMode & 0x10000) || !saveSeen) && newByte;
    }
    // not type packet type (0B-)10-15, 31, or B8, so indicate new if to be published:
    newByte = true;
  } else if (payloadIndex > nr_bytes[pts][pti]) {
    // Warning: payloadIndex > expected
    printfTopicS("Warning: payloadIndex %i > expected %i for Src 0x%02X Type 0x%02X", payloadIndex, nr_bytes[pts][pti], packetSrc, packetType);
    newByte = true;
  } else if (payloadIndex + 1 < length) {
    // Warning: payloadIndex + 1 < length
    printfTopicS("Warning: payloadIndex + 1 < length");
    return 0;
  } else {
    bool pubHA = false;
    bool skipHysteresis = false;
    uint16_t pi2base = bytestart[pts][pti];
    if (applyHysteresisType && (pi2base < sizeValSeen) && payloadByteSeen[pi2base + payloadIndex - 1] && payloadByteSeen[pi2base + payloadIndex]) {
      if (applyHysteresisType == HYSTERESIS_TYPE_F8_8) {
        int16_t oldValue = ((int8_t) payloadByteVal[pi2base + payloadIndex - 1]) * 256 + payloadByteVal[pi2base + payloadIndex];
        int16_t newValue = ((int8_t) payload[payloadIndex - 1]) * 256 + payload[payloadIndex];
        skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue >= oldValue - applyHysteresis));
      } else if (applyHysteresisType == HYSTERESIS_TYPE_F8S8) {
        int16_t oldValue = ((int8_t) payloadByteVal[pi2base + payloadIndex - 1]) * 10 + payloadByteVal[pi2base + payloadIndex];
        int16_t newValue = ((int8_t) payload[payloadIndex - 1]) * 10 + payload[payloadIndex];
        skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue >= oldValue - applyHysteresis));
      } else if (applyHysteresisType == HYSTERESIS_TYPE_U16DIV10) {
        uint16_t oldValue = ((int8_t) payloadByteVal[pi2base + payloadIndex - 1]) * 10 + payloadByteVal[pi2base + payloadIndex];
        uint16_t newValue = ((int8_t) payload[payloadIndex - 1]) * 10 + payload[payloadIndex];
        skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue >= oldValue - applyHysteresis));
      } else if (applyHysteresisType == HYSTERESIS_TYPE_S16DIV10) {
        int16_t oldValue = ((int8_t) payloadByteVal[pi2base + payloadIndex - 1]) * 10 + payloadByteVal[pi2base + payloadIndex];
        int16_t newValue = ((int8_t) payload[payloadIndex - 1]) * 10 + payload[payloadIndex];
        skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue >= oldValue - applyHysteresis));
      } else if (applyHysteresisType == HYSTERESIS_TYPE_U16) {
        uint16_t oldValue = FN_u16_LE(&payloadByteVal[pi2base + payloadIndex]);
        uint16_t newValue = FN_u16_LE(&payload[payloadIndex]);
        skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue >= oldValue - applyHysteresis));
      } else if (applyHysteresisType == HYSTERESIS_TYPE_U32) {
        uint32_t oldValue = FN_u32_LE(&payloadByteVal[pi2base + payloadIndex]);
        uint32_t newValue = FN_u32_LE(&payload[payloadIndex]);
        skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue >= oldValue - applyHysteresis));
// F-series
//      } else if (applyHysteresisType == HYSTERESIS_TYPE_S_F8_8) {
//        int16_t oldValue = ((int8_t) payloadByteVal[pi2base + payloadIndex - 1]) * 256 + payloadByteVal[pi2base + payloadIndex];
//        if (payloadByteVal[pi2base + payloadIndex - 2]) oldValue = -oldValue;
//        int16_t newValue = ((int8_t) payload[payloadIndex - 1]) * 256 + payload[payloadIndex];
//        if (payload[payloadIndex - 2]) newValue = -newValue;
//        skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue >= oldValue - applyHysteresis));
      }
    }
    for (byte i = payloadIndex + 1 - length; i <= payloadIndex; i++) {
      uint16_t pi2 = pi2base + i;
      if (pi2 >= sizeValSeen) {
        pi2 = 0;
        printfTopicS("Warning: pi2 %i > sizeValSeen, Src 0x%02X Tp 0x%02X Index %i ", pi2, packetSrc, packetType, i);
        return 0;
      }
      if (payloadByteSeen[pi2]) {
        // this byte or at least some bits have been seen and saved before.
        if (payloadByteVal[pi2] != payload[i]) {
          newByte = (outputFilter <= maxOutputFilter) && !skipHysteresis;
          if (saveSeen && !skipHysteresis) payloadByteVal[pi2] = payload[i];
        } // else payloadByteVal seen and not changed
      } else {
        // first time for this byte
        newByte = 1;
        if (saveSeen) {
          pubHA = haConfig;
          payloadByteSeen[pi2] = 0xFF;
          payloadByteVal[pi2] = payload[i];
        }
      }
    }
    if (pubHA) {
      // MQTT discovery
      // HA key
      HA_KEY
      // HA value
      HA_VALUE
      // publish key,value
      clientPublishMqtt(ha_mqttKey, ha_mqttValue);
    }
  }
  return (haConfig || (outputMode & 0x10000) || !saveSeen) && newByte;
#else /* SAVEPACKETS */
  return (haConfig || (outputMode & 0x10000) || !saveSeen);
#endif /* SAVEPACKETS */
}

bool newPayloadBitsVal(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, byte haConfig, byte bitNr1, byte bitNr2) {
// returns whether bit bitNr of byte payload[payloadIndex] has a new value (also returns true if byte has not been saved)
#ifdef SAVEPACKETS
  bool newBits = (outputFilter == 0);
  byte pts = (packetSrc >> 6);

  byte pti = 0xFF;
  switch (packetType) {
    case 0x08 ... 0x15 : pti = packetType - PCKTP_START;
                break;
    case 0x20 : pti = (PCKTP_END - PCKTP_START) + 3;
                break;
    case 0x21 : pti = (PCKTP_END - PCKTP_START) + 4;
                break;
    case 0x30 : pti = (PCKTP_END - PCKTP_START) + 1;
                break;
    case 0x31 : pti = (PCKTP_END - PCKTP_START) + 2;
                break;
    case 0x60 ... 0x9F : pti = (PCKTP_END - PCKTP_START) + 5 + (packetType - 0x60);
                break;
    default   : /* pti = 0xFF; */ break;
  }
  if (payloadIndex == EMPTY_PAYLOAD) {
    newBits = 0;
  } else if (bitNr1 > 7) {
    // Warning: bitNr1 > 7
    newBits = 1;
  } else if (bitNr2 > 7) {
    // Warning: bitNr2 > 7
    newBits = 1;
  } else if (bitNr1 > bitNr2) {
    // Warning: bitNr1 > bitNr2
    newBits = 1;
  } else if (packetType < PCKTP_START) {
    newBits = 1;
  } else if ((packetType > PCKTP_END) && (packetType != 0x30) && (packetType != 0x31)) {
    newBits = 1;
  } else if (pti == 0xFF) {
    newBits = 1;
  } else if (payloadIndex > nr_bytes[pts][pti]) {
    // Warning: payloadIndex > expected
    newBits = 1;
  } else {
    uint16_t pi2 = bytestart[pts][pti] + payloadIndex; // no multiplication, bit-wise only for u8 type
    if (pi2 >= sizeValSeen) {
      pi2 = 0;
      printfTopicS("Warning: pi2 > sizeValSeen");
    }
    // byte bitMask = 1 << bitNr;
    byte bitsMask = ((0x01 << (bitNr2 - bitNr1 + 1)) - 1) << bitNr1;
    if (payloadByteSeen[pi2] & bitsMask) {
      if ((payloadByteVal[pi2] ^ payload[payloadIndex]) & bitsMask) {
        newBits = (outputFilter <= maxOutputFilter);
        payloadByteVal[pi2] &= (0xFF ^ bitsMask);
        payloadByteVal[pi2] |= (payload[payloadIndex] & bitsMask);
      } // else payloadBits seen and not changed
    } else {
      // first time for these bits
      if (haConfig) {
        // MQTT discovery
        // HA key
        HA_KEY
        // HA value
        HA_VALUE
        // publish key,value
        clientPublishMqtt(ha_mqttKey, ha_mqttValue);
      }
      newBits = 1;
      payloadByteVal[pi2] &= (0xFF ^ bitsMask); // if array initialized to zero, not needed
      payloadByteVal[pi2] |= payload[payloadIndex] & bitsMask;
      payloadByteSeen[pi2] |= bitsMask;
    }
    if (outputFilter > maxOutputFilter) newBits = 0;
  }
  return (haConfig || (outputMode & 0x10000)) && newBits;
#else /* SAVEPACKETS */
  return (haConfig || (outputMode & 0x10000));
#endif /* SAVEPACKETS */
}

bool newPayloadBitVal(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, byte haConfig, byte bitNr) {
  return newPayloadBitsVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, bitNr, bitNr);
}

bool newParamVal(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_key, byte haConfig, byte paramValLength, byte applyHysteresisType = 0, byte applyHysteresis = 0) {
// returns whether paramNr in paramPacketType has new value paramValue
// (also returns true if outputFilter==0)
// and stores new paramVal
#ifdef SAVEPARAMS
  bool newParam = (outputFilter == 0);

  byte pts = (paramSrc >> 6);
  byte pti = paramPacketType - PARAM_TP_START;
  uint16_t ptbv = valstart[pti] + paramNr * parnr_bytes[pti];
  uint16_t ptbs = seenstart[pti] + paramNr;

  if (paramNr >= nr_params[pti]) {
    // Warning: paramNr > PARAM_TP_PARAM_MAX
    newParam = 1;
  } else if (paramPacketType < PARAM_TP_START) {
    // Warning: paramPacketType < PARAM_TP_START
    newParam = 1;
  } else if (paramPacketType > PARAM_TP_END) {
    // Warning: paramPacketType > PARAM_TP_END
    newParam = 1;
  } else {
    if (paramSeen[pts][ptbs]) {
      bool skipHysteresis = false;
      if (applyHysteresisType == HYSTERESIS_TYPE_U16DIV10) {
        uint16_t oldValue = paramVal[pts][ptbv]       | (paramVal[pts][ptbv + 1] << 8);
        uint16_t newValue = payload[payloadIndex - 1] | (payload[payloadIndex] << 8);
        skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue >= oldValue - applyHysteresis));
      } else if (applyHysteresisType == HYSTERESIS_TYPE_S16DIV10) {
        int16_t oldValue = paramVal[pts][ptbv]       | (paramVal[pts][ptbv + 1] << 8);
        int16_t newValue = payload[payloadIndex - 1] | (payload[payloadIndex] << 8);
        skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue >= oldValue - applyHysteresis));
      }
      for (byte i = payloadIndex + 1 - paramValLength; i <= payloadIndex - (paramPacketType == 0x39 ? 3 : 0); i++) {        // 0x39 is field settings so only first byte changes
        if (paramVal[pts][ptbv] != payload[i]) {
          newParam = (outputFilter < maxOutputFilter) && !skipHysteresis;
          if (!skipHysteresis) paramVal[pts][ptbv] = payload[i];
        } // else this byte of paramValue seen and not changed
        ptbv++;
      }
    } else {
      // first time for this param
      if (haConfig) {
        // MQTT discovery
        // HA key
        HA_KEY
        // HA value
        HA_VALUE
        // publish key,value
        clientPublishMqtt(ha_mqttKey, ha_mqttValue);
      }
      newParam = 1;
      for (byte i = payloadIndex + 1 - paramValLength; i <= payloadIndex - (paramPacketType == 0x39 ? 3 : 0); i++) paramVal[pts][ptbv++] = payload[i];
      paramSeen[pts][ptbs] = 1;
    }
    if (outputFilter > maxOutputFilter) newParam = 0;
  }
  return (haConfig || (outputMode & 0x10000)) && newParam;
#else /* SAVEPARAMS */
  return (haConfig || (outputMode & 0x10000));
#endif /* SAVEPARAMS */
}

// these conversion functions from payload to (mqtt) value look at the current and often also one or more past byte(s) in the byte array

// hex (1..4 bytes), LE

uint8_t value_u8hex(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X\"", payload[payloadIndex]);
  return 1;
}

uint8_t value_u16hex_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 2, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X\"", payload[payloadIndex - 1], payload[payloadIndex]);
  return 1;
}

uint8_t value_u24hex_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 3, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X%02X\"", payload[payloadIndex - 2], payload[payloadIndex - 1], payload[payloadIndex]);
  return 1;
}

uint8_t value_u32hex_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 4, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X%02X%02X\"", payload[payloadIndex - 3], payload[payloadIndex - 2], payload[payloadIndex - 1], payload[payloadIndex]);
  return 1;
}

// unsigned integers, LE

uint8_t value_u8(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", payload[payloadIndex]);
  return 1;
}

uint8_t value_u16_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 2, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", FN_u16_LE(&payload[payloadIndex]));
  return 1;
}

uint8_t value_u24_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 3, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", FN_u24_LE(&payload[payloadIndex]));
  return 1;
}

uint8_t value_u32_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 4, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", FN_u32_LE(&payload[payloadIndex]));
  return 1;
}

uint8_t value_u32_LE_uptime(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  uint8_t bitMaskUptime = 0x01;
  if ((payload[payloadIndex - 3] == 0) && (payload[payloadIndex - 2] == 0)) {
    while (payload[payloadIndex - 1] > bitMaskUptime) { bitMaskUptime <<= 1; bitMaskUptime |= 1; }
  } else {
    bitMaskUptime = 0xFF;
  }
  payload[payloadIndex] &= ~(bitMaskUptime >> 1);
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 4, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", FN_u32_LE(&payload[payloadIndex]));
  return 1;
}

// signed integers, LE

uint8_t value_s8(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", (int8_t) payload[payloadIndex]);
  return 1;
}

byte RSSIcnt = 0xFF;
uint8_t value_s8_ratelimited(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (++RSSIcnt) return 0;
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", (int8_t) payload[payloadIndex]);
  return 1;
}

uint8_t value_s16_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 2, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", (int16_t) FN_u16_LE(&payload[payloadIndex]));
  return 1;
}

// single bit

bool FN_flag8(uint8_t b, uint8_t n)  { return (b >> n) & 0x01; }

uint8_t value_flag8(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte bitNr) {
  if (!newPayloadBitVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, bitNr))   return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", FN_flag8(payload[payloadIndex], bitNr));
  return 1;
}

// multiple bits

byte FN_bits(uint8_t b, uint8_t n1, uint8_t n2)  { return (b >> n1) & ((0x01 << (n2 - n1 + 1)) - 1); }

uint8_t value_bits(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte bitNr1, byte bitNr2) {
// use bits bitNr1 .. bitNr2
  if (!newPayloadBitsVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, bitNr1, bitNr2))   return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", FN_bits(payload[payloadIndex], bitNr1, bitNr2));
  return 1;
}

uint8_t unknownBit(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte bitNr) {
  if (!outputUnknown || !(newPayloadBitVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, bitNr))) return 0;
  snprintf(mqtt_key, MQTT_KEY_LEN, "PacketSrc_0x%02X_Type_0x%02X_Byte_%i_Bit_%i", packetSrc, packetType, payloadIndex, bitNr);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", FN_flag8(payload[payloadIndex], bitNr));
  return 1;
}

// misc

uint8_t unknownByte(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!outputUnknown || !newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;
  snprintf(mqtt_key, MQTT_KEY_LEN, "PacketSrc_0x%02X_Type_0x%02X_Byte_%i", packetSrc, packetType, payloadIndex);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X\"", payload[payloadIndex]);
  return 1;
}

uint8_t value_u8_add2k(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", payload[payloadIndex] + 2000);
  return 1;
}

int8_t FN_s4abs1c(uint8_t *b)        { int8_t c = b[0] & 0x0F; if (b[0] & 0x10) return -c; else return c; }

uint8_t value_s4abs1c(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", FN_s4abs1c(&payload[payloadIndex]));
  return 1;
}

float FN_u8div10(uint8_t *b)     { return (b[0] * 0.1);}

uint8_t value_u8div10(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 1, 1 /* , HYSTERESIS_TYPE_U16DIV10, HYSTERESIS_SIZE_U16DIV10 */ )) return 0;
#ifdef __AVR__
  dtostrf(FN_u8div10(&payload[payloadIndex]), 1, 1, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", FN_u8div10(&payload[payloadIndex]));
#endif
  return 1;
}

float FN_u16div10_LE(uint8_t *b)     { if (b[-1] == 0xFF) return 0; else return (b[0] * 0.1 + b[-1] * 25.6);}

uint8_t value_u16div10_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 2, 1, HYSTERESIS_TYPE_U16DIV10, HYSTERESIS_SIZE_U16DIV10)) return 0;
#ifdef __AVR__
  dtostrf(FN_u16div10_LE(&payload[payloadIndex]), 1, 1, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", FN_u16div10_LE(&payload[payloadIndex]));
#endif
  return 1;
}

uint8_t value_trg(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"Empty_Payload_%02X00%02X\"", packetSrc, packetType);
  return 1;
}

// 16 bit fixed point reals

float FN_f8_8(uint8_t *b)            { return (((int8_t) b[-1]) + (b[0] * 1.0 / 256)); }

uint8_t value_f8_8(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 2, 1, HYSTERESIS_TYPE_F8_8, HYSTERESIS_SIZE_F8_8)) return 0;
#ifdef __AVR__
  dtostrf(FN_f8_8(&payload[payloadIndex]), 1, 1, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", FN_f8_8(&payload[payloadIndex]));
#endif
  return 1;
}

float FN_f8s8(uint8_t *b)            { return ((float)(int8_t) b[-1]) + ((float) b[0]) /  10;}

uint8_t value_f8s8(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 2, 1, HYSTERESIS_TYPE_F8S8, HYSTERESIS_SIZE_F8S8)) return 0;
#ifdef __AVR__
  dtostrf(FN_f8s8(&payload[payloadIndex]), 1, 1, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", FN_f8s8(&payload[payloadIndex]));
#endif
  return 1;
}

// change to Watt ?
uint8_t value_f(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, float v, int length = 0, byte thresholdType = 0, uint16_t thresholdSize = 0) {
  if ((length > 0) && (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, length, 1, thresholdType, thresholdSize))) return 0;
  if (!length) newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 1, 1, thresholdType, thresholdSize);
#ifdef __AVR__
  dtostrf(v, 1, 2, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.2f", v);
#endif
  return (outputFilter > maxOutputFilter ? 0 : 1);
}

uint8_t value_s(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, int v, int length = 0) {
  if (length && (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, length, 1))) return 0;
  if (!length) newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 1, 1);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", v);
  return (outputFilter > maxOutputFilter ? 0 : 1);
}

uint8_t value_timeString(char* mqtt_value, char* timestring) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"%s\"", timestring);
  return (outputFilter > maxOutputFilter ? 0 : 1);
}

// Parameters, unsigned, BE

uint8_t param_value_u_BE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte paramValLength) {
  if (!newParamVal(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, haConfig, paramValLength)) return 0;
  uint32_t v = 0;
  byte p = payloadIndex;
  switch (paramValLength) {
    case 4 : v = payload[p--];  v <<= 8; // fallthrough
    case 3 : v |= payload[p--]; v <<= 8; // fallthrough
    case 2 : v |= payload[p--]; v <<= 8; // fallthrough
    case 1 : v |= payload[p]; break;
    default: return 0;
  };
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", v);
  return 1;
}

// Parameters, signed, BE

uint8_t param_value_s_BE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte paramValLength) {
  if (!newParamVal(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, haConfig, paramValLength)) return 0;
  uint32_t v = 0;
  byte p = payloadIndex;
  switch (paramValLength) {
    case 4 : v = payload[p--];  v <<= 8; // fallthrough
    case 3 : v |= payload[p--]; v <<= 8; // fallthrough
    case 2 : v |= payload[p--]; v <<= 8; // fallthrough
    case 1 : v |= payload[p]; break;
    default: return 0;
  };
  if (v & 0x80 << ((paramValLength - 1) << 3)) {
    v = 1 + ((~v) & ~(0xFFFFFF00  << ((paramValLength - 1) << 3)));
    snprintf(mqtt_value, MQTT_VALUE_LEN, "-%i", v);
  } else {
    snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", v);
  }
  return 1;
}

// Parameters, unsigned, LE

uint8_t param_value_u_LE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte paramValLength) {
  if (!newParamVal(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, haConfig, paramValLength)) return 0;
  uint32_t v = 0;
  byte p = payloadIndex - paramValLength;
  switch (paramValLength) {
    case 4 : v = payload[++p];  v <<= 8; // fallthrough
    case 3 : v |= payload[++p]; v <<= 8; // fallthrough
    case 2 : v |= payload[++p]; v <<= 8; // fallthrough
    case 1 : v |= payload[++p]; break;
    default: return 0;
  };
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", v);
  return 1;
}

// Parameters, signed, LE

uint8_t param_value_s_LE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte paramValLength) {
  if (!newParamVal(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, haConfig, paramValLength)) return 0;
  uint32_t v = 0;
  byte p = payloadIndex - paramValLength;
  switch (paramValLength) {
    case 4 : v = payload[++p];  v <<= 8; // fallthrough
    case 3 : v |= payload[++p]; v <<= 8; // fallthrough
    case 2 : v |= payload[++p]; v <<= 8; // fallthrough
    case 1 : v |= payload[++p]; break;
    default: return 0;
  };
  if (v & 0x80 << ((paramValLength - 1) << 3)) {
    v = 1 + ((~v) & ~(0xFFFFFF00  << ((paramValLength - 1) << 3)));
    snprintf(mqtt_value, MQTT_VALUE_LEN, "-%i", v);
  } else {
    snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", v);
  }
  return 1;
}

// u16div10, s16div10 BE

uint8_t param_value_u16div10_BE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte paramValLength) {
// assuming paramValLength = 2
  if (!newParamVal(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, haConfig, paramValLength, HYSTERESIS_TYPE_U16DIV10, HYSTERESIS_SIZE_U16DIV10)) return 0;
  uint16_t v = (payload[payloadIndex] << 8) | payload[payloadIndex - 1];
#ifdef __AVR__
  dtostrf(v * 0.1, 1, 1, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", v * 0.1);
#endif
  return 1;
}

uint8_t param_value_s16div10_BE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte paramValLength) {
// assuming paramValLength = 2
  if (!newParamVal(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, haConfig, paramValLength, HYSTERESIS_TYPE_S16DIV10, HYSTERESIS_SIZE_S16DIV10)) return 0;
  int16_t v = (payload[payloadIndex] << 8) | payload[payloadIndex - 1];
#ifdef __AVR__
  dtostrf(v * 0.1, 1, 1, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", v * 0.1);
#endif
  return 1;
}

// u16div10_and_u16hex, BE, useful for reverse engineering parameters

uint8_t param_value_u16div10_and_u16hex_BE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte paramValLength) {
// assuming paramValLength = 2
  if (!newParamVal(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, haConfig, paramValLength)) return 0;
  uint16_t v = (payload[payloadIndex] << 8) | payload[payloadIndex - 1];
#ifdef __AVR__
  mqtt_value[0] = '\"';
  dtostrf(v * 0.1, 1, 1, mqtt_value + 1);
  snprintf(mqtt_value + strlen(mqtt_value), MQTT_VALUE_LEN - strlen(mqtt_value), "0x%02X%02X\"", payload[payloadIndex - 1], payload[payloadIndex]);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"%1.1f_0x%04X\"", v * 0.1, v);
#endif
  return 1;
}

// misc, hex output, LE (switching to BE)

/*
byte unknownParam_LE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte paramValLength) {
  if (!outputUnknown || !newParamVal(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, haConfig, paramValLength)) return 0;
  snprintf(mqtt_key, MQTT_KEY_LEN, "ParamSrc_0x%02X_Type_0x%02X_Nr_0x%04X", paramSrc, paramPacketType, paramNr);
  switch (paramValLength) {
    case 1 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X\"", payload[payloadIndex]); break;
    case 2 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X\"", payload[payloadIndex - 1], payload[payloadIndex]); break;
    case 3 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X%02X\"", payload[payloadIndex - 2], payload[payloadIndex - 1], payload[payloadIndex]); break;
    case 4 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X%02X%02X\"", payload[payloadIndex - 3], payload[payloadIndex - 2], payload[payloadIndex - 1], payload[payloadIndex]); break;
    default : return 0;
  }
  return 1;
}

byte param_value_hex_LE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte paramValLength) {
  if (!newParamVal(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, haConfig, paramValLength)) return 0;
  switch (paramValLength) {
    case 1 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X\"", payload[payloadIndex]); break;
    case 2 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X\"", payload[payloadIndex - 1], payload[payloadIndex]); break;
    case 3 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X%02X\"", payload[payloadIndex - 2], payload[payloadIndex - 1], payload[payloadIndex]); break;
    case 4 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X%02X%02X\"", payload[payloadIndex - 3], payload[payloadIndex - 2], payload[payloadIndex - 1], payload[payloadIndex]); break;
  }
  return 1;
}
*/

byte unknownParam_BE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte paramValLength) {
  if (!outputUnknown || !newParamVal(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, haConfig, paramValLength)) return 0;
  snprintf(mqtt_key, MQTT_KEY_LEN, "ParamSrc_0x%02X_Type_0x%02X_Nr_0x%04X", paramSrc, paramPacketType, paramNr);
  switch (paramValLength) {
    case 1 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X\"", payload[payloadIndex]); break;
    case 2 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X\"", payload[payloadIndex], payload[payloadIndex - 1]); break;
    case 3 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X%02X\"", payload[payloadIndex], payload[payloadIndex - 1], payload[payloadIndex - 2]); break;
    case 4 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X%02X%02X\"", payload[payloadIndex], payload[payloadIndex - 1], payload[payloadIndex - 2], payload[payloadIndex - 3]); break;
    default : return 0;
  }
  return 1;
}

byte param_value_hex_BE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte paramValLength) {
  if (!newParamVal(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, haConfig, paramValLength)) return 0;
  switch (paramValLength) {
    case 1 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X\"", payload[payloadIndex]); break;
    case 2 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X\"", payload[payloadIndex], payload[payloadIndex - 1]); break;
    case 3 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X%02X\"", payload[payloadIndex], payload[payloadIndex - 1], payload[payloadIndex - 2]); break;
    case 4 : snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X%02X%02X\"", payload[payloadIndex], payload[payloadIndex - 1], payload[payloadIndex - 2], payload[payloadIndex - 3]); break;
  }
  return 1;
}

uint8_t common_field_setting(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, uint16_t paramNr, char* mqtt_key, char* mqtt_value) {
  switch (paramNr) {
    // field settings [0_XX] (don't use - in FIELDKEY/topic)
    case 0x00 : FIELDKEY("[0_00]_A.3.1.2.3_LWT_Heating_Add_WD_Curve_High_Amb_LWT");
    case 0x01 : FIELDKEY("[0_01]_A.3.1.2.3_LWT_Heating_Add_WD_Curve_Low_Amb_LWT");
    case 0x02 : FIELDKEY("[0_02]_A.3.1.2.3_LWT_Heating_Add_WD_Curve_High_Amb");
    case 0x03 : FIELDKEY("[0_03]_A.3.1.2.3_LWT_Heating_Add_WD_Curve_Low_Amb");
    case 0x04 : FIELDKEY("[0_04]_A.3.1.2.4_LWT_Cooling_Add_WD_Curve_High_Amb_LWT");
    case 0x05 : FIELDKEY("[0_05]_A.3.1.2.4_LWT_Cooling_Add_WD_Curve_Low_Amb_LWT");
    case 0x06 : FIELDKEY("[0_06]_A.3.1.2.4_LWT_Cooling_Add_WD_Curve_High_Amb");
    case 0x07 : FIELDKEY("[0_07]_A.3.1.2.4_LWT_Cooling_Add_WD_Curve_Low_Amb");         // cooling/heating reversed? (A.8 other information than A.3.1.2 ??
    case 0x0B : FIELDKEY("[0_0B]_A.4.7_DHW_WD_Curve_High_Amb_Setpoint");
    case 0x0C : FIELDKEY("[0_0C]_A.4.7_DHW_WD_Curve_Low_Amb_Setpoint");
    case 0x0D : FIELDKEY("[0_0D]_A.4.7_DHW_WD_Curve_High_Amb");
    case 0x0E : FIELDKEY("[0_0E]_A.4.7_DHW_WD_Curve_Low_Amb");
    // field settings [1_XX]
    case 0x0F : FIELDKEY("[1_00]_A.3.1.1.3_LWT_Heating_Main_WD_Curve_Low_Amb"); // graph -20 - 5
    case 0x10 : FIELDKEY("[1_01]_A.3.1.1.3_LWT_Heating_Main_WD_Curve_High_Amb"); // 10-20
    case 0x11 : FIELDKEY("[1_02]_A.3.1.1.3_LWT_Heating_Main_WD_Curve_Low_Amb_LWT"); // [9_00] - [9_01] def 60
    case 0x12 : FIELDKEY("[1_03]_A.3.1.1.3_LWT_Heating_Main_WD_Curve_High_Amb_LWT"); // [9_00] - min(45,[9_01]) def 35
    case 0x13 : FIELDKEY("[1_04]_LWT_Cooling_Main_WD_Enabled"); // 0 disabled, 1 (default) enabled
    case 0x14 : FIELDKEY("[1_05]_LWT_Cooling_Add_WD_Enabled"); // 0 disabled, 1 (default) enabled
    case 0x15 : FIELDKEY("[1_06]_A.3.1.1.4_LWT_Cooling_Main_WD_Curve_Low_Amb"); // graph 10-25
    case 0x16 : FIELDKEY("[1_07]_A.3.1.1.4_LWT_Cooling_Main_WD_Curve_High_Amb"); // 25-43
    case 0x17 : FIELDKEY("[1_08]_A.3.1.1.4_LWT_Cooling_Main_WD_Curve_Low_Amb_LWT"); // [9_03] - [9_02]
    case 0x18 : FIELDKEY("[1_09]_A.3.1.1.4_LWT_Cooling_Main_WD_Curve_High_Amb_LWT"); // [9_03] - [9_02]
    case 0x19 : FIELDKEY("[1_0A]_A.6.4_Averaging_Time"); // 0: no avg, 1 (default) 12 h, 2 24h, 3 48h, 4 72h
    // field settings [2_XX]
    case 0x1E : FIELDKEY("[2_00]_A.4.4.2_DHW_Disinfection_Operation_Day");
    case 0x1F : FIELDKEY("[2_01]_A.4.4.1_DHW_Disinfection");
    case 0x20 : FIELDKEY("[2_02]_A.4.4.3_DHW_Disinfection_Start_Time");
    case 0x21 : FIELDKEY("[2_03]_A.4.4.4_DHW_Disinfection_Target_Temperature");
    case 0x22 : FIELDKEY("[2_04]_A.4.4.5_DHW_Disinfection_Duration");
    case 0x23 : FIELDKEY("[2_05]_Room_Antifrost_Temperature"); // 4-16 step 1 default 8
    case 0x24 : FIELDKEY("[2_06]_Unspecified_2_06_ro_1");
    case 0x27 : FIELDKEY("[2_09]_A.3.2.3_Ext_Room_Sensor_Offset"); // -5 .. 5 step 0.5
    case 0x28 : FIELDKEY("[2_0A]_A.3.2.2_Room_Temperature_Offset"); // -5 .. 5 step 0.5
    case 0x29 : FIELDKEY("[2_0B]_A.6.5_Ext_Amb_Sensor_Offset"); // -5..5 step 0.5
    // field settings [3_XX]
    case 0x2D : FIELDKEY("[3_00]_A.6.1_Auto_Restart_Allowed"); // 1 No, 1 default Yes
    case 0x2E : FIELDKEY("[3_01]_Unspecified_3_01_rw_0");
    case 0x2F : FIELDKEY("[3_02]_Unspecified_3_02_rw_1");
    case 0x30 : FIELDKEY("[3_03]_Unspecified_3_03_rw_4");
    case 0x31 : FIELDKEY("[3_04]_Unspecified_3_04_rw_2");
    case 0x32 : FIELDKEY("[3_05]_Unspecified_3_05_rw_1");
    case 0x33 : FIELDKEY("[3_06]_A.3.2.1.2_Temp_Range_Room_Heating_Max");
    case 0x34 : FIELDKEY("[3_07]_A.3.2.1.1_Temp_Range_Room_Heating_Min");
    case 0x35 : FIELDKEY("[3_08]_A.3.2.1.4_Temp_Range_Room_Cooling_Max");
    case 0x36 : FIELDKEY("[3_09]_A.3.2.1.3_Temp_Range_Room_Cooling_Min");
    // field settings [4_XX]
    case 0x3C : FIELDKEY("[4_00]_Unspecified_4_00_ro_1");
    case 0x3D : FIELDKEY("[4_01]_Unspecified_4_01_ro_0");
    case 0x3E : FIELDKEY("[4_02]_A.3.3.1_Heating_Amb_Temperature_Max"); // space heating OFF temp, 14-35
    case 0x3F : FIELDKEY("[4_03]_Unspecified_4_03_ro_3");
    case 0x40 : FIELDKEY("[4_04]_Unspecified_4_04_rw_1");
    case 0x41 : FIELDKEY("[4_05]_Unspecified_4_05_rw_0");
    case 0x42 : FIELDKEY("[4_06]_Unspecified_4_06_rw_0");
    case 0x43 : FIELDKEY("[4_07]_Unspecified_4_07_ro_1");
    case 0x44 : FIELDKEY("[4_08]_A.6.3.1_Power_Consumption_Control_Mode"); // 0 default: no limitation, 1 continuous, 2 digital inputs
    case 0x45 : FIELDKEY("[4_09]_A.6.3.2_Power_Consumption_Control_Type"); // 0 Current, 1 Power
    case 0x47 : FIELDKEY("[4_0B]_Auto_Cooling_Heating_Changeover_Hysteresis"); //1-10 step 0.5 default 1
    case 0x49 : FIELDKEY("[4_0D]_Auto_Cooling_Heating_Changeover_Offset"); // 1-10 step0.5 default 3
    // field settings [5_XX]
    case 0x4B : FIELDKEY("[5_00]_Unspecified_5_00_rw_0");
    case 0x4C : FIELDKEY("[5_01]_A.5.2.2_Equilibrium_Temp");
    case 0x4D : FIELDKEY("[5_02]_Unspecified_5_02_ro_0");
    case 0x4E : FIELDKEY("[5_03]_Unspecified_5_03_rw_0");
    case 0x4F : FIELDKEY("[5_04]_Unspecified_5_04_rw_10");
    case 0x50 : FIELDKEY("[5_05]_A.6.3.3_A.6.3.5.1_Power_Consumption_Control_Amp_Value_1"); // 0-50A, step 1A
    case 0x51 : FIELDKEY("[5_06]_A.6.3.5.1_Power_Consumption_Control_Amp_Value_2");
    case 0x52 : FIELDKEY("[5_07]_A.6.3.5.1_Power_Consumption_Control_Amp_Value_3");
    case 0x53 : FIELDKEY("[5_08]_A.6.3.5.1_Power_Consumption_Control_Amp_Value_4");
    case 0x54 : FIELDKEY("[5_09]_A.6.3.4_A.6.3.6.1_Power_Consumption_Control_kW_Value_1"); // 0-20kW, step 0.5kW
    case 0x55 : FIELDKEY("[5_0A]_A.6.3.6.1_Power_Consumption_Control_kW_Value_2");
    case 0x56 : FIELDKEY("[5_0B]_A.6.3.6.1_Power_Consumption_Control_kW_Value_3");
    case 0x57 : FIELDKEY("[5_0C]_A.6.3.6.1_Power_Consumption_Control_kW_Value_4");
    case 0x58 : FIELDKEY("[5_0D]_Unspecified_5_0D_ro_1");
    case 0x59 : FIELDKEY("[5_0E]_Unspecified_5_0E_ro_0");
    // field settings [6_XX]
    case 0x5A : FIELDKEY("[6_00]_Temp_Diff_Determining_On"); // 0-20 step 1 default 2
    case 0x5B : FIELDKEY("[6_01]_Temp_Diff_Determining_Off"); // 0-20 step 1 default 2
    case 0x5C : FIELDKEY("[6_02]_Unspecified_6_02_ro_0");
    case 0x5D : FIELDKEY("[6_03]_Unspecified_6_03_ro_0");
    case 0x5E : FIELDKEY("[6_04]_Unspecified_6_04_ro_0");
    case 0x5F : FIELDKEY("[6_05]_Unspecified_6_05_ro_0");
    case 0x60 : FIELDKEY("[6_06]_Unspecified_6_06_ro_0");
    case 0x61 : FIELDKEY("[6_07]_Unspecified_6_07_rw_0");
    case 0x62 : FIELDKEY("[6_08]_Unspecified_6_08_rw_5");
    case 0x63 : FIELDKEY("[6_09]_Unspecified_6_09_rw_0");
    case 0x64 : FIELDKEY("[6_0A]_7.4.3.1_Tank_Storage_Comfort"); // 30 - [6_0E], step 1
    case 0x65 : FIELDKEY("[6_0B]_7.4.3.2_Tank_Storage_Eco"); // 30-min(50,[6_0E]), step
    case 0x66 : FIELDKEY("[6_0C]_7.4.3.3_Tank_Reheat"); //idem
    case 0x67 : FIELDKEY("[6_0D]_A.4.1_DHW_Reheat"); // 0 reheat_only 1 reheat+schedule 2 (default) schedule_only
    case 0x68 : FIELDKEY("[6_0E]_A.4.5_DHW_Setpoint_Max");
    // field settings [7_XX]
    case 0x69 : FIELDKEY("[7_00]_Unspecified_7_00_ro_0");
    case 0x6A : FIELDKEY("[7_01]_Unspecified_7_01_ro_2");
    case 0x6B : FIELDKEY("[7_02]_A.2.1.8_Number_Of_LWT_Zones"); // 0-1
    case 0x6C : FIELDKEY("[7_03]_PE_Factor"); // PE factor 0-6, step 0.1, default 2.5
    case 0x6D : FIELDKEY("[7_04]_A.6.7_Savings_Mode_Ecological"); // 0 economical 1 ecological
    case 0x6E : FIELDKEY("[7_05]_Unspecified_7_05_ro_0");
    // field settings [8_XX]
    case 0x78 : FIELDKEY("[8_00]_DHW_Running_Time_Min"); // 0-20 min step 1 min default 5 min
    case 0x79 : FIELDKEY("[8_01]_DHW_Running_Time_Max"); // 5-95 min step 5 min default 30 min
    case 0x7A : FIELDKEY("[8_02]_Anti_Recycling_Time"); // 0-10 hour step 0.5 hour default 30 min
    case 0x7B : FIELDKEY("[8_03]_Unspecified_8_03_ro_50");
    case 0x7C : FIELDKEY("[8_04]_Unspecified_8_04_ro_0");
    case 0x7D : FIELDKEY("[8_05]_A.3.1.1.5_LWT_Modulation_Allow"); // 0 No, 1 (default) Yes
    case 0x7E : FIELDKEY("[8_06]_LWT_Modulation_Max"); // 0: No, 1 (default): Yes
    case 0x7F : FIELDKEY("[8_07]_7.4.2.3_LWT_Main_Cooling_Comfort"); // [9_03] - [9_02] step 1, default 45
    case 0x80 : FIELDKEY("[8_08]_7.4.2.4_LWT_Main_Cooling_Eco"); // [9_03] - [9_02] step 1, default 45
    case 0x81 : FIELDKEY("[8_09]_7.4.2.1_LWT_Main_Heating_Comfort"); // [9_01] - [9_00] step 1, default 45
    case 0x82 : FIELDKEY("[8_0A]_7.4.2.2_LWT_Main_Heating_Eco"); // [9_01] - [9_00] step 1, default 45
    case 0x83 : FIELDKEY("[8_0B]_Flow_Target_Heatpump"); // 10-20 step 0.5 default 15
    case 0x84 : FIELDKEY("[8_0C]_Flow_Target_Hybrid"); // 10-20 step 0.5 default 10
    case 0x85 : FIELDKEY("[8_0D]_Flow_Target_Gasboiler"); // 10-20 step 0.5 default 14
    // field settings [9_XX]
    case 0x87 : FIELDKEY("[9_00]_A.3.1.1.2.2_Temp_Heating_Main_Max"); // 37-80 step 1
    case 0x88 : FIELDKEY("[9_01]_A.3.1.1.2.1_Temp_Heating_Main_Min"); // 15-37 step 1
    case 0x89 : FIELDKEY("[9_02]_A.3.1.1.2.4_Temp_Cooling_Main_Max"); // 18-22 step
    case 0x8A : FIELDKEY("[9_03]_A.3.1.1.2.3_Temp_Cooling_Main_Min"); // 5-18 step 1
    case 0x8B : FIELDKEY("[9_04]_Unspecified_9_04_ro_1");
    case 0x8C : FIELDKEY("[9_05]_A.3.1.2.2.1_Temp_Heating_Add_Max");
    case 0x8D : FIELDKEY("[9_06]_A.3.1.2.2.2_Temp_Heating_Add_Min");
    case 0x8E : FIELDKEY("[9_07]_A.3.1.2.2.3_Temp_Cooling_Add_Max");
    case 0x8F : FIELDKEY("[9_08]_A.3.1.2.2.4_Temp_Cooling_Add_Min");
    case 0x90 : FIELDKEY("[9_09]_Unspecified_9_09_rw_5");
    case 0x91 : FIELDKEY("[9_0A]_Unspecified_9_0A_rw_5");
    case 0x92 : FIELDKEY("[9_0B]_A.3.1.1.7_Emitter_Slow"); // 0 quick, 1 slow
    case 0x93 : FIELDKEY("[9_0C]_Room_Temperature_Hysteresis"); // 1-6 step 0.5 default 1.0C
    // field settings [A_XX]
    case 0x96 : FIELDKEY("[A_00]_Unspecified_A_00_0"); // 0-(0)-0 or 0-(1)-7  for src=2? oth=01/09. Stepbits=01.
    case 0x97 : FIELDKEY("[A_01]_Unspecified_A_01_0"); // 0-(0)-0 or 0-(1)-7  for src=2?
    case 0x98 : FIELDKEY("[A_02]_Unspecified_A_02_0"); // 0-(0)-0 or 0-(1)-7  for src=2?
    case 0x99 : FIELDKEY("[A_03]_Unspecified_A_03_0"); // 0-(0)-0 or 0-(1)-7  for src=2?
    case 0x9A : FIELDKEY("[A_04]_Unspecified_A_04_0"); // 0-(0)-0 or 0-(1)-7  for src=2?
    // field settings [B_XX]
    case 0xA5 : FIELDKEY("[B_00]_Unspecified_B_00_0");
    case 0xA6 : FIELDKEY("[B_01]_Unspecified_B_01_0");
    case 0xA7 : FIELDKEY("[B_02]_Unspecified_B_02_0");
    case 0xA8 : FIELDKEY("[B_03]_Unspecified_B_03_0");
    case 0xA9 : FIELDKEY("[B_04]_Unspecified_B_04_0");
    // field settings [C_XX]
    case 0xB4 : FIELDKEY("[C_00]_DHW_Priority"); // 0 (default) Solar priority, 1: heat pump priority
    case 0xB5 : FIELDKEY("[C_01]_Unspecified_C_01_ro_0");
    case 0xB6 : FIELDKEY("[C_02]_Unspecified_C_02_ro_0");
    case 0xB7 : FIELDKEY("[C_03]_Unspecified_C_03_rw_0");
    case 0xB8 : FIELDKEY("[C_04]_Unspecified_C_04_ro_3");
    case 0xB9 : FIELDKEY("[C_05]_A.2.2.4_Contact_Type_Main"); // 1 thermo on/off 2 C/H request
    case 0xBA : FIELDKEY("[C_06]_A.2.2.5_Contact_Type_Add"); // 1 thermo on/off 2 C/H request
    case 0xBB : FIELDKEY("[C_07]_A.2.1.7_Unit_Control_Method"); // 0 LWT control, 1 Ext-RT control, 2 default=RT control
    case 0xBC : FIELDKEY("[C_08]_A.2.2.B_External_Sensor"); // 0 No, 1 Outdoor sensor, 2 Room sensor
    case 0xBD : FIELDKEY("[C_09]_A.2.2.6.3_Digital_IO_PCB_Alarm_Output"); // 0 normally open, 1 normally closed
    case 0xBE : FIELDKEY("[C_0A]_Indoor_Quick_Heatup_Enable"); // 0:disable, 1 default enable
    case 0xC0 : FIELDKEY("[C_0C]_7.4.5.1_Electricity_Price_High"); // together with D-0C
    case 0xC1 : FIELDKEY("[C_0D]_7.4.5.2_Electricity_Price_Medium"); // together with D-0D
    case 0xC2 : FIELDKEY("[C_0E]_7.4.5.3_Electricity_Price_Low"); // together with D-0E
    // field settings [D_XX]
    case 0xC3 : FIELDKEY("[D_00]_Unspecified_D_00_rw_0");
    case 0xC4 : FIELDKEY("[D_01]_A.2.1.6_Preferential_kWh_Rate"); // 0 no, 1 active-open, 2 active-closed
    case 0xC5 : FIELDKEY("[D_02]_A.2.2.A_DHW_Pump"); // 0 No, 1 Secondary rtm ([E-06] = 1), 2 Disinf. Shunt ([E_06] = 1)
    case 0xC6 : FIELDKEY("[D_03]_LWT_0C_Compensation_Shift"); // 0 (default) Disabled, 1: 2C (-2..+2), 2: 4C (-2..+2), 3: 2C (-4..+4C), 4C (-4..+4C)
    case 0xC7 : FIELDKEY("[D_04]_A.2.2.7_Demand_PCB"); // 0 No, 1 power consumption control
    case 0xC8 : FIELDKEY("[D_05]_Unspecified_D_05_ro_0");
    case 0xCA : FIELDKEY("[D_07]_A.2.2.6.2_Digital_IO_PCB_Solar_Kit"); // 0-1
    case 0xCB : FIELDKEY("[D_08]_A.2.2.8_External_kWh_Meter_1"); // 0: No, 1-5: 0.1 - 1000 pulses/kWh
    case 0xCC : FIELDKEY("[D_09]_Unspecified_D_09_ro_0");
    case 0xCD : FIELDKEY("[D_0A]_A.2.2.C_External_Gas_Meter_1"); // 0 Not, 1-3 1-10-100 /m3
    case 0xCE : FIELDKEY("[D_0B]_Unspecified_D_0B_rw_2");
    case 0xCF : FIELDKEY("[D_0C]_7.4.5.1_Electricity_Price_High_do_not_use"); // together with C-0C
    case 0xD0 : FIELDKEY("[D_0D]_7.4.5.2_Electricity_Price_Medium_do_not_use"); // together with C-0D
    case 0xD1 : FIELDKEY("[D_0E]_7.4.5.3_Electricity_Price_Low_do_not_use"); // together with C-0E
    // field settings [E_XX]
    case 0xD2 : FIELDKEY("[E_00]_A.2.1.1_RO_Unit_Type"); // RO 3
    case 0xD3 : FIELDKEY("[E_01]_A.2.1.2_RO_Compressor_Type"); // RO 0
    case 0xD4 : FIELDKEY("[E_02]_A.2.1.3_RO_Indoor_Software_Type"); // RO 1 or 2
    case 0xD5 : FIELDKEY("[E_03]_Unspecified_E_03_ro_0");
    case 0xD6 : FIELDKEY("[E_04]_A.2.1.A_RO_Power_Saving_Available"); // RO 1=yes
    case 0xD7 : FIELDKEY("[E_05]_A.2.2.1_DHW_Operation"); // 0-1
    case 0xD8 : FIELDKEY("[E_06]_A.2.2.2_DHW_Tank_Installed"); // 0-1
    case 0xD9 : FIELDKEY("[E_07]_A.2.2.3_DHW_Tank_Type"); // 4 do-not-change
    case 0xDA : FIELDKEY("[E_08]_Power_Saving_Enabled"); // 0: Disabled, 1 (default): Enabled
    case 0xDB : FIELDKEY("[E_09]_Unspecified_E_09_rw_0");
    case 0xDC : FIELDKEY("[E_0A]_Unspecified_E_0A_ro_0");
    // field settings [F_XX]
    case 0xE1 : FIELDKEY("[F_00]_Pump_Operation_Outside_Range_Allowed"); // 0: (default) disabled, 1: enabled
    case 0xE2 : FIELDKEY("[F_01]_A.3.3.2_Cooling_Amb_Temperature_Min"); // space cooling On temp 10-35 def 20
    case 0xE3 : FIELDKEY("[F_02]_Unspecified_F_02_rw_3");
    case 0xE4 : FIELDKEY("[F_03]_Unspecified_F_03_rw_5");
    case 0xE5 : FIELDKEY("[F_04]_Unspecified_F_04_rw_0");
    case 0xE6 : FIELDKEY("[F_05]_Unspecified_F_05_rw_0");
    case 0xE7 : FIELDKEY("[F_06]_Unspecified_F_06_rw_0");
    case 0xEA : FIELDKEY("[F_09]_Pump_Operation_During_Flow_abnormality"); // 0: (default) disabled, 1: enabled
    case 0xEB : FIELDKEY("[F_0A]_Unspecified_F_0A_rw_0");
    case 0xEC : FIELDKEY("[F_0B]_A.3.1.1.6.1_Shutoff_Valve_Closed_During_Thermo_OFF"); // 0 default No, 1 Yes
    case 0xED : FIELDKEY("[F_0C]_A.3.1.1.6.2_Shutoff_Valve_Closed_During_Cooling"); // 0 No, 1 default Yes
    case 0xEE : FIELDKEY("[F_0D]_A.2.1.9_Pump_Operation_Mode"); // 0 continuous, 1 sample ([C-07]=0, LWT control), 2 request ([C-07] <> 0, RT control)
    case 0xFFFF : return 0; // dummy field setting // TODO check
    default   : // 31x packetSrc = 0x40 (src = '1'): all printed values zero.
                byte f = packetSrc ? 0xFF : 0x00;
                if ((payload[payloadIndex] == (f | 0x01) )   && (payload[payloadIndex - 1] == f) && (payload[payloadIndex - 2] == f)  && (payload[payloadIndex - 3] == f)) return 0;
                if ((payload[payloadIndex] ==  f         )   && (payload[payloadIndex - 1] == f) && (payload[payloadIndex - 2] == f)  && (payload[payloadIndex - 3] == f)) return 0; // TODO check ?
                printfTopicS("* Fieldsetting unknown 0x%02X 0x%02X    0x%02X 0x%02X 0x%02X 0x%02X", packetSrc, paramNr, payload[payloadIndex - 3], payload[payloadIndex - 2], payload[payloadIndex - 1], payload[payloadIndex]);
                FIELDKEY("Fieldsetting_Unknown");
  }
  // byte 2
  int8_t fieldSettingMin          = (payload[payloadIndex - 1] & 0x80) ? -(int8_t) (payload[payloadIndex - 1] & 0x7F) : (int8_t) payload[payloadIndex - 1];
  // byte 3 step as FP
  byte fieldSettingStepExponentSign = (payload[payloadIndex] & 0x80) >> 7;  // bit 7
  byte fieldSettingStepMantissa = (payload[payloadIndex] & 0x38) >> 3; // perhaps &0x78         // bits (6?), 5, 4, 3
  byte fieldSettingStepExponent = (payload[payloadIndex] & 0x02) >> 1; // perhaps &0x06         // bits (2?), 1
  // byte3 unknown
  byte fieldSettingUnknownBitsStep = payload[payloadIndex] & 0x45; // bits 6,2,0  : '1': bit0=0, '2': bit0=1. bits 6,2 always zero.
  // byte 0 unknown
  byte fieldSettingUnknownBitsVal  = payload[payloadIndex - 3] & 0xC0; // bits 7,6   (src='0': 0x00,0x80,0xC0; src='2': 0x00, 0xC0)
  uint16_t fieldSettingMax        = fieldSettingMin + payload[payloadIndex - 2] * (fieldSettingStepMantissa * (fieldSettingStepExponent ? (fieldSettingStepExponentSign ? 0.100001 : 10) : 1)); // <0.1 >10 not yet observed / unsure how to implement
  float fieldSettingVal        = fieldSettingMin + (payload[payloadIndex - 3] & 0x3F)  * (fieldSettingStepMantissa * (fieldSettingStepExponent ? (fieldSettingStepExponentSign ? 0.1 : 10) : 1)); // <0.1 >10 not yet observed / unsure how to implement
  if (fieldSettingStepExponentSign) {
    snprintf(mqtt_value, MQTT_VALUE_LEN, "{\"val\":%1.1f, \"min\":%i, \"max\":%i, \"stepsize\":0.%i, \"bits\":\"%i%i\", \"bitsstep\":\"0x%02X\"}", fieldSettingVal, fieldSettingMin, fieldSettingMax, fieldSettingStepMantissa, fieldSettingUnknownBitsVal >> 7, (fieldSettingUnknownBitsVal >> 6) & 0x01, fieldSettingUnknownBitsStep);
  } else {
    snprintf(mqtt_value, MQTT_VALUE_LEN, "{\"val\":%1.0f, \"min\":%i, \"max\":%i, \"stepsize\":%i, \"bits\":\"%i%i\", \"bitsstep\":\"0x%02X\"}", fieldSettingVal, fieldSettingMin, fieldSettingMax, fieldSettingStepMantissa * (fieldSettingStepExponent ? 10 : 1), fieldSettingUnknownBitsVal >> 7, (fieldSettingUnknownBitsVal >> 6) & 0x01, fieldSettingUnknownBitsStep);
  }
  return 1;
}

uint8_t field_setting(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig/*, char &cat*/) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 4, 1)) return 0;
  byte FieldNr1 = (packetType + 0x04) & 0x0F;
  byte FieldNr2 = (8 - ((packetType & 0xF0) >> 4)) * 5 + (payloadIndex >> 2);
  uint16_t paramNr = FieldNr1 * 0x0F + FieldNr2;
  return common_field_setting(packetSrc, packetType, payloadIndex, payload, paramNr, mqtt_key, mqtt_value/*, cat*/);
};

uint8_t param_field_setting(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig/*, char &cat*/) {
  if (!newParamVal(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, haConfig, 4)) return 0;
  return common_field_setting(paramSrc, paramPacketType, payloadIndex, payload, paramNr, mqtt_key, mqtt_value/*, cat*/);
};

// return 0:            use this for all bytes (except for the last byte) which are part of a multi-byte parameter
// BITBASIS (for j==8): this byte is to be treated on a bitbasis (by re-calling this function with j=0..7
//                      switch-statement for j is needed as shown below
//                      indicate for j=0..7: "KEY("KnownParameter"); VALUE_flag8" if bit usage is known
//                      indicate for j=0..7: "UNKNOWN_BIT", mqtt_key will then be set to "Bit-0x[source]-0x[payloadnr]-0x[location]-[bitnr]"
// BITBASIS_UNKNOWN:    shortcut for entire bit-switch statement if bit usage of all bits is unknown
// UNKNOWN_BYTE:        byte function is unknown, mqtt_key will be: "PacketSrc-0xXX-Type-0xXX-Byte-I-Bit-I", value will be hex
// UNKNOWN_BIT:         bit function is unknown, mqtt_key will be: "PacketSrc-0xXX-Type-0xXX-Byte-I-Bit-I", value will be hex
// UNKNOWN_PARAM:       parameter function is unknown, mqtt_key will be "ParamSrc-0xXX-Type-0xXX-Nr-0xXXXX", value will be hex
// VALUE_u8:            1-byte unsigned integer value at current location payload[i]
// VALUE_s8:            1-byte signed integer value at current location payload[i]
// VALUE_u16:           3-byte unsigned integer value at location payload[i - 1]..payload[i]
// VALUE_u24:           3-byte unsigned integer value at location payload[i-2]..payload[i]
// VALUE_u32:           4-byte unsigned integer value at location payload[i-3]..payload[i]
// VALUE_u8_add2k:      for 1-byte value (2000+payload[i]) (for year value)
// VALUE_s4abs1c:        for 1-byte value -10..10 where bit 4 is sign and bit 0..3 is value
// VALUE_f8_8           for 2-byte float in f8_8 format (see protocol description)
// VALUE_f8s8           for 2-byte float in f8s8 format (see protocol description)
// VALUE_u8div10        for 1-byte integer to be divided by 10 (used for water pressure in 0.1 bar)
// VALUE_u16div10       for 2-byte integer to be divided by 10 (used for flow in 0.1m/s)
// VALUE_F(value)       for self-calculated float parameter value
// VALUE_u32hex         for 4-byte hex value (used for sw version)
// VALUE_header         for empty payload string
// TERMINATEJSON        returns 9 to signal end of package, signal for json string termination

#define VALUE_u8hex             { return       value_u8hex(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); break; }
#define VALUE_u16hex_LE         { return      value_u16hex_LE(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u24hex_LE         { return      value_u24hex_LE(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u32hex_LE         { return      value_u32hex_LE(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }

#define VALUE_u8                { return          value_u8(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); break; }
#define VALUE_u16_LE            { return         value_u16_LE(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u24_LE            { return         value_u24_LE(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u32_LE            { return         value_u32_LE(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u32_LE_uptime     { return         value_u32_LE_uptime(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }

#define VALUE_s8                { return          value_s8(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_s8_ratelimited    { return          value_s8_ratelimited(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_s16_LE            { return         value_s16_LE(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }

#define VALUE_flag8             { return       value_flag8(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, bitNr); }
#define VALUE_bits(n1, n2)      { return       value_bits(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, n1, n2); }
#define UNKNOWN_BIT             { CAT_UNKNOWN; return        unknownBit(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, bitNr); }

#define VALUE_u8_add2k          { return    value_u8_add2k(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_s4abs1c             { return      value_s4abs1c(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }

#define VALUE_u8div10           { return        value_u8div10(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u16div10_LE       { return    value_u16div10_LE(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u16div10_LE_changed(ch)  { ch = value_u16div10_LE(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); return ch; }

#define VALUE_f8_8              { return        value_f8_8(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_f8_8_changed(ch)  { ch = value_f8_8(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); return ch; }

#define VALUE_f8s8              { return        value_f8s8(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_F(v)              { return           value_f(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, v); }
#define VALUE_F_L(v, l)         { return           value_f(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, v, l); }
#define VALUE_F_L_thr(v, l, tt, tv) { return       value_f(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, v, l, tt, tv); }
#define VALUE_S_L(v, l)         { return           value_s(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, v, l); }
#define VALUE_timeString(s)     { return  value_timeString(mqtt_value, s); }

#define UNKNOWN_BYTE            { CAT_UNKNOWN; return unknownByte(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_header            { return            value_trg(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }

#define PARAM_VALUE_u8         { return      param_value_u_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
//#define PARAM_VALUE_u16_LE     { return      param_value_u_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
//#define PARAM_VALUE_u24_LE     { return      param_value_u_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
//#define PARAM_VALUE_u32_LE     { return      param_value_u_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_s8         { return      param_value_s_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
//#define PARAM_VALUE_s16_LE     { return      param_value_s_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
//#define PARAM_VALUE_s24_LE     { return      param_value_s_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
//#define PARAM_VALUE_s32_LE     { return      param_value_s_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_u16_BE     { return      param_value_u_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_u24_BE     { return      param_value_u_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_u32_BE     { return      param_value_u_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_s16_BE     { return      param_value_s_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_s24_BE     { return      param_value_s_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_s32_BE     { return      param_value_s_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_u16div10_BE { return param_value_u16div10_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_s16div10_BE { return param_value_s16div10_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_u8hex       { return      param_value_hex_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_u16hex_LE   { return      param_value_hex_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_u16hex_BE   { return      param_value_hex_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_u24hex_LE   { return      param_value_hex_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_u24hex_BE   { return      param_value_hex_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_VALUE_u32hex_LE   { return      param_value_hex_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define PARAM_FIELD_SETTING           { return param_field_setting(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig/*, cat*/); }

#define PARAM_VALUE_u16div10_and_u16hex_BE { return param_value_u16div10_and_u16hex_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }

#define UNKNOWN_PARAM8          { CAT_UNKNOWN; return unknownParam_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define UNKNOWN_PARAM16         { CAT_UNKNOWN; return unknownParam_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define UNKNOWN_PARAM24         { CAT_UNKNOWN; return unknownParam_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }
#define UNKNOWN_PARAM32         { CAT_UNKNOWN; return unknownParam_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, paramValLength); }

#define HANDLE_PARAM(paramValLength)     { return handleParam(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, /*cat,*/ paramValLength); }

#define FIELD_SETTING           { return     field_setting(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig/*, cat*/); }

#define BITBASIS_UNKNOWN        { switch (bitNr) { case 8 : BITBASIS; default : UNKNOWN_BIT; } }




#define TERMINATEJSON           { return 9; }

#define BITBASIS                { return newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 1, 0) << 3; }
// BITBASIS returns 8 if at least one bit of a byte changed, or if at least one bit of a byte hasn't been seen before, otherwise 0
// value of byte and of seen status should not be saved (yet) (saveSeen=0), this is done on bit basis
// stateclass in HA: 1 measurement 2 total_increasing (increases only exc for periodic reset) 3 total (may increase/decrease)

#define HACONFIG { haConfig = 1; uom = 0; stateclass = 0;};
#define HATEMP { uom = 1;  stateclass = 1;};
#define HAPOWER {uom = 2;  stateclass = 1;};
#define HAFLOW  {uom = 3;  stateclass = 0;};
#define HAKWH   {uom = 4;  stateclass = 2;};
#define HAHOURS {uom = 5;  stateclass = 0;};
#define HASECONDS {uom = 6;  stateclass = 0;};
#define HAMILLISECONDS {uom = 7;  stateclass = 0;};
#define HABYTES   {uom = 8;  stateclass = 1;};
#define HAEVENTS  {uom = 9;  stateclass = 3;};
// uom 10..12 used by Hitachi
#define HAPRESSURE  {uom = 13;  stateclass = 0;};

byte handleParam(byte paramSrc, byte paramPacketType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, /*char &cat,*/ byte paramValLength) {
// similar to bytes2keyvalue but using indirect parameter references in payloads

  uint16_t paramNr = (((uint16_t) payload[payloadIndex - paramValLength]) << 8) | payload[payloadIndex - paramValLength - 1];

  byte haConfig = 0; // set to 1 by HACONFIG

  maxOutputFilter = 9; // default all changes visible

  switch (paramPacketType) {
    case 0x35 : switch (paramSrc) {
// 35: 8-bit, use paramValue8
//
// includes my F035 observed parameters
// and those from from https://github.com/budulinek/Daikin-P1P2---UDP-Gateway/blob/main/Payload-data-write.md
// not sure if they apply to this model:
// 03 silent mode
// 2F LWT control
// 31 room temp control
// 3A heating/cooling (1=heating 2=cooling)
// 40 DHW control
// 48 DHW boost
      case 0x00 : // parameters main -> aux
      case 0x40 : // parameters aux -> main
                                                                                                    CAT_SETTING;
                  switch (paramNr) {
        case 0x0000 : PARAM_KEY("Reboot_related_Q10-35-00");                                                                                           PARAM_VALUE_u8; //  0x00 0x01
        case 0x0001 : PARAM_KEY("Reboot_related_Q11-35-01");                                                                                           PARAM_VALUE_u8; //  0x00 0x01
        case 0x0003 : PARAM_KEY("Quiet_Mode_35");                                                                                                PARAM_VALUE_u8;
        case 0x0004 : PARAM_KEY("Reboot_related_Q11-35-01");                                                                                           PARAM_VALUE_u8; //  0x00 0x01
        case 0x0008 : PARAM_KEY("Compressor_OnOff");                                                                                             PARAM_VALUE_u8;
        case 0x0035 : PARAM_KEY("Defrost_Q_35");                                                                                                 PARAM_VALUE_u8;
        case 0x0036 : PARAM_KEY("Defrost_Q_36");                                                                                                 PARAM_VALUE_u8;
        case 0x000A : PARAM_KEY("Heatpump_Active_Q0");                                                                                           PARAM_VALUE_u8; // ?
        case 0x003C : PARAM_KEY("Heatpump_Active_Q1");                                                                                           PARAM_VALUE_u8; // ? or circ pump?
        case 0x000E : PARAM_KEY("Circulation_Pump");                                                                                             PARAM_VALUE_u8;
        case 0x0011 : PARAM_KEY("DHW_Related_Q");                                                                                                PARAM_VALUE_u8; // DHW  ??
        case 0x0012 : PARAM_KEY("Gasboiler_Active");                                                                                             PARAM_VALUE_u8;
        case 0x0019 : PARAM_KEY("kWh_Preferential_Mode");                                                                                        PARAM_VALUE_u8;
        case 0x002E : PARAM_KEY("Climate_LWT_OnOff_1");                                                                                          PARAM_VALUE_u8;
        case 0x002F : PARAM_KEY("Climate_LWT_OnOff_2");                                                                                          PARAM_VALUE_u8;
        case 0x0030 : if (paramSrc == 0x00) {
                        PARAM_KEY("Climate_RT_OnOff");
                      } else {
                        PARAM_KEY("Climate_RT_OnOff_1");
                      }                                                                                                                          PARAM_VALUE_u8; // main->aux
        case 0x0031 : if (paramSrc == 0x40) {
                        PARAM_KEY("Climate_RT_OnOff");
                      } else {
                        PARAM_KEY("Climate_RT_OnOff_2");
                      }                                                                                                                          PARAM_VALUE_u8; // aux->main
        case 0x0032 : PARAM_KEY("Heating_Cooling_Q32");                                                                                          PARAM_VALUE_u8; // 0/1 hc off/on? 1=heating?
        case 0x0039 : PARAM_KEY("Climate_Mode_Heating_Cooling_1");                                                                               PARAM_VALUE_u8; // 1=heating/2=cooling
        case 0x003A : PARAM_KEY("Climate_Mode_Heating_Cooling_2");                                                                               PARAM_VALUE_u8; // 1=heating/2=cooling
        case 0x0037 : PARAM_KEY("IU_Operation_Mode");                                                                                            PARAM_VALUE_u8; // 0x00 0x01 0x04 0x05 ?? status depends on DHW on/off, heating on/off, and tapwater-flow yes/no ? / seems to match combined info from S1_Valve_Zone_Main and S1_Valve_DHW_Tank
        case 0x003F : PARAM_KEY("DHW_OnOff_2");                                                                                                  PARAM_VALUE_u8; // DHW on/off setting
        case 0x0040 : PARAM_KEY("DHW_OnOff_3");                                                                                                  PARAM_VALUE_u8; // DHW on/off setting
        case 0x0041 : PARAM_KEY("DHW_OnOff_4");                                                                                                  PARAM_VALUE_u8; // always 0 on tank-less model?
        case 0x0042 : PARAM_KEY("DHW_OnOff_5");                                                                                                  PARAM_VALUE_u8; // always 0 on tank-less model?
        case 0x0047 : PARAM_KEY("DHW_Boost_OnOff_1");                                                                                            PARAM_VALUE_u8; // always 0 on tank-less model?
        case 0x0048 : PARAM_KEY("DHW_Boost_OnOff_2");                                                                                            PARAM_VALUE_u8; // always 0 on tank-less model?
        case 0x0050 : PARAM_KEY("DHW_Active");                                                                                                   PARAM_VALUE_u8; // DHW active (DHW flow sensor)
        case 0x0055 : PARAM_KEY("Heating_modus_1");                                                                                              PARAM_VALUE_u8; // ABS / WD / ABS+prog / WD+prog // bits 0-1 separately? for Climate_Mode_Wd and Climate_Mode_use_deviation
        case 0x0056 : PARAM_KEY("Heating_modus_2");                                                                                              PARAM_VALUE_u8; // idem, writable // bits 0-1 separately?
        case 0x009D : PARAM_KEY("Counter_Schedules");                                                                                            PARAM_VALUE_u8; // increments at scheduled changes
        case 0x013A ... 0x0145 : return 0; // characters for product name
        case 0x00A2 : return 0; // useless sequence counter?
        // params which change or non-zero, function tbd
        case 0x0013 : PARAM_KEY("Reboot_related_Q12-35-13");                                                                                           PARAM_VALUE_u8; //  0x00 0x01
        case 0x0023 : PARAM_KEY("Reboot_related_Q13-35-23");                                                                                           PARAM_VALUE_u8; //  0x00 0x01 UI/LCD?
        case 0x0027 : PARAM_KEY("Reboot_related_Q14-35-27");                                                                                           PARAM_VALUE_u8; //  0x00 0x01 UI/LCD?
        case 0x004D : PARAM_KEY("Reboot_related_Q15-35-4D");                                                                                           PARAM_VALUE_u8; //  0x00 0x01
        case 0x004E : PARAM_KEY("Reboot_related_Q15-35-4E");                                                                                           PARAM_VALUE_u8; //
        case 0x005A : PARAM_KEY("Reboot_related_Q16-35-5A");                                                                                           PARAM_VALUE_u8; //  0x00 0x01 related to F036-0x0002
        case 0x005C : PARAM_KEY("Reboot_related_Q17-35-5C");                                                                                           PARAM_VALUE_u8; //  0x00 0x7F
        case 0x009B : PARAM_KEY("Reboot_related_Q18-35-9B");                                                                                           PARAM_VALUE_u8; //  0x00 0x01 0x02
        case 0x0021 : // fallthrough
        case 0x0022 : // fallthrough
        case 0x004F : // fallthrough
        case 0x0088 : // fallthrough
        case 0x008D : // fallthrough
        case 0x0093 : // fallthrough
        case 0x0098 : // fallthrough
        case 0x009A : // fallthrough
        case 0x00B4 : // fallthrough
        case 0x00B6 : // fallthrough
        case 0x00B7 : // fallthrough
        case 0x00C2 : // fallthrough
        case 0x00C3 : // fallthrough
        case 0x00C5 : // fallthrough
        case 0x00C6 : // fallthrough
        case 0x00C7 : // fallthrough
        case 0x00C8 : // fallthrough
        case 0x00C9 : // fallthrough
        case 0x00CA : // fallthrough
        case 0x00CC : // fallthrough
        case 0x010C : // fallthrough
        case 0x011B : // fallthrough
        case 0x011C : // fallthrough
        case 0x011F : // fallthrough
        case 0x0121 : // fallthrough
        case 0x0122 : UNKNOWN_PARAM8; // PARAM_KEY_HEX;
        case 0xFFFF : return 0;
        default     : UNKNOWN_PARAM8;
        }
      default   : return 0;
      }

// 36: 16-bit, use paramValue8
//
// includes my F036 observed parameters
// and those from from https://github.com/budulinek/Daikin-P1P2---UDP-Gateway/blob/main/Payload-data-write.md
// not sure if they apply to this model:
// 00 room temp setpoint
// 03 DHW setpoint
// 06 LWT setpoint main zone, only in fixed LWT mode
// 08 LWT setoint deviation main zone, only in weather-dependent LWT mode
// 0B LWT setpoint additional zone, only in fixed LWT mode
// 0D LWT setpoint deviation additional zone, only in weather-dependent LWT mode

    case 0x36 :
                                                                // SETTINGS and MEASUREMENT // Temperatures and system operation
                switch (paramSrc) {
      case 0x00 :
      case 0x40 :
                  switch (paramNr) {
        case 0x0000 : PARAM_KEY("Target_Temperature_Room_Heating");                                 CAT_SETTING;                                 PARAM_VALUE_u16div10_BE;
        case 0x0001 : PARAM_KEY("Target_Temperature_Room_Cooling");                                 CAT_SETTING;                                 PARAM_VALUE_u16div10_BE;
        case 0x0002 : PARAM_KEY("Temperature_Room_1");                                              CAT_TEMP;                                    PARAM_VALUE_u16div10_BE; // Temproom1 (reported by main controller)
        case 0x0003 : PARAM_KEY("Target_Temperature_DHW_1");                                        CAT_SETTING;                                 PARAM_VALUE_u16div10_BE; // = 0x13-0x40-0
        case 0x0004 : PARAM_KEY("Target_Temperature_DHW_2");                                        CAT_SETTING;                                 PARAM_VALUE_u16div10_BE;
        case 0x0005 : PARAM_KEY("Temperature_DHW_Tank");                                            CAT_TEMP;                                    PARAM_VALUE_s16div10_BE; // R5T DHW_tank temp
        case 0x0006 : PARAM_KEY("Target_Setpoint_LWT_Heating_Zone_Main");                            CAT_SETTING;                                 PARAM_VALUE_u16div10_BE;
        case 0x0007 : PARAM_KEY("Target_Setpoint_LWT_Cooling_Zone_Main");                            CAT_SETTING;                                 PARAM_VALUE_u16div10_BE;
        case 0x0008 : PARAM_KEY("Deviation_LWT_Heating_Zone_Main");                                 CAT_SETTING;                                 PARAM_VALUE_s16div10_BE;
        case 0x0009 : PARAM_KEY("Deviation_LWT_Cooling_Zone_Main");                                 CAT_SETTING;                                 PARAM_VALUE_s16div10_BE;
        case 0x000A : PARAM_KEY("Target_LWT_Zone_Main");                                            CAT_TEMP;                                    PARAM_VALUE_u16div10_BE; // (in 0.5 C)
        case 0x000B : PARAM_KEY("Target_Setpoint_LWT_Heating_Zone_Add");                             CAT_SETTING;                                 PARAM_VALUE_u16div10_BE;
        case 0x000C : PARAM_KEY("Target_Setpoint_LWT_Cooling_Zone_Add");                             CAT_SETTING;                                 PARAM_VALUE_u16div10_BE;
        case 0x000D : PARAM_KEY("Deviation_LWT_Heating_Zone_Add");                                  CAT_SETTING;                                 PARAM_VALUE_s16div10_BE;
        case 0x000E : PARAM_KEY("Deviation_LWT_Cooling_Zone_Add");                                  CAT_SETTING;                                 PARAM_VALUE_s16div10_BE;
        case 0x000F : PARAM_KEY("Target_LWT_Zone_Add");                                             CAT_TEMP;                                    PARAM_VALUE_u16div10_BE;

        case 0x0010 : PARAM_KEY("Target_Temperature_Room_Q10");                                     CAT_SETTING;                                 PARAM_VALUE_u16div10_BE; // 30.0
        case 0x0011 : PARAM_KEY("Temperature_Outside_1");                                           CAT_TEMP;                                    PARAM_VALUE_s16div10_BE; // Tempout1  in 0.5 C // ~ 0x11-0x40-5
        case 0x0012 : PARAM_KEY("Temperature_Outside_2");                                           CAT_TEMP;                                    PARAM_VALUE_s16div10_BE; // Tempout2 in 0.1 C
        case 0x0013 : PARAM_KEY("Temperature_Room_2");                                              CAT_TEMP;                                    PARAM_VALUE_u16div10_BE; // Temproom2 (copied by Daikin system with minor delay)
        case 0x0014 : PARAM_KEY("Temperature_RWT");                                                 CAT_TEMP;                                    PARAM_VALUE_u16div10_BE; // TempRWT rounded down/different calculus (/255?)?
        case 0x0015 : PARAM_KEY("Temperature_MWT");                                                 CAT_TEMP;                                    PARAM_VALUE_u16div10_BE; // TempMWT
        case 0x0016 : PARAM_KEY("Temperature_LWT");                                                 CAT_TEMP;                                    PARAM_VALUE_u16div10_BE; // TempLWT rounded DOWN (or different calculus than /256; perhaps /255?)
        case 0x0017 : PARAM_KEY("Temperature_Refrigerant_1");                                       CAT_TEMP;                                    PARAM_VALUE_s16div10_BE; // TempRefr1
        case 0x0018 : PARAM_KEY("Temperature_Refrigerant_2");                                       CAT_TEMP;                                    PARAM_VALUE_s16div10_BE; // TempRefr2
        case 0x0027 : PARAM_KEY("Target_Temperature_DHW_3");                                        CAT_SETTING;                                 PARAM_VALUE_u16div10_BE; // = 0x13-0x40-0 ??
// 0x13-0x40-[1-7] unknown;  = 0x0028-0x0029 ?
        case 0x002A : PARAM_KEY("Flow");                                                            CAT_MEASUREMENT;                             PARAM_VALUE_u16div10_BE; // Flow  0x13-0x40-9
        case 0x002B : PARAM_KEY("SW_Version_Inside_Unit");                                          CAT_SETTING;                                 PARAM_VALUE_u16hex_BE;
        case 0x002C : if (payload[payloadIndex] || payload[payloadIndex - 1]) {
                        PARAM_KEY("SW_Version_Outside_Unit");                                       CAT_SETTING;                                 PARAM_VALUE_u16hex_BE;
                      }
                      return 0;
        case 0x002D : PARAM_KEY("Unknown_P36_002D");                                                CAT_SETTING;                                 PARAM_VALUE_u16hex_BE; // no Temp. value 0016 or 040C. Schedule related ?
        case 0x002E : PARAM_KEY("Unknown_P36_002E");                                                CAT_SETTING;                                 PARAM_VALUE_u16hex_BE; // 0x0000
        case 0xFFFF : return 0;
        default     : UNKNOWN_PARAM16;
        }
      default   : return 0;
      }

    case 0x37 :
                switch (paramSrc) {
      case 0x00 : // 0x01010D 0x010D00 reboot related?
                  PARAM_KEY("Reboot_related_param37_Q");                                            CAT_UNKNOWN;                                PARAM_VALUE_u24hex_BE;
      case 0x40 :
                  switch (paramNr) {
        case 0xFFFF : return 0;
        default     : UNKNOWN_PARAM24;
        }
      default   : return 0;
      }

    case 0x38 :
                                                                // counters
                switch (paramSrc) {
      case 0x00 :                                                                                   CAT_COUNTER;
                  switch (paramNr) {
        case 0x0000 : PARAM_KEY("Electricity_Consumed_Backup_Heating");                                                                          PARAM_VALUE_u32_BE;
        case 0x0001 : PARAM_KEY("Electricity_Consumed_Backup_DHW");                                                                              PARAM_VALUE_u32_BE;
        case 0x0002 : PARAM_KEY("Electricity_Consumed_Compressor_Heating");                                                                      PARAM_VALUE_u32_BE;
        case 0x0003 : PARAM_KEY("Electricity_Consumed_Compressor_Cooling");                                                                      PARAM_VALUE_u32_BE;
        case 0x0004 : PARAM_KEY("Electricity_Consumed_Compressor_DHW");                                                                          PARAM_VALUE_u32_BE;
        case 0x0005 : PARAM_KEY("Electricity_Consumed_Total");                                                                                   PARAM_VALUE_u32_BE;
        case 0x0006 : PARAM_KEY("Energy_Produced_Heatpump_Heating");                                                                             PARAM_VALUE_u32_BE;
        case 0x0007 : PARAM_KEY("Energy_Produced_Heatpump_Cooling");                                                                             PARAM_VALUE_u32_BE;
        case 0x0008 : PARAM_KEY("Energy_Produced_Heatpump_DHW");                                                                                 PARAM_VALUE_u32_BE;
        case 0x0009 : PARAM_KEY("Energy_Produced_Heatpump_Total");                                                                               PARAM_VALUE_u32_BE;

        case 0x000D : PARAM_KEY("Hours_Circulation_Pump");                                                                                       PARAM_VALUE_u32_BE;
        case 0x000E : PARAM_KEY("Hours_Compressor_Heating");                                                                                     PARAM_VALUE_u32_BE;
        case 0x000F : PARAM_KEY("Hours_Compressor_Cooling");                                                                                     PARAM_VALUE_u32_BE;
        case 0x0010 : PARAM_KEY("Hours_Compressor_DHW");                                                                                         PARAM_VALUE_u32_BE;
        case 0x0011 : PARAM_KEY("Hours_Backup1_Heating");                                                                                        PARAM_VALUE_u32_BE;
        case 0x0012 : PARAM_KEY("Hours_Backup1_DHW");                                                                                            PARAM_VALUE_u32_BE;
        case 0x0013 : PARAM_KEY("Hours_Backup2_Heating");                                                                                        PARAM_VALUE_u32_BE;
        case 0x0014 : PARAM_KEY("Hours_Backup2_DHW");                                                                                            PARAM_VALUE_u32_BE;
        case 0x0015 : PARAM_KEY("Hours_Unknown_Function");                                                                                       PARAM_VALUE_u32_BE;
        case 0x0016 : PARAM_KEY("Hours_Drain_Pan_Heater");                                                                                       PARAM_VALUE_u32_BE; // Drain pan heater (BPV/bodemplaat verwarming), thanks to Pim Snoeks for finding this function
        case 0x001A : PARAM_KEY("Hours_Gasboiler_Heating");                                                                                      PARAM_VALUE_u32_BE;
        case 0x001B : PARAM_KEY("Hours_Gasboiler_DHW");                                                                                          PARAM_VALUE_u32_BE;
        case 0x001C : PARAM_KEY("Starts_Compressor");                                                                                            PARAM_VALUE_u32_BE;
        case 0x001D : PARAM_KEY("Starts_Gasboiler");                                                                                             PARAM_VALUE_u32_BE;
        case 0x001E : PARAM_KEY("Packet15_Byte0_2");                                                                                             PARAM_VALUE_u32_BE;
        case 0xFFFF : return 0;
        default     : UNKNOWN_PARAM32;
      }
      case 0x40 : // parameters written by auxiliary controller (does this happen?)
                  switch (paramNr) {
        case 0xFFFF : return 0;
        default     : UNKNOWN_PARAM32;
        }
      default   : return 0;
    }
    case 0x39 :
                switch (paramSrc) {
      case 0x00 :
      case 0x40 :
                  PARAM_FIELD_SETTING;
      default : return 0;
    }

// includes my F03A observed parameters
// and those from from https://github.com/budulinek/Daikin-P1P2---UDP-Gateway/blob/main/Payload-data-write.md
// not sure if they apply to this model:
//
// 00 24h_format
// 31 holiday_enable
// 3B dec_delimiter
// 3D flow_unit
// 3F temp_unit
// 40 energy_unit
// 4B DST
// 4C Silent_mode
// 4D Silent_level
// 4E Operation_hc_mode
// 5B holiday
// 5E heating_schedule
// 5F cooling_schedule
// 64 DHW_schedule

    case 0x3A :
                                                                                                    CAT_SETTING; // PacketSrc 3A system settings and operation
                switch (paramSrc) {
      case 0x00 :
      case 0x40 :
                  switch (paramNr) {
        case 0x0000 : PARAM_KEY("Format_24h");                                                                                                   PARAM_VALUE_u8;
        case 0x0031 : PARAM_KEY("Holiday_Enable");                                                                                               PARAM_VALUE_u8;
        case 0x003B : PARAM_KEY("Decimal_Delimiter");                                                                                            PARAM_VALUE_u8;
        case 0x003D : PARAM_KEY("Unit_Flow");                                                                                                    PARAM_VALUE_s8;
        case 0x003F : PARAM_KEY("Unit_Temperature");                                                                                             PARAM_VALUE_u8;
        case 0x0040 : PARAM_KEY("Unit_Energy");                                                                                                  PARAM_VALUE_s8;
        case 0x004B : PARAM_KEY("Unit_DST");                                                                                                     PARAM_VALUE_s8;
        case 0x004C : PARAM_KEY("Quiet_UI_Mode");                                                                                                PARAM_VALUE_s8; // interface quiet-menu mode 0=auto 1=off 2=on
        case 0x004D : PARAM_KEY("Quiet_Level");                                                                                                  PARAM_VALUE_s8; // 0=mild .. 2=most silent
        case 0x004E : PARAM_KEY("Climate_Mode");                                                                                                 PARAM_VALUE_s8; // heating/cooling/auto
        case 0x005B : PARAM_KEY("Holiday");                                                                                                      PARAM_VALUE_s8;
        case 0x005E : PARAM_KEY("Schedule_Heating");                                                                                             PARAM_VALUE_s8;
        case 0x005F : PARAM_KEY("Schedule_Cooling");                                                                                             PARAM_VALUE_s8;
        case 0x0064 : PARAM_KEY("Schedule_DHW");                                                                                                 PARAM_VALUE_s8;
        case 0x005D : PARAM_KEY("DHW_temp_semidegree");                                                                                          PARAM_VALUE_u8; // TODO: semi degrees; 0x00 = -16C; 0x98 = 60C
        case 0xFFFF : return 0;
        case 0x0003 ... 0x0008 : // observed non-zero values 0x74/0x2C/0x3E/0x42/0x52/..
        case 0x0009 ... 0x001C : // all 0x52
        case 0x0033            : // all 0x05
        case 0x0037 ... 0x003A : // observed non-zero values 0x74/0x2C/0x3E/0x42/0x52/..
        case 0x0041            : // observed non-zero values 0x74/0x2C/0x3E/0x42/0x52/..
        case 0x0044 ... 0x0048  : // observed non-zero values 0x74/0x2C/0x3E/0x42/0x52/..
        case 0x005C            : // observed non-zero values 0x74/0x2C/0x3E/0x42/0x52/..
        case 0x0060            : // observed non-zero values 0x74/0x2C/0x3E/0x42/0x52/..
        case 0x0066            : // observed non-zero values 0x74/0x2C/0x3E/0x42/0x52/..
        case 0x0067            : // observed non-zero values 0x74/0x2C/0x3E/0x42/0x52/..
        case 0x006A            : // observed non-zero values 0x74/0x2C/0x3E/0x42/0x52/..
        case 0x006B            : // observed non-zero values 0x74/0x2C/0x3E/0x42/0x52/..
        default     : UNKNOWN_PARAM8;
      }
      default   : return 0;
    }

    case 0x3B : // unknown
                switch (paramSrc) {
      case 0x00 :
      case 0x40 :
                  switch (paramNr) {
        case 0xFFFF : return 0;
        case 0x0001 : // observed 3803
        case 0x0024 ... 0x0028 : // observed 0x4221 0x8E0A 0x8D15 0x4A0A 0xA503
        case 0x002B ... 0x002E : // observed 0xF405 0x0012 0x260B 0xE60E
        case 0x0048 ... 0x0051 : // observed ...
        case 0x0053 ... 0x0058 : // observed ...
        case 0x005C ... 0x005F : // observed ...
        case 0x0084 ... 0x0089 : // observed ...
        case 0x008E ... 0x0097 : // observed ...
        case 0x00AE : // observed 0x6900
        default     : UNKNOWN_PARAM16;
        }
      default   : return 0;
      }

    case 0x3C : // unknown
                switch (paramSrc) {
      case 0x00 :
      case 0x40 :
                  switch (paramNr) {
        case 0xFFFF : return 0;
        case 0x0000 : // observed 0x0F0C18
        case 0x0001 : // observed 0x0F0C1B (diff 3, 3-byte parameters)
        default     : UNKNOWN_PARAM24;
        }
      default   : return 0;
      }

/* 0x3D : ?
        case      : FIELDKEY("[____]_7.4.1.1_Target_Temperature_Room_Heating_Comfort");   // step : A.3.2.4, range [3-07 - 3-06]
        case      : FIELDKEY("[____]_7.4.1.2_Target_Temperature_Room_Cooling_Eco");
        case      : FIELDKEY("[____]_7.4.1.3_Target_Temperature_Room_Heating_Comfort");   // step A.3.2.4, range [3-09 - 3-08 ]
        case      : FIELDKEY("[____]_7.4.1.4_Target_Temperature_Room_Cooling_Eco");
        case      : FIELDKEY("[____]_7.4.2.5_Target_Temperature_Room_Heating_Comfort");   // Deviation -10 .. +10
        case      : FIELDKEY("[____]_7.4.2.6_Target_Temperature_Room_Cooling_Eco");
        case      : FIELDKEY("[____]_7.4.2.7_Target_Temperature_Room_Heating_Comfort");
        case      : FIELDKEY("[____]_7.4.2.8_Target_Temperature_Room_Cooling_Eco");
                    FIELDKEY("[____]_A.5.2.1_DHW_Auto_Emergency_Operation"); // 0 default: Manual, 1 Automatic
                    FIELDKEY("[____]_A.6.B___Calorific_Value"); // 7-40 step 0,1 default 10.5
                    FIELDKEY("[____]_A.3.2.4_Room_Temperature_Step"); // 0: 1C, 1: 0.5C
                  : FIELDKEY("[____]_7.4.4___Quiet_Level"); // 0,1,2 = level 1,2,3
                  : FIELDKEY("[____]_A.4.3.1_DHW_Setpoint_Readout_Type"); // 0: temperature, 1: graphical
                  : FIELDKEY("[____]_A.4.3.2.1_Conversion_Persons_1"); //  32-80C 42C
                  : FIELDKEY("[____]_A.4.3.2.2_Conversion_Persons_2"); //  0-20C 6C
                  : FIELDKEY("[____]_A.4.3.2.3_Conversion_Persons_3"); //  0-20C 15C
                  : FIELDKEY("[____]_A.4.3.2.4_Conversion_Persons_4"); //  0-20C 17C
                  : FIELDKEY("[____]_A.4.3.2.5_Conversion_Persons_5"); //  0-20C 1C
                  : FIELDKEY("[____]_A.4.3.2.6_Conversion_Persons_6"); //  0-20C 1C
                  : FIELDKEY("[____]_A.4.6___DHW_Setpoint_Mode"); // 0 absolute, 1 WD
        case      : FIELDKEY("[____]_7.4.2.5_LWT_Main_Deviation_Heating_ComfortTarget_Temperature_Room_Heating_Comfort");   // Deviation -10 .. +10 // cannot access these in WD mode?
        case      : FIELDKEY("[____]_7.4.2.6_Target_Temperature_Room_Cooling_Eco");
        case      : FIELDKEY("[____]_7.4.2.7_Target_Temperature_Room_Heating_Comfort");
        case      : FIELDKEY("[____]_7.4.2.8_Target_Temperature_Room_Cooling_Eco");
                    FIELDKEY("[____]_A.3.1.1_LWT_Setpoint_Mode_Main_Zone"); // 0 Absolute, 1 (default) Weather Dep, 2 Abs+Scheduled, 3 WD+Scheduled
                    FIELDKEY("[____]_A.3.2.1_LWT_Setpoint_Mode_Add_Zone"); // 0 Absolute, 1 (default) Weather Dep, 2 Abs+Scheduled, 3 WD+Scheduled
                    FIELDKEY("[____]_7.4.6___Gas_Price"); // 8,0 / kWH;    0,00-990/kWh or 0,00-290/MBtu,  data type 1 or 2 digit FP?
                    FIELDKEY("[____]_A.2.1.B_Location_User_Interface"); // 0 at unit, 1 in room
*/

    case 0x3D :
                                                                // unknown
                switch (paramSrc) {
      case 0x00 :
      case 0x40 :
                  switch (paramNr) {
        case 0xFFFF : return 0;
        case 0x001E : PARAM_KEY("Packet15_Byte3_5");                                                                                             PARAM_VALUE_u32_BE;
        case 0x0002 : // observed non-zero
        case 0x0005 : // observed non-zero
        case 0x000B : // observed non-zero
        case 0x000E : // observed non-zero
        case 0x000F : // observed non-zero
        case 0x0011 : // observed non-zero
        case 0x0012 : // observed non-zero
        case 0x0017 : // observed non-zero 0x402F0200
        case 0x001B : // observed non-zero 0x402F0200
        case 0x001A : // observed non-zero
        case 0x001F : // observed non-zero 0x00000352 = packet15 bytes 3-5
        default   : byte f = paramSrc ? 0xFF : 0x00;
                    if ((payload[payloadIndex] == f)   && (payload[payloadIndex - 1] == f) && (payload[payloadIndex - 2] == f)  && (payload[payloadIndex - 3] == f)) return 0;
//                    FIELDKEY("Fieldsetting_Q_Unknown");
                    UNKNOWN_PARAM32;
        }
      default   : return 0;
      }
    default   : return 0; // unknown packet type
  }
}

#ifdef SAVESCHEDULE
uint16_t mqtt_value_p[2];
char mqtt_value_schedule[2][MQTT_VALUE_LEN + 500];
const char weekDay[7][3] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
#endif

uint16_t parameterWritesDone = 0; // # writes done by ATmega; perhaps useful for ESP-side queueing

byte bytesbits2keyvalue(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, /*char &cat, char &src,*/ byte bitNr) {
// payloadIndex: new payload documentation counts payload bytes starting at 0 (following Budulinek's suggestion)
// payloadIndex == EMPTY_PAYLOAD : empty payload (used during restart)

  bool newMem = 0;
  static byte newSched[2];
  byte haConfig = 0;

  maxOutputFilter = 9; // default all changes visible, unless changed below
  CAT_UNKNOWN; // cat unknown unless changed below

  float Power1, Power2;
  static float LWT = 0, RWT = 0, MWT = 0, Flow = 0;
  static byte prevSec = 0xFF;
  static byte prevMin = 0xFF;
  static float P1avg = 0.0;
  static float P2avg = 0.0;
  static float P1minavg = 0.0;
  static float P2minavg = 0.0;
  byte P1cnt = 0;
  byte P2cnt = 0;
  static bool MWT_changed = 0;
  static bool LWT_changed = 0;
  static bool RWT_changed = 0;
  static bool Flow_changed = 0;

  bool printTimeString1;
  static byte weeklyMomentCounter[2];
  static byte dailyMomentsCounter[2];
  static byte dailyMoments[2];
  static byte dayCounter[2];
  static bool isWeeklySchedule[2];
  static bool isSilentSchedule[2];
  static bool isHCSchedule[2];
  static bool isElectricityPriceSchedule[2];

  static uint16_t byteLoc2[2];
  byte payloadByte = 0;
  byte* payloadPointer = 0;
  if (payloadIndex != EMPTY_PAYLOAD) {
    payloadByte = payload[payloadIndex];
    payloadPointer = &payload[payloadIndex];
  };

  byte PS = packetSrc ? ((packetSrc == 0x80) ? 6 : 1) : 0; // 0x80 used? TODO
  byte src = PS;
  if ((packetType & 0xF0) == 0x30) src += 2;  // 2, 3 from/to auxiliary controller (use 4, 5 for 2nd auxiliary controller?)
  if ((packetType & 0xF8) == 0x00) src = 7; // boot
  if ((packetType & 0xF8) == 0x08) src = PS + 8; // pseudo-packets ESP / ATmega
  SRC(src); // set char in mqtt_key prefix

  switch (packetType) {
    // Restart packet types 00-05 (restart procedure 00-05 followed by 20, 60-9F, A1, B1, B8, 21)
    case 0x00 : switch (payloadIndex) {
      case  EMPTY_PAYLOAD : KEY("RestartPhase0");                                                                                                VALUE_header;
      default   : UNKNOWN_BYTE; // not observed
    }
    case 0x01 : switch (payloadIndex) {
      case  EMPTY_PAYLOAD : KEY("RestartPhase1");                                                                                                VALUE_header;
      default   : UNKNOWN_BYTE; // not observed
    }
    case 0x03 : switch (payloadIndex) {
      case EMPTY_PAYLOAD : KEY("RestartPhase3");                                                                                                 VALUE_header;
      case    0 : KEY("RestartPhase3bitPatternNr");                                                                                              VALUE_u8hex;
      case    1 : // fallthrough
      case    2 : return 0;
      case    3 : KEY("RestartPhase3bitPattern");                                                                                                VALUE_u24hex_LE;
      default   : UNKNOWN_BYTE; // not observed
    }
    case 0x04 : switch (payloadIndex) {
      case EMPTY_PAYLOAD : KEY("RestartPhase4");                                                                                                 VALUE_header;
      case    0 : // fallthrough
      case    1 : // fallthrough
      case    2 : return 0;
      case    3 : KEY("RestartPhase4data1");                                                                                                     VALUE_u32hex_LE;
      default   : UNKNOWN_BYTE; // not observed
    }
    case 0x05 : switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case EMPTY_PAYLOAD : KEY("PacketTarget");                                                                                                VALUE_header;
        case    0 : // fallthrough
        case    1 : // fallthrough
        case    2 : return 0;
        case    3 : KEY("RestartPhase4data2");                                                                                                   VALUE_u32hex_LE;
        default   : UNKNOWN_BYTE; // not observed
      }
      case 0x40 : switch (payloadIndex) {
        case    2 : // fallthrough
        case    5 : // fallthrough
        case    8 : // fallthrough
        case   11 : // fallthrough
        case   14 : // fallthrough
        case   17 : KEY("RestartPhase5data1");                                                                                                   VALUE_u24hex_LE;
        case   19 : KEY("RestartPhase5data2");                                                                                                   VALUE_u16hex_LE;
        default   : return 0; // 0th and every 3rd byte 01, others 00, likely no info here
      }
    }
    // Main package communication packet types 10-16
    case 0x10 :                                                 CAT_SETTING;
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : switch (bitNr) {
          case    8 : BITBASIS;
          case    0 : KEY("Climate_OnOff");                                HACONFIG;                                                              VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case    1 : switch (bitNr) {
          case    8 : BITBASIS;
          case    0 : KEY("Climate_Mode_Heating");                                  HACONFIG;                                                              VALUE_flag8; // =? Space_CH_Op_or_BPH // wasClimate_LWT_OnOff
          case    1 : KEY("Climate_Mode_Cooling");                               HACONFIG;                                                              VALUE_flag8;
          case    7 : KEY("Climate_RT_OnOff");                                                                                                          VALUE_flag8; // =? Space_heating_Operation_ONOFF
//        case    2 : KEY("Reboot_related_Q10-00-1-2");                                                                                              VALUE_flag8;
//        case    3 : KEY("Reboot_related_Q10-00-1-3");                                                                                              VALUE_flag8;
//        case    4 : KEY("Reboot_related_Q10-00-1-4");                                                                                              VALUE_flag8;
//        case    5 : KEY("Reboot_related_Q10-00-1-5");                                                                                              VALUE_flag8;
//        case    6 : KEY("Reboot_related_Q10-00-1-6");                                                                                              VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case    2 : switch (bitNr) {
          case    8 : BITBASIS;
          case    0 : KEY("DHW_OnOff");                                    HACONFIG;                                                             VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case    7 : return 0;
        case    8 : KEY("Target_Temperature_Room_Heating");                HACONFIG; HATEMP;                                                     VALUE_f8s8;
        case    9 : switch (bitNr) {
          case    8 : BITBASIS;
                      // 0..4 reboot related ?
          case    5 : KEY("Heating_Cooling_Auto");                         HACONFIG;                                                             VALUE_flag8;
          case    6 : return 0;
          case    7 : KEY("Quiet_Level");                                  HACONFIG;                                                             VALUE_bits(6, 7); // 2-bit : 01 = silent-level 1; 02= level 3; 03=level 3; and 0 if Quiet_Mode is 0 (off)
          default   : UNKNOWN_BIT;
        }
        case   10 : switch (bitNr) {
          case    8 : BITBASIS;
          case    2 : KEY("Quiet_Mode_OnOff");                             /* no HACONFIG as Quiet_level>0 implies Quiet_Mode */                  VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case   12 : switch (bitNr) {
          case    8 : BITBASIS; // bit 3 reboot related?
          case    3 : KEY("Reboot_related_Q10-00-12-3");                                                                                              VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
//        case   13 : KEY("Reboot_related_Q10-00-13");                       CAT_UNKNOWN;                                                          VALUE_u8hex;
        case   15 : return 0;
        case   16 : KEY("Target_Temperature_Room_Cooling");                HACONFIG; HATEMP;                                                     VALUE_f8s8; // 15.0, 20.0, or 26.0?, or 111.5 (?!)
        case   17 : switch (bitNr) { // DHW (tank) mode
          case    8 : BITBASIS;
          case    1 : KEY("DHW_Booster_Active");                           HACONFIG;                                                             VALUE_flag8;
          case    6 : KEY("DHW_Related_Q");                                                                                                      VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case   18 : return 0;
        case   19 : KEY("DHW_Setpoint_Request");                                                                                                 VALUE_f8s8; // 8 or 16 bit?
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : switch (bitNr) {
          case  8 : BITBASIS;
          case  0 :   KEY("Climate_OnOff");                             HACONFIG;                                                              VALUE_flag8; // was Climate_LWT_OnOff
          case  1 :   KEY("Climate_Mode_Q2");                                   HACONFIG;                                                              VALUE_flag8;
          default :   UNKNOWN_BIT;
        }
        case    1 : switch (bitNr) {
          case    8 : BITBASIS;
          case    7 : KEY("Climate_RT_OnOff");                                                                                                      VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case    2 : switch (bitNr) {
          case    8 : BITBASIS;
          case    0 : KEY("Climate_Mode_Heating");                        HACONFIG;                                                              VALUE_flag8; // earlier than /0/ in auto mode // was: Valve_Heating
          case    1 : KEY("Climate_Mode_Cooling");                        HACONFIG;                                                              VALUE_flag8; // earlier than /0/ in auto mode // was: Valve_Cooling
          case    4 : KEY("Climate_Mode_Change_Pulse");                   HACONFIG;                                                              VALUE_flag8; // in auto mode, signals 1 until Climate_Mode_Heating /0/ matches above values
          case    5 : KEY("Valve_Zone_Main");                             HACONFIG;                                                              VALUE_flag8;
          case    6 : KEY("Valve_Zone_Add");                                                                                                     VALUE_flag8;
          case    7 : KEY("Valve_DHW_Tank");                              HACONFIG;                                                              VALUE_flag8; // depends on DHW_OnOff and on Heatpump/Compressor being active
          default   : UNKNOWN_BIT;
        }
        case    3 : switch (bitNr) {
          case    8 : BITBASIS;
          case    0 : KEY("Valve_3Way");                                                                                                         VALUE_flag8; // depends on DHW_OnOff
          case    4 : KEY("SHC_Tank");                                                                                                           VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case    4 : return 0;
        case    5 : KEY("DHW_Setpoint_Response");                          HACONFIG; HATEMP;                                                     VALUE_f8s8; // 8 or 16 bit?
        case    6 : BITBASIS_UNKNOWN;
        case    8 : return 0;
        case    9 : KEY("Target_Temperature_Room");                                                                                              VALUE_f8s8;
        case   10 : switch (bitNr) {
          case    8 : BITBASIS;
          case    5 : KEY("Climate_Mode_Auto");                                                                                                  VALUE_flag8;
          default   : UNKNOWN_BIT; /* perhaps reboot related */
        }
        case   11 : switch (bitNr) {
          case    8 : BITBASIS;
          case    2 : KEY("Quiet_Mode");                                                                                                         VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case   12 : KEY("ErrorCode1");                                     HACONFIG; CAT_MEASUREMENT;                                            VALUE_u8hex; // coding mechanism unknown. 024D2C = HJ-11 08B908 = 89-2 08B90C = 89-3
        case   13 : KEY("ErrorCode2");                                     HACONFIG; CAT_MEASUREMENT;                                            VALUE_u8hex;
        case   14 : KEY("ErrorSubCode");                                   HACONFIG; CAT_MEASUREMENT;                                            VALUE_u8hex; // >>2 for Subcode, and what are 2 lsbits ??
//        case   15 : KEY("Reboot_related_Q10-40-15");                                                                                             VALUE_u8hex;
        case   17 : switch (bitNr) {
          case    8 : BITBASIS;
//        case    6 : KEY("BUH1_OnOff_Q");                                 HACONFIG;                                                             VALUE_flag8;
          case    1 : KEY("Defrost_Operation");                            HACONFIG;                                                             VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case   18 : switch (bitNr) {
          case    8 : BITBASIS;
          case    0 : KEY("Compressor_OnOff");                             HACONFIG;                                                             VALUE_flag8;
          case    3 : KEY("Circulation_Pump_OnOff");                       HACONFIG;                                                             VALUE_flag8;
          case    5 : KEY("BUH_2_OnOff");                                  HACONFIG;                                                             VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case   19 : switch (bitNr) {
          case  8 : BITBASIS;
          case  1 : KEY("Gasboiler_Active_0");                             HACONFIG;                                                             VALUE_flag8; // not gasboiler/BUH?
          case  2 : KEY("DHW_Mode_OnOff");                                 HACONFIG;                                                             VALUE_flag8; // ??
//          case  1 : KEY("DHWactive1");                                                                                                           VALUE_flag8;
                    default : UNKNOWN_BIT;
        }
        default :             UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x11 :                                                            CAT_TEMP; // includes maxOutputFilter = 1;
                switch (packetSrc) { // temperatures+flow
      case 0x00 : switch (payloadIndex) {
        case    0 : return 0;
        case    1 : KEY("Temperature_Room");                               HACONFIG; HATEMP;                                                     VALUE_f8_8;
//        case    7 : KEY("Reboot_related_Q11-00-7");                                                                                                   VALUE_u8hex;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : return 0;
        case    1 : KEY("Temperature_Leaving_Water");                      HACONFIG; HATEMP;
                    LWT = FN_f8_8(payloadPointer);                                                                                               VALUE_f8_8_changed(LWT_changed); // R2T
        case    2 : return 0;                         // Domestic hot water temperature (on some models)
        case    3 : KEY("Temperature_DHW_Tank");                            HACONFIG; HATEMP; VALUE_f8_8;                                        VALUE_f8_8;   // R5T DHW tank, unconnected on EHYHBX08AAV3?, then reading -40.7/-40.8
        case    4 : return 0;                         // Outside air temperature (low res)
        case    5 : KEY("Temperature_Outside_1");                          HACONFIG; HATEMP;                                                     VALUE_f8_8; // outside temperature outside unit
        case    6 : return 0;
        case    7 : KEY("Temperature_Return_Water");                       HACONFIG; HATEMP;                                                                   // R4T
                    RWT = FN_f8_8(payloadPointer);                                                                                               VALUE_f8_8_changed(RWT_changed);
        case    8 : return 0;
        case    9 : KEY("Temperature_HP2Gas_Water");                       HACONFIG;HATEMP;                                                                    // R1T Leaving water temperature on the heat exchanger
                    MWT = FN_f8_8(payloadPointer);                                                                                               VALUE_f8_8_changed(MWT_changed);
        case   10 : return 0;                         // Refrigerant temperature
        case   11 : KEY("Temperature_Refrigerant_1");                      HACONFIG;  HATEMP;                                                    VALUE_f8_8;  // R3T refrigerant temp liquid side
        case   12 : return 0;                         // Room temperature copied with minor delay by Daikin system
        case   13 : KEY("Temperature_Room");                               HACONFIG; HATEMP;                                                     VALUE_f8_8;
        case   14 : return 0;                         // Outside air temperature
        case   15 : KEY("Temperature_Outside_2");                          HACONFIG; HATEMP;                                                     VALUE_f8_8; // outside air temperature based on outside unit or based on R6T external sensor
        case   16 : return 0;                         // Unused ?
        case   17 : KEY("Temperature_Unused_400011_16");                   HACONFIG; HATEMP;                                                     VALUE_f8_8;
        case   18 : return 0;                         // Outside air temperature
        case   19 : KEY("Temperature_Unused_400011_18");                   HACONFIG; HATEMP;                                                     VALUE_f8_8;
        default   : UNKNOWN_BYTE;
      }
      default   : UNKNOWN_BYTE;
    }
    case 0x12 : switch (packetSrc) { // date+time
      case 0x00 :
                                                                           CAT_MEASUREMENT; // includes maxOutputFilter = 1;
                  switch (payloadIndex) {
        case    0 : switch (bitNr) {
          case  8 : BITBASIS;
          case  1 : return 0; // suppress hourly pulse reporting
                    KEY("Hourly_Pulse_0");                                                                                                       VALUE_flag8;
          default :                                                                                                                              UNKNOWN_BIT;
        }
        case    1 : switch (payloadByte) {
                      case 0 : strncpy (TimeString1, "Mo", 2); break;
                      case 1 : strncpy (TimeString1, "Tu", 2); break;
                      case 2 : strncpy (TimeString1, "We", 2); break;
                      case 3 : strncpy (TimeString1, "Th", 2); break;
                      case 4 : strncpy (TimeString1, "Fr", 2); break;
                      case 5 : strncpy (TimeString1, "Sa", 2); break;
                      case 6 : strncpy (TimeString1, "Su", 2); break;
                    }
                    TimeString2[0] = TimeString1[0];
                    TimeString2[1] = TimeString1[1];
                    return 0;
        case    2 : // Hour
                    if (SelectTimeString2) {
                      SelectTimeString2--;
                    }
                    // Switch to TimeString1 if we have not seen a 00Fx31 payload for a few cycli (>6 in view of counter request blocking 6 cycli)
                    TimeString1[14] = '0' + (payloadByte / 10);
                    TimeString1[15] = '0' + (payloadByte % 10);
                    return 0;
                    KEY("Hours");                                                                                                      VALUE_u8;
        case    3 : // Minute
                    // if (!(SelectTimeString2) && ((TimeString1[18]-'0') != (payloadByte % 10))) TimeStringCnt = 0; // reset at start of new minute
                    if (payloadByte != prevMin) {
                      prevMin = payloadByte;
                      // newMin = 1;
                      TimeString1[17] = '0' + (payloadByte / 10);
                      TimeString1[18] = '0' + (payloadByte % 10);
                      if (!SelectTimeString2) printTimeString1 = true;
                    }
                    return 0;
        case    4 : // Year
                    TimeString1[5] = '0' + (payloadByte / 10);
                    TimeString1[6] = '0' + (payloadByte % 10);
                    return 0;
        case    5 : // Month
                    TimeString1[8] = '0' + (payloadByte / 10);
                    TimeString1[9] = '0' + (payloadByte % 10);
                    return 0;
        case    6 : // Day
                    TimeString1[11] = '0' + (payloadByte / 10);
                    TimeString1[12] = '0' + (payloadByte % 10);
                    if (printTimeString1) {
                      printTimeString1 = false;
                      SRC(9);
                      KEY("Date_Time");                                                                                 maxOutputFilter = 2;     VALUE_timeString(TimeString1);
                    }
                    return 0;
        case   12 : KEY("Reboot_trigger");                                                                                                             VALUE_u8hex; // Reboot trigger?
        case   13 : BITBASIS_UNKNOWN;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 :
                                                                                                    CAT_SETTING; // PacketSrc 40 PacketType12 system settings and operation
                  switch (payloadIndex) {
        case    1 : KEY("Reboot_related_Q12-40-1");                                                                                                   VALUE_u8hex; // 0x00 0x40
        case   10 : switch (bitNr) {
          case    8 : BITBASIS;
          case    4 : KEY("Preferential_Mode");                            HACONFIG;                                                             VALUE_flag8;
          default   : UNKNOWN_BIT;
        }

        case   11 : KEY("Restart_Byte");                                                                                                         VALUE_u8hex;
        case   12 : switch (bitNr) {
          case    8 : BITBASIS;
          case    0 : KEY("Climate_On");                                   HACONFIG;                                                             VALUE_flag8;
          case    6 : KEY("System_On_Q");                                 HACONFIG;                                                             VALUE_flag8; // status depends both on DHW on/off and heating on/off ?
          case    7 : KEY("DHW_Active_1");                                 HACONFIG;                                                             VALUE_flag8; // (one of the DHW_Active's is likely Gasboiler_Active?)
          default   : UNKNOWN_BIT;
        }
        case   13 : KEY("Reboot_related_Q12-40-13");                                                                                                   VALUE_u8hex; // 0x00 0x04, or 0x0C during boot?
        default : UNKNOWN_BYTE;
      }
      default   : UNKNOWN_BYTE;
    }
    case 0x13 :
                                                                                                    CAT_SETTING; // PacketType13 system settings and operation, and flow measurement
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case  0 : KEY("UI_Reboot_related_Q");                              HACONFIG;                                                             VALUE_u8hex;
        case    2 : switch (bitNr) {
          case    8 : BITBASIS;
          case    6 : KEY("Climate_Mode_WD");                             HACONFIG;                                                             VALUE_flag8;
          case    7 : KEY("Climate_Mode_use_deviation");                  HACONFIG;                                                             VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        default : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : return 0;
        case    1 : KEY("Target_Temperature_DHW");                         HACONFIG; HATEMP;                                                     VALUE_f8s8;
        case    3 : switch (bitNr) {
          case    8 : BITBASIS;
          case    6 : KEY("Climate_Mode_WD");                             HACONFIG;                                                             VALUE_flag8;
          case    7 : KEY("Climate_Mode_use_deviation");                  HACONFIG;                                                             VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case    6 : KEY("Water_pressure");                                 HACONFIG; HAPRESSURE;    CAT_MEASUREMENT;                             VALUE_u8div10;
        case    8 : return 0;
        case    9 : KEY("Flow");                                           HACONFIG; HAFLOW;
                    Flow = FN_u16div10_LE(payloadPointer);                 CAT_MEASUREMENT;                                                      VALUE_u16div10_LE_changed(Flow_changed);
        case   10 : return 0;
        case   11 : KEY("SW_Version_inside_unit");                                                                                               VALUE_u16hex_LE;
        case   12 : return 0;
        case   13 : if (payloadByte || payload[payloadIndex - 1]) { KEY("SW_Version_Outside_Unit");                                                VALUE_u16hex_LE; }
                    return 0;
        default :   UNKNOWN_BYTE;
      }
      default   : UNKNOWN_BYTE;
    }
    case 0x14 :
                                                                           CAT_SETTING; // PacketType14 temperature setpoints and targets
                switch (packetSrc) {
      case 0x00 :
                  switch (payloadIndex) {
        case    0 : return 0;
        case    1 : KEY("Temperature_Setpoint_LWT_ABS_Heating_Zone_Main");                              HACONFIG; HATEMP;                            VALUE_f8_8; // or f8s8? // in ABS mode
        case    2 : return 0;
        case    3 : KEY("Temperature_Setpoint_LWT_ABS_Cooling_Zone_Main");                              HACONFIG; HATEMP;                            VALUE_f8_8; // or f8s8? // in ABS mode
        case    4 : return 0;
        case    5 : KEY("Temperature_Setpoint_LWT_ABS_Heating_Zone_Add");                               HACONFIG; HATEMP;                            VALUE_f8_8; // or f8s8? // in ABS mode
        case    6 : return 0;
        case    7 : KEY("Temperature_Setpoint_LWT_ABS_Cooling_Zone_Add");                               HACONFIG; HATEMP;                            VALUE_f8_8; // or f8s8? // in ABS mode
        case    8 : KEY("Deviation_LWT_Heating_Zone_Main");                                                                                      VALUE_s4abs1c;
        case    9 : KEY("Deviation_LWT_Cooling_Zone_Main");                                                                                      VALUE_s4abs1c; // guess
        case   10 : KEY("Deviation_LWT_Heating_Zone_Add");                                                                                       VALUE_s4abs1c;
        case   11 : KEY("Deviation_LWT_Cooling_Zone_Add");                                                                                       VALUE_s4abs1c; // guess
        case   12 : // first package 37 instead of 00 (55 = initial target temp?)
        case   13 :
        case   14 :
        default :   UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : return 0;                                                     // no need to output to HA if duplicate from above
        case    1 : KEY("Temperature_Setpoint_LWT_ABS_Heating_Zone_Main");
        case    2 : return 0;
        case    3 : KEY("Temperature_Setpoint_LWT_ABS_Cooling_Zone_Main");
        case    4 : return 0;
        case    5 : KEY("Temperature_Setpoint_LWT_ABS_Heating_Zone_Add");
        case    6 : return 0;
        case    7 : KEY("Temperature_Setpoint_LWT_ABS_Cooling_Zone_Add");
        case    8 : KEY("Deviation_LWT_Heating_Zone_Main");
        case    9 : KEY("Deviation_LWT_Cooling_Zone_Main");
        case   10 : KEY("Deviation_LWT_Heating_Zone_Add");
        case   11 : KEY("Deviation_LWT_Cooling_Zone_Add");
        case   15 : return 0;
        case   16 : if (payloadByte || payload[payloadIndex - 1]) {
                      KEY("Target_LWT_Zone_Main");                                                    HACONFIG; HATEMP; CAT_TEMP;                  VALUE_f8s8; // TODO check s8/_8 temp in basic packet // really f8s8
                    }
                    return 0; // 2.4, too low, or cooling?
        case   17 : return 0;
        case   18 : if (payloadByte || payload[payloadIndex - 1]) {
                      KEY("Target_LWT_Zone_Add");                                                     HACONFIG; HATEMP; CAT_TEMP;                  VALUE_f8s8;
                    }
                    return 0;
        default :   UNKNOWN_BYTE;
      }
      default   :   UNKNOWN_BYTE;
    }
    case 0x15 :
                                                                           CAT_TEMP  ; // PacketType15 system settings and operation
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : return 0;
        case    1 : return 0;
        case    2 : KEY("Reboot-related-u24");                                                       CAT_UNKNOWN;                                      VALUE_u24hex_LE;
        default :   UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : return 0;
        case    1 : KEY("Temperature_Unused_400015_0");          HACONFIG; HATEMP;                                                                                  VALUE_f8_8;
        case    2 : return 0;
        case    3 : KEY("Temperature_Refrigerant_2");           HACONFIG; HATEMP;                                                                VALUE_f8_8;       // =? Brine_inlet_temp
        case    4 : return 0;
        case    5 : KEY("Temperature_Refrigerant_3_Q");           HACONFIG; HATEMP;                                                                VALUE_f8_8;
        default :             UNKNOWN_BYTE;
      }
      default :               UNKNOWN_BYTE;
    }
    // case 0x16 : // TODO reintroduce 0x16 decoding
    case 0x20 ... 0x21 : switch (packetSrc) {                   //  0x20 0x21 sensor-related?
      case 0x00 : // fallthrough
        //          return 0; // TODO check for payload content?
        default : UNKNOWN_BYTE; // 0x20 1-byte payload, currently not stored in history; 0x20 empty poayload
      case 0x40 : switch (payloadIndex) {
        case  0 : // 0x00 0x01
        case  2 : // 0x01 0x02 (related to Pref / cooling / ??)
        default : UNKNOWN_BYTE;
      }
    }
    case 0x60 ... 0x8F : switch (packetSrc) {                   // field setting
      case 0x00 : // fallthrough
         //         return 0; // TODO check for payload content?
      case 0x40 : switch (payloadIndex) {
        case  3        : // fallthrough
        case  7        : // fallthrough
        case 11        : // fallthrough
        case 15        : // fallthrough
        case 19        :                 FIELD_SETTING;
        case  0 ...  2 : // fallthrough
        case  4 ...  6 : // fallthrough
        case  8 ... 10 : // fallthrough
        case 12 ... 14 : // fallthrough
        case 16 ... 18 : return 0;
        default : UNKNOWN_BYTE;
      }
    }

// 0x90-0x9F restart info?
    case 0x90 ... 0x9F : switch (packetSrc) {
      case 0x00 : /* if (payloadByte) */ UNKNOWN_BYTE;
//                  return 0;
      case 0x40 : /* if (payloadByte) */ UNKNOWN_BYTE;
//                  return 0;
      default : UNKNOWN_BYTE;
    }
// 0xA1 name of outside unit
// 0xB1 name of inside unit
    case 0xA1 : return 0; // TODO return text
    case 0xB1 : return 0; // TODO return text
    case 0xB8 :
                                                                           HACONFIG; CAT_COUNTER    ; // PacketType B8 system settings and operation
                switch (packetSrc) {
      case 0x00 :             return 0;                         // no info in 0x00 payloads regardless of subtype
      case 0x40 : switch (payload[0]) {
        case 0x00 : switch (payloadIndex) {                                // payload B8 subtype 00
          case  0 : return 0; // subtype
          case  1 : return 0;
          case  2 : return 0;
          case  3 : KEY("Electricity_Consumed_Backup_Heating");                           HAKWH;                                                 VALUE_u24_LE; // electricity used for room heating, backup heater
          case  4 : return 0;
          case  5 : return 0;
          case  6 : KEY("Electricity_Consumed_Backup_DHW");                               HAKWH;                                                 VALUE_u24_LE; // electricity used for DHW, backup heater
          case  7 : return 0;
          case  8 : return 0;
          case  9 : KEY("Electricity_Consumed_Compressor_Heating");                       HAKWH;                                                 VALUE_u24_LE; // electricity used for room heating, compressor
          case 10 : return 0;
          case 11 : return 0;
          case 12 : KEY("Electricity_Consumed_Compressor_Cooling");                       HAKWH;                                                 VALUE_u24_LE; // electricity used for cooling, compressor
          case 13 : return 0;
          case 14 : return 0;
          case 15 : KEY("Electricity_Consumed_Compressor_DHW");                           HAKWH;                                                 VALUE_u24_LE; // eletricity used for DHW, compressor
          case 16 : return 0;
          case 17 : return 0;
          case 18 : KEY("Electricity_Consumed_Total");                                    HAKWH;                                                 VALUE_u24_LE; // electricity used, total
          default : UNKNOWN_BYTE;
        }
        case 0x01 : switch (payloadIndex) {                                // payload B8 subtype 01
          case  0 : return 0; // subtype
          case  1 : return 0;
          case  2 : return 0;
          case  3 : KEY("Energy_Produced_Heatpump_Heating");                              HAKWH;                                                 VALUE_u24_LE; // energy produced for room heating
          case  4 : return 0;
          case  5 : return 0;
          case  6 : KEY("Energy_Produced_Heatpump_Cooling");                              HAKWH;                                                 VALUE_u24_LE; // energy produced when cooling
          case  7 : return 0;
          case  8 : return 0;
          case  9 : KEY("Energy_Produced_Heatpump_DHW");                                  HAKWH;                                                 VALUE_u24_LE; // energy produced for DHW
          case 10 : return 0;
          case 11 : return 0;
          case 12 : KEY("Energy_Produced_Heatpump_Total");                                HAKWH;                                                 VALUE_u24_LE; // energy produced total
          default : UNKNOWN_BYTE;
        }
        case 0x02 : switch (payloadIndex) {                                // payload B8 subtype 02
          case  0 :           return 0; // subtype
          case  1 :           return 0;
          case  2 :           return 0;
          case  3 :           KEY("Hours_Circulation_Pump");                              HAHOURS;                                               VALUE_u24_LE;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 :           KEY("Hours_Compressor_Heating");                            HAHOURS;                                               VALUE_u24_LE;
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :           KEY("Hours_Compressor_Cooling");                            HAHOURS;                                               VALUE_u24_LE;
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 :           KEY("Hours_Compressor_DHW");                                HAHOURS;                                               VALUE_u24_LE;
          default :           UNKNOWN_BYTE;
        }
        case 0x03 : switch (payloadIndex) {                                // payload B8 subtype 03
          case  0 :           return 0; // subtype
          case  1 :           return 0;
          case  2 :           return 0;
          case  3 :           KEY("Hours_Backup1_Heating");                               HAHOURS;                                               VALUE_u24_LE;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 :           KEY("Hours_Backup1_DHW");                                   HAHOURS;                                               VALUE_u24_LE;
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :           KEY("Hours_Backup2_Heating");                               HAHOURS;                                               VALUE_u24_LE;
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 :           KEY("Hours_Backup2_DHW");                                   HAHOURS;                                               VALUE_u24_LE;
          case 13 :           return 0;
          case 14 :           return 0;
          case 15 :           KEY("Hours_Unknown_34");                                    HAHOURS;                                               VALUE_u24_LE; // ?
          case 16 :           return 0;
          case 17 :           return 0;
          case 18 :           KEY("Hours_Drain_Pan_Heater");                              HAHOURS;                                               VALUE_u24_LE; // Drain pan heater (BPV/bodemplaat verwarming), thanks to Pim Snoeks for finding this function
          default :           UNKNOWN_BYTE;
        }
        case 0x04 : switch (payloadIndex) {                                // payload B8 subtype 04
          case  0 :           return 0; // subtype
          case  1 :           return 0;
          case  2 :           return 0;
          case  3 :           KEY("Counter_Unknown_00");                                                                                         VALUE_u24_LE;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 :           KEY("Counter_Unknown_01");                                                                                         VALUE_u24_LE;
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :           KEY("Counter_Unknown_02");                                                                                         VALUE_u24_LE;
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 :           KEY("Starts_Compressor");                                                                                          VALUE_u24_LE;
          default :           UNKNOWN_BYTE;
        }
        case 0x05 : switch (payloadIndex) {                                // payload B8 subtype 05
          case  0 :           return 0; // subtype
          case  1 :           return 0;
          case  2 :           return 0;
          case  3 :           KEY("Hours_Gasboiler_Heating");                                    HAHOURS;                                        VALUE_u24_LE;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 :           KEY("Hours_Gasboiler_DHW");                                        HAHOURS;                                        VALUE_u24_LE;
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :           KEY("Counter_Gasboiler_Heating");                                                                                  VALUE_u24_LE; // older models report 0
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 :           KEY("Counter_Gasboiler_DHW");                                                                                      VALUE_u24_LE; // older models report 0
          case 13 :           return 0;
          case 14 :           return 0;
          case 15 :           KEY("Starts_Gasboiler");                                                                                           VALUE_u24_LE;
          case 16 :           return 0;
          case 17 :           return 0;
          case 18 :           KEY("Counter_Gasboiler_Total");                                                                                    VALUE_u24_LE; // older models report 0
          default :           UNKNOWN_BYTE;
        }
        default :             UNKNOWN_BYTE;
      }
      default :               UNKNOWN_BYTE;
    }
    // Packet types 0x3x from main controller (0x00) and from first auxiliary controller (0xF0)
    // Code here assumes there is no secondary controller active (TODO: what if)
    case 0x30 :                                                                                     CAT_MEASUREMENT;  // PacketType 30 will be used here to communicate power and COP calculations
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case  0 : // bytes in 0x30 message do not seem to contain usable information
                  // as this is the final payload in a package
                  // we (ab)use this to communicate calculated power
                  // power produced by gas boiler / based on 4182 J/kg K, 1 kg/l
                  if (LWT_changed || MWT_changed || Flow_changed) {
                    Power1 = (LWT - MWT) * Flow * 69.7;
                    KEY("Heat_Production_Gasboiler");
                    LWT_changed = 0;                                      HACONFIG; HAPOWER;
                    SRC(9);                                                                                                                      VALUE_F(Power1);
                  } else return 0;
        case  1 : // power produced by heat pump
                  if (RWT_changed || MWT_changed || Flow_changed) {
                    Power2 = (MWT - RWT) * Flow * 69.7;
                    KEY("Heat_Production_Heatpump");
                    RWT_changed = MWT_changed = Flow_changed = 0;
                    SRC(9);                                                HACONFIG; HAPOWER;                                                    VALUE_F(Power2);
                  } else return 0;
        case  2 : // terminate json string at end of package
                  TERMINATEJSON;
        default : // these bytes change but don't carry real information
                  // they are used to request or announce further 3x payloads
                  // so we return 0 to signal they don't need to be output
                  return 0;
      }
      case 0x40 : switch (payloadIndex) {
        default : return 0;
      }
      default : UNKNOWN_BYTE;
    }
    case 0x31 :                                                            CAT_MEASUREMENT; // PacketType B8 system settings and operation
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case  0 :             BITBASIS_UNKNOWN;
        case  1 :             KEY("F031_1");                               CAT_UNKNOWN;                                                          VALUE_u8hex;
        case  2 :             KEY("F031_2");                               CAT_UNKNOWN;                                                          VALUE_u8hex;
        case  3 :             KEY("UI_status_LCD");                         CAT_UNKNOWN;                                                          VALUE_u8hex; // 01=LCD off, 81=LCD on ??; 0x80/0x00 also seen during reboot (UI off/on) (LCD off/on) ?
        case  4 :             KEY("UI_Installer_Mode");                         CAT_UNKNOWN;                                                          VALUE_u8hex; // B4=installer, 74=user mode 60=during reboot?
        case  5 : switch (bitNr) {
          case    0 : KEY("Reboot_related_Q31-00-5-0");                       CAT_UNKNOWN;                                                          VALUE_flag8;
          case    8 : BITBASIS;
          case    6 : return 0; // suppress hourly pulse reporting
                      KEY("Hourly_Pulse_1");                               CAT_SETTING;                                                          VALUE_flag8;
          case    2 : return 0; // suppress error messages caused by inserted messages and counter requests
                      KEY("Protocol_Missing_F0_Reply_Detected");           CAT_SETTING;                                                          VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case  6 :             SelectTimeString2 = 4;
                              // Prefer use of TimeString2,
                              // unless we are not seeing these
                              TimeString2[5] = '0' + (payloadByte / 10);
                              TimeString2[6] = '0' + (payloadByte % 10);
                              return 0;
                              KEY("Year_2");                                                                                                     VALUE_u8_add2k;
        case  7 :
                              TimeString2[8] = '0' + (payloadByte / 10);
                              TimeString2[9] = '0' + (payloadByte % 10);
                              return 0;
                              KEY("Month_2");                                                                                                    VALUE_u8;
        case  8 :
                              TimeString2[11] = '0' + (payloadByte / 10);
                              TimeString2[12] = '0' + (payloadByte % 10);
                              return 0;
                              KEY("Day_2");                                                                                                      VALUE_u8;
        case  9 :
                              TimeString2[14] = '0' + (payloadByte / 10);
                              TimeString2[15] = '0' + (payloadByte % 10);
                              return 0;
                              KEY("Hours_2");                                                                                                    VALUE_u8;
        case 10 :
                              TimeString2[17] = '0' + (payloadByte / 10);
                              TimeString2[18] = '0' + (payloadByte % 10);
                              return 0;
                              KEY("Minutes_2");                                                                                                  VALUE_u8;
        case 11 :
                              // if ((SelectTimeString2) && ((TimeString2[18] - '0') != (payloadByte % 10))) TimeStringCnt = 0; // reset at start of new minute
                              TimeString2[20] = '0' + (payloadByte / 10);
                              TimeString2[21] = '0' + (payloadByte % 10);
                              if (payloadByte != prevSec) {
                                prevSec = payloadByte;
                                KEY("Date_Time");
                                SRC(9);                                                                                 maxOutputFilter = 2;     VALUE_timeString(TimeString2);
                              }
                              return 0;
                              KEY("Seconds_2");                                                                                                  VALUE_u8;
        default:              return 0;
      }
      case 0x40 : switch (payloadIndex) {
        default:              return 0;
      }
    }
    // PacketTypes 32,33,34,3F not observed
    case 0x32 : // fallthrough
    case 0x33 : // fallthrough
    case 0x34 : // fallthrough
    case 0x3F : return 0;
    // PacketTypes 35,3A 8-bit parameters
    case 0x35 : // fallthrough
    case 0x3A :                                                            CAT_SETTING;
                switch (packetSrc) {
      case 0x00 : // fallthrough
      case 0x40 : switch (payloadIndex) {
        case    2 :             // fallthrough
        case    5 :             // fallthrough
        case    8 :             // fallthrough
        case   11 :             // fallthrough
        case   14 :             // fallthrough
        case   17 :             HANDLE_PARAM(1); // 8-bit parameter handling
        default   :             return 0; // nothing to do
      }
      default :               return 0; // unknown packetSrc
    }
    case 0x36 : // fallthrough 16-bit
    case 0x3B :
                switch (packetSrc) {
      case 0x00 : // fallthrough
      case 0x40 : switch (payloadIndex) {
        case    3 : // fallthrough
        case    7 : // fallthrough
        case   11 : // fallthrough
        case   15 : // fallthrough
        case   19 :                                                                                                                              HANDLE_PARAM(2); // 16-bit parameter handling
        default   : return 0; // nothing to do
      }
      default :               return 0; // unknown packetSrc
    }
    case 0x37 : // fallthrough 24-bit
    case 0x3C :
                switch (packetSrc) {
      case 0x00 : // fallthrough
      case 0x40 : switch (payloadIndex) {
        case    4 :             // fallthrough
        case    9 :             // fallthrough
        case   14 :             // fallthrough
        case   19 :             HANDLE_PARAM(3); // 24-bit parameter handling
        default   :             return 0; // nothing to do
      }
      default :               return 0; // unknown packetSrc
    }
    case 0x38 : // fallthrough 24-bit
    case 0x39 : // fallthrough 24-bit
    case 0x3D :
                switch (packetSrc) {
      case 0x00 : // fallthrough
      case 0x40 : switch (payloadIndex) {
        case    5 :             // fallthrough
        case   11 :             // fallthrough
        case   17 :             HANDLE_PARAM(4); // 32-bit parameter handling
        default   :             return 0; // nothing to do
      }
      default :               return 0; // unknown packetSrc
    }
#ifdef SAVESCHEDULE
    case 0x3E :
                switch (packetSrc) {
      case 0x00 : // fallthrough
      case 0x40 : switch (payload[0]) {
        case 0x00 : // announcement of memory data transfer
                  switch (payloadIndex) {
          case    0 : // payload[0] = 0x00 here
                      return 0;
          case    1 : // LSB memory location
                      scheduleMemLoc[PS] = payloadByte;
                      return 0;
          case    2 : // MSB memory location
                      scheduleMemLoc[PS] |= ((uint16_t) (payloadByte << 8));
                      if (scheduleMemLoc[PS] == 0xFFFF) {
                        // dummy padding
                      } else if (scheduleMemLoc[PS] > SCHEDULE_MEM_START + SCHEDULE_MEM_SIZE) {
                        // Warning: scheduleMemloc out of expected range
                        //Sprint("WARNING1 scheduleMemloc out of expected range %x", scheduleMemLoc[PS]);
                      } else if (scheduleMemLoc[PS] < SCHEDULE_MEM_START) {
                        // Warning: scheduleMemloc out of expected range
                        //Sprint("WARNING1 scheduleMemloc out of expected range %x", scheduleMemLoc[PS]);
                      } // else nothing to do
                      return 0;
          case    3 : // length of data transfer
                      scheduleSeq[PS] = 0;
                      if (scheduleMemLoc[PS] != 0xFFFF) {
                        scheduleLength[PS] = payloadByte;
                        if (scheduleMemLoc[PS] + scheduleLength[PS] > SCHEDULE_MEM_START + SCHEDULE_MEM_SIZE) {
                          // WARNING scheduleMemLoc+scheduleLength out of expected range
                        }
                      }
                      newSched[PS] = 0;
                      weeklyMomentCounter[PS] = 0;
                      dayCounter[PS] = 0;
                      dailyMomentsCounter[PS] = 0;
                      dailyMoments[PS] = 6;
                      isSilentSchedule[PS] = false;
                      isHCSchedule[PS] = false;
                      isElectricityPriceSchedule[PS] = false;
                      mqtt_value_schedule[PS][0] = '\"';
                      mqtt_value_p[PS] = 1;
                      byteLoc2[PS] = scheduleMemLoc[PS];
                      isWeeklySchedule[PS] = true;
                      switch (scheduleMemLoc[PS]) {
                        case 0x0250 : dailyMoments[PS] = 2; isSilentSchedule[PS] = true; break;
                        case 0x026C : dailyMoments[PS] = 2; break;
                        case 0x0288 : dailyMoments[PS] = 4; break;
                        case 0x02C0 : dailyMoments[PS] = 4; break;
                        case 0x02F8 : isWeeklySchedule[PS] = true; break;
                        case 0x034C : break;
                        case 0x03A0 : break;
                        case 0x03F4 : break;
                        case 0x0448 : isHCSchedule[PS] = true; break;
                        case 0x049C : isHCSchedule[PS] = true; break;
                        case 0x04F0 : isHCSchedule[PS] = true; break;
                        case 0x0544 : isHCSchedule[PS] = true; break;
                        case 0x0598 : break;
                        case 0x0640 : isElectricityPriceSchedule[PS] = true; break;
                        case 0x0694 : isWeeklySchedule[PS] = false; break; // 12 bytes, ??
                        case 0x06A0 : isWeeklySchedule[PS] = false; break; // 15 bytes, ??
                        case 0x06AF : isWeeklySchedule[PS] = false; break; // 15 bytes, ??
                        case 0x06BE : isWeeklySchedule[PS] = false; break; // 35 bytes, could be weekly 7 * 5 ?
                        case 0x06E1 : isWeeklySchedule[PS] = false; break; // 45 bytes, ??
                        default : ;
                      }
                      return 0;
          case    4 : // usually 0x00 (Src=0x00, supported) or 0xFF (Src=0x40, not (yet) supported?)
                      if (payloadByte == 0xFF) scheduleLength[PS] = 0;
                      return 0;
          default : return 0;
        }
        case 0x01 :
                  switch (payloadIndex) {
          case    0 : // payload[0] = 0x01 here
                      return 0;
          case    1 : // sequence number, verify
                      if (scheduleLength[PS] && (scheduleSeq[PS]++ != payloadByte)) {
                        // WARNING subseq nr out of sync
                        printfTopicS("Warning: 0x3E subsequence out of sync");
                      };
                      return 0;
          default   : // we received one byte from/for the schedule memory
                      newMem = 0;
                      if (scheduleLength[PS]) {
                        if ((!scheduleMemSeen[PS][scheduleMemLoc[PS] - SCHEDULE_MEM_START]) || (scheduleMem[PS][scheduleMemLoc[PS] - SCHEDULE_MEM_START] != payloadByte)) {
                          // new (or first) value for this memory byte
                          newMem = 1;
                          newSched[PS] = 1;
                        };
                        scheduleMem[PS][scheduleMemLoc[PS] - SCHEDULE_MEM_START] = payloadByte;
                        scheduleMemSeen[PS][scheduleMemLoc[PS] - SCHEDULE_MEM_START] = 1;
                        uint16_t byteLoc = scheduleMemLoc[PS];
#define MAXSECTION 10 // max info length of <hour> or <info for schedule>
                        if (mqtt_value_p[PS] + MAXSECTION + 3 < MQTT_VALUE_LEN) {
                          if (isWeeklySchedule[PS]) {
                            if (byteLoc & 0x01) {
                              if (payloadByte != 0xFF) {
                                // 2nd byte: new schedule content
                                if (isSilentSchedule[PS]) {
                                  mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MQTT_VALUE_LEN - mqtt_value_p[PS], "%X ", payloadByte);
                                } else if (isHCSchedule[PS]) {
                                  switch (payloadByte) {
                                    case 0x00 : mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MAXSECTION, "Eco "); break;
                                    case 0x01 : mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MAXSECTION, "Comfort "); break;
                                    case 0x0C : mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MAXSECTION, "-10 "); break;
                                    case 0x34 : mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MAXSECTION, "+10 "); break;
                                    default   : mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MAXSECTION, "... ");
                                  }
                                } else if (isElectricityPriceSchedule[PS]) {
                                  switch (payloadByte) {
                                    case 0x00 : mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MAXSECTION, "Low "); break;
                                    case 0x01 : mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MAXSECTION, "Med "); break;
                                    case 0x02 : mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MAXSECTION, "High "); break;
                                    default   : mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MAXSECTION, "... ");
                                  }
                                } else {
                                  // unkown schedule
                                  mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MAXSECTION, "%i ", payloadByte);
                                }
                              }
                            } else {
                              if (payloadByte != 0xFF) {
                                // 1st byte: time of new schedule moment
                                while (byteLoc >= byteLoc2[PS]) {
                                  if (!dailyMomentsCounter[PS]) {
                                    if (byteLoc == byteLoc2[PS]) {
                                      mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MAXSECTION, "%s ", weekDay[dayCounter[PS]]);
                                    }
                                    dailyMomentsCounter[PS] = dailyMoments[PS];
                                    dayCounter[PS]++;
                                  }
                                  dailyMomentsCounter[PS]--;
                                  byteLoc2[PS] += 2;
                                }
                                mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MAXSECTION, "%i:%i0 ", payloadByte / 6, payloadByte % 6);
                              }
                            }
                          } else {
                            mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], MAXSECTION, "%02X", payloadByte);
                          }
                        }
                        scheduleMemLoc[PS]++;
                        if (--scheduleLength[PS] == 0) {
                          // Last byte of memory fragment received, so generate mqtt_value for output
                         if (*(mqtt_value_schedule[PS] + mqtt_value_p[PS] - 1) == ' ') mqtt_value_p[PS]--;
                         mqtt_value_p[PS] += snprintf(mqtt_value_schedule[PS] + mqtt_value_p[PS], 2, "\"");
                          strncpy(mqtt_value, mqtt_value_schedule[PS], MQTT_VALUE_LEN);
                          mqtt_value[MQTT_VALUE_LEN - 1] = '\0';
                          if (mqtt_value_p[PS] <= 2) strncpy(mqtt_value, "\"Empty\"", 10);

                          switch (scheduleMemLoc[PS]) {
                            case 0x0250 : break; // Warning: at end of read operation, scheduleMemLoc points to start of next memory segment; table thus 1 line shifted versus protocol-doc
                            case 0x026C : KEY("Program_Silent_Mode"); break;
                            case 0x0288 : KEY("Program_Q0_2_Moments_Per_Day"); break;
                            case 0x02C0 : KEY("Program_Q1_4_Moments_Per_Day"); break;
                            case 0x02F8 : KEY("Program_Q2_4_Moments_Per_Day"); break;
                            case 0x034C : KEY("Program_Q3_Nightly_Filled"); break;
                            case 0x03A0 : KEY("Program_Q4_6_Moments_Per_Day"); break;
                            case 0x03F4 : KEY("Program_Q5_6_Moments_Per_Day"); break;
                            case 0x0448 : KEY("Program_Q6_6_Moments_Per_Day"); break;
                            case 0x049C : KEY("Program_Heating_Own_Program_1"); break;
                            case 0x04F0 : KEY("Program_Heating_Own_Program_2"); break;
                            case 0x0544 : KEY("Program_Heating_Own_Program_3"); break;
                            case 0x0598 : KEY("Program_Cooling_Own_Program_1"); break;
                            case 0x05EC : KEY("Program_Q7_6_Moments_Per_Day"); break;
                            case 0x0640 : KEY("Program_Q8_6_Moments_Per_Day"); break;
                            case 0x0694 : KEY("Program_Electricity_Price"); break;
                            case 0x06A0 : KEY("Memory_0x0694_0x069F"); break; // phone number customer service UI
                            case 0x06AF : KEY("Memory_0x06A0_0x06AE"); break;
                            case 0x06BE : KEY("Memory_0x06AF_0x06BD"); break; // error history (5 errors E1 E2 YY MM DD HH mm)
                            case 0x06E1 : KEY("Memory_0x06BE_0x06E0"); break; // names of 3 heating schedules (15 characters each)
                            case 0x070E : KEY("Memory_0x06E1_0x070D"); break;
                            default : KEY("Memory_Unknown"); break;
                          }
                          CAT_SCHEDULE;
                          return newSched[PS];
                        }
                      } else {
                        // Receiving more (padding) bytes than fragment length
                      }
                      return 0; // return newMem;
        }; return 0;
        default   : return 0; // not observed
      }
      default : UNKNOWN_BYTE // unknown source
    }
#else
    case 0x3E : return 0;
#endif /* SAVESCHEDULE */
// PSEUDO_PACKETS
#include "P1P2_pseudo.h"
    default : UNKNOWN_BYTE // unknown PacketByte
  }
  return 0;
}

byte bits2keyvalue(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte j) {
  strcpy(mqtt_key, mqttKeyPrefix);
  byte b = bytesbits2keyvalue(packetSrc, packetType, payloadIndex, payload, mqtt_key + MQTT_KEY_PREFIXLEN, mqtt_value, j) ;
  return b;
}

byte bytes2keyvalue(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value) {
  return bits2keyvalue(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, 8) ;
}

#endif /* P1P2_Daikin_ParameterConversion_EHYHB */
