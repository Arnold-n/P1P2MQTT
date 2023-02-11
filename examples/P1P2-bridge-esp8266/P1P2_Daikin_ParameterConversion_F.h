/* P1P2_Daikin_ParameterConversion_F.h
 *
 * First attempt for product-dependent code for
 * - FDYQ180MV1
 * - FDY125LV1
 * and perhaps usefule other F-series models
 *
 * Copyright (c) 2019-2022 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * WARNING: P1P2-bridge-esp8266 is end-of-life, and will be replaced by P1P2MQTT
 *
 * Version history
 * 20230211 v0.9.33 0xA3 thermistor read-out F-series
 * 20230108 v0.9.31 sensor prefix, use 4th IPv4 byte for HA MQTT discovery, fix bit history for 0x30/0x31; added pseudo controlLevel
 * 20221108 v0.9.25 ADC support
 * 20220918 v0.9.22 degree symbol, hwID, 32-bit outputMode
 * 20220904 v0.9.20 new file for F-series VRV reporting in MQTT for 2 models
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

#ifndef P1P2_Daikin_ParameterConversion_F
#define P1P2_Daikin_ParameterConversion_F

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

char TimeString1[20] = "Mo 2000-00-00 00:00";    // reads time from packet type 0x12
char TimeString2[23] = "Mo 2000-00-00 00:00:00"; // reads time from packet type 0x31 (+weekday from packet 12)
byte SelectTimeString2 = 0; // to check whether TimeString2 was recently seen
int TimeStringCnt = 0;  // reset when TimeString is changed, for use in P1P2TimeStamp
byte maxOutputFilter = 0;

#ifdef SAVEPACKETS
#define PCKTP_START  0x08 // 0x08-0x11, 0x18 mapped to 0x12, 0x20 mapped to 0x13, 0x30-3F mapped to 0x14-0x23, 0x80 mapped to 0x24 // 0x37 zone name/0xC1 service mode subtype would require special coding, leave it out for now
#define PCKTP_ARR_SZ 29
//byte packetsrc                                      = { {00                                                                                               }, {40                                                                                               }}
//byte packettype                                     = { {08,  09,  0A,  0B,  0C,  0D,  0E,  0F,  10,  11,  18,  20,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  3A,  3B,  3C,  3D,  3E,  3F,  80 }, { 08,  09,  0A,  0B,  0C,  0D,  0E,  0F,  10,  11,  18,  20,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  3A,  3B,  3C,  3D,  3E,  3F, 80 }}
const PROGMEM uint32_t nr_bytes[2] [PCKTP_ARR_SZ]     = { { 0,  20,  20,  20,   0,  20,  20,  20,  20,  10,   7,   1,  20,   0,   0,   0,   0,   0,   0,   0,  20,  12,  11,  20,  12,   0,   0,   0,  10 }, {  0,  20,  20,  20,   0,  20,  20,  20,  20,  12,   0,  20,   0,   0,   0,   0,   0,   0,   0,   0,  20,   4,   7,  19,   2,   0,   0,   0, 10 }};
const PROGMEM uint32_t bytestart[2][PCKTP_ARR_SZ]     = { { 0,   0,  20,  40,  60,  60,  80, 100, 120, 140, 150, 157, 158, 178, 178, 178, 178, 178, 178, 178, 178, 198, 210, 221, 241, 253, 253, 253, 253 }, {263, 263, 283, 303, 323, 323, 343, 363, 383, 403, 415, 415, 435, 435, 435, 435, 435, 435, 435, 435, 435, 455, 459, 466, 485, 487, 487, 487, 497 /*, sizeValSeen = 497 */ }};
#define sizeValSeen 497
byte payloadByteVal[sizeValSeen]  = { 0 };
byte payloadByteSeen[sizeValSeen] = { 0 };
#endif /* SAVEPACKETS */

#define EMPTY_PAYLOAD 0xFF

char ha_mqttKey[HA_KEY_LEN];
char ha_mqttValue[HA_VALUE_LEN];
static byte uom = 0;
static byte stateclass = 0;

bool newPayloadBytesVal(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, byte haConfig, byte length, bool saveSeen) {
// returns true if a packet parameter is observed for the first time ((and publishes it for homeassistant if haConfig==true)
// if (outputFilter <= maxOutputFilter), it detects if a parameter has changed, and returns true if changed (or if outputFilter==0)
// Also, if saveSeen==true, the new value is saved
//
#ifdef SAVEPACKETS
  bool newByte = (outputFilter == 0);
  byte pts = (packetSrc >> 6) & 0x01;  // 800018 -> 000018
  byte pti = 0xFF;
// ugly hard-coding, code is end-of-life, 0x0D-0x11, 0x18 mapped to 0x12, 0x20 mapped to 0x13, 0x30 mapped to 0x14, 0x30-3F mapped to 0x15-0x24, 0x80 mapped to 0x25 // 0x37 zone name/0xC1 service mode subtype would require special coding, leave it out for now
  switch (packetType) {
    case 0x08 ... 0x11 : pti = packetType - PCKTP_START; break;        // 0 .. 9
    case          0x18 : pti = 0x12 - PCKTP_START; break;              // 10
    case          0x20 : pti = 0x13 - PCKTP_START; break;              // 11
    case 0x30 ... 0x3F : pti = packetType - 36; break;                 // 12 .. 27
    case          0x80 : pti = 28; break;                              // 28
    default            : /* pti = 0xFF;*/ newByte = 1; break;          // or newByte = 0;?
  }
  if (payloadIndex == EMPTY_PAYLOAD) {
    newByte = 0;
  } else if (pti != 0xFF) {
    if (payloadIndex > nr_bytes[pts][pti]) {
      // Warning: payloadIndex > expected
      Sprint_P(true, true, true, PSTR(" Warning: payloadIndex %i > expected %i for Src 0x%02X Type 0x%02X"), payloadIndex, nr_bytes[pts][pti], packetSrc, packetType);
      newByte = 1;
    } else if (payloadIndex + 1 < length) {
      // Warning: payloadIndex + 1 < length
      Sprint_P(true, true, true, PSTR(" Warning: payloadIndex + 1 < length"));
      return 0;
    } else {
      bool pubHA = false;
      for (byte i = payloadIndex + 1 - length; i <= payloadIndex; i++) {
        uint16_t pi2 = bytestart[pts][pti] + i;
        if (pi2 >= sizeValSeen) {
          pi2 = 0;
          Sprint_P(true, true, true, PSTR("Warning: pi2 > sizeValSeen"));
          return 0;
        }
        if (payloadByteSeen[pi2]) {
          // this byte or at least some bits have been seen and saved before.
          if (payloadByteVal[pi2] != payload[i]) {
            newByte = (outputFilter <= maxOutputFilter);
            if (saveSeen) payloadByteVal[pi2] = payload[i];
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
        client_publish_mqtt(ha_mqttKey, ha_mqttValue);
      }
    }
  }
  return (haConfig || (outputMode & 0x10000) || !saveSeen) && newByte;
#else /* SAVEPACKETS */
  return (haConfig || (outputMode & 0x10000) || !saveSeen);
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

uint16_t FN_u16_LE(uint8_t *b)          { return (                (((uint16_t) b[-1]) << 8) | (b[0]));}
uint32_t FN_u24_LE(uint8_t *b)          { return (((uint32_t) b[-2] << 16) | (((uint32_t) b[-1]) << 8) | (b[0]));}
uint32_t FN_u32_LE(uint8_t *b)          { return (((uint32_t) b[-3] << 24) | ((uint32_t) b[-2] << 16) | (((uint32_t) b[-1]) << 8) | (b[0]));}

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
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 2, 1)) return 0;
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

float FN_f8_8(uint8_t *b)            { return                    (((int8_t) b[-1]) + (b[0] * 1.0 / 256)); }
float FN_s_f8_8(uint8_t *b)          { return (b[-2] ? -1 : 1) * (((int8_t) b[-1]) + (b[0] * 1.0 / 256)); }

uint8_t value_f8_8(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 2, 1)) return 0;
#ifdef __AVR__
  dtostrf(FN_f8_8(&payload[payloadIndex]), 1, 2, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.3f", FN_f8_8(&payload[payloadIndex]));
#endif
  return 1;
}

uint8_t value_s_f8_8(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 3, 1)) return 0;
#ifdef __AVR__
  dtostrf(FN_s_f8_8(&payload[payloadIndex]), 1, 2, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.3f", FN_s_f8_8(&payload[payloadIndex]));
#endif
  return 1;
}

float FN_f8s8(uint8_t *b)            { return ((float)(int8_t) b[-1]) + ((float) b[0]) /  10;}

uint8_t value_f8s8(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, 2, 1)) return 0;
#ifdef __AVR__
  dtostrf(FN_f8s8(&payload[payloadIndex]), 1, 1, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.3f", FN_f8s8(&payload[payloadIndex]));
#endif
  return 1;
}

// change to Watt ?
uint8_t value_f(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, float v, int length = 0) {
  if (length && (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, length, 1))) return 0;
#ifdef __AVR__
  dtostrf(v, 1, 3, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.3f", v);
#endif
  return (outputFilter > maxOutputFilter ? 0 : 1);
}

uint8_t value_s(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, int v, int length = 0) {
  if (length && (!newPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, mqtt_key, haConfig, length, 1))) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", v);
  return (outputFilter > maxOutputFilter ? 0 : 1);
}

uint8_t value_timeString(char* mqtt_value, char* timestring) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"%s\"", timestring);
  return (outputFilter > maxOutputFilter ? 0 : 1);
}

// return 0:            use this for all bytes (except for the last byte) which are part of a multi-byte parameter
// UNKNOWN_BYTE:        byte function is unknown, mqtt_key will be: "PacketSrc-0xXX-Type-0xXX-Byte-I-Bit-I", value will be hex
// VALUE_u8:            1-byte unsigned integer value at current location payload[i]
// VALUE_s8:            1-byte signed integer value at current location payload[i]
// VALUE_u16:           3-byte unsigned integer value at location payload[i - 1]..payload[i]
// VALUE_u24:           3-byte unsigned integer value at location payload[i-2]..payload[i]
// VALUE_u32:           4-byte unsigned integer value at location payload[i-3]..payload[i]
// VALUE_u8_add2k:      for 1-byte value (2000+payload[i]) (for year value)
// VALUE_s4abs1c:        for 1-byte value -10..10 where bit 4 is sign and bit 0..3 is value
// VALUE_f8_8           for 2-byte float in f8_8 format (see protocol description)
// VALUE_s_f8_8         for 2-byte float in f8_8 format (see protocol description) with leading sign (0x40=negative)
// VALUE_f8s8           for 2-byte float in f8s8 format (see protocol description)
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

#define VALUE_u8_add2k          { return    value_u8_add2k(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_s4abs1c             { return      value_s4abs1c(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }

#define VALUE_u16div10_LE       { return    value_u16div10_LE(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u16div10_LE_changed(ch)  { ch = value_u16div10_LE(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); return ch; }

#define VALUE_f8_8              { return        value_f8_8(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_s_f8_8              { return      value_s_f8_8(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_f8_8_changed(ch)  { ch = value_f8_8(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); return ch; }

#define VALUE_f8s8              { return        value_f8s8(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_F(v)              { return           value_f(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, v); }
#define VALUE_F_L(v, l)         { return           value_f(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, v, l); }
#define VALUE_S_L(v, l)         { return           value_s(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, v, l); }
#define VALUE_timeString(s)     { return  value_timeString(mqtt_value, s); }

#define UNKNOWN_BYTE            { CAT_UNKNOWN; return unknownByte(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_header            { return            value_trg(packetSrc, packetType, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }

#define TERMINATEJSON           { return 9; }

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

uint16_t parameterWritesDone = 0; // # writes done by ATmega; perhaps useful for ESP-side queueing

byte bytesbits2keyvalue(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, /*char &cat, char &src,*/ byte bitNr) {
// payloadIndex: new payload documentation counts payload bytes starting at 0 (following Budulinek's suggestion)
// payloadIndex == EMPTY_PAYLOAD : empty payload (used during restart)

  byte haConfig = 0;

  maxOutputFilter = 9; // default all changes visible, unless changed below
  CAT_UNKNOWN; // cat unknown unless changed below

  static byte C1_subtype = 0;

  byte payloadByte = 0;
  byte* payloadPointer = 0;
  if (payloadIndex != EMPTY_PAYLOAD) {
    payloadByte = (payloadIndex == EMPTY_PAYLOAD ? 0 : payload[payloadIndex]);
    payloadPointer = (payloadIndex == EMPTY_PAYLOAD ? 0 : &payload[payloadIndex]);
  };

  byte PS = packetSrc ? ((packetSrc == 0x80) ? 6 : 1) : 0;
  byte src = PS;
  if ((packetType & 0xF0) == 0x30) src += 2;  // 2, 3 from/to auxiliary controller (use 4, 5 for 2nd auxiliary controller?)
  if ((packetType & 0xF8) == 0x00) src = 7; // boot
  if ((packetType & 0xF8) == 0x08) src = PS + 8; // pseudo-packets ESP / ATmega
  SRC(src); // set char in mqtt_key prefix

  switch (packetType) {
    // Restart packet types 00-05 (restart procedure 00-05 followed by 20, 60-9F, A1, B1, B8, 21)
    // Thermistor package
    // 0DAC = 13 // other 085F
    // 145A = 20 // other 2A98
    case 0xA3 :
                switch (packetSrc) {
      case 0x00 : return 0;
      case 0x40 : switch (payloadIndex) {
        case    0 :
        case    3 :
        case    6 :
        case    9 :
        case   12 :
        case   15 :
        case    1 :
        case    4 :
        case    7 :
        case   10 :
        case   13 :
        case   16 : return 0;
        case    2 : if (payload[payloadIndex - 2] & 0x80) return 0;
                    KEY("Th1");                                            HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_s_f8_8;
        case    5 : if (payload[payloadIndex - 2] & 0x80) return 0;
                    KEY("Th2");                                            HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_s_f8_8;
        case    8 : if (payload[payloadIndex - 2] & 0x80) return 0;
                    KEY("Th3");                                            HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_s_f8_8;
        case   11 : if (payload[payloadIndex - 2] & 0x80) return 0;
                    KEY("Th4");                                            HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_s_f8_8;
        case   14 : if (payload[payloadIndex - 2] & 0x80) return 0;
                    KEY("Th5");                                            HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_s_f8_8;
        case   17 : if (payload[payloadIndex - 2] & 0x80) return 0;
                    KEY("Th6");                                            HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_s_f8_8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    // Main package communication packet types 10-16
    case 0x10 :
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : KEY("Power_Off_On");                                   HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    1 : KEY("Target_Operating_Mode");                          HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    2 : KEY("Actual_Operating_Mode");                          HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    3 : KEY("Target_Temperature");                             HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_u8;
        case    4 : KEY("Unknown_000010_4");                               HACONFIG;                                                             VALUE_u8;
        case    5 : KEY("Fan_Speed");                                      HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    6 : KEY("Unknown_000010_6");                               HACONFIG;                                                             VALUE_u8;
        case    7 : KEY("Unknown_000010_7");                               HACONFIG;                                                             VALUE_u8;
        case    8 : KEY("Unknown_000010_8");                               HACONFIG;                                                             VALUE_u8;
        case    9 : KEY("Unknown_000010_9");                               HACONFIG;                                                             VALUE_u8;
        case   10 : KEY("Unknown_000010_10");                              HACONFIG;                                                             VALUE_u8;
        case   11 : KEY("Unknown_000010_11");                              HACONFIG;                                                             VALUE_u8;
        case   12 : KEY("Unknown_000010_12");                              HACONFIG;                                                             VALUE_u8;
        case   13 : KEY("System_status");                                  HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   14 : KEY("Unknown_000010_14");                              HACONFIG;                                                             VALUE_u8;
        case   15 : KEY("Unknown_000010_15");                              HACONFIG;                                                             VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : KEY("Power_Off_On");                                   HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    1 : KEY("Unknown_000010_1");                               HACONFIG;                                                             VALUE_u8;
        case    2 : KEY("Target_Operating_Mode");                          HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    3 : KEY("Actual_Operating_Mode");                          HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    4 : KEY("Target_Temperature");                             HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_u8;
        case    5 : KEY("Unknown_000010_5");                               HACONFIG;                                                             VALUE_u8;
        case    6 : KEY("Fan_Speed");                                      HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    7 : KEY("Unknown_000010_7");                               HACONFIG;                                                             VALUE_u8;
        case    8 : KEY("Unknown_000010_8");                               HACONFIG;                                                             VALUE_u8;
        case    9 : KEY("Unknown_000010_9");                               HACONFIG;                                                             VALUE_u8;
        case   10 : KEY("Unknown_000010_10");                              HACONFIG;                                                             VALUE_u8;
        case   11 : KEY("Unknown_000010_11");                              HACONFIG;                                                             VALUE_u8;
        case   12 : KEY("Unknown_000010_12");                              HACONFIG;                                                             VALUE_u8;
        case   13 : KEY("Unknown_000010_13");                              HACONFIG;                                                             VALUE_u8;
        case   14 : KEY("System_status");                                  HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   15 : KEY("Unknown_000010_15");                              HACONFIG;                                                             VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x11 :
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : KEY("Unknown_000011_0");                               HACONFIG;                                                             VALUE_u8;
        case    1 : KEY("Unknown_000011_1");                               HACONFIG;                                                             VALUE_u8;
        case    2 : return 0;
        case    3 : KEY("Filter_related");                                 HACONFIG; CAT_SETTING;                                                VALUE_u16_LE;
        case    4 : KEY("Unknown_000011_4");                               HACONFIG;                                                             VALUE_u8;
        case    5 : return 0;
        case    6 : KEY("Temperature_main_controller");                    HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_f8_8;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : KEY("Unknown_400011_0");                               HACONFIG;                                                             VALUE_u8;
        case    1 : KEY("Unknown_400011_1");                               HACONFIG;                                                             VALUE_u8;
        case    2 : KEY("Unknown_400011_2");                               HACONFIG;                                                             VALUE_u8;
        case    3 : KEY("Unknown_400011_3");                               HACONFIG;                                                             VALUE_u8;
        case    4 : KEY("Unknown_400011_4");                               HACONFIG;                                                             VALUE_u8;
        case    5 : return 0;
        case    6 : KEY("Temperature_inside_air_intake");                  HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_f8_8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x18 :
                switch (packetSrc) {
      case 0x80 : switch (payloadIndex) {
        case    0 : KEY("Heatpump_on");                                    HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    1 : KEY("Unknown_800018_1");                               HACONFIG;                                                             VALUE_u8;
        case    2 : KEY("Unknown_800018_2");                               HACONFIG;                                                             VALUE_u8;
        case    3 : KEY("Unknown_800018_3");                               HACONFIG;                                                             VALUE_u8;
        case    4 : KEY("Unknown_800018_4");                               HACONFIG;                                                             VALUE_u8;
        case    5 : KEY("Unknown_800018_5");                               HACONFIG;                                                             VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x20 :
                switch (packetSrc) {
      case 0x40 : switch (payloadIndex) {
        case    0 : KEY("Unknown_400020_0");                               HACONFIG;                                                             VALUE_u8;
        case    1 : KEY("Unknown_400020_1");                               HACONFIG;                                                             VALUE_u8;
        case    2 : KEY("Unknown_400020_2");                               HACONFIG;                                                             VALUE_u8;
        case    3 : KEY("Unknown_400020_3");                               HACONFIG;                                                             VALUE_u8;
        case    4 : KEY("Unknown_400020_4");                               HACONFIG;                                                             VALUE_u8;
        case    5 : KEY("Unknown_400020_5");                               HACONFIG;                                                             VALUE_u8;
        case    6 : KEY("Unknown_400020_6");                               HACONFIG;                                                             VALUE_u8;
        case    7 : KEY("Unknown_400020_7");                               HACONFIG;                                                             VALUE_u8;
        case    8 : KEY("Unknown_400020_8");                               HACONFIG;                                                             VALUE_u8;
        case    9 : KEY("Unknown_400020_9");                               HACONFIG;                                                             VALUE_u8;
        case   10 : KEY("Unknown_400020_10");                              HACONFIG;                                                             VALUE_u8;
        case   11 : KEY("Unknown_400020_11");                              HACONFIG;                                                             VALUE_u8;
        case   12 : KEY("Unknown_400020_12");                              HACONFIG;                                                             VALUE_u8;
        case   13 : KEY("Unknown_400020_13");                              HACONFIG;                                                             VALUE_u8;
        case   14 : KEY("Unknown_400020_14");                              HACONFIG;                                                             VALUE_u8;
        case   15 : KEY("Unknown_400020_15");                              HACONFIG;                                                             VALUE_u8;
        case   16 : KEY("Unknown_400020_16");                              HACONFIG;                                                             VALUE_u8;
        case   17 : KEY("Unknown_400020_17");                              HACONFIG;                                                             VALUE_u8;
        case   18 : KEY("Unknown_400020_18");                              HACONFIG;                                                             VALUE_u8;
        case   19 : KEY("Unknown_400020_19");                              HACONFIG;                                                             VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x30 :
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : KEY("Unknown_00F030_0");                               HACONFIG;                                                             VALUE_u8;
        case    1 : KEY("Unknown_00F030_1");                               HACONFIG;                                                             VALUE_u8;
        case    2 : KEY("Unknown_00F030_2");                               HACONFIG;                                                             VALUE_u8;
        case    3 : KEY("Unknown_00F030_3");                               HACONFIG;                                                             VALUE_u8;
        case    4 : KEY("Unknown_00F030_4");                               HACONFIG;                                                             VALUE_u8;
        case    5 : KEY("Unknown_00F030_5");                               HACONFIG;                                                             VALUE_u8;
        case    6 : KEY("Unknown_00F030_6");                               HACONFIG;                                                             VALUE_u8;
        case    7 : KEY("Unknown_00F030_7");                               HACONFIG;                                                             VALUE_u8;
        case    8 : KEY("Unknown_00F030_8");                               HACONFIG;                                                             VALUE_u8;
        case    9 : KEY("Unknown_00F030_9");                               HACONFIG;                                                             VALUE_u8;
        case   10 : KEY("Unknown_00F030_10");                              HACONFIG;                                                             VALUE_u8;
        case   11 : KEY("Unknown_00F030_11");                              HACONFIG;                                                             VALUE_u8;
        case   12 : KEY("Unknown_00F030_12");                              HACONFIG;                                                             VALUE_u8;
        case   13 : KEY("Unknown_00F030_13");                              HACONFIG;                                                             VALUE_u8;
        case   14 : KEY("Unknown_00F030_14");                              HACONFIG;                                                             VALUE_u8;
        case   15 : KEY("Unknown_00F030_15");                              HACONFIG;                                                             VALUE_u8;
        case   16 : KEY("Unknown_00F030_16");                              HACONFIG;                                                             VALUE_u8;
        case   17 : KEY("Unknown_00F030_17");                              HACONFIG;                                                             VALUE_u8;
        case   18 : KEY("Unknown_00F030_18");                              HACONFIG;                                                             VALUE_u8;
        case   19 : KEY("Unknown_00F030_19");                              HACONFIG;                                                             VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
/*
 *  case 0x37 : // FDYQ180MV1 zone names TBD
 */
   case 0x38 :                                                              // FDY125LV1 operation mode, added bytes 16-19 for FXMQ and FTQ
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : KEY("Power_Off_On");                                   HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    1 : KEY("Unknown_00F038_1");                               HACONFIG;                                                             VALUE_u8;
        case    2 : KEY("Target_Operating_Mode");                          HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    3 : KEY("Actual_Operating_Mode");                          HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    4 : KEY("Target_Temperature_Cooling");                     HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_u8;
        case    5 : KEY("Error_Setting_1_Q");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    6 : KEY("Fan_Speed_Cooling");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    7 : KEY("Unknown_00F038_7");                               HACONFIG;                                                             VALUE_u8;
        case    8 : KEY("Target_Temperature_Heating");                     HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_u8;
        case    9 : KEY("Error_Setting_2_Q");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   10 : KEY("Fan_Speed_Heating");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   11 : KEY("Unknown_00F038_11");                              HACONFIG;                                                             VALUE_u8;
        case   12 : KEY("Unknown_00F038_12");                              HACONFIG;                                                             VALUE_u8;
        case   13 : KEY("Unknown_00F038_13");                              HACONFIG;                                                             VALUE_u8;
        case   14 : KEY("Clear_Error_Code");                               HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   15 : KEY("Unknown_00F038_15");                              HACONFIG;                                                             VALUE_u8;
        case   16 : KEY("Unknown_00F038_16");                              HACONFIG;                                                             VALUE_u8;
        case   17 : KEY("Unknown_00F038_17");                              HACONFIG;                                                             VALUE_u8;
        case   18 : KEY("Unknown_00F038_18");                              HACONFIG;                                                             VALUE_u8;
        case   19 : KEY("Unknown_00F038_19");                              HACONFIG;                                                             VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : KEY("Power_Off_On");                                   HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    1 : KEY("Target_Operating_Mode");                          HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    2 : KEY("Target_Temperature_Cooling");                     HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_u8;
        case    3 : KEY("Error_Setting_1_Q");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    4 : KEY("Fan_Speed_Cooling");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    5 : KEY("Unknown_40F038_5");                               HACONFIG;                                                             VALUE_u8;
        case    6 : KEY("Target_Temperature_Heating");                     HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_u8;
        case    7 : KEY("Error_Setting_2_Q");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    8 : KEY("Fan_Speed_Heating");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    9 : KEY("Unknown_40F038_9");                               HACONFIG;                                                             VALUE_u8;
        case   10 : KEY("Unknown_40F038_10");                              HACONFIG;                                                             VALUE_u8;
        case   11 : KEY("Clear_Error_Code");                               HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   12 : KEY("Clear_Error_Code");                               HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   13 : KEY("Unknown_40F038_13");                              HACONFIG;                                                             VALUE_u8;
        case   14 : KEY("Fan_Mode_Q");                                     HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   15 : KEY("Unknown_40F038_15");                              HACONFIG;                                                             VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x39 :                                                            // FDY125LV1 filter?
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : KEY("Unknown_00F039_0");                               HACONFIG;                                                             VALUE_u8;
        case    1 : KEY("Unknown_00F039_1");                               HACONFIG;                                                             VALUE_u8;
        case    2 : KEY("Unknown_00F039_2");                               HACONFIG;                                                             VALUE_u8;
        case    3 : KEY("Unknown_00F039_3");                               HACONFIG;                                                             VALUE_u8;
        case    4 : KEY("Unknown_00F039_4");                               HACONFIG;                                                             VALUE_u8;
        case    5 : KEY("Unknown_00F039_5");                               HACONFIG;                                                             VALUE_u8;
        case    6 : KEY("Unknown_00F039_6");                               HACONFIG;                                                             VALUE_u8;
        case    7 : KEY("Unknown_00F039_7");                               HACONFIG;                                                             VALUE_u8;
        case    8 : return 0;
        case    9 : KEY("Filter_related");                                 HACONFIG; CAT_SETTING;                                                VALUE_u16hex_LE;
        case   10 : KEY("Unknown_00F039_10");                              HACONFIG;                                                             VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : KEY("Filter_Alarm_reset_Q");                           HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    1 : KEY("Unknown_40F039_1");                               HACONFIG;                                                             VALUE_u8;
        case    2 : return 0;
        case    3 : KEY("Filter_related");                                 HACONFIG; CAT_SETTING;                                                VALUE_u16hex_LE;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x3B :                                                           // FDYQ180MV1 operation mode
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : KEY("Power_Off_On");                                   HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    1 : KEY("Unknown_00F03B_1");                               HACONFIG;                                                             VALUE_u8;
        case    2 : KEY("Target_Operating_Mode");                          HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    3 : KEY("Actual_Operating_Mode");                          HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    4 : KEY("Target_Temperature_Cooling");                     HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_u8;
        case    5 : KEY("Error_Setting_1_Q");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    6 : KEY("Fan_Speed_Cooling");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    7 : KEY("Unknown_00F03B_7");                               HACONFIG;                                                             VALUE_u8;
        case    8 : KEY("Target_Temperature_Heating");                     HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_u8;
        case    9 : KEY("Error_Setting_2_Q");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   10 : KEY("Fan_Speed_Heating");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   11 : KEY("Unknown_00F03B_11");                              HACONFIG;                                                             VALUE_u8;
        case   12 : KEY("Unknown_00F03B_12");                              HACONFIG;                                                             VALUE_u8;
        case   13 : KEY("Unknown_00F03B_13");                              HACONFIG;                                                             VALUE_u8;
        case   14 : KEY("Clear_Error_Code");                               HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   15 : KEY("Unknown_00F03B_15");                              HACONFIG;                                                             VALUE_u8;
        case   16 : KEY("Unknown_00F03B_16");                              HACONFIG;                                                             VALUE_u8;
        case   17 : KEY("Zone_status");                                    HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   18 : KEY("Fan_mode");                                       HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   19 : KEY("Unknown_00F03B_14");                              HACONFIG;                                                             VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : KEY("Power_Off_On");                                   HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    1 : KEY("Target_Operating_Mode");                          HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    2 : KEY("Target_Temperature_Cooling");                     HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_u8;
        case    3 : KEY("Error_Setting_1_Q");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    4 : KEY("Fan_Speed_Cooling");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    5 : KEY("Unknown_40F03B_5");                               HACONFIG;                                                             VALUE_u8;
        case    6 : KEY("Target_Temperature_Heating");                     HACONFIG; CAT_TEMP;    HATEMP;                                        VALUE_u8;
        case    7 : KEY("Error_Setting_2_Q");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    8 : KEY("Fan_Speed_Heating");                              HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    9 : KEY("Unknown_40F03B_9");                               HACONFIG;                                                             VALUE_u8;
        case   10 : KEY("Unknown_40F03B_10");                              HACONFIG;                                                             VALUE_u8;
        case   11 : KEY("Unknown_40F03B_11");                              HACONFIG;                                                             VALUE_u8;
        case   12 : KEY("Unknown_40F03B_12");                              HACONFIG;                                                             VALUE_u8;
        case   13 : KEY("Unknown_40F03B_13");                              HACONFIG;                                                             VALUE_u8;
        case   14 : KEY("Unknown_40F03B_14");                              HACONFIG;                                                             VALUE_u8;
        case   15 : KEY("Unknown_40F03B_15");                              HACONFIG;                                                             VALUE_u8;
        case   16 : KEY("Zone_status");                                    HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   17 : KEY("Fan_mode");                                       HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case   18 : KEY("Unknown_40F03B_18");                              HACONFIG;                                                             VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x3C :                                                            // FDYQ180MV1 filter
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : KEY("Unknown_00F03C_0");                               HACONFIG;                                                             VALUE_u8;
        case    1 : KEY("Unknown_00F03C_1");                               HACONFIG;                                                             VALUE_u8;
        case    2 : KEY("Unknown_00F03C_2");                               HACONFIG;                                                             VALUE_u8;
        case    3 : KEY("Unknown_00F03C_3");                               HACONFIG;                                                             VALUE_u8;
        case    4 : KEY("Unknown_00F03C_4");                               HACONFIG;                                                             VALUE_u8;
        case    5 : KEY("Unknown_00F03C_5");                               HACONFIG;                                                             VALUE_u8;
        case    6 : KEY("Unknown_00F03C_6");                               HACONFIG;                                                             VALUE_u8;
        case    7 : KEY("Unknown_00F03C_7");                               HACONFIG;                                                             VALUE_u8;
        case    8 : return 0;
        case    9 : KEY("Filter_related");                                 HACONFIG; CAT_SETTING;                                                VALUE_u16hex_LE;
        case   10 : KEY("Unknown_00F03C_10");                              HACONFIG;                                                             VALUE_u8;
        case   11 : KEY("Unknown_00F03C_11");                              HACONFIG;                                                             VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : KEY("Filter_Alarm_reset_Q");                           HACONFIG; CAT_SETTING;                                                VALUE_u8;
        case    1 : KEY("Unknown_40F03C_1");                               HACONFIG;                                                             VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0xC1 :                                                            // FDY125LV1 service mode
                switch (packetSrc) {
      case 0x40 : switch (payloadIndex) {
        case    0 : C1_subtype = payloadByte; break;
        case    1 : KEY("Unknown_0000C1_1");                               HACONFIG;                                                             VALUE_u8;
        case    2 : return 0;
        case    3 : switch (C1_subtype){
          case   1 : KEY("Temperature_Air_Intake");                        HACONFIG; CAT_TEMP; HATEMP;                                           VALUE_f8_8;
          case   2 : KEY("Temperature_Heat_Exchanger");                    HACONFIG; CAT_TEMP; HATEMP;                                           VALUE_f8_8;
          default  : UNKNOWN_BYTE;
        }
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
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
#endif /* P1P2_Daikin_ParameterConversion_F */
