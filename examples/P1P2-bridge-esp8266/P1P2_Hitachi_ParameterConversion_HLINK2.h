/* P1P2_Daikin_ParameterConversion_EHYHB.h product-dependent code for EHYHBX08AAV3 and perhaps other EHYHB models
 *
 * Copyright (c) 2019-2023 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * WARNING: P1P2-bridge-esp8266 is end-of-life, and will be replaced by P1P2MQTT
 *
 * Version history
 * 20230611 v0.9.38 Hlink merge into main branch
 * 20230526 v0.9.36 threshold
 * 20230211 v0.9.33H fixed flag8/HA
 * 20230122 v0.9.31 H-link data
 *
 */

// This file translates data from the H-link bus to (mqtt_key,mqtt_value) pairs, code here is highly device-dependent,
// in order to implement parameter decoding for another product, rename this file and make any necessary changes
// (in particular in the big switch statements below).
//
// The main conversion function here is bytes2keyvalue(..),
// which is called for every individual byte of every payload.
// It aims to calculate a <mqtt_key,mqtt_value> pair for mqtt output.
//
// Its return value determines what to do with the result <mqtt_key,mqtt_value>:
//
// Return value:
// 0 no value to return
// 1 indicates that a new (mqtt_key,mqtt_value) pair can be output via serial, telnet, and/or mqtt
// 8 this byte should be treated on an individual bit basis by calling bits2keyvalue(.., bitNr) 8 times with 0 <= bitNr <= 7
//
// The core of the translation is formed by large nested switch statements.
// Many of the macro's include break statements and make the core more readable.

#ifndef P1P2_Daikin_ParameterConversion_HLINK
#define P1P2_Daikin_ParameterConversion_HLINK

#include "P1P2_Config.h"
#include "P1P2Serial_ADC.h"

#define CAT_SETTING      { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'S'); }  // system settings
#define CAT_TEMP         { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'T'); maxOutputFilter = 1; }  // TEMP
#define CAT_MEASUREMENT  { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'M'); maxOutputFilter = 1; }  // non-TEMP measurements
#define CAT_COUNTER      { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'C'); } // kWh/hour counters
#define CAT_PSEUDO2      { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'B'); } // 2nd ESP operation
#define CAT_PSEUDO       { if (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] != 'B') mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'A'; } // ATmega/ESP operation
#define CAT_TARGET       { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'D'); } // D desired
#define CAT_DAILYSTATS   { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'R'); } // Results per day (tbd)
#define CAT_SCHEDULE     { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'E'); } // Schedule
#define CAT_UNKNOWN      { (mqtt_key[MQTT_KEY_PREFIXCAT - MQTT_KEY_PREFIXLEN] = 'U'); }

#define SRC(x)           { (mqtt_key[MQTT_KEY_PREFIXSRC - MQTT_KEY_PREFIXLEN] = '0' + x); }

#ifdef __AVR__

// ATmega328 works on 16MHz Arduino Uno, not on Minicore...., use strlcpy_PF, F()
#define KEY(K)        strlcpy_PF(mqtt_key, F(K), MQTT_KEY_LEN - 1)

#elif defined ESP8266

// ESP8266, use strncpy_P, PSTR()

#include <pgmspace.h>
#ifdef REVERSE_ENGINEER
#define KEY(K) { byte l = snprintf(mqtt_key, MQTT_KEY_LEN, "0x%02X_0x%02X_%i_", packetType, packetSrc, payloadIndex); strncpy_P(mqtt_key + l, PSTR(K), MQTT_KEY_LEN - l); mqtt_key[MQTT_KEY_LEN - 1] = '\0'; };
#else
#define KEY(K) { strncpy_P(mqtt_key, PSTR(K), MQTT_KEY_LEN); mqtt_key[MQTT_KEY_LEN - 1] = '\0'; };
#endif

#else /* not ESP8266, not AVR, so RPI/Ubuntu/.., use strncpy  */

#define KEY(K) { strncpy(mqtt_key, K, MQTT_KEY_LEN - 1); mqtt_key[MQTT_KEY_LEN - 1] = '\0'; };

#endif /* __AVR__ / ESP8266 */

byte maxOutputFilter = 0;

#ifdef SAVEPACKETS

// for Hitachi, use 1-dimensional arry nr_bytes[]
// 0 .. 7 for pseudo packets 000008 - 00000F
// 8 .. F for pseudo packets 400008 - 40000F
// 
// Hitachi type 1 
// 10 21 00 0B
// 11 21 00 12
// 12 89 00 27
// 13 89 00 2D
// 14 41 00 18
//    89 06
//    21 06
//    41 06
//
// Hitachi type 2 
// 15 21 00 29 xx xx xx xx F1
// 16 21 00 29 xx xx xx xx F2
// 17 21 00 29 xx xx xx xx F3
// 18 21 00 29 xx xx xx xx F4
// 19 21 00 29 xx xx xx xx F5
// 1A 89 00 17 xx xx xx xx E1
// 1B 89 00 29 xx xx xx xx E2
// 1C 89 00 29 xx xx xx xx E3
// 1D 89 00 29 xx xx xx xx E4
// 1E 89 00 29 xx xx xx xx E5
// 
// Hitachi type 3
//
// 15 21 00 29 xx xx xx xx F1
// 16 21 00 29 xx xx xx xx F2
// 17 21 00 29 xx xx xx xx F3
// 18 21 00 29 xx xx xx xx F4
// 19 21 00 29 xx xx xx xx F5 (not observed)
// 1A 89 00 17 xx xx xx xx E1
// 1B 89 00 29 xx xx xx xx E2
// 1C 89 00 29 xx xx xx xx E3
// 1D 89 00 29 xx xx xx xx E4
// 1E 89 00 29 xx xx xx xx E5 (not observed)
// 1F 41 00 29 xx xx xx xx E1
// 20 8A 00 29 xx xx xx xx F1


#define NR_BYTES_SZ 0x21
//byte pti                                      =   0..F for pseudo                                                                  10-14                            15-1E                                             1F-20
const PROGMEM uint32_t nr_bytes[NR_BYTES_SZ]  = {  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,      7,  14,  20,  35,  41,         37,  37,  37,  37,  37,  19,  37,  37,  37,  37,  37,  37 };
const PROGMEM uint32_t bytestart[NR_BYTES_SZ] = {   0,  20,  40,  60,  80, 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300,    320, 327, 341, 361, 396,/*rst*/ 320, 357, 394, 431, 468, 505, 524, 561, 598, 635, 672, 709 /* , 746 */ };
#define sizeValSeen 746
byte payloadByteVal[sizeValSeen]  = { 0 };
byte payloadByteSeen[sizeValSeen] = { 0 };
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
#define HYSTERESIS_SIZE_F8_8 25     // 25/256 ~ 0.1
#define HYSTERESIS_SIZE_F8S8 1      //  1/10  = 0.1
#define HYSTERESIS_SIZE_U16DIV10 1  //  1/10  = 0.1
#define HYSTERESIS_SIZE_S16DIV10 1  //  1/10  = 0.1
// #define HYSTERESIS_TYPE_S_F_S8 2 // F-series

uint16_t FN_u16_LE(uint8_t *b)          { return (                (((uint16_t) b[-1]) << 8) | (b[0]));}
uint32_t FN_u24_LE(uint8_t *b)          { return (((uint32_t) b[-2] << 16) | (((uint32_t) b[-1]) << 8) | (b[0]));}
uint32_t FN_u32_LE(uint8_t *b)          { return (((uint32_t) b[-3] << 24) | ((uint32_t) b[-2] << 16) | (((uint32_t) b[-1]) << 8) | (b[0]));}

void resetDataStructures(void) {
// warning: called in onMqttMessage callback, do not output via mqtt or telnet in this function
#ifdef SAVEPACKETS
  uint16 j;
  for (j = 0; j < sizeValSeen; j++) {
    payloadByteVal[j]  = 0;
    payloadByteSeen[j] = 0;
  }
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

  byte pti;
  switch (packetType) {
    // pseudo packets
    case 0x08 ... 0x0F : pti = packetType - (packetSrc ? 0 : 8); break;
    default: pti = 0xFF; break;
  }
  byte payload7 = (packetType > 7) ? payload[4] : 0;
  switch ((packetSrc << 8) | packetType) {
    // HLINK packets jetblack
    case 0x210B : pti = 0x10; break;
    case 0x2112 : pti = 0x11; break;
    case 0x4118 : pti = 0x12; break;
    case 0x8927 : pti = 0x13; break;
    case 0x892D : pti = 0x14; break;
    // new in v0.9.39
    case 0x2129 : pti = ((payload7 < 0xF1) || (payload7 > 0xF5)) ? 0xFF : 0x15 + (payload7 - 0xF1); break;
    case 0x8917 : /* fall-through */
    case 0x8929 : pti = ((payload7 < 0xE1) || (payload7 > 0xE5)) ? 0xFF : 0x15 + (payload7 - 0xE1); break;
    // new in v0.9.39a
    case 0x4129 : pti = 0x1F; break;
    case 0x8A29 : pti = 0x20; break;
    default     : break;
  }


  // printfTopicS("newpayloadbytesval pti=0x%02X packetSrc=0x%02X packetType=0x%02X payload7=0x%02X", pti, packetSrc, packetType, payload7);


  if (pti == 0xFF) {
    newByte = 1;
  } else if (payloadIndex > nr_bytes[pti]) {
    // Warning: payloadIndex > expected
    printfTopicS("Warning: payloadIndex %i > expected %i for Src 0x%02X Type 0x%02X", payloadIndex, nr_bytes[pti], packetSrc, packetType);
    newByte = true;
  } else if (payloadIndex + 1 < length) {
    // Warning: payloadIndex + 1 < length
    printfTopicS("Warning: payloadIndex + 1 < length");
    return 0;
  } else {
    bool pubHA = false;
    bool skipHysteresis = false;
    uint16_t pi2base = bytestart[pti];
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
        printfTopicS("Warning: pi2 > sizeValSeen");
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

bool newPayloadBitVal(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, byte haConfig, byte bitNr) {
// returns whether bit bitNr of byte payload[payloadIndex] has a new value (also returns true if byte has not been saved)
#ifdef SAVEPACKETS
  bool newBit = (outputFilter == 0);
  byte pti;
  switch (packetType) {
    // pseudo packets
    case 0x08 ... 0x0F : pti = packetType - (packetSrc ? 0 : 8); break;
    default: pti = 0xFF; break;
  }
  byte payload7 = (payload[2] > 7) ? payload[7] : 0;
  switch ((packetSrc << 8) | packetType) {
    // HLINK packets jetblack
    case 0x210B : pti = 0x10; break;
    case 0x2112 : pti = 0x11; break;
    case 0x4118 : pti = 0x12; break;
    case 0x8927 : pti = 0x13; break;
    case 0x892D : pti = 0x14; break;
    // new in v0.9.39
    case 0x2129 : pti = ((payload7 < 0xF1) || (payload7 > 0xF5)) ? 0xFF : 0x15 + (payload7 - 0xF1); break;
    case 0x8917 : /* fall-through */
    case 0x8929 : pti = ((payload7 < 0xE1) || (payload7 > 0xE5)) ? 0xFF : 0x15 + (payload7 - 0xE1); break;
    // new in v0.9.39a
    case 0x4129 : pti = 0x1F; break;
    case 0x8A29 : pti = 0x20; break;
    default     : break;
  }
  if (bitNr > 7) {
    // Warning: bitNr > 7
    newBit = true;
  } else if (pti == 0xFF) {
    newBit = true;
  } else if (payloadIndex > nr_bytes[pti]) {
    // Warning: payloadIndex > expected
    printfTopicS("Warning: payloadIndex %i > expected %i for Src 0x%02X Type 0x%02X", payloadIndex, nr_bytes[pti], packetSrc, packetType);
    newBit = true;
  } else {
    bool pubHA = false;
    uint16_t pi2 = bytestart[pti] + payloadIndex; // no multiplication, bit-wise only for u8 type
    if (pi2 >= sizeValSeen) {
      pi2 = 0;
      printfTopicS("Warning: pi2 > sizeValSeen");
      return 0;
    }
    byte bitMask = 1 << bitNr;
    if (payloadByteSeen[pi2] & bitMask) {
      // this bit has been seen and saved before.
      if ((payloadByteVal[pi2] ^ payload[payloadIndex]) & bitMask) {
        newBit = (outputFilter <= maxOutputFilter);
        payloadByteVal[pi2] &= (0xFF ^ bitMask);
        payloadByteVal[pi2] |= (payload[payloadIndex] & bitMask);
      } // else payloadBit seen and not changed
    } else {
      // first time for this bit
      if (haConfig) {
        // MQTT discovery
        // HA key
        HA_KEY
        // HA value
        HA_VALUE
        // publish key,value
        clientPublishMqtt(ha_mqttKey, ha_mqttValue);
      }
      newBit = 1;
      payloadByteVal[pi2] &= (0xFF ^ bitMask); // if array initialized to zero, not needed
      payloadByteVal[pi2] |= payload[payloadIndex] & bitMask;
      payloadByteSeen[pi2] |= bitMask;
    }
    if (outputFilter > maxOutputFilter) newBit = 0;
  }
  return (haConfig || (outputMode & 0x10000)) && newBit;
#else /* SAVEPACKETS */
  return (haConfig || (outputMode & 0x10000));
#endif /* SAVEPACKETS */
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
  if (length && (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, length, 1, thresholdType, thresholdSize))) return 0;
  if (!length) newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 1, 1);
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

// return 0:            use this for all bytes (except for the last byte) which are part of a multi-byte parameter
// BITBASIS (for j==8): this byte is to be treated on a bitbasis (by re-calling this function with j=0..7
//                      switch-statement for j is needed as shown below
//                      indicate for j=0..7: "KEY("KnownParameter"); VALUE_flag8" if bit usage is known
//                      indicate for j=0..7: "UNKNOWN_BIT", mqtt_key will then be set to "Bit-0x[source]-0x[payloadnr]-0x[location]-[bitnr]"
// BITBASIS_UNKNOWN:    shortcut for entire bit-switch statement if bit usage of all bits is unknown
// UNKNOWN_BYTE:        byte function is unknown, mqtt_key will be: "PacketSrc-0xXX-Type-0xXX-Byte-I-Bit-I", value will be hex
// UNKNOWN_BIT:         bit function is unknown, mqtt_key will be: "PacketSrc-0xXX-Type-0xXX-Byte-I-Bit-I", value will be hex
// VALUE_u8:            1-byte unsigned integer value at current location payload[i]
// VALUE_s8:            1-byte signed integer value at current location payload[i]
// VALUE_u16:           3-byte unsigned integer value at location payload[i - 1]..payload[i]
// VALUE_u24:           3-byte unsigned integer value at location payload[i-2]..payload[i]
// VALUE_u32:           4-byte unsigned integer value at location payload[i-3]..payload[i]
// VALUE_u8_add2k:      for 1-byte value (2000+payload[i]) (for year value)
// VALUE_s4abs1c:        for 1-byte value -10..10 where bit 4 is sign and bit 0..3 is value
// VALUE_f8_8           for 2-byte float in f8_8 format (see protocol description)
// VALUE_f8s8           for 2-byte float in f8s8 format (see protocol description)
// VALUE_u16div10       for 2-byte integer to be divided by 10 (used for flow in 0.1m/s)
// VALUE_F(value)       for self-calculated float parameter value
// VALUE_u32hex         for 4-byte hex value (used for sw version)
// VALUE_header         for empty payload string

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
#define UNKNOWN_BIT             { CAT_UNKNOWN; return        unknownBit(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, bitNr); }

#define VALUE_u8_add2k          { return    value_u8_add2k(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_s4abs1c             { return      value_s4abs1c(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }

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

#define BITBASIS_UNKNOWN        { switch (bitNr) { case 8 : BITBASIS; default : UNKNOWN_BIT; } }
#define BITBASIS                { return newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 1, 0) << 3; }
// BITBASIS returns 8 if at least one bit of a byte changed, or if at least one bit of a byte hasn't been seen before, otherwise 0
// value of byte and of seen status should not be saved (yet) (saveSeen=0), this is done on bit basis

#define HACONFIG { haConfig = 1; uom = 0; stateclass = 0;};
#define HATEMP { uom = 1;  stateclass = 1;};          // C
#define HAPOWER {uom = 2;  stateclass = 1;};          // W
#define HAFLOW  {uom = 3;  stateclass = 0;};          // L/s
#define HAKWH   {uom = 4;  stateclass = 2;};          // kWh
#define HAHOURS {uom = 5;  stateclass = 0;};          // h
#define HASECONDS {uom = 6;  stateclass = 0;};        // s
#define HAMILLISECONDS {uom = 7;  stateclass = 0;};   // ms
#define HABYTES   {uom = 8;  stateclass = 1;};        // byte
#define HAEVENTS  {uom = 9;  stateclass = 2;};        // events
#define HACURRENT {uom = 10; stateclass = 1;};        // A
#define HAFREQ    {uom = 11; stateclass = 1;};        // Hz
#define HAPERCENT {uom = 12; stateclass = 1;};        // %

// stateclass 0 default
// stateclass 1 measurement
// stateclass 2 total_increasing

uint16_t parameterWritesDone = 0; // # writes done by ATmega; perhaps useful for ESP-side queueing

byte bytesbits2keyvalue(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, /*char &cat, char &src,*/ byte bitNr) {
// payloadIndex: new payload documentation counts payload bytes starting at 0 (following Budulinek's suggestion)

  byte haConfig = 0;

  maxOutputFilter = 9; // default all changes visible, unless changed below
  CAT_UNKNOWN; // cat unknown unless changed below

  byte src;
  switch (packetSrc) {
    case 0x21 : src = 2; break; // indoor unit info
    case 0x89 : src = 8; break; // IU and OU system info
    case 0x8A : src = 7; break; // ?
    case 0x41 : src = 4; break; // sender is Hitachi remote control or Airzone
    case 0x00 : src = 0; break; // pseudopacket, ATmega
    case 0x40 : src = 1; break; // pseudopacket, ESP
    default   : src = 9; break;
  }
  SRC(src); // set char in mqtt_key prefix

  // For Hitachi ducted Unit. Interface is connected on the H-Link bus of the Indoor Unit <<==>> remote control.
  // An Airzone system is also connected to the bus
  // IU : RPI 4.0 FSN4E (Ducted Unit)
  // OU : RAS 4 HVCNC1E (Micro DRV IVX confort)
  // Remote control : PC-ARFP1E
  // year of fabrication : 2017
  // Airzone system easyzone with Hitachi RPI interface generation 2 (to be checked)
  //
  //
  HACONFIG;
  switch (packetSrc) {
    // 0x89 : sender is certainly the indoor unit providing data from the system
    //        including data from the outdoor unit
    case 0x89: switch (packetType) {
      case 0x29: switch (payload[4])  { // payload[4] can be 0xE1..0xE5, currently decode only 0xE2
        case 0xE2: switch (payloadIndex) {
          case  6: KEY("OUExpansionValve");                                                           CAT_MEASUREMENT; HAPERCENT;                  VALUE_u8;
          case  7: KEY("IUExpansionValve");                                                           CAT_MEASUREMENT; HAPERCENT;                  VALUE_u8;
          case  8: KEY("TargetCompressorFrequency");                                                  CAT_MEASUREMENT; HAFREQ;                     VALUE_u8;
          case  9: KEY("ControlCircuitRunStop");                                                      CAT_MEASUREMENT;                             VALUE_u8;
          case 10: KEY("HeatpumpIntensity");                                                          CAT_MEASUREMENT;                             VALUE_u8;
          case 14: KEY("IUAirInletTemperature");                                                      CAT_TEMP; HATEMP;                            VALUE_s8;
          case 15: KEY("IUAirOutletTemperature");                                                     CAT_TEMP; HATEMP;                            VALUE_s8;
          case 20: KEY("OUHeatExchangerTemperatureOutput");                                           CAT_TEMP; HATEMP;                            VALUE_s8;
          case 21: KEY("IUGasPipeTemperature");                                                       CAT_TEMP; HATEMP;                            VALUE_s8;
          case 22: KEY("IULiquidPipeTemperature");                                                    CAT_TEMP; HATEMP;                            VALUE_s8;
          case 23: KEY("OutdoorAirTemperature");                                                      CAT_TEMP; HATEMP;                            VALUE_s8;
          case 24: KEY("TempQ");                                                                      CAT_TEMP; HATEMP;                            VALUE_s8;
          case 25: KEY("CompressorTemperature");                                                      CAT_TEMP; HATEMP;                            VALUE_s8;
          case 26: KEY("TemperatureEvaporator");                                                      CAT_TEMP; HATEMP;                            VALUE_s8;
          case 29: KEY("TemperatureSetting");                                                         CAT_SETTING; HATEMP;                         VALUE_s8;
          case 37: return 0; // do not report checksum
          default  : UNKNOWN_BYTE;
        }
        default: switch (payloadIndex) {
          case 37: return 0; // do not report checksum
          default  : UNKNOWN_BYTE;
        }
      }
      // type 1 of message has length 0x2D, the most interesting (jetblack system)
      case 0x2D:
                                                                                                    CAT_MEASUREMENT;
                 switch (payloadIndex) {
        // case 0x06 : KEY("Unknown-892D--06");                                                                                                                                                                                                 VALUE_s8; // useless
        case 0x07: KEY("IUAirInletTemperature");                                                    CAT_TEMP; HATEMP;                            VALUE_s8;
        case 0x08: KEY("IUAirOutletTemperature");                                                   CAT_TEMP; HATEMP;                            VALUE_s8;
        case 0x09: KEY("IULiquidPipeTemperature");                                                  CAT_TEMP; HATEMP;                            VALUE_s8;
        case 0x0A: KEY("IURemoteSensorAirTemperature");                                             CAT_TEMP; HATEMP;                            VALUE_s8;
        case 0x0B: KEY("OutdoorAirTemperature");                                                    CAT_TEMP; HATEMP;                            VALUE_s8;
        case 0x0C: KEY("IUGasPipeTemperature");                                                     CAT_TEMP; HATEMP;                            VALUE_s8;
        case 0x0D: KEY("OUHeatExchangerTemperature1");                                              CAT_TEMP; HATEMP;                            VALUE_s8;
        case 0x0E: KEY("OUHeatExchangerTemperature2");                                              CAT_TEMP; HATEMP;                            VALUE_s8;
        case 0x0F: KEY("CompressorTemperature");                                                    CAT_TEMP; HATEMP;                            VALUE_s8;
        case 0x10: KEY("HighPressure");                                                             CAT_MEASUREMENT;                             VALUE_u8;
        case 0x11: KEY("LowPressure_x10");                                                          CAT_MEASUREMENT;                             VALUE_u8;
        case 0x12: KEY("TargetCompressorFrequency");                                                CAT_MEASUREMENT; HAFREQ;                     VALUE_u8;
        case 0x13: KEY("CompressorFrequency");                                                      CAT_MEASUREMENT; HAFREQ;                     VALUE_u8;
        case 0x14: KEY("IUExpansionValve");                                                         CAT_MEASUREMENT; HAPERCENT;                  VALUE_u8;
        case 0x15: KEY("OUExpansionValve");                                                         CAT_MEASUREMENT; HAPERCENT;                  VALUE_u8;
        // case 0x16 : KEY("Unknown-892D--16");                                                                                                  VALUE_u8; // useless
        // case 0x17 : KEY("Unknown-892D--17");                                                                                                  VALUE_u8; // useless
        case 0x18: KEY("CompressorCurrent");                                                        CAT_MEASUREMENT; HACURRENT;                  VALUE_u8; // ?
        // case 0x19 : KEY("Unknown-892D--19");                                                                                                  VALUE_u8; // useless
        // case 0x1A : KEY("Unknown-892D--1A");                                                                                                  VALUE_u8; // useless
        // case 0x1B : KEY("Unknown-892D--1B");                                                                                                  VALUE_u8; // useless
        // case 0x1C : KEY("Unknown-892D--1C");                                                                                                  VALUE_u8; // useless
        // case 0x1D : KEY("Unknown-892D--1D");                                                                                                  VALUE_u8; // useless
        // case 0x1E : KEY("Unknown-892D--1E");                                                                                                  VALUE_u8; // useless
        // case 0x1F : KEY("Unknown-892D--1F");                                                                                                  VALUE_u8; // useless
        // case 0x20 : KEY("Unknown-892D--20");                                                                                                  VALUE_u8; // useless
        case 0x21: switch (bitNr) {
          case 8: BITBASIS;
          case 0: KEY("892D-21-0");                                                                                                              VALUE_flag8;
          case 1: KEY("892D-21-1-OUnitOn");                                                                                                      VALUE_flag8;
          case 2: KEY("892D-21-2");                                                                                                              VALUE_flag8;
          case 3: KEY("892D-21-3-CompressorOn");                                                                                                 VALUE_flag8;
          case 4: KEY("892D-21-4");                                                                                                              VALUE_flag8;
          case 5: KEY("892D-21-5");                                                                                                              VALUE_flag8;
          case 6: KEY("892D-21-6");                                                                                                              VALUE_flag8;
          case 7: KEY("892D-21-7-OUStarting");                                                                                                   VALUE_flag8;
          default: UNKNOWN_BIT;
        }
        case 0x22: switch (bitNr) {
          case 8: BITBASIS;
          case 0: KEY("892D-22-0");                                                                                                              VALUE_flag8;
          case 1: KEY("892D-22-1");                                                                                                              VALUE_flag8;
          case 2: KEY("892D-22-2");                                                                                                              VALUE_flag8;
          case 3: KEY("892D-22-3");                                                                                                              VALUE_flag8;
          case 4: KEY("892D-22-4");                                                                                                              VALUE_flag8;
          case 5: KEY("892D-22-5");                                                                                                              VALUE_flag8;
          case 6: KEY("892D-22-6");                                                                                                              VALUE_flag8;
          case 7: KEY("892D-22-7");                                                                                                              VALUE_flag8;
          default: UNKNOWN_BIT;
        }
        // case 0x23 : KEY("Unknown-892D--23");                                                                                                  VALUE_u8; // useless
        // case 0x24 : KEY("Unknown-892D--24");                                                                                                  VALUE_u8; // useless
        // case 0x25 : KEY("Unknown-892D--25");                                                                                                  VALUE_u8; // useless
        // case 0x26 : KEY("Unknown-892D--26");                                                                                                  VALUE_u8; // useless
        // case 0x27 : KEY("Unknown-892D--27");                                                                                                  VALUE_u8; // useless
        // case 0x28 : KEY("Unknown-892D--28");                                                                                                  VALUE_u8; // useless
        case 0x29: return 0; // do not report checksum
        default  : UNKNOWN_BYTE;
      }
      case 0x27: // type 2 of message
                                                                                                    CAT_MEASUREMENT;
                 switch (payloadIndex) {
        // case 0x06 : KEY("Unknown-8927--06");                                                                                                  VALUE_u8; // useless
        case 0x07: // 8 bit byte : 65 HEAT, 64 HEAT STOP, 33 VENTIL, 67 DEFROST
                   switch (bitNr) {
          case 8: BITBASIS;
          case 0: KEY("8927-07-0-OUnitOn");                                                                                                      VALUE_flag8;
          case 1: KEY("8927-07-1-DEFROST");                                                                                                      VALUE_flag8;
          case 2: KEY("8927-07-2");                                                                                                              VALUE_flag8;
          case 3: KEY("8927-07-3");                                                                                                              VALUE_flag8;
          case 4: KEY("8927-07-4");                                                                                                              VALUE_flag8;
          case 5: KEY("8927-07-5-VentilMode");                                                                                                   VALUE_flag8;
          case 6: KEY("8927-07-6-HeatMode");                                                                                                     VALUE_flag8;
          case 7: KEY("8927-07-7");                                                                                                              VALUE_flag8;
          default: UNKNOWN_BIT;
        }
        case 0x08: switch (bitNr) {
          case 8: BITBASIS;
          case 0: KEY("8927-08-0");                                                                                                              VALUE_flag8;
          case 1: KEY("8927-08-1");                                                                                                              VALUE_flag8;
          case 2: KEY("8927-08-2");                                                                                                              VALUE_flag8;
          case 3: KEY("8927-08-3");                                                                                                              VALUE_flag8;
          case 4: KEY("8927-08-4");                                                                                                              VALUE_flag8;
          case 5: KEY("8927-08-5");                                                                                                              VALUE_flag8;
          case 6: KEY("8927-08-6");                                                                                                              VALUE_flag8;
          case 7: KEY("8927-08-7");                                                                                                              VALUE_flag8;
          default: UNKNOWN_BIT;
        }
        case 0x09: KEY("TemperatureSetpoint");                                                      CAT_TEMP; HATEMP;                            VALUE_u8;
        // case 0x0A: KEY("8927--0A-clock");                                                                                                     VALUE_u8; // 8 bit byte : 129 / 193 each 30s
        // case 0x0B : KEY("Unknown-8927--0B");                                                                                                  VALUE_u8; // useless
        // case 0x0C : KEY("Unknown-8927--0C");                                                                                                  VALUE_u8; // useless
        // case 0x0D : KEY("Unknown-8927--0D");                                                                                                  VALUE_u8; // useless
        // case 0x0E : KEY("Unknown-8927--0E");                                                                                                  VALUE_u8; // useless
        // case 0x0F : KEY("Unknown-8927--0F");                                                                                                  VALUE_u8; // useless
        // case 0x10 : KEY("Unknown-8927--10");                                                                                                  VALUE_u8; // useless
        // case 0x11 : KEY("Unknown-8927--11");                                                                                                  VALUE_u8; // useless
        // case 0x12 : KEY("Unknown-8927--12");                                                                                                  VALUE_u8; // useless
        // case 0x13 : KEY("Unknown-8927--13");                                                                                                  VALUE_u8; // useless
        // case 0x14 : KEY("Unknown-8927--14");                                                                                                  VALUE_u8; // useless
        // case 0x15 : KEY("Unknown-8927--15");                                                                                                  VALUE_u8; // useless
        case 0x16: KEY("8927--16-unsure");                                                                                                       VALUE_u8; // ?
        // case 0x17 : KEY("Unknown-8927--17");                                                                                                  VALUE_u8; // useless
        // case 0x18 : KEY("Unknown-8927--18");                                                                                                  VALUE_u8; // useless
        // case 0x19 : KEY("Unknown-8927--19");                                                                                                  VALUE_u8; // useless
        // case 0x1A : KEY("Unknown-8927--1A");                                                                                                  VALUE_u8; // useless
        // case 0x1B : KEY("Unknown-8927--1B");                                                                                                  VALUE_u8; // useless
        case 0x1C: switch (bitNr) {
          case 8: BITBASIS;
          case 0: KEY("8927-1C-0");                                                                                                              VALUE_flag8;
          case 1: KEY("8927-1C-1-PREHEAT");                                                                                                      VALUE_flag8;
          case 2: KEY("8927-1C-2");                                                                                                              VALUE_flag8;
          case 3: KEY("8927-1C-3");                                                                                                              VALUE_flag8;
          case 4: KEY("8927-1C-4");                                                                                                              VALUE_flag8;
          case 5: KEY("8927-1C-5");                                                                                                              VALUE_flag8;
          case 6: KEY("8927-1C-6");                                                                                                              VALUE_flag8;
          case 7: KEY("8927-1C-7");                                                                                                              VALUE_flag8;
          default: UNKNOWN_BIT;
        }
        case 0x23: return 0; // do not report checksum
        default  : UNKNOWN_BYTE;
      }
      default  : UNKNOWN_BYTE; // TODO was return 0; // unknown packet type
    }
    // 0x21 : sender is certainly the indoor unit
    case 0x21: switch (packetType) {
      case 0x29: switch (payload[4])  { // payload[4] can be 0xF1..0xF5, currently do not decode
        default: switch (payloadIndex) {
          case 37: return 0; // do not report checksum
          default  : UNKNOWN_BYTE;
        }
      }
      // type 1 of message has length 0x12
      case 0x12:                                                                                    CAT_SETTING;
                 switch (payloadIndex) {
        // case 0x06 : KEY("Unknown-2112--06");
        case 0x07: // AC MODE 195 HEAT, 192 HEAT STOP, 160 VENTIL STOP, 163 VENTIL
                   switch (bitNr) {
          case 8: BITBASIS;
          case 0: KEY("ACMode0UnitOn");                                                                                                          VALUE_flag8;
          case 1: KEY("ACMode1UnitOn");                                                                                                          VALUE_flag8;
          case 2: KEY("ACMode2");                                                                                                                VALUE_flag8;
          case 3: KEY("ACMode3");                                                                                                                VALUE_flag8;
          case 4: KEY("ACMode4");                                                                                                                VALUE_flag8;
          case 5: KEY("ACMode5Ventil");                                                                                                          VALUE_flag8;
          case 6: KEY("ACMode6Heat");                                                                                                            VALUE_flag8;
          case 7: KEY("ACMode7");                                                                                                                VALUE_flag8;
          default: UNKNOWN_BIT;
        }
        case 0x08: // VENTILATION 8 bit byte : 8 LOW, 4 MED, 2 HIGH
                   switch (bitNr) {
          case 8: BITBASIS;
          case 1: KEY("VentilHighOn");                                                                                                           VALUE_flag8;
          case 2: KEY("VentilMedOn");                                                                                                            VALUE_flag8;
          case 3: KEY("VentilLowOn");                                                                                                            VALUE_flag8;
          default: UNKNOWN_BIT;
        }
        case 0x09: KEY("TemperatureSetpoint");                                                      CAT_TEMP;                                    VALUE_u8; // HATEMP;
        // case 0x0A : KEY("Unknown-2112--0A");                                                                                                  VALUE_u8; // useless
        case 0x0B: KEY("Unknown-2112--0B");                                                                                                      VALUE_u8; // ?
        // case 0x0C : KEY("Unknown-2112--0C");                                                                                                  VALUE_u8; // useless
        // case 0x0D : KEY("Unknown-2112--0D");                                                                                                  VALUE_u8; // useless
        case 0x0E: return 0; // do not report checksum
        default  : UNKNOWN_BYTE;
      }
      /*
      // type 2 of message hs length 0x0B, no information found in these messages
      case 0x0B :                                                                                   CAT_SETTING;
                  switch (payloadIndex) {
        case 0x06 : KEY("Unknown-210B--06");                                                                                                     VALUE_u8; // ?
        case 0x06 : KEY("Unknown-210B--06");                                                                                                     VALUE_u8; // ?
        default   : UNKNOWN_BYTE;
      }
      */
      default  : UNKNOWN_BYTE; // return 0; // unknown packet type
    }
    // 0x41 : sender is Hitachi remote control or Airzone
    case 0x41: switch (packetType) {
      // type 1 of message has length 0x18
      case 0x18:                                                                                    CAT_SETTING;
                 switch (payloadIndex) {
        // ********************
        // ***** Here we process all data as they are useful to send a remote command
        // ********************
        case 0x00: KEY("Unknown-AZ00");                                                                                                          VALUE_u8; // useless
        case 0x01: KEY("Unknown-AZ01");                                                                                                          VALUE_u8; // useless
        case 0x02: KEY("Unknown-AZ02");                                                                                                          VALUE_u8; // useless
        case 0x03: KEY("Unknown-AZ03");                                                                                                          VALUE_u8; // useless
        case 0x04: KEY("Unknown-AZ04");                                                                                                          VALUE_u8; // useless
        case 0x05: KEY("Unknown-AZ05");                                                                                                          VALUE_u8; // useless
        case 0x06: KEY("Unknown-AZ06");                                                                                                          VALUE_u8; // useless
        case 0x07: switch (bitNr) {
          case 8: BITBASIS;
          case 0: KEY("SetACMode0UnitOn");                                                                                                       VALUE_flag8;
          case 1: KEY("SetACMode1UnitOn");                                                                                                       VALUE_flag8;
          case 2: KEY("SetACMode2");                                                                                                             VALUE_flag8;
          case 3: KEY("SetACMode3");                                                                                                             VALUE_flag8;
          case 4: KEY("SetACMode4");                                                                                                             VALUE_flag8;
          case 5: KEY("SetACMode5Ventil");                                                                                                       VALUE_flag8;
          case 6: KEY("SetACMode6Heat");                                                                                                         VALUE_flag8;
          case 7: KEY("SetACMode7");                                                                                                             VALUE_flag8;
          default: UNKNOWN_BIT;
        }
        case 0x08: switch (bitNr) {
          case 8: BITBASIS;
          case 1: KEY("SetVentilHighOn");                                                                                                        VALUE_flag8;
          case 2: KEY("SetVentilMedOn");                                                                                                         VALUE_flag8;
          case 3: KEY("SetVentilLowOn");                                                                                                         VALUE_flag8;
          default: UNKNOWN_BIT;
        }
        case 0x09: KEY("SetTemperatureSetpoint");                                                   CAT_TEMP; HATEMP;                            VALUE_u8;
        case 0x0A: KEY("Unknown-AZ0A");                                                                                                          VALUE_u8; // useless
        case 0x0B: KEY("Unknown-AZ0B");                                                                                                          VALUE_u8; // useless
        case 0x0C: KEY("Unknown-AZ0C");                                                                                                          VALUE_u8; // useless
        case 0x0D: KEY("Unknown-AZ0D");                                                                                                          VALUE_u8; // useless
        case 0x0E: KEY("Unknown-AZ0E");                                                                                                          VALUE_u8; // useless
        case 0x0F: KEY("Unknown-AZ0F");                                                                                                          VALUE_u8; // useless
        case 0x10: KEY("Unknown-AZ10");                                                                                                          VALUE_u8; // useless
        case 0x11: KEY("Unknown-AZ11");                                                                                                          VALUE_u8; // useless
        case 0x12: KEY("Unknown-AZ12");                                                                                                          VALUE_u8; // useless
        case 0x13: KEY("Unknown-AZ13");                                                                                                          VALUE_u8; // useless
        case 0x14: return 0; // do not report checksum
        default: UNKNOWN_BYTE;
      }
      case 0x29: switch (payload[4])  { // payload[4] can be 0xE1..0xE5, seen E1 only, currently decode only 0xE2
        case 0xE1: switch (payloadIndex) {
          case 0x25: return 0; // do not report checksum
          default : UNKNOWN_BYTE;
        }
        default  : return 0;
      }
      default: UNKNOWN_BYTE; // TODO was return 0; // unknown packet type
    }
    // 0x8A : sender is ?
    case 0x8A: switch (packetType) {
      case 0x29: switch (payload[4])  { // payload[4] can be 0xF1..0xF5, seen F1 only, currently decode only 0xE2
        case 0xF1: switch (payloadIndex) {
          case 0x25: return 0; // do not report checksum
          default : UNKNOWN_BYTE;
        }
        default  : return 0;
      }
      default: UNKNOWN_BYTE; // TODO was return 0; // unknown packet type
    }
    default: break; // do nothing
  }
  // restart switch as pseudotypes 00000B 40000B coincide with H-link 21000B ; Src/Type reversed
  switch (packetType) {
  // PSEUDO_PACKETS
#include "P1P2_pseudo.h"
    default: UNKNOWN_BYTE; // break; // do nothing
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
#endif /* P1P2_Daikin_ParameterConversion_HLINK */
