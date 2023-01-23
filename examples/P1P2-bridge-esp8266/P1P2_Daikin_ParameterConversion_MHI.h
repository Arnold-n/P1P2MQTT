/* P1P2_Daikin_ParameterConversion_EHYHB.h product-dependent code for EHYHBX08AAV3 and perhaps other EHYHB models
 *
 * Copyright (c) 2019-2022 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * WARNING: P1P2-bridge-esp8266 is end-of-life, and will be replaced by P1P2MQTT
 *
 * Version history
 * 20230117 v0.9.30 H-link file (only for pseudo packets)
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

#ifndef P1P2_Daikin_ParameterConversion_H
#define P1P2_Daikin_ParameterConversion_H

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
#define KEY(K) { byte l = snprintf(mqtt_key, MQTT_KEY_LEN, "0x%02X_%i_", packetSrc, payloadIndex); strncpy_P(mqtt_key + l, PSTR(K), MQTT_KEY_LEN - l); mqtt_key[MQTT_KEY_LEN - 1] = '\0'; };
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
#define PCKTP_START  0x08
#define PCKTP_END    0x0F // assume first byte of MHI packet is source identifier: 00, 80, 0C, mapped to 10, 11, 12
#define PCKTP_ARR_SZ (PCKTP_END - PCKTP_START + 4)
//byte packettype                                  = {00,  0C,  80} // TODO map 00  01-7F or 00-7F?
const PROGMEM uint32_t nr_bytes [PCKTP_ARR_SZ]     = {40,  40,  40};
const PROGMEM uint32_t bytestart[PCKTP_ARR_SZ]     = { 0,  40,  80 /* , sizeValSeen=120 */ };
#define sizeValSeen 340
byte payloadByteVal[sizeValSeen]  = { 0 };
byte payloadByteSeen[sizeValSeen] = { 0 };
#endif /* SAVEPACKETS */

char ha_mqttKey[HA_KEY_LEN];
char ha_mqttValue[HA_VALUE_LEN];
static byte uom = 0;
static byte stateclass = 0;

bool newPayloadBytesVal(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, byte haConfig, byte length, bool saveSeen) {
// returns true if a packet parameter is observed for the first time ((and publishes it for homeassistant if haConfig==true)
// if (outputFilter <= maxOutputFilter), it detects if a parameter has changed, and returns true if changed (or if outputFilter==0)
// Also, if saveSeen==true, the new value is saved
// In BITBASIS calls, saveSeen should be false such that data can be handled again on bit-basis
//
#ifdef SAVEPACKETS
  bool newByte = (outputFilter == 0);
  byte pti;
  switch (packetSrc) {
    // MHI packets
    case 0x00 : pti = 0x00; break;
    case 0x01 ... 0x7F : pti = 0x01; break;
    case 0x80 : pti = 0x02; break;
    default : pti = 0xFF; break;
  }
  if (pti == 0xFF) {
    newByte = 1;
  } else if (payloadIndex > nr_bytes[pti]) {
    // Warning: payloadIndex > expected
    Sprint_P(true, true, true, PSTR(" Warning: payloadIndex %i > expected %i for Src 0x%02X"), payloadIndex, nr_bytes[pti], packetSrc);
    newByte = true;
  } else if (payloadIndex + 1 < length) {
    // Warning: payloadIndex + 1 < length
    Sprint_P(true, true, true, PSTR(" Warning: payloadIndex + 1 < length"));
    return 0;
  } else {
    bool pubHA = false;
    for (byte i = payloadIndex + 1 - length; i <= payloadIndex; i++) {
      uint16_t pi2 = bytestart[pti] + i;
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
  return (haConfig || (outputMode & 0x10000) || !saveSeen) && newByte;
#else /* SAVEPACKETS */
  return (haConfig || (outputMode & 0x10000) || !saveSeen);
#endif /* SAVEPACKETS */
}

bool newPayloadBitVal(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, byte haConfig, byte bitNr) {
// returns whether bit bitNr of byte payload[payloadIndex] has a new value (also returns true if byte has not been saved)
#ifdef SAVEPACKETS
  bool newBit = (outputFilter == 0);
  byte pti;
  switch (packetSrc) {
    // MHI packets
    case 0x00 : pti = 0x00; break;
    case 0x01 ... 0x7F : pti = 0x01; break;
    case 0x80 : pti = 0x02; break;
    default : pti = 0xFF; break;
  }
  if (bitNr > 7) {
    // Warning: bitNr > 7
    newBit = 1;
  } else if (payloadIndex > nr_bytes[pti]) {
    // Warning: payloadIndex > expected
    newBit = 1;
  } else {
    uint16_t pi2 = bytestart[pti] + payloadIndex; // no multiplication, bit-wise only for u8 type
    if (pi2 >= sizeValSeen) {
      pi2 = 0;
      Sprint_P(true, true, true, PSTR("Warning: pi2 > sizeValSeen"));
    }
    byte bitMask = 1 << bitNr;
    if (payloadByteSeen[pi2] & bitMask) {
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
        client_publish_mqtt(ha_mqttKey, ha_mqttValue);
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

uint8_t value_u8hex(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X\"", payload[payloadIndex]);
  return 1;
}

uint8_t value_u16hex_LE(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 2, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X\"", payload[payloadIndex - 1], payload[payloadIndex]);
  return 1;
}

uint8_t value_u24hex_LE(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 3, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X%02X\"", payload[payloadIndex - 2], payload[payloadIndex - 1], payload[payloadIndex]);
  return 1;
}

uint8_t value_u32hex_LE(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 4, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X%02X%02X%02X\"", payload[payloadIndex - 3], payload[payloadIndex - 2], payload[payloadIndex - 1], payload[payloadIndex]);
  return 1;
}

// unsigned integers, LE

uint16_t FN_u16_LE(uint8_t *b)          { return (                (((uint16_t) b[-1]) << 8) | (b[0]));}
uint32_t FN_u24_LE(uint8_t *b)          { return (((uint32_t) b[-2] << 16) | (((uint32_t) b[-1]) << 8) | (b[0]));}
uint32_t FN_u32_LE(uint8_t *b)          { return (((uint32_t) b[-3] << 24) | ((uint32_t) b[-2] << 16) | (((uint32_t) b[-1]) << 8) | (b[0]));}

uint8_t value_u8(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", payload[payloadIndex]);
  return 1;
}

uint8_t value_u16_LE(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 2, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", FN_u16_LE(&payload[payloadIndex]));
  return 1;
}

uint8_t value_u24_LE(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 3, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", FN_u24_LE(&payload[payloadIndex]));
  return 1;
}

uint8_t value_u32_LE(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 4, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", FN_u32_LE(&payload[payloadIndex]));
  return 1;
}

uint8_t value_u32_LE_uptime(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  uint8_t bitMaskUptime = 0x01;
  if ((payload[payloadIndex - 3] == 0) && (payload[payloadIndex - 2] == 0)) {
    while (payload[payloadIndex - 1] > bitMaskUptime) { bitMaskUptime <<= 1; bitMaskUptime |= 1; }
  } else {
    bitMaskUptime = 0xFF;
  }
  payload[payloadIndex] &= ~(bitMaskUptime >> 1);
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 4, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", FN_u32_LE(&payload[payloadIndex]));
  return 1;
}

// signed integers, LE

uint8_t value_s8(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", (int8_t) payload[payloadIndex]);
  return 1;
}

byte RSSIcnt = 0xFF;
uint8_t value_s8_ratelimited(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (++RSSIcnt) return 0;
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", (int8_t) payload[payloadIndex]);
  return 1;
}

uint8_t value_s16_LE(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 2, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", (int16_t) FN_u16_LE(&payload[payloadIndex]));
  return 1;
}

// single bit

bool FN_flag8(uint8_t b, uint8_t n)  { return (b >> n) & 0x01; }

uint8_t value_flag8(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte bitNr) {
  if (!newPayloadBitVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, bitNr))   return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", FN_flag8(payload[payloadIndex], bitNr));
  return 1;
}

uint8_t unknownBit(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, byte bitNr) {
  if (!outputUnknown || !(newPayloadBitVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, bitNr))) return 0;
  snprintf(mqtt_key, MQTT_KEY_LEN, "PacketSrc_0x%02X_Byte_%i_Bit_%i", packetSrc, payloadIndex, bitNr);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", FN_flag8(payload[payloadIndex], bitNr));
  return 1;
}

// misc

uint8_t unknownByte(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!outputUnknown || !newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;

// MHI TODO
  if ((packetSrc >= 0x00) && (packetSrc <= 0x7F)) packetSrc = 0x01;

  snprintf(mqtt_key, MQTT_KEY_LEN, "PacketSrc_0x%02X_Byte_%i", packetSrc, payloadIndex);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"0x%02X\"", payload[payloadIndex]);
  return 1;
}

uint8_t value_u8_add2k(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", payload[payloadIndex] + 2000);
  return 1;
}

int8_t FN_s4abs1c(uint8_t *b)        { int8_t c = b[0] & 0x0F; if (b[0] & 0x10) return -c; else return c; }

uint8_t value_s4abs1c(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 1, 1)) return 0;
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", FN_s4abs1c(&payload[payloadIndex]));
  return 1;
}

float FN_u16div10_LE(uint8_t *b)     { if (b[-1] == 0xFF) return 0; else return (b[0] * 0.1 + b[-1] * 25.6);}

uint8_t value_u16div10_LE(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 2, 1)) return 0;
#ifdef __AVR__
  dtostrf(FN_u16div10_LE(&payload[payloadIndex]), 1, 1, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", FN_u16div10_LE(&payload[payloadIndex]));
#endif
  return 1;
}

uint8_t value_trg(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"Empty_Payload_%02X\"", packetSrc);
  return 1;
}

// 16 bit fixed point reals

float FN_f8_8(uint8_t *b)            { return (((int8_t) b[-1]) + (b[0] * 1.0 / 256)); }

uint8_t value_f8_8(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 2, 1)) return 0;
#ifdef __AVR__
  dtostrf(FN_f8_8(&payload[payloadIndex]), 1, 2, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.3f", FN_f8_8(&payload[payloadIndex]));
#endif
  return 1;
}

float FN_f8s8(uint8_t *b)            { return ((float)(int8_t) b[-1]) + ((float) b[0]) /  10;}

uint8_t value_f8s8(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig) {
  if (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 2, 1)) return 0;
#ifdef __AVR__
  dtostrf(FN_f8s8(&payload[payloadIndex]), 1, 1, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.3f", FN_f8s8(&payload[payloadIndex]));
#endif
  return 1;
}

// change to Watt ?
uint8_t value_f(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, float v, int length = 0) {
  if (length && (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, length, 1))) return 0;
  if (!length) newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 1, 1);
#ifdef __AVR__
  dtostrf(v, 1, 3, mqtt_value);
#else
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.3f", v);
#endif
  return (outputFilter > maxOutputFilter ? 0 : 1);
}

uint8_t value_s(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte haConfig, int v, int length = 0) {
  if (length && (!newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, length, 1))) return 0;
  if (!length) newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 1, 1);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", v);
  return (outputFilter > maxOutputFilter ? 0 : 1);
}

uint8_t value_timeString(char* mqtt_value, char* timestring) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "\"%s\"", timestring);
  return (outputFilter > maxOutputFilter ? 0 : 1);
}

// BITBASIS (for j==8): this byte is to be treated on a bitbasis (by re-calling this function with j=0..7
//                      switch-statement for j is needed as shown below
//                      indicate for j=0..7: "KEY("KnownParameter"); VALUE_flag8" if bit usage is known
//                      indicate for j=0..7: "UNKNOWN_BIT", mqtt_key will then be set to "Bit-0x[source]-0x[payloadnr]-0x[location]-[bitnr]"
// BITBASIS_UNKNOWN:    shortcut for entire bit-switch statement if bit usage of all bits is unknown
// UNKNOWN_BYTE:        byte function is unknown, mqtt_key will be: "PacketSrc-0xXX-Byte-I-Bit-I", value will be hex
// UNKNOWN_BIT:         bit function is unknown, mqtt_key will be: "PacketSrc-0xXX-Byte-I-Bit-I", value will be hex
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
// TERMINATEJSON        returns 9 to signal end of package, signal for json string termination

#define VALUE_u8hex             { return       value_u8hex(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); break; }
#define VALUE_u16hex_LE         { return      value_u16hex_LE(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u24hex_LE         { return      value_u24hex_LE(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u32hex_LE         { return      value_u32hex_LE(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }

#define VALUE_u8                { return          value_u8(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); break; }
#define VALUE_u16_LE            { return         value_u16_LE(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u24_LE            { return         value_u24_LE(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u32_LE            { return         value_u32_LE(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u32_LE_uptime     { return         value_u32_LE_uptime(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }

#define VALUE_s8                { return          value_s8(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_s8_ratelimited    { return          value_s8_ratelimited(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_s16_LE            { return         value_s16_LE(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }

#define VALUE_flag8             { return       value_flag8(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, bitNr); }
#define UNKNOWN_BIT             { CAT_UNKNOWN; return        unknownBit(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, bitNr); }

#define VALUE_u8_add2k          { return    value_u8_add2k(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_s4abs1c             { return      value_s4abs1c(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }

#define VALUE_u16div10_LE       { return    value_u16div10_LE(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_u16div10_LE_changed(ch)  { ch = value_u16div10_LE(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); return ch; }

#define VALUE_f8_8              { return        value_f8_8(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_f8_8_changed(ch)  { ch = value_f8_8(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); return ch; }

#define VALUE_f8s8              { return        value_f8s8(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_F(v)              { return           value_f(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, v); }
#define VALUE_F_L(v, l)         { return           value_f(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, v, l); }
#define VALUE_S_L(v, l)         { return           value_s(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig, v, l); }
#define VALUE_timeString(s)     { return  value_timeString(mqtt_value, s); }

#define UNKNOWN_BYTE            { CAT_UNKNOWN; return unknownByte(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }
#define VALUE_header            { return            value_trg(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, haConfig); }

#define BITBASIS_UNKNOWN        { switch (bitNr) { case 8 : BITBASIS; default : UNKNOWN_BIT; } }
#define TERMINATEJSON           { return 9; }

#define BITBASIS                { return newPayloadBytesVal(packetSrc, payloadIndex, payload, mqtt_key, haConfig, 1, 0) << 3; }
// BITBASIS returns 8 if at least one bit of a byte changed, or if at least one bit of a byte hasn't been seen before, otherwise 0
// value of byte and of seen status should not be saved (yet) (saveSeen=0), this is done on bit basis

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

byte bytesbits2keyvalue(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, /*char &cat, char &src,*/ byte bitNr) {
// payloadIndex: new payload documentation counts payload bytes starting at 0 (following Budulinek's suggestion)
// payloadIndex == EMPTY_PAYLOAD : empty payload (used during restart)

  byte haConfig = 0;

  maxOutputFilter = 9; // default all changes visible, unless changed below
  CAT_UNKNOWN; // cat unknown unless changed below

  byte payloadByte = 0;
  byte* payloadPointer = 0;
  if (payloadIndex != EMPTY_PAYLOAD) {
    payloadByte = (payloadIndex == EMPTY_PAYLOAD ? 0 : payload[payloadIndex]);
    payloadPointer = (payloadIndex == EMPTY_PAYLOAD ? 0 : &payload[payloadIndex]);
  };

  SRC(0);


  // P1/P2: Main/Peripheral; peripheral-address; packet-type
  // MHI:   address;         data
  // pseudo: FF              00                  08..0F
//  if ((packetSrc >= 0x00) && (packetSrc <= 0x7F)) packetSrc = 0x01;
  switch (packetSrc) {
    case 0x00 :
    case 0x01 ... 0x07 :
    case 0x10 ... 0x7F : switch (payloadIndex) {
      case 14 : return 0; // CRC
      default : UNKNOWN_BYTE;
    }
    case 0x80 :
                switch (payloadIndex) {
      case  1 : KEY("Mode");                                                                                                       VALUE_u8hex;
      case  2 : KEY("Fan_Speed");                                                                                                  VALUE_u8hex;
      case  3 : KEY("Set_Temp");                                                                                                   VALUE_F((payloadByte - 0x80) * 0.5);
      case  4 : KEY("RC_Indoor_Temp");                                                                                             VALUE_F((payloadByte - 0x6B) * 0.5);
      case 14 : return 0; // CRC
      default : UNKNOWN_BYTE;
    }
// no pseudo messages #include "P1P2_pseudo.h"
    default : UNKNOWN_BYTE // unknown PacketByte
  }
}

byte bits2keyvalue(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value, byte j) {
  strcpy(mqtt_key, mqttKeyPrefix);
  byte b = bytesbits2keyvalue(packetSrc, payloadIndex, payload, mqtt_key + MQTT_KEY_PREFIXLEN, mqtt_value, j) ;
  return b;
}

byte bytes2keyvalue(byte packetSrc, byte payloadIndex, byte* payload, char* mqtt_key, char* mqtt_value) {
  return bits2keyvalue(packetSrc, payloadIndex, payload, mqtt_key, mqtt_value, 8) ;
}
#endif /* P1P2_Daikin_ParameterConversion_H */
