/* P1P2_ParameterConversion.h
 *
 * Copyright (c) 2019-2024 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 * 20240519 v0.9.51 field settings added, quiet_mode re-enabled for 0x0100 outputmode
 * 20240519 v0.9.49 hysteresis-check for V_Interface
 * 20240515 v0.9.46 full HA MQTT discovery and control, lots of other changes
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
 * 20220918 v0.9.22 degree symbol, hwID, 32-bit EE.outputMode
 * 20220821 v0.9.17-fix1 corrected negative deviation temperature reporting, more temp settings reported
 * 20220817 v0.9.17 license change
 * 20220808 v0.9.15 extended verbosity command, unique OTA hostname, minor fixes
 * 20220802 v0.9.14 AVRISP, wifimanager, mqtt settings, EEPROM, telnet, EE.outputMode, EE.outputFilter, ...
 * ..
 *
 */

// This file translates data from the P1/P2 bus to <mqttTopic,mqtt_value> pairs, code here is highly device-dependent,
// in order to implement parameter decoding for another product, rename this file and make any necessary changes
// mqttTopic and mqtt_value are globals, so take care
// (in particular in the big switch statements below).
//
// The main conversion function here is bytes2keyvalue(..),
// which is called for every individual byte of every payload.
// It aims to calculate a <mqttTopic,mqtt_value> pair for mqtt output.
//
// Return value 0 for success, or >0 indicating which bits should be individually handled
//
// The core of the translation is formed by large nested switch statements.
// Many of the macro's include break statements and make the core more readable.

#ifndef P1P2_ParameterConversion
#define P1P2_ParameterConversion

#define CAT_SETTING       { mqttTopic[ mqttTopicCatChar ] = 'S'; } // system settings, setpoints (even if WD)
#define CAT_TEMP          { mqttTopic[ mqttTopicCatChar ] = 'T'; maxOutputFilter = 1; } // temperature measurements
#define CAT_MEASUREMENT   { mqttTopic[ mqttTopicCatChar ] = 'M'; maxOutputFilter = 1; } // other measurements
#define CAT_FIELDSETTING  { mqttTopic[ mqttTopicCatChar ] = 'F'; } // field settings
#define CAT_COUNTER       { mqttTopic[ mqttTopicCatChar ] = 'C'; } // kWh/hour/start counters
#define CAT_PSEUDO        { mqttTopic[ mqttTopicCatChar ] = 'A'; } // ATmega/ESP operation
#define CAT_PSEUDO_SYSTEM { mqttTopic[ mqttTopicCatChar ] = 'C'; } // system operation (COP etc) // conflicts with CAT_COUNTER, but no real need to change - otherwise could be 'B'
#define CAT_SCHEDULE      { mqttTopic[ mqttTopicCatChar ] = 'E'; } // Schedule
#define CAT_UNKNOWN       { mqttTopic[ mqttTopicCatChar ] = 'U'; }
#define SRC(x)            { mqttTopic[ mqttTopicSrcChar ] = '0' + x; }

// SRC values
// Daikin:
// 0 main controller to heat pump
// 1 (reply)
// 2 main controller to F0
// 3 (reply)
// 4 (reserved for main controller to F1)
// 5 (reply)
// 6 (reserved for main controller to FF)
// 7 (reply)
// 8 pseudo ATmega
// 9 pseudo ESP (incl date_time, power)
//
// A 80* messages (use value ('C' - '0'))
// B boot message (use value ('B' - '0'))
//
// Hitachi:
// 2 indoor unit info
// 8 IU and OU system info
// 7 ?
// 4 sender is Hitachi remote control or Airzone
// 0 pseudopacket, ATmega (differs from Daikin convention)
// 1 pseudopacket, ESP    (differs from Daikin convention)
// 9 other

// ESP8266, use strncpy_P (with care!), and PSTR()
#include <pgmspace.h>
#define mymin(a,b) ((a<b) ? a : b)

// macro usage
//
// regular entity reporting done by calling KEYx_PUB_CONFIG_CHECK_ENTITY which calls
//            CHECK(x), determines only new/changed, and generates pubHaEntity mask
//            PUB_CONFIG(K)   -> if needed, publishes config (if pubHa && haConfig && ...???), executes "return 0" if publishHA fails
//            CHECK_ENTITY(K) -> executes "return 0" if nothing to be published, otherwise (if pubHa||pubEntity / hysteresis): prepares to publish topic (in VALUE*)
// after which the code is supposed to call
//            VALUE           -> publishes value, and stores new value if succesful
//
// Calling order is: (set vars) CHECK PUB_CONFIG CHECK_ENTITY VALUE

#define CHECK(length)      { pubHaEntity = newCheckPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, length, 1); }
#define CHECKPARAM(length) { pubHaEntity = newCheckParamVal       (paramSrc, paramPacketType, paramNr, payloadIndex, payload, length); }
#define BITBASIS           { pubHaEntityBits = newCheckPayloadBytesVal(packetSrc, packetType, payloadIndex, payload, 1, 0); return (pubHaEntityBits & 0xFF); }
#define BITBASIS_UNKNOWN   { switch (bitNr) { case 8 : BITBASIS; default : UNKNOWN_BIT; } }

uint16_t pubHaEntity = 0;
uint16_t pubHaEntityBits = 0;
byte bitsMask = 0;
byte FSB = 0;

#define CHECKBIT           { bitsMask = (0x01 << bitNr); pubHaEntity = pubHaEntityBits & ((bitsMask << 8) | bitsMask); }
#define CHECKBITS(x, y)    { if (x > 7) { printfTopicS("bitNr1 %i > 7", x); return 0; };  \
                             if (y > 7) { printfTopicS("bitNr2 % > 7", y); return 0; };  \
                             if (x > y) { printfTopicS("bitNr1 %i > bitNr2 %i", x, y); return 0; }; \
                             bitsMask = ((0x01 << (y - x + 1)) - 1) << x; \
                             pubHaEntity = pubHaEntityBits & ((bitsMask << 8) | bitsMask); \
}

#define pubHa (pubHaEntity & 0xFF00)
#define pubHaBit (pubHaEntityBits & (0x0100 << bitNr))
#define pubEntity (pubHaEntity & 0x00FF)

#define KEEPCONFIG 1 // do not make optional items unavailable (do not use DEL_CONFIG)

#define VALUE_flag8             { if (haDevice == HA_SENSOR) HADEVICE_BINSENSOR; value_flag8(packetSrc, packetType, payloadIndex, payload, mqtt_value, bitNr); return 0; }

#define HACONFIG { haConfig = 1; }

#define KEY(K) { strncpy_P(mqttTopic + mqttTopicPrefixLength, PSTR(K), mymin(MQTT_TOPIC_LEN - mqttTopicPrefixLength, strlen(K) + 1)); mqttTopic[ MQTT_TOPIC_LEN - 1 ] = '\0'; };

#define PUB_CONFIG  { if (pubHa && haConfig && (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc))) return 0; }
#define DEL_CONFIG  { if (pubHa && haConfig && (!deleteHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc))) return 0; }
#define PUB_CONFIG_COMMON_NAME(x)  { if (pubHa && haConfig && (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc, 1, x))) return 0; }
#define DEL_CONFIG_COMMON_NAME(x)  { if (pubHa && haConfig && (!deleteHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc, 1, x))) return 0; }

#define CHECK_ENTITY { if (!pubEntity || (!(haConfig || (EE.outputMode & 0x0100)))) return 0; };

#define KEY1_PUB_CONFIG_CHECK_ENTITY(K)    { CHECK(1); KEY(K); PUB_CONFIG; CHECK_ENTITY; }
#define KEY2_PUB_CONFIG_CHECK_ENTITY(K)    { CHECK(2); KEY(K); PUB_CONFIG; CHECK_ENTITY; }
#define KEY3_PUB_CONFIG_CHECK_ENTITY(K)    { CHECK(3); KEY(K); PUB_CONFIG; CHECK_ENTITY; }
#define KEY4_PUB_CONFIG_CHECK_ENTITY(K)    { CHECK(4); KEY(K); PUB_CONFIG; CHECK_ENTITY; }

#define KEY1_DEL_CONFIG_CHECK_ENTITY(K)    { CHECK(1); KEY(K); DEL_CONFIG; CHECK_ENTITY; }
#define KEY2_DEL_CONFIG_CHECK_ENTITY(K)    { CHECK(2); KEY(K); DEL_CONFIG; CHECK_ENTITY; }
#define KEY3_DEL_CONFIG_CHECK_ENTITY(K)    { CHECK(3); KEY(K); DEL_CONFIG; CHECK_ENTITY; }
#define KEY4_DEL_CONFIG_CHECK_ENTITY(K)    { CHECK(4); KEY(K); DEL_CONFIG; CHECK_ENTITY; }

#define KEYBIT_PUB_CONFIG_PUB_ENTITY(K)          { if (haDevice == HA_SENSOR) HADEVICE_BINSENSOR; CHECKBIT;        KEY(K); PUB_CONFIG; CHECK_ENTITY; VALUE_flag8; }
#define KEYBIT_PUB_CONFIG_PUB_ENTITY_INV(K)      { if (haDevice == HA_SENSOR) HADEVICE_BINSENSOR; CHECKBIT;        KEY(K); PUB_CONFIG; CHECK_ENTITY; VALUE_flag8_inv; }
#define KEYBIT_PUB_CONFIG_CHECK_ENTITY(K)        { if (haDevice == HA_SENSOR) HADEVICE_BINSENSOR; CHECKBIT;        KEY(K); PUB_CONFIG; CHECK_ENTITY;  }
#define KEYBITS_PUB_CONFIG_PUB_ENTITY(x, y, K)   {                                                CHECKBITS(x, y); KEY(K); PUB_CONFIG; CHECK_ENTITY; VALUE_bits(x, y);}
#define KEYBITS_PUB_CONFIG_CHECK_ENTITY(x, y, K) {                                                CHECKBITS(x, y); KEY(K); PUB_CONFIG; CHECK_ENTITY; }

#define PARAM_KEY(K) { CHECKPARAM(paramValLength); KEY(K); PUB_CONFIG; CHECK_ENTITY; }

#define FIELDKEY(K) { if (FSB == 1) break;  \
                      CAT_FIELDSETTING; \
                      KEY(K); \
                      if (FSB == 2) { \
                        pubHaEntity = 0xFFFF; \
                      } else switch (packetType) {\
                        case 0x39 : CHECKPARAM(4); break; \
                        case 0x60 ... 0x8F : CHECK(4); break;\
                        default : printfTopicS("FIELDKEY unexpected packetType %i", packetType); break;\
                      }\
                      PUB_CONFIG; \
                      CHECK_ENTITY; break;}

#define KEYHEADER(K)    { pubHaEntity = 1; if (haConfig || (EE.outputMode & 0x0100)) { KEY(K); VALUE_header; }; return 0; } // for packet with empty payload

#define SUBDEVICE(x) { strlcpy(deviceSubName, x, DEVICE_SUBNAME_LEN); };

char timeString1[20] = "Mo 2000-00-00 00:00";    // reads time from packet type 0x12
// EKHBRD: char timeString1[12] = "Mo 00:00:00";    // reads time from packet type 0x12
char timeString2[23] = "Mo 2000-00-00 00:00:00"; // reads time from packet type 0x31 (+weekday from packet 12)

//==================================================================================================================

#define EXTRA_AV_STRING_ADD(formatstring, ...) { \
  extraAvailabilityStringLength += snprintf_P(extraAvailabilityString + extraAvailabilityStringLength, EXTRA_AVAILABILITY_STRING_LEN - extraAvailabilityStringLength, PSTR(formatstring) __VA_OPT__(,) __VA_ARGS__); \
  if (extraAvailabilityStringLength > extraAvailabilityStringLengthMax) extraAvailabilityStringLengthMax = extraAvailabilityStringLength; \
  if (extraAvailabilityStringLength >= EXTRA_AVAILABILITY_STRING_LEN) { \
    extraAvailabilityString[0] = '\0'; \
    extraAvailabilityStringLength = 0; \
  } \
}

//==================================================================================================================

#define HADEVICE_CLIMATE_TEMPERATURE(temp_stat_topic, temp_min, temp_max, temp_step) { \
  topicCharSpecific('P'); \
  HACONFIGMESSAGE_ADD(  \
    "\"temp_stat_t\":\"%s/%s\"," \
    "\"min_temp\":%1.1f," \
    "\"max_temp\":%1.1f," \
    "\"temp_step\":%1.1f," \
    , mqttTopic, temp_stat_topic \
    , (float) temp_min, (float) temp_max, (float) temp_step);  \
}

#define HADEVICE_CLIMATE_TEMPERATURE_CURRENT(curr_temp_topic) { \
  topicCharSpecific('P'); \
  HACONFIGMESSAGE_ADD(  \
  "\"curr_temp_t\":\"%s/%s\"," \
  , mqttTopic, curr_temp_topic); \
}

#define HADEVICE_CLIMATE_MODES(mode_stat_topic, modes, modes_tx) { \
  topicCharSpecific('P'); \
  HACONFIGMESSAGE_ADD( \
    "\"mode_stat_t\":\"%s/%s\"," \
    "\"mode_stat_tpl\":\"{%% set modes={%s} %%}{{ modes[value] if value in modes.keys() else modes['0']}}\"," \
    "\"modes\":[%s]," \
    , mqttTopic, mode_stat_topic \
    , modes_tx \
    , modes); \
}

//==================================================================================================================

// F-series fan mode

#define HADEVICE_CLIMATE_FAN_MODES(fan_mode_stat_topic, fan_modes, fan_modes_tx) { \
  topicCharSpecific('P'); \
  HACONFIGMESSAGE_ADD( \
    "\"fan_mode_stat_t\":\"%s/%s\"," \
    "\"fan_mode_stat_tpl\":\"{%% set modes={%s} %%}{{ modes[value] if value in modes.keys() else modes['0']}}\"," \
    "\"fan_modes\":[%s]," \
    , mqttTopic, fan_mode_stat_topic \
    , fan_modes_tx \
    , fan_modes); \
}

#define HADEVICE_CLIMATE_FAN_MODE_COMMAND_TEMPLATE(fan_mode_cmd_template) { \
  topicWrite; \
  HACONFIGMESSAGE_ADD( \
    "\"fan_mode_cmd_t\":\"%s\"," \
    "\"fan_mode_cmd_tpl\":\"%s\"," \
    , mqttTopic, fan_mode_cmd_template); \
}

//==================================================================================================================
#define HADEVICE_CLIMATE_TEMPERATURE_COMMAND(temp_cmd_template) { \
  topicWrite; \
  HACONFIGMESSAGE_ADD( \
    "\"temp_cmd_t\":\"%s\"," \
    "\"temp_cmd_tpl\":\"%s\"," \
    , mqttTopic, temp_cmd_template); \
}

#define HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE(mode_cmd_template) { \
  topicWrite; \
  HACONFIGMESSAGE_ADD( \
    "\"mode_cmd_t\":\"%s\"," \
    "\"mode_cmd_tpl\":\"%s\"," \
    , mqttTopic, mode_cmd_template); \
}

//==================================================================================================================

#define HADEVICE_AVAILABILITY(availability_topic, on_value, off_value) { \
  topicCharSpecific('P'); \
  EXTRA_AV_STRING_ADD( \
    ",{\"topic\":\"%s/%s\",\"pl_avail\":%i,\"pl_not_avail\":%i}" \
    , mqttTopic, availability_topic, on_value, off_value); \
}

//==================================================================================================================

#define HADEVICE_SELECT_OPTIONS(modes, modes_tx) { \
  topicCharSpecific('P'); \
  HACONFIGMESSAGE_ADD( \
    "\"val_tpl\":\"{%% set modes={%s} %%}{{ modes[value] if (value) in modes.keys() else modes['0']}}\"," \
    "\"options\":[%s]," \
    , modes_tx \
    , modes); \
}

#define HADEVICE_SELECT_COMMAND_TEMPLATE(select_cmd_template) { \
  topicWrite; \
  HACONFIGMESSAGE_ADD( \
    "\"cmd_t\":\"%s\"," \
    "\"cmd_tpl\":\"%s\"," \
    , mqttTopic, select_cmd_template); \
}

// not used
#define HADEVICE_OPTIMISTIC { \
  HACONFIGMESSAGE_ADD( \
    "\"optimistic\":\"true\","); \
}

//==================================================================================================================

#define HADEVICE_SENSOR_VALUE_TEMPLATE(sensor_val_template) { \
  topicWrite; \
  HACONFIGMESSAGE_ADD( \
    "\"val_tpl\":\"%s\"," \
    , sensor_val_template); \
}

//==================================================================================================================

#define HADEVICE_NUMBER_RANGE(nr_min, nr_max, nr_step) { \
  topicCharSpecific('P'); \
  HACONFIGMESSAGE_ADD(  \
    "\"min\":%f," \
    "\"max\":%f," \
    "\"step\":%f," \
    , (float) nr_min, (float) nr_max, (float) nr_step);\
}

#define HADEVICE_NUMBER_MODE(nr_mode) { \
  topicCharSpecific('P'); \
  HACONFIGMESSAGE_ADD(  \
    "\"mode\":\"%s\"," \
    , nr_mode); \
}

#define HADEVICE_NUMBER_COMMAND_TEMPLATE(number_cmd_template) { \
  topicWrite; \
  HACONFIGMESSAGE_ADD( \
    "\"cmd_t\":\"%s\"," \
    "\"cmd_tpl\":\"%s\"," \
    , mqttTopic, number_cmd_template); \
}

//==================================================================================================================


// update, restart, identify
#define HADEVICE_BUTTON_CLASS(button_class) { \
  topicCharSpecific('P'); \
  HACONFIGMESSAGE_ADD(  \
    "\"dev_cla\":\"%s\"," \
    , button_class); \
}

#define HADEVICE_BUTTON_COMMAND(button_cmd) { \
  topicWrite; \
  HACONFIGMESSAGE_ADD( \
    "\"cmd_t\":\"%s\"," \
    "\"cmd_tpl\":\"%s\"," \
    , mqttTopic, button_cmd); \
}

//==================================================================================================================

#define HADEVICE_SWITCH_PAYLOADS(payload_off, payload_on) { \
  topicWrite; \
  HACONFIGMESSAGE_ADD( \
    "\"stat_off\":0," \
    "\"stat_on\":1," \
    "\"pl_off\":\"%s\"," \
    "\"pl_on\":\"%s\"," \
    "\"cmd_t\":\"%s\"," \
    , payload_off, payload_on, mqttTopic); \
}

// ====================== define data structures ======================

#ifdef E_SERIES

#define PARAM_TP_START      0x35
#define PARAM_TP_END        0x3D
#define PARAM_ARR_SZ (PARAM_TP_END - PARAM_TP_START + 1)   // 0       1       2       3       4       5       6       7       8
const PROGMEM uint32_t  nr_params[PARAM_ARR_SZ]      = { 0x017D, 0x0030, 0x0002, 0x001F, 0x00F0, 0x006C, 0x00B0, 0x0002, 0x0020 }; // number of parameters observed
//byte packettype                                    = {   0x35,   0x36,   0x37,   0x38,   0x39,   0x3A,   0x3B,   0x3C,   0x3D };
const PROGMEM uint32_t  parnr_bytes [PARAM_ARR_SZ]   = {      1,      2,      3,      4,      4,      1,      2,      3,      4 }; // byte per parameter // was 8-bit
const PROGMEM uint32_t   valstart[PARAM_ARR_SZ]      = { 0x0000, 0x017D, 0x01DD, 0x01E3, 0x025F, 0x061F, 0x068B, 0x07EB, 0x07F1 /* , 0x0871 */ }; // valstart = sum  (parnr_bytes * nr_params)
const PROGMEM uint32_t  seenstart[PARAM_ARR_SZ]      = { 0x0000, 0x017D, 0x01AD, 0x01AF, 0x01CE, 0x02BE, 0x032A, 0x03DA, 0x03DC /* , 0x03FC */ }; // seenstart = sum (parnr_bytes)

#define sizeParamVal  0x0871
#define sizeParamSeen    128 // ceil(0x03FC/8) = ceil(1020/8) = 128

#define PCKTP_START  0x0B
#define PCKTP_END    0x15 // 0x0D-0x15 and 0x31 to 0x16 0x20 0x21 0x60-0x9F mapped to 0x17-0x58
#define PCKTP_ARR_BLOCK (PCKTP_END - PCKTP_START + 3 + 18 /* for 60-8F: + 48 */)
#define PCKTP_ARR_SZ    ((2 * PCKTP_ARR_BLOCK) + 2)

// 00F030 : used for setting up HA, at least 11 needed, for now 14 reserved
// 40F030 : not used, no storage needed
// 00F031 : 12 bytes, storage is needed only for first 6
// 40F031 : not used, no storage needed

//0B,  0C,  0D,  0E,  0F,  10,  11,  12,  13,  14,  15,  30,  31,  20,  21,   /* 60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  6A,  6B,  6C,  6D,  6E,  6F,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  7A,  7B,  7C,  7D,  7E,  7F,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  8A,  8B,  8C,  8D,  8E,  8F, */  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  9A,  9B,  9C,  9D,  9E,  9F
const PROGMEM uint32_t nr_bytes[PCKTP_ARR_SZ]     =
{
//0000xx
  0,    0,   0,  20,  20,  20,  20,  20,  20,  20,  20,  14,   6,  20,  20,   /* 20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20, */  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,
//4000xx
 20,   20,  20,  20,  20,  20,  20,  20,  20,  20,  20,   0,   6,  20,  20,   /* 20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20, */  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,
//800010, 800018
  17, 6,
 };

const PROGMEM uint32_t bytestart[PCKTP_ARR_SZ]     =
{  0,   0,   0,   0,  20,  40,  60,  80, 100, 120, 140, 160, 174, 180, 200,                                                                                                                                                                                                                                                         220, 240, 260, 280, 300, 320, 340, 360, 380, 400, 420, 440, 460, 480, 500, 520,
 540, 560, 580, 600, 620, 640, 660, 680, 700, 720, 740, 760, 760, 766, 786,                                                                                                                                                                                                                                                         806, 826, 846, 866, 886, 906, 926, 946, 966, 986,1006,1026,1046,1066,1086,1106,
 1126,1143, /* sizePayloadByteVal=1149 */ };
#define sizePayloadByteVal 1149
#define sizePayloadByteSeen 144 // ceil(1149/8)
#define sizePayloadBitsSeen 33

#ifdef SAVESCHEDULE
#define SCHEDULE_MEM_START 0x0250
#define SCHEDULE_MEM_SIZE 0x048E          // this model uses only 0x0250 - 0x070D
uint16_t scheduleMemLoc[2];
uint8_t scheduleLength[2];
uint8_t scheduleSeq[2];
byte scheduleMem[2][SCHEDULE_MEM_SIZE] = {};     // 2 * 1166 = 2332 bytes
bool scheduleMemSeen[2][SCHEDULE_MEM_SIZE] = {}; // 2 * 1166 = 2332 bytes
#endif /* SAVESCHEDULE */

#elif defined F_SERIES

#define PCKTP_START  0x08 // 0x08..0x12, others mapped, // 0x37 zone name/0xC1 service mode subtype would require special coding, not supported yet
#define PCKTP_ARR_BLOCK 38
#define PCKTP_ARR_SZ ((2 * PCKTP_ARR_BLOCK) + 1)
//byte packetsrc                                   = { 00                                                                                                                                                                                          , 40                                                                                                                                                                                         ,    80 }
//byte packettype                                  = { 08,  09,  0A,  0B,  0C,  0D,  0E,  0F,  10,  11,  12,  15,  17,  18,  19,  1F,  20,  21,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  3A,  3B,  3C,  3D,  3E,  3F,  80,  A1,  A3,  B1,  08,  09,  0A,  0B,  0C,  0D,  0E,  0F,  10,  11,  12,  15,  17,  18,  19,  1F,  20,  21,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  3A,  3B,  3C,  3D,  3E,  3F,  80,  A1,  A3, B1 ,    18 }
const PROGMEM uint32_t nr_bytes [PCKTP_ARR_SZ]     = {  0,  20,  20,  20,   0,  20,  20,  20,  20,  17,  17,  18,   6,   7,   5,   0,   1,  20,  20,  20,   0,   0,   0,   0,   0,   0,  20,  12,  11,  20,  12,   0,   0,   0,  10,  16,   0,  18,   0,  20,  20,  20,   0,  20,  20,  20,  20,  20,  20,  18,  11,  19,   4,  20,  20,  20,   0,  20,   0,   0,   0,   0,   0,   0,  20,   5,   7,  19,   2,   0,   0,   0,  10,  16,  19, 18,    11     };
const PROGMEM uint32_t bytestart[PCKTP_ARR_SZ]     = {  0,   0,  20,  40,  60,  60,  80, 100, 120, 140, 157, 174, 192, 198, 205, 210, 210, 211, 231, 251, 271, 271, 271, 271, 271, 271, 271, 291, 303, 314, 334, 346, 346, 346, 346, 356, 372, 372, 390, 390, 410, 430, 450, 450, 470, 490, 510, 530, 550, 570, 588, 599, 618, 622, 642, 662, 682, 682, 702, 702, 702, 702, 702, 702, 702, 722, 727, 734, 753, 755, 755, 755, 755, 765, 781, 800, 818  /*, sizePayloadByteVal = 829 */ };

#define sizePayloadByteVal  829
#define sizePayloadByteSeen 104 // ceil(829/8)

#define sizePayloadBitsSeen 1

#elif defined MHI_SERIES

#define sizePayloadBitsSeen 0x14

// no pseudo packet translation yet due to packetSrc conflict

#define PCKTP_START  0x08
#define PCKTP_END    0x0F // assume first byte of MHI packet is source identifier: 00, 80, 0C, mapped to 10, 11, 12
#define PCKTP_ARR_SZ (PCKTP_END - PCKTP_START + 4)
//byte packettype                                  = {00,  0C,  80} // map 00  01-7F or 00-7F?
// map 00-01 to 00-01
// map 02-07 to 02
// map 80-81 to 03-04
// 16 bytes each
const PROGMEM uint32_t nr_bytes [PCKTP_ARR_SZ]     = { 16,  16,  16,  16,  16  };
const PROGMEM uint32_t bytestart[PCKTP_ARR_SZ]     = {  0,  16,  32,  48,  64  /* , sizePayloadByteVal=80 */ };
#define sizePayloadByteVal  80
#define sizePayloadByteSeen 10 // ceil(80/8)

#elif defined TH_SERIES // for now, handle Toshiba as Hitachi

// for Hitachi:
// 0 .. 7 for pseudo packets 000008 - 00000F
// 8 .. F for pseudo packets 400008 - 40000F
//
// Hitachi model 1, source 1
// 10: 21 00 0B
// 11: 21 00 12
// 12: 89 00 27
// 13: 89 00 2D xx xx xx xx XX (XX = 0xE1 .. 0xE5, currently decode only 0xE2)
// 14: 41 00 18
//   : 89 06 (acknowledgement only)
//   : 21 06 (acknowledgement only)
//   : 41 06 (acknowledgement only)
//
// Hitachi, source 2
//
// 15: 21 00 29 xx xx xx xx F1
// 16: 21 00 29 xx xx xx xx F2
// 17: 21 00 29 xx xx xx xx F3
// 18: 21 00 29 xx xx xx xx F4
// 19: 21 00 29 xx xx xx xx F5
// 1A: 89 00 17 xx xx xx xx E1
// 1B: 89 00 29 xx xx xx xx E2
// 1C: 89 00 29 xx xx xx xx E3
// 1D: 89 00 29 xx xx xx xx E4
// 1E: 89 00 29 xx xx xx xx E5
//
// Hitachi, source 3
//
// 15: 21 00 29 xx xx xx xx F1
// 16: 21 00 29 xx xx xx xx F2
// 17: 21 00 29 xx xx xx xx F3
// 18: 21 00 29 xx xx xx xx F4
// 19: 21 00 29 xx xx xx xx F5 (not observed)
// 1A: 89 00 17 xx xx xx xx E1
// 1B: 89 00 29 xx xx xx xx E2
// 1C: 89 00 29 xx xx xx xx E3
// 1D: 89 00 29 xx xx xx xx E4
// 1E: 89 00 29 xx xx xx xx E5 (not observed)
// 1F: 41 00 29 xx xx xx xx E1
// 20: 8A 00 29 xx xx xx xx F1
// .. and more, reserved?
//
// Hitachi, source 4
//
// ..
// 21:
// ..
// 29:
//
// Hitachi model 2
//
// 13: 89 00 2D xx xx xx xx XX (for 0xE2 different entities from Hitachi model 1 nr 13, currently decode only 0xE2)
// 2A: 21 00 1C
//
// Hitachi HiBox AHP-SMB-01 and ATW-ATAG-02
//
// 2B: 79 00 2E
// 2C: FF 00 23
//
// nr_bytes = value 3rd byte - 4; or - 3 to cover CS_GEN byte

#define sizePayloadBitsSeen 12 // for Hitachi, but not for Toshiba

#define PCKTP_ARR_SZ 0x2D
//byte pti                                      =   0..F  (0..7 for 000008..00000F, 8..F for 400008..40000F)                           10-14                           15-1E                                             1F-20                                               2A       2B     2C
//byte pti                                      =    0    1    2    3    4    5    6    7    8    9   0A   0B   0C   0D   0E   0F      10   11   12   13   14          15   16   17   18   19   1A   1B   1C   1D   1E   1F   20   21  22  23  24  25  26  27  28  29        2A       2B     2C  // length 25->24 when CS_GEN implemented
//3rd byte                                      =                                                                                      0B   12   27   2D   18          29   29   29   29   29   17   29   29   29   29   29   29    ?   ?   ?   ?   ?   ?   ?   ?   ?        1C       2E     23
const PROGMEM uint32_t nr_bytes[PCKTP_ARR_SZ]  = {  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,      7,  14,  20,  35,  41,         37,  37,  37,  37,  37,  19,  37,  37,  37,  37,  37,  37,  50, 50, 50, 50, 50, 50, 50, 50, 50,       25     , 43,    32 };
const PROGMEM uint32_t bytestart[PCKTP_ARR_SZ] = {   0,  20,  40,  60,  80, 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300,    320, 327, 341, 361, 396,/*rst*/ 320, 357, 394, 431, 468, 505, 524, 561, 598, 635, 672, 709, 746,796,846,896,946,996,1046,1096,1146,    1196   , 1221,1264     /* , 746 -> 1296 */ };
#define sizePayloadByteVal 1296
#define sizePayloadByteSeen 162 // ceil(1296/8)

#elif defined W_SERIES

#define PCKTP_START  0x0C
#define PCKTP_END    0x0F // 0x0C - 0x0F (40 only)
#define PCKTP_ARR_BLOCK (PCKTP_END - PCKTP_START + 1)
#define PCKTP_ARR_SZ    (2 * PCKTP_ARR_BLOCK)

// 00F030 : used for setting up HA, at least 11 needed, for now 14 reserved
// 40F030 : not used, no storage needed
// 00F031 : 12 bytes, storage is needed only for first 6
// 40F031 : not used, no storage needed

//0C,  0D,  0E,  0F
const PROGMEM uint32_t nr_bytes[PCKTP_ARR_SZ]     =
{
//0000xx
  0,   0,   0,   0,
//4000xx
  0,  20,  20,  20,
 };

const PROGMEM uint32_t bytestart[PCKTP_ARR_SZ]     =
{  0,   0,   0,   0,
   0,   0,  20,  40,  /* sizePayloadByteVal=60 */ };
#define sizePayloadByteVal  60
#define sizePayloadByteSeen 8 // ceil(60/8)
#define sizePayloadBitsSeen 1

#endif /* *_SERIES */

typedef struct RTCdata {
  uint16_t RTCversion;
  uint16_t RTCdataLength;
  // ==========================
  byte month;
  byte wday;
  byte hour;
#ifdef E_SERIES
  uint32_t electricityConsumedCompressorHeating;
  uint32_t energyProducedCompressorHeating;
  int16_t power1;
  int16_t power2;
  int16_t COP_realtime;
  float LWT;
  float RWT;
  float MWT;
  float Flow;
  bool compressor;
  byte selectTimeString;
  byte climateModeACH; // bits 7-6 CH active; 5-4 CH last-request; 2 auto; 1-0 CH request
  byte climateModeC; // heating 0 cooling 1
  byte climateMode;
  byte controlRTX10;
  byte heatingOnlyX10;
  byte defrostActive;
  byte useDHW;
  byte numberOfLWTzonesX10;
  byte allowModulationX10;
  byte modulationMaxX10;
  uint16_t DHWsetpointMaxX10;
  uint16_t LWTheatMainMaxX10;
  uint16_t LWTheatMainMinX10;
  uint16_t LWTheatAddMaxX10;
  uint16_t LWTheatAddMinX10;
  uint16_t LWTcoolMainMaxX10;
  uint16_t LWTcoolMainMinX10;
  uint16_t LWTcoolAddMaxX10;
  uint16_t LWTcoolAddMinX10;
  uint16_t roomTempHeatingMinX10;
  uint16_t roomTempHeatingMaxX10;
  uint16_t roomTempCoolingMinX10;
  uint16_t roomTempCoolingMaxX10;
  byte HCmode;
  byte prevSec;
  byte prevMin;
  byte first030;
  byte first030a;
  byte overshootX10;
  byte quietMode;
  byte quietLevel;
#endif /* E_SERIES */
};

typedef struct mqttSaveStruct {
  uint16_t Mversion;
  uint16_t MdataLength;
  RTCdata R;
  byte payloadByteVal[sizePayloadByteVal];
  byte payloadByteSeen[sizePayloadByteSeen];
#ifdef E_SERIES
  byte paramVal [2][sizeParamVal];
  byte paramSeen[2][sizeParamSeen];
  byte cntByte [36]; // for 0xB8 counters we store 36 bytes; 30 should suffice but not worth the extra code
#endif /* E_SERIES */
  byte payloadBitsSeen[sizePayloadBitsSeen];
};

// local
byte bcnt = 0;

mqttSaveStruct M;
#define RTC_VERSION 7
#define M_VERSION 8

void checkSize() {
#ifdef E_SERIES
  for (byte i = 1; i < PARAM_ARR_SZ; i++) {
// correct for byte-aligning seenstart[0x39]
    if (seenstart[i] != seenstart[i - 1] + nr_params[i - 1] + (  (PARAM_TP_START + i == 0x39) ? 0 : 0)) printfTopicS("seenstart error i %i seenstart[i] 0x%04X seenstart[i-1] 0x%04X nr_params[i-1] 0x%04X", i, seenstart[i], seenstart[i-1], nr_params[i-1]);
    if (valstart[i] != valstart[i - 1] + nr_params[i - 1] * parnr_bytes[i - 1]) printfTopicS("valstart error i 0x%04X valstart[i] 0x%04X valstart[i-1] 0x%04X nr_params[i-1] 0x%04X", i, valstart[i], valstart[i-1], nr_params[i-1]);
  }
#endif /* E_SERIES */
  for (uint16_t i = 1; i < PCKTP_ARR_SZ; i++) {
    if (bytestart[i] != bytestart[i - 1] + nr_bytes[i - 1]) printfTopicS("bytestart error i %i bytestart[i] 0x%04X bytestart[i-1] 0x%04X nr_bytes[i-1]", i, bytestart[i], bytestart[i-1], nr_bytes[i-1]);
  }
}

// local
byte maxOutputFilter = 0;
uint16_t extraAvailabilityStringLengthMax = 0;
#ifdef E_SERIES
uint32_t espUptime030 = 0;
#endif /* E_SERIES */

// global variable to maintain info from check* to pub*
int16_t pi2 = -1;

void registerSeenByte() {
  if (pi2 >= 0) M.payloadByteSeen[pi2 >> 3] |= (1 << (pi2 & 0x07));
  return;
}

byte  createButtonsSwitches(void) {
  HARESET;
  KEY("Restart_P1P2Monitor_ATmega");
  SUBDEVICE("_Bridge");
  HADEVICE_BUTTON;
  HADEVICE_BUTTON_CLASS("restart");
  HADEVICE_BUTTON_COMMAND("A");
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

  HARESET;
  KEY("Restart_P1P2MQTT_ESP");
  SUBDEVICE("_Bridge");
  HADEVICE_BUTTON;
  HADEVICE_BUTTON_CLASS("restart");
  HADEVICE_BUTTON_COMMAND("D0");
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

  HARESET;
  KEY("MQTT_Rebuild");
  SUBDEVICE("_Bridge");
  HADEVICE_BUTTON;
  HADEVICE_BUTTON_CLASS("restart");
  HADEVICE_BUTTON_COMMAND("D3");
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

  HARESET;
  KEY("MQTT_Delete_Own_Rebuild");
  SUBDEVICE("_Setup");
  HADEVICE_BUTTON;
  HADEVICE_BUTTON_CLASS("update");
  HADEVICE_BUTTON_COMMAND("D12");
  HADEVICE_AVAILABILITY("S\/9\/ESP_Throttling", 0, 1);
  HADEVICE_AVAILABILITY("A\/9\/HA_Setup", 1, 0);
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

  HARESET;
  KEY("MQTT_Delete_All_Rebuild");
  SUBDEVICE("_Setup");
  HADEVICE_BUTTON;
  HADEVICE_BUTTON_CLASS("update");
  HADEVICE_BUTTON_COMMAND("D14");
  HADEVICE_AVAILABILITY("S\/9\/ESP_Throttling", 0, 1);
  HADEVICE_AVAILABILITY("A\/9\/HA_Setup", 1, 0);
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

  HARESET;
  KEY("EEPROM_ESP_Save_Changes");
  SUBDEVICE("_Bridge");
  HADEVICE_BUTTON;
  HADEVICE_BUTTON_CLASS("update");
  HADEVICE_BUTTON_COMMAND("D5");
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

  HARESET;
  KEY("EEPROM_ESP_Undo_Changes");
  SUBDEVICE("_Bridge");
  HADEVICE_BUTTON;
  HADEVICE_BUTTON_CLASS("restart");
  HADEVICE_BUTTON_COMMAND("D6");
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

  HARESET;
  KEY("Factory_Reset_ESP_After_Restart");
  SUBDEVICE("_Setup");
  HADEVICE_BUTTON;
  HADEVICE_BUTTON_CLASS("update");
  HADEVICE_BUTTON_COMMAND("D7");
  HADEVICE_AVAILABILITY("A\/9\/HA_Setup", 1, 0);
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

  HARESET;
  KEY("Factory_Reset_ESP_Cancel");
  SUBDEVICE("_Setup");
  HADEVICE_BUTTON;
  HADEVICE_BUTTON_CLASS("restart");
  HADEVICE_BUTTON_COMMAND("D8");
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

#ifdef E_SERIES
  HARESET;
  KEY("EEPROM_ESP_Set_Cons_Prod_Counters");
  SUBDEVICE("_Bridge");
  HADEVICE_BUTTON;
  //  HADEVICE_BUTTON_CLASS("*");
  HADEVICE_BUTTON_COMMAND("D13");
  HADEVICE_AVAILABILITY("A\/9\/HA_Setup", 1, 0);
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

  HARESET;
  KEY("Daikin_Restart_Careful");
  SUBDEVICE("_FieldSettings");
  HADEVICE_BUTTON;
  HADEVICE_BUTTON_CLASS("restart");
  HADEVICE_BUTTON_COMMAND("L99");
  HADEVICE_AVAILABILITY("S\/0\/Altherma_On", 0, 1);
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

  HARESET;
  KEY("Daikin_Defrost_Request");
  SUBDEVICE("_Mode");
  HADEVICE_BUTTON;
  //  HADEVICE_BUTTON_CLASS("*");
  HADEVICE_BUTTON_COMMAND("E35003601");
  HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

  HARESET;
  KEY("LCD_On");
  SUBDEVICE("_UI");
  HADEVICE_BUTTON;
  //  HADEVICE_BUTTON_CLASS("*");
  HADEVICE_BUTTON_COMMAND("L81");
  HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
  HADEVICE_AVAILABILITY("S\/2\/Main_LCD_Light", 0, 1); // Only available if main LCD is off
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

  HARESET;
  KEY("LCD_Off");
  SUBDEVICE("_UI");
  HADEVICE_BUTTON;
  //  HADEVICE_BUTTON_CLASS("*");
  HADEVICE_BUTTON_COMMAND("L80");
  HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;


  HARESET;
  KEY("Mode_0_Normal_User");
  SUBDEVICE("_UI");
  HADEVICE_BUTTON;
  //  HADEVICE_BUTTON_CLASS("*");
  HADEVICE_BUTTON_COMMAND("L90");
  HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;


  HARESET;
  KEY("Mode_1_Advanced_User");
  SUBDEVICE("_UI");
  HADEVICE_BUTTON;
  //  HADEVICE_BUTTON_CLASS("*");
  HADEVICE_BUTTON_COMMAND("L91");
  HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

  HARESET;
  KEY("Mode_2_Installer");
  SUBDEVICE("_UI");
  HADEVICE_BUTTON;
  //  HADEVICE_BUTTON_CLASS("*");
  HADEVICE_BUTTON_COMMAND("L92");
  HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
  HADEVICE_AVAILABILITY("S\/2\/Main_Installer", 0, 1); // Only available if main not in installer mode
  if (!publishHomeAssistantConfig(deviceSubName, haDevice, haEntity, haEntityCategory, haPrecision, haButtonDeviceClass, useSrc)) return 0;

#endif /* E_SERIES */

  registerSeenByte(); // do not publish entity, as only configs are needed
}

void resetDataStructures(void) {
  checkSize();
  M.Mversion = M_VERSION;
  M.MdataLength = sizeof(M);
  for (uint16_t j = 0; j < sizePayloadByteVal; j++) {
    M.payloadByteVal[j]  = 0;
  }
  for (uint16_t j = 0; j < sizePayloadByteSeen; j++) {
    M.payloadByteSeen[j] = 0;
  }
#ifdef E_SERIES
  for (byte i = 0; i < 2; i++) {
    for (uint16_t j = 0; j < sizeParamVal; j++)  if ((j < valstart[0x39 - PARAM_TP_START])  || (j >= valstart[0x3A - PARAM_TP_START])  || (i == 1)) M.paramVal[i][j] = 0;
    for (uint16_t j = 0; j < sizeParamSeen; j++) if ((j < seenstart[0x39 - PARAM_TP_START]) || (j >= seenstart[0x3A - PARAM_TP_START]) || (i == 1)) M.paramSeen[i][j >> 3] &= (0xFF ^  (1 << (j & 0x07)));
  }
  for (byte j = 0; j < 36; j++) M.cntByte[j] = 0xFF;
#endif /* E_SERIES */
  for (byte j = 0; j < sizePayloadBitsSeen; j++) {
    M.payloadBitsSeen[j] = 0;
  }
#ifdef SAVESCHEDULE
  byte i;
  uint16 j;
  for (i = 0; i < 2; i++) {
    for (j = 0; j < SCHEDULE_MEM_SIZE; j++) scheduleMem[i][j] = 0;
    for (j = 0; j < SCHEDULE_MEM_SIZE; j++) scheduleMemSeen[i][j] = false;
  }
#endif /* SAVESCHEDULE */
}

#ifdef E_SERIES
void resetFieldSettings(void) {
  for (uint16_t j = valstart[0x39 - PARAM_TP_START]; j < valstart[0x3A - PARAM_TP_START]; j++) M.paramVal[0][j] = 0;
  for (uint16_t j = seenstart[0x39 - PARAM_TP_START]; j < seenstart[0x3A - PARAM_TP_START]; j++) M.paramSeen[0][j >> 3] &= (0xFF ^  (1 << (j & 0x07)));
}
#endif /* E_SERIES */

void initDataRTC() {
  // 4 bytes to test the waters
  M.R.RTCversion = RTC_VERSION;
  M.R.RTCdataLength = sizeof(M.R);
// ----------------------------------------------
  M.R.month = 255;
  M.R.wday = 255;
  M.R.hour = 255;
#ifdef E_SERIES
  M.R.electricityConsumedCompressorHeating = 0;
  M.R.energyProducedCompressorHeating = 0;
  M.R.power1 = 0;
  M.R.power2 = 0;
  M.R.COP_realtime = 0;
  M.R.selectTimeString = 0;
  M.R.climateModeACH = 11;
  M.R.climateModeC = 0;
  M.R.LWT = 0;
  M.R.RWT = 0;
  M.R.MWT = 0;
  M.R.Flow = 0;
  M.R.prevSec = 0xFF;
  M.R.prevMin = 0xFF;
  M.R.compressor = 0;
  M.R.useDHW = 3;  // assume 0x01=tank-installed 0x02=DHW-installed (until field setting detection)
  M.R.controlRTX10 = 0; // default (for me) RT, (Daikin default = 20)
  M.R.numberOfLWTzonesX10 = 0;
  M.R.climateMode = 0; // 0x40=WD 0x00=ABS 0x80/0xC0 = +prog
  M.R.allowModulationX10 = 0;
  M.R.modulationMaxX10 = 0;
  M.R.DHWsetpointMaxX10 = 650; // 750 for tank
  M.R.LWTheatMainMaxX10 = 800;
  M.R.LWTheatMainMinX10 = 250;
  M.R.LWTheatAddMaxX10 = 800;
  M.R.LWTheatAddMinX10 = 250;
  M.R.LWTcoolMainMaxX10 = 220;
  M.R.LWTcoolMainMinX10 =  50;
  M.R.LWTcoolAddMaxX10 = 220;
  M.R.LWTcoolAddMinX10 =  50;
  M.R.roomTempHeatingMinX10 = 120;
  M.R.roomTempHeatingMaxX10 = 300;
  M.R.roomTempCoolingMinX10 = 150;
  M.R.roomTempCoolingMaxX10 = 350;
  M.R.HCmode = 1; // assume heatingMode
  M.R.heatingOnlyX10 = 0; // assume cooling capacity (until field setting detection)
  M.R.defrostActive = 0;
#define heatingMode (M.R.HCmode & 0x01)
#define coolingMode (M.R.HCmode & 0x02)
  M.R.first030 = 0;
  M.R.first030a = 0;
  M.R.overshootX10 = 10;
  M.R.quietMode = 0;
  M.R.quietLevel = 0;
#endif /* E_SERIES */
}

#define EMPTY_PAYLOAD 0xFF

typedef enum {
  HYSTERESIS_TYPE_NONE,
  HYSTERESIS_TYPE_F8_8_BE,
  HYSTERESIS_TYPE_F8_8_LE,
  HYSTERESIS_TYPE_F8S8_LE,
  HYSTERESIS_TYPE_U8,
  HYSTERESIS_TYPE_S8,
  HYSTERESIS_TYPE_U16_LE,
  HYSTERESIS_TYPE_S16_LE,
  HYSTERESIS_TYPE_U16_BE,
  HYSTERESIS_TYPE_S16_BE,
  HYSTERESIS_TYPE_U32_LE,
//  HYSTERESIS_TYPE_S32_LE,
  HYSTERESIS_TYPE_U32_BE
//  HYSTERESIS_TYPE_S32_BE
} hysttype;

#define HYST_RESET     { applyHysteresisType = HYSTERESIS_TYPE_NONE;   applyHysteresis = 0; }
#define HYST_U8(x)     { applyHysteresisType = HYSTERESIS_TYPE_U8;     applyHysteresis = x; }
#define HYST_S8(x)     { applyHysteresisType = HYSTERESIS_TYPE_S8;     applyHysteresis = x; }
#define HYST_U16_LE(x) { applyHysteresisType = HYSTERESIS_TYPE_U16_LE; applyHysteresis = x; }
#define HYST_U16_BE(x) { applyHysteresisType = HYSTERESIS_TYPE_U16_BE; applyHysteresis = x; }
#define HYST_U32_LE(x) { applyHysteresisType = HYSTERESIS_TYPE_U32_LE; applyHysteresis = x; }
#define HYST_U32_BE(x) { applyHysteresisType = HYSTERESIS_TYPE_U32_BE; applyHysteresis = x; }

#define HYST_S16_LE(x) { applyHysteresisType = HYSTERESIS_TYPE_S16_LE; applyHysteresis = x; }
#define HYST_S16_BE(x) { applyHysteresisType = HYSTERESIS_TYPE_S16_BE; applyHysteresis = x; }

#define HYST_F8_8_LE(x)   { applyHysteresisType = HYSTERESIS_TYPE_F8_8_LE;   applyHysteresis = x; }
#define HYST_F8_8_BE(x)   { applyHysteresisType = HYSTERESIS_TYPE_F8_8_BE;   applyHysteresis = x; }
#define HYST_F8S8_LE(x)   { applyHysteresisType = HYSTERESIS_TYPE_F8S8_LE;   applyHysteresis = x; }

void writePseudoSystemPacket0B(void) {
#ifdef E_SERIES
  readHex[0] = 0x40;
  readHex[1] = 0x00;
  readHex[2] = 0x0B;
  uint16_t COPx1000;
  // COP_before_bridge
  if (EE.electricityConsumedCompressorHeating1 > 0) {
    COPx1000 = (1000 * EE.energyProducedCompressorHeating1) / EE.electricityConsumedCompressorHeating1;
  } else {
    COPx1000 = 0;
  }
  readHex[3] = (COPx1000 >> 8) & 0xFF;
  readHex[4] = COPx1000 & 0xFF;
  // COP_after_bridge
  if ((M.R.electricityConsumedCompressorHeating > EE.electricityConsumedCompressorHeating1) && (EE.electricityConsumedCompressorHeating1 > 0)) {
    COPx1000 = (1000 * (M.R.energyProducedCompressorHeating - EE.energyProducedCompressorHeating1)) / (M.R.electricityConsumedCompressorHeating - EE.electricityConsumedCompressorHeating1);
  } else {
    COPx1000 = 0;
  }
  readHex[5] = (COPx1000 >> 8) & 0xFF;
  readHex[6] = COPx1000 & 0xFF;
  // COP_lifetime
  if (M.R.electricityConsumedCompressorHeating > 0) {
    COPx1000 = (1000 * M.R.energyProducedCompressorHeating) / M.R.electricityConsumedCompressorHeating;
  } else {
    COPx1000 = 0;
  }
  readHex[7] = (COPx1000 >> 8) & 0xFF;
  readHex[8] = COPx1000 & 0xFF;

  writePseudoPacket(readHex, 9);
#endif /* E_SERIES */
}

void writePseudoSystemPacket0C(void) {
  readHex[0] = 0x40;
  readHex[1] = 0x00;
  readHex[2] = 0x0C;
#ifdef E_SERIES
  readHex[3]  = (M.R.power1 >> 8) & 0xFF;
  readHex[4]  = M.R.power1 & 0xFF;

  readHex[5]  = (M.R.power2 >> 8) & 0xFF;
  readHex[6]  = M.R.power2 & 0xFF;

  readHex[7]  = (ePower >> 8) & 0xFF;
  readHex[8]  = ePower & 0xFF;

  readHex[9]   = (eTotal >> 24) & 0xFF;
  readHex[10]  = (eTotal >> 16) & 0xFF;
  readHex[11]  = (eTotal >> 8) & 0xFF;
  readHex[12]  = eTotal & 0xFF;

  readHex[13] = (M.R.COP_realtime >> 8) & 0xFF;
  readHex[14] = M.R.COP_realtime & 0xFF;

  readHex[15]  = (bPower >> 8) & 0xFF;
  readHex[16]  = bPower & 0xFF;

  readHex[17]  = (bTotal >> 24) & 0xFF;
  readHex[18]  = (bTotal >> 16) & 0xFF;
  readHex[19]  = (bTotal >> 8) & 0xFF;
  readHex[20]  = bTotal & 0xFF;

  writePseudoPacket(readHex, 21);
#endif /* E_SERIES */
#ifdef W_SERIES
  readHex[3] = (electricityMeterActivePower >> 8) & 0xFF;
  readHex[4] =  electricityMeterActivePower       & 0xFF;
  readHex[5] = (electricityMeterTotal      >> 24) & 0xFF;
  readHex[6] = (electricityMeterTotal      >> 16) & 0xFF;
  readHex[7] = (electricityMeterTotal       >> 8) & 0xFF;
  readHex[8] =  electricityMeterTotal             & 0xFF;
  writePseudoPacket(readHex, 9);
#endif /* W_SERIES */
}

void writePseudoSystemPacket0D(void) {
  readHex[0] = 0x40;
  readHex[1] = 0x00;
  readHex[2] = 0x0D;
#ifdef E_SERIES
  if (M.R.climateModeACH & 0x02) {
    M.R.climateModeC = 1; // cooling (request)
  } else if (M.R.climateModeACH & 0x01) {
    M.R.climateModeC = 0; // heating (request)
  } else if (M.R.climateModeACH & 0x80) {
    M.R.climateModeC = 1; // cooling (actual)
  } else if (M.R.climateModeACH & 0x40) {
    M.R.climateModeC = 0; // heating (actual)
  } else if (M.R.climateModeACH & 0x20) {
    M.R.climateModeC = 1; // cooling (historic)
  } else if (M.R.climateModeACH & 0x10) {
    M.R.climateModeC = 0; // heating (historic)
  } else {
    M.R.climateModeC = 0; // oh ?
  };
  readHex[3] = M.R.climateModeC;
  if (M.R.climateModeACH & 0x04) readHex[3] = 2; // auto
  readHex[4] = M.R.controlRTX10 / 10;
  readHex[5] = M.R.controlRTX10  ? 1 : 0;

  readHex[6] = M.R.allowModulationX10 / 10;
  readHex[7] = M.R.modulationMaxX10 / 10;
  readHex[8] = (M.R.numberOfLWTzonesX10 / 10) + 1;

  readHex[9] = eTotalAvailable;
  readHex[10] = bTotalAvailable;

  readHex[11] = M.R.overshootX10 / 10;
  readHex[12] = M.R.quietMode;
  readHex[13] = M.R.quietLevel;

  writePseudoPacket(readHex, 14); // history currently max 14+3
#endif /* E_SERIES */
}

uint8_t clientPublish(const char* mqtt_value, uint8_t qos) {
  topicCharSpecificSlash('P'); // slash to add entityname
  if (EE.outputMode & 0x0020) clientPublishTelnet(mqttTopic, mqtt_value);
  if (EE.outputMode & 0x0002) return clientPublishMqtt(mqttTopic, qos, MQTT_RETAIN_DATA, mqtt_value);
  return 1;
}

uint32_t u_payloadValue_LE(byte* payloadIndexed, byte valLength) {
  uint32_t v = 0;
  byte* p = payloadIndexed + 1 - valLength;
  for (byte i = valLength; i > 0; i--) { v <<= 8 ; v |= *p++ ; }
  return v;
}

int32_t s_payloadValue_LE(byte* payloadIndexed, byte valLength) {
  uint32_t t = u_payloadValue_LE(payloadIndexed, valLength);
  switch (valLength) {
    case 1 : if (t & 0x80) t |= 0xFFFFFF00; break;
    case 2 : if (t & 0x8000) t |= 0xFFFF0000; break;
    case 3 : if (t & 0x800000) t |= 0xFF000000; break;
    case 4 : break;
  }
  return (int32_t) t;
}

uint32_t u_payloadValue_BE(byte* payloadIndexed, byte valLength) {
  uint32_t v = 0;
  byte* p = payloadIndexed;
  for (byte i = valLength; i > 0; i--) { v <<= 8 ; v |= *p--; }
  return v;
}

// within loop
uint16_t applyHysteresis = 0;
byte applyHysteresisType = 0;

// global variable to maintain info from newCheckPayloadBytesVal to newCheckPayloadBitsVal

#ifdef H_SERIES
byte calculatePti(const byte packetSrc, const byte packetType, byte* payload)
#else /* H_SERIES */
byte calculatePti(const byte packetSrc, const byte packetType)
#endif /* H_SERIES */
{
  byte pti;

#ifdef E_SERIES

  switch (packetType) {
    case PCKTP_START ... 0x15 : pti = packetType - PCKTP_START;
                break;
    case 0x20 : pti = (PCKTP_END - PCKTP_START) + 3;
                break;
    case 0x21 : pti = (PCKTP_END - PCKTP_START) + 4;
                break;
    case 0x30 : pti = (PCKTP_END - PCKTP_START) + 1;
                break;
    case 0x31 : pti = (PCKTP_END - PCKTP_START) + 2;
                break;
    case 0x60 ... 0x8F : pti = 0xFE; // no history, handle as new
                break;
    case 0x90 ... 0x9F : pti = (PCKTP_END - PCKTP_START) + 5 + (packetType - 0x90);
                break;
    default   : pti = 0xFF; break;
  }
  if (pti != 0xFF) switch (packetSrc) {
    case 0x00  : // do nothing
                 break;
    case 0x40  : pti += PCKTP_ARR_BLOCK;
                 break;
    case 0x80  : pti = (PCKTP_ARR_BLOCK << 1); // overrule earlier pti calculation
                 switch (packetType) {
                   case 0x00 ... 0x05 : pti = 0xFE; break;
                   case 0x10 : break;
                   case 0x18 : pti++;
                               break;
                   default :   pti = 0xFF;
                               break;
                 }
                 break;
    default    : // do nothing
                 break;
  }

#elif defined W_SERIES /* *_SERIES */

  switch (packetType) {
    case 0x0C ... 0x0F : pti = packetType - PCKTP_START;
                break;
    default   : pti = 0xFF; break;
  }
  if (pti != 0xFF) switch (packetSrc) {
    case 0x00  : // do nothing
                 break;
    case 0x40  : pti += PCKTP_ARR_BLOCK;
                 break;
    default    : // do nothing
                 break;
  }

#elif defined F_SERIES /* *_SERIES */

   switch (packetType) {
    case 0x08 ... 0x12 : pti = packetType - PCKTP_START; break;        // 0 .. 9  -> 0..10
    case          0x15 : pti = 0x13 - PCKTP_START; break;              //       11
    case          0x17 : pti = 0x14 - PCKTP_START; break;              //       12
    case          0x18 : pti = 0x15 - PCKTP_START; break;              // 10 -> 13 // +3
    case          0x19 : pti = 0x16 - PCKTP_START; break;              //       14
    case          0x1F : pti = 0x17 - PCKTP_START; break;              // 11 // +4 -> 15
    case          0x20 : pti = 0x18 - PCKTP_START; break;              // 12 -> 16
    case          0x21 : pti = 0x19 - PCKTP_START; break;              // 13 -> 17
    case 0x30 ... 0x3F : pti = packetType - 30; break;                 // 14 .. 29 +4 -> 18..33
    case          0x80 : pti = 34; break;                              // 30 -> 34
    case          0xA1 : pti = 35; break;                              //       35
    case          0xA3 : pti = 36; break;                              // 31 -> 36
    case          0xB1 : pti = 37; break;                              //       37
    default            : pti = 0xFF; break;                            // no history
  }
  if (pti != 0xFF) switch (packetSrc) {
    case 0x00  : // do nothing
                 break;
    case 0x40  : pti += PCKTP_ARR_BLOCK;
                 break;
    case 0x80  : pti = (PCKTP_ARR_BLOCK << 1); // overrule earlier pti calculation
                 switch (packetType) {
                   case 0x18 : break;
                   default :   pti = 0xFF;
                               break;
                 }
                 break;
    default    : // do nothing
                 break;
  }

#elif defined H_SERIES /* *_SERIES */

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
    // new in v0.9.45
    case 0x1909 : pti = 0x21; break;
    case 0x190A : pti = 0x22; break;
    case 0x1910 : pti = 0x23; break;
    case 0x230A : pti = 0x24; break;
    case 0x231C : pti = 0x25; break;
    case 0x2909 : pti = 0x26; break;
    case 0x4909 : pti = 0x27; break;
    case 0x4923 : pti = 0x28; break;
    case 0x4930 : pti = 0x29; break;
    // new in v0.9.47
    case 0x211C : pti = 0x2A; break;
    // new in v0.9.55rc1
    case 0x792E : pti = 0x2B; break;
    case 0xFF23 : pti = 0x2C; break;
    default     : break;
  }

#elif defined MHI_SERIES /* *_SERIES */

  switch (packetSrc) {
    // MHI packets
    case 0x00 : pti = 0x00; break;
    case 0x01 : pti = 0x01; break;
    case 0x02 ... 0x07 : pti = 0x02; break;
    case 0x80 : pti = 0x03; break;
    case 0x81 : pti = 0x04; break;
    default : pti = 0xFF; break;
  }

#endif /* *_SERIES */
  return pti;
}

uint16_t newCheckPayloadBytesVal(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, byte length, bool byteBasis) {
// returns (8-bit pubHa, 8-bit pubEntity)
// returns (as pubHa) if homeassistant config msg should be published
// returns (as pubEntity) which bits are new or, if (EE.outputFilter <= maxOutFilter), have been changed subject to an optional hysteresis check

  if (payloadIndex == EMPTY_PAYLOAD) {
    return 0;
  }

  byte newBits = 0;
  byte changedBits = (EE.outputFilter ? 0x00 : 0xFF);

#ifdef H_SERIES
  byte pti = calculatePti(packetSrc, packetType, payload); // pti index for nr_bytes/bytestart arrays
#else /* H_SERIES */
  byte pti = calculatePti(packetSrc, packetType); // pti index for nr_bytes/bytestart arrays
#endif /* H_SERIES */

  if ((pti < 0xFE) && (pti >= PCKTP_ARR_SZ)) {
    printfTopicS("pti 0x%02X >= 0x%02X for Src 0x%02X Tp 0x%02X", pti, PCKTP_ARR_SZ, packetSrc, packetType);
    return 0;
  }
  if (pti == 0xFE) return 1; // always handle as new, but not for HA, meant for 0x80 boot messages
#ifdef MHI_SERIES
  if (pti == 0xFF) {
    printfTopicS("Unknown packetSrc 0x%02X", packetSrc);
    return 0;
  }
  if (length && (payloadIndex >= nr_bytes[pti])) {
    pti = 0xFF; // for BitsVal
#ifdef W_SERIES
    if (packetType != 0x0C)
#endif /* W_SERIES */
    printfTopicS("Warning: payloadIndex %i > expected %i for Src 0x%02X", payloadIndex, nr_bytes[pti], packetSrc);
    return 0;
  }
#endif /* MHI_SERIES */
#ifdef E_SERIES
  if (packetType == 0xB8) {
    // Handle 0xB8 history
    pi2 = payloadIndex;
    if (pi2 > 13) pi2 += 2;
    pi2 >>= 2; // if payloadIndex is 3, 6, 9, 12, 15, 18, pi2 is now 0..5
    pi2 += 6 * payload[0];
    if (M.cntByte[pi2] & 0x80) {
      newBits = 1;
      // MQTT discovery
      topicCharSpecific('P');
      snprintf_P(extraAvailabilityString, EXTRA_AVAILABILITY_STRING_LEN, PSTR(",{\"topic\":\"%s\/A\/8\/Counter_Request_Function\",\"pl_avail\":1,\"pl_not_avail\":0}")
      , mqttTopic // (P)
      );
    }
    // it's a 3-byte (slow) counter, so sufficient if we look at the 7 least significant bits LSB byte (LE); bit 7 = "!seen"
    if ((M.cntByte[pi2] != (payload[payloadIndex] & 0x7F)) && (EE.outputFilter <= maxOutputFilter)) newBits = 1;
    return ((M.cntByte[pi2] & 0x80) << 1) | newBits;
  }
#endif /* E_SERIES */
#ifndef MHI_SERIES
  if (pti == 0xFF) {
    printfTopicS("Unknown packetSrc 0x%02X packetType 0x%02X index %i", packetSrc, packetType, payloadIndex);
    return 0;
  }
  if (length && (payloadIndex >= nr_bytes[pti])) {
    printfTopicS("Warning: payloadIndex %i > expected %i for Src 0x%02X Type 0x%02X", payloadIndex, nr_bytes[pti], packetSrc, packetType);
    pti = 0xFF; // for BitsVal
    return 0;
  }
#endif /* !MHI_SERIES */
  if (payloadIndex + 1 < length) {
    // Warning: payloadIndex + 1 < length
    printfTopicS("Warning: payloadIndex + 1 < length");
    return 0;
  }
  bool skipHysteresis = false;
  pi2 = bytestart[pti] + payloadIndex;

  if (applyHysteresisType && (pi2 < sizePayloadByteVal) && (M.payloadByteSeen[pi2 >> 3] && (1 << (pi2 & 0x07)))) {
    switch (applyHysteresisType) {
      case HYSTERESIS_TYPE_F8_8_BE : { int16_t oldValue = ((int8_t) M.payloadByteVal[pi2]) * 256 + M.payloadByteVal[pi2 - 1];
                                       int16_t newValue = ((int8_t) payload[payloadIndex]) * 256 + payload[payloadIndex - 1];
                                       skipHysteresis = ((newValue <= oldValue + (int16_t) applyHysteresis) && (newValue + (int16_t) applyHysteresis >= oldValue));
                                     }
                                     break;
      case HYSTERESIS_TYPE_F8_8_LE : { int16_t oldValue = ((int8_t) M.payloadByteVal[pi2 - 1]) * 256 + M.payloadByteVal[pi2];
                                       int16_t newValue = ((int8_t) payload[payloadIndex - 1]) * 256 + payload[payloadIndex];
                                       skipHysteresis = ((newValue <= oldValue + (int16_t) applyHysteresis) && (newValue + (int16_t) applyHysteresis >= oldValue));
                                     }
                                     break;
      case HYSTERESIS_TYPE_F8S8_LE : { int16_t oldValue = ((int8_t) M.payloadByteVal[pi2 - 1]) * 10 + M.payloadByteVal[pi2];
                                       int16_t newValue = ((int8_t) payload[payloadIndex - 1]) * 10 + payload[payloadIndex];
                                       skipHysteresis = ((newValue <= oldValue + (int16_t) applyHysteresis) && (newValue + (int16_t) applyHysteresis >= oldValue));
                                     }
                                     break;
      case HYSTERESIS_TYPE_U16_BE:   { uint16_t oldValue = u_payloadValue_BE(M.payloadByteVal + pi2, 2);
                                       uint16_t newValue = u_payloadValue_BE(payload + payloadIndex, 2);
                                       skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue + applyHysteresis >= oldValue));
                                     }
                                     break;
      case HYSTERESIS_TYPE_U32_BE:   { uint32_t oldValue = u_payloadValue_BE(M.payloadByteVal + pi2, 4);
                                       uint32_t newValue = u_payloadValue_BE(payload + payloadIndex, 4);
                                       skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue + applyHysteresis >= oldValue));
                                     }
                                     break;
      case HYSTERESIS_TYPE_S16_BE:   { int16_t oldValue = (uint16_t) u_payloadValue_BE(M.payloadByteVal + pi2, 2);
                                       int16_t newValue = (uint16_t) u_payloadValue_BE(payload + payloadIndex, 2);
                                       skipHysteresis = ((newValue <= oldValue + (int16_t) applyHysteresis) && (newValue + (int16_t) applyHysteresis >= oldValue));
                                     }
                                     break;
//    case HYSTERESIS_TYPE_S32_BE:   { int32_t oldValue = u_payloadValue_BE(M.payloadByteVal + pi2, 4);
//                                     int32_t newValue = u_payloadValue_BE(payload + payloadIndex, 4);
//                                     skipHysteresis = ((newValue <= oldValue + (int16_t) applyHysteresis) && (newValue + (int16_t) applyHysteresis >= oldValue));
//                                   }
//                                   break;
      case HYSTERESIS_TYPE_U8:       { uint8_t oldValue = u_payloadValue_LE(M.payloadByteVal + pi2, 1);
                                       uint8_t newValue = u_payloadValue_LE(payload + payloadIndex, 1);
                                       skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue + applyHysteresis >= oldValue));
                                     }
                                     break;
      case HYSTERESIS_TYPE_S8:       { int8_t oldValue = (uint8_t) u_payloadValue_LE(M.payloadByteVal + pi2, 1);
                                       int8_t newValue = (uint8_t) u_payloadValue_LE(payload + payloadIndex, 1);
                                       skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue + applyHysteresis >= oldValue));
                                     }
                                     break;
      case HYSTERESIS_TYPE_U16_LE:   { uint16_t oldValue = u_payloadValue_LE(M.payloadByteVal + pi2, 2);
                                       uint16_t newValue = u_payloadValue_LE(payload + payloadIndex, 2);
                                       skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue + applyHysteresis >= oldValue));
                                     }
                                     break;
      case HYSTERESIS_TYPE_U32_LE:   { uint32_t oldValue = u_payloadValue_LE(M.payloadByteVal + pi2, 4);
                                       uint32_t newValue = u_payloadValue_LE(payload + payloadIndex, 4);
                                       skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue + applyHysteresis >= oldValue));
                                     }
                                     break;
      case HYSTERESIS_TYPE_S16_LE:   { int16_t oldValue = (uint16_t) u_payloadValue_LE(M.payloadByteVal + pi2, 2);
                                       int16_t newValue = (uint16_t) u_payloadValue_LE(payload + payloadIndex, 2);
                                       skipHysteresis = ((newValue <= oldValue + (int16_t) applyHysteresis) && (newValue + (int16_t) applyHysteresis >= oldValue));
                                     }
                                     break;
//    case HYSTERESIS_TYPE_S32_LE:   { int32_t oldValue = u_payloadValue_LE(M.payloadByteVal + pi2, 2);
//                                     int32_t newValue = u_payloadValue_LE(payload + payloadIndex, 2);
//                                     skipHysteresis = ((newValue <= oldValue + (int16_t) applyHysteresis) && (newValue + (int16_t) applyHysteresis >= oldValue));
//                                   }
//                                   break;
      default :                      skipHysteresis = false;
                                     break;
    }
  }

  if (byteBasis) {
    newBits = (M.payloadByteSeen[pi2 >> 3] & (1 << (pi2 & 0x07))) ? 0 : 1;
  } else {
    if (bcnt >= sizePayloadBitsSeen) {
      printfTopicS("bcnt %i > size ", bcnt);
      return 0;
    }
    newBits =  M.payloadBitsSeen[bcnt] ^ 0xFF;
  }

  uint16_t pi2i = pi2;
  if (length) {
    for (int8_t i = payloadIndex; i + length > payloadIndex; i--) {
      if (pi2i >= sizePayloadByteVal) {
#ifdef MHI_SERIES
        printfTopicS("Warning: pi2i %i > sizePayloadByteVal, Src 0x%02X Index %i payloadIndex %i ", pi2i, packetSrc, i, payloadIndex);
#else /* MHI_SERIES */
        printfTopicS("Warning: pi2i %i > sizePayloadByteVal, Src 0x%02X Tp 0x%02X Index %i payloadIndex %i ", pi2i, packetSrc, packetType, i, payloadIndex);
#endif /* MHI_SERIES */
        return 0;
      }
      if ((EE.outputFilter <= maxOutputFilter) && !skipHysteresis) changedBits |= (M.payloadByteVal[pi2i] ^ payload[i]);
      pi2i--;
    }
  } else {
    if (EE.outputFilter <= maxOutputFilter) changedBits = 0xFF;
  }
  return (newBits << 8) | (newBits | changedBits);
}

#ifndef H_SERIES
void registerUnseenByte(byte packetSrc, byte packetType, byte payloadIndex)
{

  byte pti = calculatePti(packetSrc, packetType);
  if (pti == 0xFF) return;
  uint16_t pi2 = bytestart[pti] + payloadIndex;

  M.payloadByteSeen[pi2 >> 3] &= ~(1 << (pi2 & 0x07));
}
#endif /* !H_SERIES */

/* not needed (yet)
void registerUnseenBit(byte bcnt, byte bitNr)
{
  if (bcnt >= sizePayloadBitsSeen) {
    printfTopicS("bcnt %i > size ", bcnt);
    return;
  }
  M.payloadBitsSeen[bcnt] &= (0xFF ^ (1 << bitNr));
}
*/

#ifdef MHI_SERIES
uint8_t publishEntityByte(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, const char* mqtt_value, byte length) {
  if (clientPublish(mqtt_value, haQos) && (pi2 >= 0)) {
    M.payloadByteSeen[pi2 >> 3] |= (1 << (pi2 & 0x07));
    uint16_t pi2i = pi2;
    for (int8_t i = payloadIndex; i + length > payloadIndex; i--) {
      if (pi2i >= sizePayloadByteVal) {
        printfTopicS("Warning: pi2i %i > sizePayloadByteVal, Src 0x%02X Index %i payloadIndex %i", pi2i, packetSrc, i, payloadIndex);
        return 0;
      }
      M.payloadByteVal[pi2i] = payload[i];
      pi2i--;
    }
  } // else retry later
  return 0;
}
#else /* MHI_SERIES */
uint8_t publishEntityByte(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, const char* mqtt_value, byte length) {
  if (clientPublish(mqtt_value, haQos) && (pi2 >= 0)) {
#ifdef E_SERIES
    if (packetType == 0xB8) {
      M.cntByte[pi2] = payload[payloadIndex] & 0x7F;
      return 0;
    }
#endif /* E_SERIES */
    M.payloadByteSeen[pi2 >> 3] |= (1 << (pi2 & 0x07));
    uint16_t pi2i = pi2;
    for (int8_t i = payloadIndex; i + length > payloadIndex; i--) {
      if (pi2i >= sizePayloadByteVal) {
        printfTopicS("Warning: pi2i %i > sizePayloadByteVal, Src 0x%02X Tp 0x%02X Index %i payloadIndex %i", pi2i, packetSrc, packetType, i, payloadIndex);
        return 0;
      }
      M.payloadByteVal[pi2i] = payload[i];
      pi2i--;
    }
  } // else retry later
  return 0;
}
#endif /* MHI_SERIES */

uint8_t publishEntityBits(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, const char* mqtt_value) {
  if (clientPublish(mqtt_value, haQos)) {
    if (bcnt >= sizePayloadBitsSeen) {
      printfTopicS("bcnt %i > size ", bcnt);
      return 0;
    }
    M.payloadBitsSeen[bcnt] |= bitsMask;
    M.payloadByteVal[pi2] &= (0xFF ^ bitsMask);
    M.payloadByteVal[pi2] |= payload[payloadIndex] & bitsMask;
    pubHaEntityBits &= (0xFFFF ^ (bitsMask | (bitsMask << 8)));
  } // else retry later
  return 0;
}

uint8_t doNotPublishEntityBits(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, const char* mqtt_value) {
  if (bcnt >= sizePayloadBitsSeen) {
    printfTopicS("bcnt %i > size ", bcnt);
    return 0;
  }
  M.payloadBitsSeen[bcnt] |= bitsMask;
  M.payloadByteVal[pi2] &= (0xFF ^ bitsMask);
  M.payloadByteVal[pi2] |= payload[payloadIndex] & bitsMask;
  pubHaEntityBits &= (0xFFFF ^ (bitsMask | (bitsMask << 8)));
  return 0;
}

#ifdef E_SERIES
// global variables to maintain info from check* to pub*
uint16_t ptbs = 0;
byte ppti = 0;
byte ppts = 0;

uint16_t newCheckParamVal(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, byte paramValLength) {
// returns whether paramNr in paramPacketType has new value M.paramValue
// (also returns true if EE.outputFilter==0)
  bool pubParam = (EE.outputFilter == 0);
  ppts = (paramSrc >> 6) & 0x01; // 0x00 -> 0; 0x40 -> 1; others not used
  if (paramSrc & 0xBF) printfTopicS("UNEXPECTED paramSrc 0x%02X in newCHeckParamVal", paramSrc);
  ppti = paramPacketType - PARAM_TP_START;
  uint16_t ptbv = valstart[ppti] + paramNr * parnr_bytes[ppti];
  ptbs = seenstart[ppti] + paramNr;

  if (paramNr == 0xFFFF) {
    return 0;
  }
  if (paramNr >= nr_params[ppti]) {
    printfTopicS("paramNr 0x%04X >= expected nr_params 0x%04X for packetType 0x%02X", paramNr, nr_params[ppti], paramPacketType);
    return 0;
  }
  if (paramPacketType < PARAM_TP_START) {
    printfTopicS("paramTp 0x%04X < PARAM_TP_START", paramPacketType);
    return 0;
  }
  if (paramPacketType > PARAM_TP_END) {
    printfTopicS("paramTp 0x%04X > PARAM_TP_END", paramPacketType);
    return 0;
  }
  if (M.paramSeen[ppts][ptbs >> 3] & (1 << (ptbs & 0x07))) {
    bool skipHysteresis = false;

    switch (applyHysteresisType) {
      case HYSTERESIS_TYPE_U16_BE:   {
                                       uint16_t oldValue = u_payloadValue_BE(M.paramVal[ppts] + ptbv + 1, 2);
                                       uint16_t newValue = u_payloadValue_BE(payload + payloadIndex, 2);
                                       skipHysteresis = ((newValue <= oldValue + applyHysteresis) && (newValue + applyHysteresis >= oldValue));
                                     }
                                     break;
      case HYSTERESIS_TYPE_S16_BE:   {
                                       int16_t oldValue = (uint16_t) u_payloadValue_BE(M.paramVal[ppts] + ptbv + 1, 2);
                                       int16_t newValue = (uint16_t) u_payloadValue_BE(payload + payloadIndex, 2);
                                       skipHysteresis = ((newValue <= oldValue + (int16_t) applyHysteresis) && (newValue + (int16_t) applyHysteresis >= oldValue));
                                     }
                                     break;
      default :                      skipHysteresis = false;
                                     break;
    }
    for (byte i = payloadIndex + 1 - paramValLength; i <= payloadIndex - ((paramPacketType == 0x39) ? /* 3 */ 0 : 0); i++) {        // 0x39 is field settings so only first byte changes, but (new fs strategy:) check all
      if (M.paramVal[ppts][ptbv++] != payload[i]) pubParam = (EE.outputFilter <= maxOutputFilter) && !skipHysteresis;
    }
  } else {
    // first time for this param
    // only available in bridge-control-function mode, so include avaibility check:
    if (paramPacketType != 0x39) HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
    pubParam = 1; // first time value shown irrespective of EE.outputFilter
  }

  return ((((M.paramSeen[ppts][ptbs >> 3] >> (ptbs & 0x07)) & 0x01) ^ 0x01) << 8) | pubParam;
}

void registerSeenParam() {
  M.paramSeen[ppts][ptbs >> 3] |= (1 << (ptbs & 0x07));
  return;
}

uint8_t publishEntityParam(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, const char* mqtt_value, byte paramValLength) {
  uint16_t ptbv = valstart[ppti] + paramNr * parnr_bytes[ppti];
  if (clientPublish(mqtt_value, haQos)) {
    for (byte i = payloadIndex + 1 - paramValLength; i <= payloadIndex /* - ((paramPacketType == 0x39) ? 3 : 0) */; i++) M.paramVal[ppts][ptbv++] = payload[i];
    M.paramSeen[ppts][ptbs >> 3] |= (1 << (ptbs & 0x07));
    return 1;
  } else {
    // retry later
    return 0;
  }
}
#endif /* E_SERIES */

// these conversion functions from payload to (mqtt) value look at the current and often also one or more past byte(s) in the byte array

// hex (1..4 bytes), LE

uint8_t value_u_hex_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, byte length) {
  byte* p = payload + payloadIndex - length;
  snprintf(mqtt_value, MQTT_VALUE_LEN - 2, "0x");
  for (byte i = 1; i <= length; i++) snprintf(mqtt_value + i * 2, MQTT_VALUE_LEN - i * 2, "%02X", *++p);
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, length);
}

// unsigned integers, LE

uint8_t value_u0(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  registerSeenByte(); // do not publish entity, as only config is needed
  return 0;
}

uint8_t value_u_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, byte length) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", u_payloadValue_LE(payload + payloadIndex, length));
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, length);
}

// signed integers, LE

uint8_t value_s_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, byte length) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", s_payloadValue_LE(payload + payloadIndex, length));
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, length);
}

uint8_t value_s8(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", (int8_t) payload[payloadIndex]);
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, 1);
}

uint8_t value_s16_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", (int16_t) (uint16_t) u_payloadValue_LE(payload + payloadIndex , 2));
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, 2);
}

uint8_t value_sdiv100_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, byte length) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.2f", s_payloadValue_LE(payload + payloadIndex, length) * 0.01);
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, length);
}

uint8_t value_sdiv10_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, byte length) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", s_payloadValue_LE(payload + payloadIndex, length) * 0.1);
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, length);
}

uint8_t value_udiv10(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, byte length) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", u_payloadValue_LE(payload + payloadIndex, length) * 0.1);
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, length);
}

uint8_t value_udiv1000_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, byte length) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.3f", u_payloadValue_LE(payload + payloadIndex, length) * 0.001);
  return  publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, length);
}

// single bit

bool FN_flag8(uint8_t b, uint8_t n)  { return (b >> n) & 0x01; }

uint8_t value_flag8(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, byte bitNr, bool invertValue=false) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", invertValue ^ FN_flag8(payload[payloadIndex], bitNr));
  return publishEntityBits(packetSrc, packetType, payloadIndex, payload, mqtt_value);
}

// multiple bits

byte FN_bits(uint8_t b, uint8_t n1, uint8_t n2)  { return (b >> n1) & ((0x01 << (n2 - n1 + 1)) - 1); }

uint8_t value_bits(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, byte bitNr1, byte bitNr2) {
  // use bits bitNr1 .. bitNr2
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", FN_bits(payload[payloadIndex], bitNr1, bitNr2));
  return publishEntityBits(packetSrc, packetType, payloadIndex, payload, mqtt_value);
}

uint8_t unknownBit(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, byte bitNr) {
  snprintf(mqttTopic + mqttTopicPrefixLength, MQTT_TOPIC_LEN, "PacketSrc_0x%02X_Type_0x%02X_Byte_%i_Bit_%i", packetSrc, packetType, payloadIndex, bitNr);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", FN_flag8(payload[payloadIndex], bitNr));
  return publishEntityBits(packetSrc, packetType, payloadIndex, payload, mqtt_value);
}

// multiple bits, do not publish

uint8_t value_bits_nopub(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, byte bitNr1, byte bitNr2) {
  // use bits bitNr1 .. bitNr2
  return doNotPublishEntityBits(packetSrc, packetType, payloadIndex, payload, mqtt_value);
}

// misc

uint8_t unknownByte(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  snprintf(mqttTopic + mqttTopicPrefixLength, MQTT_TOPIC_LEN, "PacketSrc_0x%02X_Type_0x%02X_Byte_%i", packetSrc, packetType, payloadIndex);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "0x%02X %i", payload[payloadIndex], payload[payloadIndex]);
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, 1);
}

uint8_t value_u8_add2k(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", payload[payloadIndex] + 2000);
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, 1);
}

int8_t FN_s4abs1c(uint8_t *b)        { int8_t c = b[0] & 0x0F; if (b[0] & 0x10) return -c; else return c; }

uint8_t value_s4abs1c(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", FN_s4abs1c(&payload[payloadIndex]));
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, 1);
}

float FN_u8div10(uint8_t *b)     { return (b[0] * 0.1);}

uint8_t value_u8div10(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", FN_u8div10(&payload[payloadIndex]));
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, 1);
}

float FN_u16div10_LE(uint8_t *b)     { if (b[-1] == 0xFF) return 0; else return (b[0] * 0.1 + b[-1] * 25.6);}

float FN_u16div100_LE(uint8_t *b)     { if (b[-1] == 0xFF) return 0; else return (b[0] * 0.01 + b[-1] * 2.56);}

uint8_t value_u16div10_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", FN_u16div10_LE(&payload[payloadIndex]));
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, 2);
}

uint8_t value_u16div100_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.2f", FN_u16div100_LE(&payload[payloadIndex]));
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, 2);
}

uint8_t value_header(byte packetSrc, byte packetType, char* mqtt_value) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "Empty_Payload_%02X00%02X", packetSrc, packetType);
  clientPublish(mqtt_value, haQos);
  return 0;
}

#ifdef MHI_SERIES
uint8_t value_mode(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, int v) {
  switch (v) {
    case  0 : snprintf(mqtt_value, MQTT_VALUE_LEN, "Auto"); break;
    case  1 : snprintf(mqtt_value, MQTT_VALUE_LEN, "Dry"); break;
    case  2 : snprintf(mqtt_value, MQTT_VALUE_LEN, "Cool"); break;
    case  3 : snprintf(mqtt_value, MQTT_VALUE_LEN, "Fan"); break;
    case  4 : snprintf(mqtt_value, MQTT_VALUE_LEN, "Heat"); break;
    default : snprintf(mqtt_value, MQTT_VALUE_LEN, "?"); break;
  }
  return publishEntityBits(packetSrc, packetType, payloadIndex, payload, mqtt_value);
}
#endif /* MHI_SERIES */

uint8_t value_s(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, int v, int length = 0) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", v);
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, length);
}

uint8_t value_s_bit(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, byte v, byte bitNr) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", v);
  return publishEntityBits(packetSrc, packetType, payloadIndex, payload, mqtt_value);
}

uint8_t value_s_nosave(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, int v) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", v);
  clientPublish(mqtt_value, haQos);
}

uint8_t value_f(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, float v, int length = 0) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.3f", v); // make resolution depend on PRECISION?
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, length);
}

// 16 bit fixed point reals

float FN_f8_8_BE(uint8_t *b)                       { return (((int8_t) b[0]) + (b[-1] * 1.0 / 256)); }
float FN_f8_8_LE(uint8_t *b)                       { return (((int8_t) b[-1]) + (b[0] * 1.0 / 256)); }
float FN_s_f8_8_LE(uint8_t *b)  { return (b[-2] ? -1 : 1) * (((int8_t) b[-1]) + (b[0] * 1.0 / 256)); }

uint8_t value_f8_8_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.3f", FN_f8_8_LE(&payload[payloadIndex]));
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, 2);
}

uint8_t value_s_f8_8_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.3f", FN_s_f8_8_LE(&payload[payloadIndex]));
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, 3);
}

uint8_t value_f8_8_BE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.3f", FN_f8_8_BE(&payload[payloadIndex]));
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, 2);
}

float FN_f8s8_LE(uint8_t *b)            { return ((float)(int8_t) b[-1]) + ((float) b[0]) /  10;}

uint8_t value_f8s8_LE(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", FN_f8s8_LE(&payload[payloadIndex]));
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, 2);
}

uint8_t value_textString(char* mqtt_value, char* textString) {
  registerSeenByte(); // record that config was published
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%s", textString);
  clientPublish(mqtt_value, haQos);
  return 0;
}

uint8_t value_textStringOnce(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value, char* textString) {
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%s", textString);
  return publishEntityByte(packetSrc, packetType, payloadIndex, payload, mqtt_value, 1);
}

#ifdef E_SERIES
// Parameters, unsigned, BE

uint8_t param_value_u_BE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_value, byte paramValLength) {
  uint32_t v = u_payloadValue_BE(payload + payloadIndex, paramValLength);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", v);
  return publishEntityParam(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);
}

// Parameters, unsigned, LE

uint8_t param_value_u_LE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_value, byte paramValLength) {
  uint32_t v = u_payloadValue_BE(payload + payloadIndex, paramValLength);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%u", v);
  return publishEntityParam(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);
}

// Parameters, signed, LE

uint8_t param_value_s_LE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_value, byte paramValLength) {
  if (paramValLength != 2) {
    printfTopicS("Only s16_LE supported, not %i bit", 8*paramValLength);
    return 0;
  }
  int16_t v = (uint16_t) u_payloadValue_BE(payload + payloadIndex, paramValLength);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%i", v);
  return publishEntityParam(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);
}

// u16div10, s16div10 BE

uint8_t param_value_u16div10_BE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_value, byte paramValLength) {
  // assuming paramValLength = 2
  if (paramValLength != 2) {
    printfTopicS("Only u16div10_BE supported, not %i bit", 8*paramValLength);
    return 0;
  }
  uint16_t v = u_payloadValue_BE(payload + payloadIndex, paramValLength);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", v * 0.1);
  return publishEntityParam(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);
}

uint8_t param_value_u32div100_BE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_value, byte paramValLength) {
  // assuming paramValLength = 4
  if (paramValLength != 4) {
    printfTopicS("Only u32_BE supported, not %i bit", 8*paramValLength);
    return 0;
  }
  uint16_t v = u_payloadValue_BE(payload + payloadIndex, paramValLength);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.2f", v * 0.01);
  return publishEntityParam(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);
}

uint8_t param_value_s16div10_BE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_value, byte paramValLength) {
  // assuming paramValLength = 2
  if (paramValLength != 2) {
    printfTopicS("Only s16div10_BE supported, not %i bit", 8*paramValLength);
    return 0;
  }
  int16_t v = (uint16_t) u_payloadValue_BE(payload + payloadIndex, paramValLength);
  snprintf(mqtt_value, MQTT_VALUE_LEN, "%1.1f", v * 0.1);
  return publishEntityParam(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);
}

// misc, hex output, BE

byte unknownParam_BE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_value, byte paramValLength) {
  if (!outputUnknown) return 0;
  snprintf(mqttTopic + mqttTopicPrefixLength, MQTT_TOPIC_LEN, "ParamSrc_0x%02X_Type_0x%02X_Nr_0x%04X", paramSrc, paramPacketType, paramNr);
  switch (paramValLength) {
    case 1 : snprintf(mqtt_value, MQTT_VALUE_LEN, "0x%02X", payload[payloadIndex]); break;
    case 2 : snprintf(mqtt_value, MQTT_VALUE_LEN, "0x%02X%02X", payload[payloadIndex], payload[payloadIndex - 1]); break;
    case 3 : snprintf(mqtt_value, MQTT_VALUE_LEN, "0x%02X%02X%02X", payload[payloadIndex], payload[payloadIndex - 1], payload[payloadIndex - 2]); break;
    case 4 : snprintf(mqtt_value, MQTT_VALUE_LEN, "0x%02X%02X%02X%02X", payload[payloadIndex], payload[payloadIndex - 1], payload[payloadIndex - 2], payload[payloadIndex - 3]); break;
    default : return 0;
  }
  return publishEntityParam(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);
}

byte param_value_hex_BE(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_value, byte paramValLength) {
  if (!outputUnknown) return 0;
  switch (paramValLength) {
    case 1 : snprintf(mqtt_value, MQTT_VALUE_LEN, "0x%02X", payload[payloadIndex]); break;
    case 2 : snprintf(mqtt_value, MQTT_VALUE_LEN, "0x%02X%02X", payload[payloadIndex], payload[payloadIndex - 1]); break;
    case 3 : snprintf(mqtt_value, MQTT_VALUE_LEN, "0x%02X%02X%02X", payload[payloadIndex], payload[payloadIndex - 1], payload[payloadIndex - 2]); break;
    case 4 : snprintf(mqtt_value, MQTT_VALUE_LEN, "0x%02X%02X%02X%02X", payload[payloadIndex], payload[payloadIndex - 1], payload[payloadIndex - 2], payload[payloadIndex - 3]); break;
  }
  return publishEntityParam(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);
}

#define UNSEEN_BYTE_00_10_19_CLIMATE_DHW registerUnseenByte(0x00, 0x10, 19);
#define UNSEEN_BYTE_00_30_0_CLIMATE_ROOM_HEATING registerUnseenByte(0x00, 0x30,  0);
#define UNSEEN_BYTE_00_30_1_CLIMATE_ROOM_COOLING registerUnseenByte(0x00, 0x30,  1);
#define UNSEEN_BYTE_00_30_2_CLIMATE_LWT_HEATING registerUnseenByte(0x00, 0x30,  2);
#define UNSEEN_BYTE_00_30_3_CLIMATE_LWT_COOLING registerUnseenByte(0x00, 0x30,  3);
#define UNSEEN_BYTE_00_30_4_CLIMATE_LWT_HEATING_ADD registerUnseenByte(0x00, 0x30,  4);
#define UNSEEN_BYTE_00_30_5_CLIMATE_LWT_COOLING_ADD registerUnseenByte(0x00, 0x30,  5);
#define UNSEEN_BYTE_00_14_05_CLIMATE_LWT_ABS_HEATING registerUnseenByte(0x00, 0x14, 5);
#define UNSEEN_BYTE_00_14_07_CLIMATE_LWT_ABS_COOLING registerUnseenByte(0x00, 0x14, 7);
#define UNSEEN_BYTE_00_14_10_CLIMATE_LWT_DEV_HEATING registerUnseenByte(0x00, 0x14, 10);
#define UNSEEN_BYTE_00_14_11_CLIMATE_LWT_DEV_COOLING registerUnseenByte(0x00, 0x14, 11);
#define UNSEEN_BYTE_40_14_18_CLIMATE_LWT_ADD registerUnseenByte(0x40, 0x14, 18);
#define UNSEEN_BYTE_40_0F_10_HEATING_ONLY registerUnseenByte(0x40, 0x0F, 10);

#define FIELDSTORE(x, registerUnseen) { \
  if (x != fieldSettingValdiv10) {\
    registerUnseen\
    x = fieldSettingValdiv10; \
  }\
}

uint8_t common_field_setting(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, uint16_t paramNr, char* mqtt_value) {
// returns 0 if PUB_CONFIG fails
// returns 1 if PUB_CONFIG succeeds
// returns 2 if no need to publish

/*
 * field setting contents, unless dummy:
 * byte 0 bits 7-6: 00 ? : always 00 to aux controller; often 80 or CO FROM indoor unit ; sometime 40 (indication of change???) FROM indoor to main, mostly 00
 * byte 0 bit 7 : writable
 * byte 0 bit 6 : readable
 * byte 0 bits 5-0: value
 * byte 1 max
 * byte 2 offset
 * byte 3 bit 7 exponent sign
 * byte 3 bit 6-3 mantissa (can be 10!)
 * byte 3 bit 2 exponent ?, always 0
 * byte 3 bit 1 exponent
 * byte 3 bit 0 (always 0 from/to main unit; often 1 for comm to aux controller and perhaps also 1 from aux controller))
 *
 * dummy: 00000001 OR FFFFFFFF
*/

  // byte 2
  int8_t fieldSettingMin          = (payload[payloadIndex - 1] & 0x80) ? -(int8_t) (payload[payloadIndex - 1] & 0x7F) : (int8_t) payload[payloadIndex - 1];
  // byte 3 step as FP
  byte fieldSettingStepExponentSign = (payload[payloadIndex] & 0x80) >> 7;  // bit 7
  byte fieldSettingStepMantissa = (payload[payloadIndex] & 0x78) >> 3; // bits 6 - 3
  byte fieldSettingStepExponent = (payload[payloadIndex] & 0x02) >> 1; // perhaps 0x03 or 0x07
  // byte3 unknown
  byte fieldSettingUnknownBitsStep = payload[payloadIndex] & 0x05; // bit2,0; bit2 always 0?; bit0 0 or 1
  // byte 0 unknown
  byte fieldSettingUnknownBitsVal  = payload[payloadIndex - 3] & 0xC0; // bits 7,6   (src='0': 0x00,0x80,0xC0; src='2': 0x00, 0xC0)
  uint16_t fieldSettingMax     = fieldSettingMin + payload[payloadIndex - 2] * (fieldSettingStepMantissa * (fieldSettingStepExponent ? (fieldSettingStepExponentSign ? 0.100001 : 10) : 1)); // <0.1 >10 not yet observed / unsure how to implement
  float fieldSettingVal        = fieldSettingMin + (payload[payloadIndex - 3] & 0x3F)  * (fieldSettingStepMantissa * (fieldSettingStepExponent ? (fieldSettingStepExponentSign ? 0.1 : 10) : 1)); // <0.1 >10 not yet observed / unsure how to implement
  int16_t fieldSettingValdiv10 = fieldSettingMin * 10 + (payload[payloadIndex - 3] & 0x3F)  * (fieldSettingStepMantissa * (fieldSettingStepExponent ? (fieldSettingStepExponentSign ? 1 : 100) : 10)); // <0.1 >10 not yet observed / unsure how to implement
  byte paramSrc = packetSrc;
  byte paramPacketType = packetType; // for dual-function FIELDKEY
  switch (paramNr) {
    // field settings [0_XX] (don't use '-' character in FIELDKEY/topic)
    // FIELDKEY should be called after FIELDSTORE as FIELDKEY will break when FSB == 1
    case 0x00 : FIELDKEY("0_00_A3123_LWT_Heating_Add_WD_Curve_High_Amb_LWT");
    case 0x01 : FIELDKEY("0_01_A3123_LWT_Heating_Add_WD_Curve_Low_Amb_LWT");
    case 0x02 : FIELDKEY("0_02_A3123_LWT_Heating_Add_WD_Curve_High_Amb");
    case 0x03 : FIELDKEY("0_03_A3123_LWT_Heating_Add_WD_Curve_Low_Amb");
    case 0x04 : FIELDKEY("0_04_A3124_LWT_Cooling_Add_WD_Curve_High_Amb_LWT");
    case 0x05 : FIELDKEY("0_05_A3124_LWT_Cooling_Add_WD_Curve_Low_Amb_LWT");
    case 0x06 : FIELDKEY("0_06_A3124_LWT_Cooling_Add_WD_Curve_High_Amb");
    case 0x07 : FIELDKEY("0_07_A3124_LWT_Cooling_Add_WD_Curve_Low_Amb");
    case 0x0B : FIELDKEY("0_0B_A47_DHW_WD_Curve_High_Amb_Setpoint");
    case 0x0C : FIELDKEY("0_0C_A47_DHW_WD_Curve_Low_Amb_Setpoint");
    case 0x0D : FIELDKEY("0_0D_A47_DHW_WD_Curve_High_Amb");
    case 0x0E : FIELDKEY("0_0E_A47_DHW_WD_Curve_Low_Amb");
    // field settings [1_XX]
    case 0x0F : FIELDKEY("1_00_A3113_LWT_Heating_Main_WD_Curve_Low_Amb"); // graph -20 - 5
    case 0x10 : FIELDKEY("1_01_A3113_LWT_Heating_Main_WD_Curve_High_Amb"); // 10-20
    case 0x11 : FIELDKEY("1_02_A3113_LWT_Heating_Main_WD_Curve_Low_Amb_LWT"); // [9_00] - [9_01] def 60
    case 0x12 : FIELDKEY("1_03_A3113_LWT_Heating_Main_WD_Curve_High_Amb_LWT"); // [9_00] - min(45,[9_01]) def 35
    case 0x13 : FIELDKEY("1_04_LWT_Cooling_Main_WD_Enabled"); // 0 disabled, 1 (default) enabled
    case 0x14 : FIELDKEY("1_05_LWT_Cooling_Add_WD_Enabled"); // 0 disabled, 1 (default) enabled
    case 0x15 : FIELDKEY("1_06_A3114_LWT_Cooling_Main_WD_Curve_Low_Amb"); // graph 10-25
    case 0x16 : FIELDKEY("1_07_A3114_LWT_Cooling_Main_WD_Curve_High_Amb"); // 25-43
    case 0x17 : FIELDKEY("1_08_A3114_LWT_Cooling_Main_WD_Curve_Low_Amb_LWT"); // [9_03] - [9_02]
    case 0x18 : FIELDKEY("1_09_A3114_LWT_Cooling_Main_WD_Curve_High_Amb_LWT"); // [9_03] - [9_02]
    case 0x19 : FIELDKEY("1_0A_A64_Averaging_Time"); // 0: no avg, 1 (default) 12 h, 2 24h, 3 48h, 4 72h
    case 0x1A : FIELDKEY("1_0B_9I_Delta_T_Heating_Main"); // version E
    case 0x1B : FIELDKEY("1_0C_9I_Delta_T_Heating_Add");
    case 0x1C : FIELDKEY("1_0D_9I_Delta_T_Cooling_Main");
    case 0x1D : FIELDKEY("1_0E_9I_Delta_T_Cooling_Add");
    // field settings [2_XX]
    case 0x1E : FIELDKEY("2_00_A442_DHW_Disinfection_Operation_Day");
    case 0x1F : FIELDKEY("2_01_A441_DHW_Disinfection");
    case 0x20 : FIELDKEY("2_02_A443_DHW_Disinfection_Start_Time");
    case 0x21 : FIELDKEY("2_03_A444_DHW_Disinfection_Setpoint");
    case 0x22 : FIELDKEY("2_04_A445_DHW_Disinfection_Duration");
    case 0x23 : FIELDKEY("2_05_Room_Antifrost_Temperature"); // 4-16 step 1 default 8
    case 0x24 : FIELDKEY("2_06_Room_Antifrost_Protection"); // 0-1 step 1 frost protection room ro
    case 0x27 : FIELDKEY("2_09_A323_Ext_Room_Sensor_Offset"); // -5 .. 5 step 0.5
    case 0x28 : FIELDKEY("2_0A_A322_Room_Temperature_Offset"); // -5 .. 5 step 0.5
    case 0x29 : FIELDKEY("2_0B_A65_Ext_Amb_Sensor_Offset"); // -5..5 step 0.5
    case 0x2A : FIELDKEY("2_0C_9I_Emitter_Type_Main"); // version E
    case 0x2B : FIELDKEY("2_0D_9I_Emitter_Type_Add");
    case 0x2C : FIELDKEY("2_0E_9I_Maximum_Current");
    // field settings [3_XX]
    case 0x2D : FIELDKEY("3_00_A61_Auto_Restart_Allowed"); // 1 No, 1 default Yes
    case 0x2E : FIELDKEY("3_01_Unspecified_3_01_rw_0");
    case 0x2F : FIELDKEY("3_02_Unspecified_3_02_rw_1");
    case 0x30 : FIELDKEY("3_03_Unspecified_3_03_rw_4");
    case 0x31 : FIELDKEY("3_04_Unspecified_3_04_rw_2");
    case 0x32 : FIELDKEY("3_05_Unspecified_3_05_rw_1");
    case 0x33 : FIELDSTORE(M.R.roomTempHeatingMaxX10, UNSEEN_BYTE_00_30_0_CLIMATE_ROOM_HEATING;);
                FIELDKEY("3_06_A3212_Temp_Range_Room_Heating_Max");
    case 0x34 : FIELDSTORE(M.R.roomTempHeatingMinX10, UNSEEN_BYTE_00_30_0_CLIMATE_ROOM_HEATING;);
                FIELDKEY("3_07_A3211_Temp_Range_Room_Heating_Min");
    case 0x35 : FIELDSTORE(M.R.roomTempCoolingMaxX10, UNSEEN_BYTE_00_30_1_CLIMATE_ROOM_COOLING;);
                FIELDKEY("3_08_A3214_Temp_Range_Room_Cooling_Max");
    case 0x36 : FIELDSTORE(M.R.roomTempCoolingMinX10, UNSEEN_BYTE_00_30_1_CLIMATE_ROOM_COOLING;);
                FIELDKEY("3_09_A3213_Temp_Range_Room_Cooling_Min");
    case 0x37 : FIELDKEY("3_0A_9I_Pump_Model"); // EHSX08P50EF
    // field settings [4_XX]
    case 0x3C : FIELDKEY("4_00_Unspecified_4_00_ro_1");
    case 0x3D : FIELDKEY("4_01_Unspecified_4_01_ro_0");
    case 0x3E : FIELDKEY("4_02_A331_Heating_Amb_Temperature_Max"); // space heating OFF temp, 14-35
    case 0x3F : FIELDKEY("4_03_Unspecified_4_03_ro_3");
    case 0x40 : FIELDKEY("4_04_Water_Pipe_Antifrost_Protection");
    case 0x41 : FIELDKEY("4_05_Unspecified_4_05_rw_0");
    case 0x42 : FIELDKEY("4_06_Unspecified_4_06_rw_0"); // later model AAV32: 0/1 "do not change this value"
    case 0x43 : FIELDKEY("4_07_Unspecified_4_07_ro_1");
    case 0x44 : FIELDKEY("4_08_A631_Power_Consumption_Control_Mode"); // 0 default: no limitation, 1 continuous, 2 digital inputs
    case 0x45 : FIELDKEY("4_09_A632_Power_Consumption_Control_Type"); // 0 Current, 1 Power
    case 0x46 : FIELDKEY("4_0A_Unspecified_4_0A_ro_0"); // 0
    case 0x47 : FIELDKEY("4_0B_Auto_Cooling_Heating_Changeover_Hysteresis"); //1-10 step 0.5 default 1
    case 0x49 : FIELDKEY("4_0D_Auto_Cooling_Heating_Changeover_Offset"); // 1-10 step0.5 default 3
    case 0x4A : FIELDKEY("4_0E_Unspecified_4_0E_ro_0");
    // field settings [5_XX]
    case 0x4B : FIELDKEY("5_00_Equilibrium_Enabled");
    case 0x4C : FIELDKEY("5_01_A522_Equilibrium_Temp");
    case 0x4D : FIELDKEY("5_02_Unspecified_5_02_ro_0");
    case 0x4E : FIELDKEY("5_03_Unspecified_5_03_rw_0");
    case 0x4F : FIELDKEY("5_04_Unspecified_5_04_rw_10");
    case 0x50 : FIELDKEY("5_05_A633_A6351_Power_Consumption_Control_Amp_Value_1"); // 0-50A, step 1A
    case 0x51 : FIELDKEY("5_06_A6351_Power_Consumption_Control_Amp_Value_2");
    case 0x52 : FIELDKEY("5_07_A6351_Power_Consumption_Control_Amp_Value_3");
    case 0x53 : FIELDKEY("5_08_A6351_Power_Consumption_Control_Amp_Value_4");
    case 0x54 : FIELDKEY("5_09_A634_A6361_Power_Consumption_Control_kW_Value_1"); // 0-20kW, step 0.5kW
    case 0x55 : FIELDKEY("5_0A_A6361_Power_Consumption_Control_kW_Value_2");
    case 0x56 : FIELDKEY("5_0B_A6361_Power_Consumption_Control_kW_Value_3");
    case 0x57 : FIELDKEY("5_0C_A6361_Power_Consumption_Control_kW_Value_4");
    case 0x58 : FIELDKEY("5_0D_Unspecified_5_0D_ro_1"); // version E: 230/400V identification, depends on model
    case 0x59 : FIELDKEY("5_0E_Unspecified_5_0E_ro_0"); // version E: 1
    // field settings [6_XX]
    case 0x5A : FIELDKEY("6_00_Temp_Diff_Determining_On"); // 0-20 step 1 default 2
    case 0x5B : FIELDKEY("6_01_Temp_Diff_Determining_Off"); // 0-20 step 1 default 2
    case 0x5C : FIELDKEY("6_02_Unspecified_6_02_ro_0");
    case 0x5D : FIELDKEY("6_03_Unspecified_6_03_ro_0");
    case 0x5E : FIELDKEY("6_04_Unspecified_6_04_ro_0");
    case 0x5F : FIELDKEY("6_05_Unspecified_6_05_ro_0");
    case 0x60 : FIELDKEY("6_06_Unspecified_6_06_ro_0");
    case 0x61 : FIELDKEY("6_07_Unspecified_6_07_rw_0");
    case 0x62 : FIELDKEY("6_08_Unspecified_6_08_rw_5");
    case 0x63 : FIELDKEY("6_09_Unspecified_6_09_rw_0");
    case 0x64 : FIELDKEY("6_0A_7431_Tank_Storage_Comfort"); // 30 - [6_0E], step 1
    case 0x65 : FIELDKEY("6_0B_7432_Tank_Storage_Eco"); // 30-min(50,[6_0E]), step
    case 0x66 : FIELDKEY("6_0C_7433_Tank_Reheat"); //idem
    case 0x67 : FIELDKEY("6_0D_A41_DHW_Reheat"); // 0 reheat_only 1 reheat+schedule 2 (default) schedule_only
    case 0x68 : FIELDSTORE(M.R.DHWsetpointMaxX10, UNSEEN_BYTE_00_10_19_CLIMATE_DHW;);
                FIELDKEY("6_0E_A45_DHW_Setpoint_Max"); // (40-65 without tank, 40-75 with tank)
    // field settings [7_XX]
    case 0x69 : FIELDKEY("7_00_Unspecified_7_00_ro_0");
    case 0x6A : FIELDKEY("7_01_Unspecified_7_01_ro_2");
    case 0x6B : FIELDSTORE(M.R.numberOfLWTzonesX10, UNSEEN_BYTE_00_30_4_CLIMATE_LWT_HEATING_ADD; UNSEEN_BYTE_00_30_5_CLIMATE_LWT_COOLING_ADD;
                                                    UNSEEN_BYTE_00_14_05_CLIMATE_LWT_ABS_HEATING; UNSEEN_BYTE_00_14_07_CLIMATE_LWT_ABS_COOLING;
                                                    UNSEEN_BYTE_00_14_10_CLIMATE_LWT_DEV_HEATING; UNSEEN_BYTE_00_14_11_CLIMATE_LWT_DEV_COOLING;
                                                    UNSEEN_BYTE_40_14_18_CLIMATE_LWT_ADD; pseudo0D = 9;);
                FIELDKEY("7_02_A218_Number_Of_LWT_Zones"); // 0-1 0: 1 zone, 1: 2 zones
    case 0x6C : FIELDKEY("7_03_PE_Factor"); // PE factor 0-6, step 0.1, default 2.5
    case 0x6D : FIELDKEY("7_04_A67_Savings_Mode_Ecological"); // 0 economical 1 ecological
    case 0x6E : FIELDKEY("7_05_Unspecified_7_05_ro_0");
    case 0x6F : FIELDKEY("7_06_9I_Compressor_Forced_Off"); // version E
    case 0x70 : FIELDKEY("7_07_9I_BBR16_Activation");
    case 0x71 : FIELDKEY("7_08_9I_Unspecified_rw_0");
    case 0x72 : FIELDKEY("7_09_9I_Pump_minimum_PWM_value");
    case 0x73 : FIELDKEY("7_0A_9I_Pump_fixed_add_zone_bizonekit"); // EHSX08P50EF
    case 0x74 : FIELDKEY("7_0B_9I_Pump_fixed_main_zone_bizonekit"); // EHSX08P50EF
    case 0x75 : FIELDKEY("7_0C_9I_Mixing_Valve_Time_bizonekit"); // EHSX08P50EF
    case 0x76 : FIELDKEY("7_0D_9I_Bivalent_hysteresis_heating"); // EHSX08P50EF
    case 0x77 : FIELDKEY("7_0E_9I_Setpoint_Offset_Excess_State"); // EHSX08P50EF
    // field settings [8_XX]
    case 0x78 : FIELDKEY("8_00_DHW_Running_Time_Min"); // 0-20 min step 1 min default 5 min
    case 0x79 : FIELDKEY("8_01_DHW_Running_Time_Max"); // 5-95 min step 5 min default 30 min
    case 0x7A : FIELDKEY("8_02_Anti_Recycling_Time"); // 0-10 hour step 0.5 hour default 30 min
    case 0x7B : FIELDKEY("8_03_Unspecified_8_03_ro_50");
    case 0x7C : FIELDKEY("8_04_Unspecified_8_04_ro_0");
    case 0x7D : // allowModulation may not be available in all modes, if so: Unseen + unavailability actions
                FIELDSTORE(M.R.allowModulationX10, pseudo0D = 9;);
                FIELDKEY("8_05_A3115_LWT_Modulation_Allow"); // 0 No, 1 (default) Yes
    case 0x7E : FIELDSTORE(M.R.modulationMaxX10, pseudo0D = 9;);
                FIELDKEY("8_06_LWT_Modulation_Max");
    case 0x7F : FIELDKEY("8_07_7423_LWT_Main_Cooling_Comfort"); // [9_03] - [9_02] step 1, default 45
    case 0x80 : FIELDKEY("8_08_7424_LWT_Main_Cooling_Eco"); // [9_03] - [9_02] step 1, default 45
    case 0x81 : FIELDKEY("8_09_7421_LWT_Main_Heating_Comfort"); // [9_01] - [9_00] step 1, default 45
    case 0x82 : FIELDKEY("8_0A_7422_LWT_Main_Heating_Eco"); // [9_01] - [9_00] step 1, default 45
    case 0x83 : FIELDKEY("8_0B_Flow_Target_Heatpump"); // 10-20 step 0.5 default 15
    case 0x84 : FIELDKEY("8_0C_Flow_Target_Hybrid"); // 10-20 step 0.5 default 10
    case 0x85 : FIELDKEY("8_0D_Flow_Target_Gasboiler"); // 10-20 step 0.5 default 14
    // field settings [9_XX]
    case 0x87 : FIELDSTORE(M.R.LWTheatMainMaxX10, UNSEEN_BYTE_00_30_2_CLIMATE_LWT_HEATING;);
                FIELDKEY("9_00_A31122_Temp_Heating_Main_Max"); // 37-80 step 1
    case 0x88 : FIELDSTORE(M.R.LWTheatMainMinX10, UNSEEN_BYTE_00_30_2_CLIMATE_LWT_HEATING;);
                FIELDKEY("9_01_A31121_Temp_Heating_Main_Min"); // 15-37 step 1
    case 0x89 : FIELDSTORE(M.R.LWTcoolMainMaxX10, UNSEEN_BYTE_00_30_3_CLIMATE_LWT_COOLING;);
                FIELDKEY("9_02_A31124_Temp_Cooling_Main_Max"); // 18-22 step
    case 0x8A : FIELDSTORE(M.R.LWTcoolMainMinX10, UNSEEN_BYTE_00_30_3_CLIMATE_LWT_COOLING;);
                FIELDKEY("9_03_A31123_Temp_Cooling_Main_Min"); // 5-18 step 1
    case 0x8B : FIELDSTORE(M.R.overshootX10, pseudo0D = 9;);
                FIELDKEY("9_04_Unspecified_9_04_ro_1"); // Overshoot
    case 0x8C : FIELDSTORE(M.R.LWTheatAddMinX10, UNSEEN_BYTE_00_30_4_CLIMATE_LWT_HEATING_ADD;);
                FIELDKEY("9_05_A31221_Temp_Heating_Add_Min");
    case 0x8D : FIELDSTORE(M.R.LWTheatAddMaxX10, UNSEEN_BYTE_00_30_4_CLIMATE_LWT_HEATING_ADD;);
                FIELDKEY("9_06_A31222_Temp_Heating_Add_Max");
    case 0x8E : FIELDSTORE(M.R.LWTcoolAddMinX10, UNSEEN_BYTE_00_30_5_CLIMATE_LWT_COOLING_ADD;);
                FIELDKEY("9_07_A31223_Temp_Cooling_Add_Min");
    case 0x8F : FIELDSTORE(M.R.LWTcoolAddMaxX10, UNSEEN_BYTE_00_30_5_CLIMATE_LWT_COOLING_ADD;);
                FIELDKEY("9_08_A31224_Temp_Cooling_Add_Max");
    case 0x90 : FIELDKEY("9_09_Delta_T");
    case 0x91 : FIELDKEY("9_0A_Unspecified_9_0A_rw_5");
    case 0x92 : FIELDKEY("9_0B_A3117_Emitter_Slow"); // 0 quick, 1 slow
    case 0x93 : FIELDKEY("9_0C_Room_Temperature_Hysteresis"); // 1-6 step 0.5 default 1.0C
    case 0x94 : FIELDKEY("9_0D_Pump_Speed_Limitation"); // some models? (0-8, 0: 100%, 1-4: 80-50%, 5-8 80-50% ??, default 6)
    case 0x95 : FIELDKEY("9_0E_Unknown"); // some models?
    // field settings [A_XX] // no longer documented/present on version E
    case 0x96 : FIELDKEY("A_00_Unspecified_A_00_0"); // 0-(0)-0 or 0-(1)-7  for src=2? oth=01/09. Stepbits=01.
    case 0x97 : FIELDKEY("A_01_Unspecified_A_01_0"); // 0-(0)-0 or 0-(1)-7  for src=2?
    case 0x98 : FIELDKEY("A_02_Unspecified_A_02_0"); // 0-(0)-0 or 0-(1)-7  for src=2?
    case 0x99 : FIELDKEY("A_03_Unspecified_A_03_0"); // 0-(0)-0 or 0-(1)-7  for src=2?
    case 0x9A : FIELDKEY("A_04_Unspecified_A_04_0"); // 0-(0)-0 or 0-(1)-7  for src=2?
    // field settings [B_XX] // no longer documented/present on version E
    case 0xA5 : FIELDKEY("B_00_Unspecified_B_00_0");
    case 0xA6 : FIELDKEY("B_01_Unspecified_B_01_0");
    case 0xA7 : FIELDKEY("B_02_Unspecified_B_02_0");
    case 0xA8 : FIELDKEY("B_03_Unspecified_B_03_0");
    case 0xA9 : FIELDKEY("B_04_Unspecified_B_04_0");
    // field settings [C_XX]
    case 0xB4 : FIELDKEY("C_00_DHW_Priority"); // 0 (default) Solar priority, 1: heat pump priority
    case 0xB5 : FIELDKEY("C_01_Unspecified_C_01_ro_0");
    case 0xB6 : FIELDKEY("C_02_External_Backup"); // hybrid: 0, ro. Others: 0: No, 1: Bivalent, 2: -, 3: -
    case 0xB7 : FIELDKEY("C_03_Bivalent_Activation_Temperature"); // Bivalent_temperature (should be at least 3C less than 5-01)
    case 0xB8 : FIELDKEY("C_04_Bivalent_Hysteresis_Temperature"); // Bivalent_hysteresis
    case 0xB9 : FIELDKEY("C_05_A224_Contact_Type_Main"); // 1 thermo on/off 2 C/H request
    case 0xBA : FIELDKEY("C_06_A225_Contact_Type_Add"); // 1 thermo on/off 2 C/H request
    case 0xBB : /* if (FSB) */ FIELDSTORE(M.R.controlRTX10, UNSEEN_BYTE_00_30_0_CLIMATE_ROOM_HEATING; UNSEEN_BYTE_00_30_1_CLIMATE_ROOM_COOLING; pseudo0D = 9;);
                // do not check for FSB value as we are not sure we received info at boot.... (and FSB has "wrong" value to ensure same SRC value in output)
                FIELDKEY("C_07_A217_Unit_Control_Method"); // 0 LWT control, 1 Ext-RT control, 2 default=RT control
    case 0xBC : FIELDKEY("C_08_A22B_External_Sensor"); // 0 No, 1 Outdoor sensor, 2 Room sensor
    case 0xBD : FIELDKEY("C_09_A2263_Digital_IO_PCB_Alarm_Output"); // 0 normally open, 1 normally closed
    case 0xBE : FIELDKEY("C_0A_Indoor_Quick_Heatup_Enable"); // 0:disable, 1 default enable
    case 0xBF : FIELDKEY("C_0B_Unspecified_0"); // EHSX08P50EF
    case 0xC0 : FIELDKEY("C_0C_7451_Electricity_Price_High"); // together with D-0C
    case 0xC1 : FIELDKEY("C_0D_7452_Electricity_Price_Medium"); // together with D-0D
    case 0xC2 : FIELDKEY("C_0E_7453_Electricity_Price_Low"); // together with D-0E
    // field settings [D_XX]
    case 0xC3 : FIELDKEY("D_00_Unspecified_D_00_rw_0");
    case 0xC4 : FIELDKEY("D_01_A216_Preferential_kWh_Rate"); // 0 no, 1 active-open, 2 active-closed
    case 0xC5 : FIELDKEY("D_02_A22A_DHW_Pump"); // 0 No, 1 Secondary rtm ([E-06] = 1), 2 Disinf. Shunt ([E_06] = 1)
    case 0xC6 : FIELDKEY("D_03_LWT_0C_Compensation_Shift"); // 0 (default) Disabled, 1: 2C (-2..+2), 2: 4C (-2..+2), 3: 2C (-4..+4C), 4C (-4..+4C)
    case 0xC7 : FIELDKEY("D_04_A227_Demand_PCB"); // 0 No, 1 power consumption control
    case 0xC8 : FIELDKEY("D_05_Unspecified_D_05_ro_0");
    case 0xCA : FIELDKEY("D_07_A2262_Digital_IO_PCB_Solar_Kit"); // 0-1
    case 0xCB : FIELDKEY("D_08_A228_External_kWh_Meter_1"); // 0: No, 1-5: 0.1 - 1000 pulses/kWh
    case 0xCC : FIELDKEY("D_09_Unspecified_D_09_ro_0");
    case 0xCD : FIELDKEY("D_0A_A22C_External_Gas_Meter_1"); // 0 Not, 1-3 1-10-100 /m3
    case 0xCE : FIELDKEY("D_0B_Unspecified_D_0B_rw_2");
    case 0xCF : FIELDKEY("D_0C_7451_Electricity_Price_High_do_not_use"); // together with C-0C
    case 0xD0 : FIELDKEY("D_0D_7452_Electricity_Price_Medium_do_not_use"); // together with C-0D
    case 0xD1 : FIELDKEY("D_0E_7453_Electricity_Price_Low_do_not_use"); // together with C-0E
    // field settings [E_XX]
    case 0xD2 : FIELDKEY("E_00_A211_RO_Unit_Type"); // RO 3
    case 0xD3 : FIELDKEY("E_01_A212_RO_Compressor_Type"); // RO 0
    case 0xD4 : FIELDSTORE(M.R.heatingOnlyX10, UNSEEN_BYTE_00_30_1_CLIMATE_ROOM_COOLING; UNSEEN_BYTE_00_30_3_CLIMATE_LWT_COOLING; UNSEEN_BYTE_00_30_5_CLIMATE_LWT_COOLING_ADD; UNSEEN_BYTE_40_0F_10_HEATING_ONLY);
                FIELDKEY("E_02_A213_RO_Indoor_Software_Type"); // RO 0:type1 (cool/heat) or 1: type2 (heat-only)            // 0=reversible(EBLA), 1=heating-only(EDLA)
    case 0xD5 : FIELDKEY("E_03_Unspecified_E_03_ro_0");
    case 0xD6 : FIELDKEY("E_04_A21A_RO_Power_Saving_Available"); // RO 1=yes
    case 0xD7 : if ((M.R.useDHW & 0x02) != (payload[payloadIndex - 3] ? 0x02 : 0x00)) {
                  UNSEEN_BYTE_00_10_19_CLIMATE_DHW;
                  M.R.useDHW = (M.R.useDHW & 0x01) | (payload[payloadIndex - 3] & 0x3F ? 0x02 : 0x00); // DHW operation: 0x02
                }
                HADEVICE_BINSENSOR;
                FIELDKEY("E_05_A221_DHW_Operation"); // 0-1 whether DHW is installed
    case 0xD8 : if ((M.R.useDHW & 0x01) != (payload[payloadIndex - 3] ? 0x01 : 0x00)) {
                  UNSEEN_BYTE_00_10_19_CLIMATE_DHW;
                  M.R.useDHW = (M.R.useDHW & 0x02) | (payload[payloadIndex - 3] & 0x3F ? 0x01 : 0x00); // tank installed: 0x01
                }
                HADEVICE_BINSENSOR;
                FIELDKEY("E_06_A222_DHW_Tank_Installed"); // 0-1
    case 0xD9 : FIELDKEY("E_07_A223_DHW_Tank_Type"); // 4 do-not-change
    case 0xDA : FIELDKEY("E_08_Power_Saving_Enabled"); // 0: Disabled, 1 (default): Enabled, Warning: should be disabled for 24-hour averaging unless separate outdoor sensor is present
    case 0xDB : FIELDKEY("E_09_Unspecified_E_09_rw_0");
    case 0xDC : FIELDKEY("E_0A_Unspecified_E_0A_ro_0");
    case 0xDD : FIELDKEY("E_0B_Unspecified_E_0B_ro_0"); // version E: Bizone kit installed
    case 0xDE : FIELDKEY("E_0C_Unspecified_E_0C_ro_0");
    case 0xDF : FIELDKEY("E_0D_Unspecified_E_0D_ro_0"); // version E: is glycol present in system?
    case 0xE0 : FIELDKEY("E_0E_Unspecified_0");
    // field settings [F_XX]
    case 0xE1 : FIELDKEY("F_00_Pump_Operation_Outside_Range_Allowed"); // 0: (default) disabled, 1: enabled
    case 0xE2 : FIELDKEY("F_01_A332_Cooling_Amb_Temperature_Min"); // space cooling On temp 10-35 def 20
    case 0xE3 : FIELDKEY("F_02_Unspecified_F_02_rw_3");
    case 0xE4 : FIELDKEY("F_03_Unspecified_F_03_rw_5");
    case 0xE5 : FIELDKEY("F_04_Unspecified_F_04_rw_0");
    case 0xE6 : FIELDKEY("F_05_Unspecified_F_05_rw_0");
    case 0xE7 : FIELDKEY("F_06_Unspecified_F_06_rw_0");
    case 0xE8 : FIELDKEY("F_07_Efficiency_Calculation_0"); // EHSX08P50EF
    case 0xE9 : FIELDKEY("F_08_Continuous_Heating_Defrost_Enable"); // EHSX08P50EF
    case 0xEA : FIELDKEY("F_09_Pump_Operation_During_Flow_Abnormality"); // 0: (default) disabled, 1: enabled
    case 0xEB : FIELDKEY("F_0A_Unspecified_F_0A_rw_0");
    case 0xEC : FIELDKEY("F_0B_A31161_Shutoff_Valve_Closed_During_Thermo_OFF"); // 0 default No, 1 Yes
    case 0xED : FIELDKEY("F_0C_A31162_Shutoff_Valve_Closed_During_Cooling"); // 0 No, 1 default Yes
    case 0xEE : FIELDKEY("F_0D_A219_Pump_Operation_Mode"); // 0 continuous, 1 sample ([C-07]=0, LWT control), 2 request ([C-07] <> 0, RT control)
    case 0xEF : FIELDKEY("F_OE_Max_Power_Tank_Heating_Support"); // EHSX08P50EF
    case 0xFFFF : return 2; // dummy field setting
    default   : // 31x packetSrc = 0x40 (src = '1'): all printed values zero.
                // byte f = packetSrc ? 0xFF : 0x00;
                byte f=0x00; // ??
                if ((payload[payloadIndex] == (f | 0x01) )   && (payload[payloadIndex - 1] == f) && (payload[payloadIndex - 2] == f)  && (payload[payloadIndex - 3] == f)) return 2;
                if ((payload[payloadIndex] ==  f         )   && (payload[payloadIndex - 1] == f) && (payload[payloadIndex - 2] == f)  && (payload[payloadIndex - 3] == f)) return 2;
                printfTopicS("Fieldsetting unknown 0x%02X %i 0x%02X    0x%02X 0x%02X 0x%02X 0x%02X", packetSrc, FSB, paramNr, payload[payloadIndex - 3], payload[payloadIndex - 2], payload[payloadIndex - 1], payload[payloadIndex]);
                FIELDKEY("Fieldsetting_Unknown");
  }

  if (FSB == 1) return 0;

  topicWrite;
  // Catch Daikin's (new?) use of 10E-1 instead of 1E0 for stepsize
  if (fieldSettingStepExponentSign && (fieldSettingStepMantissa == 10)) {
    fieldSettingStepExponentSign = 0;
    fieldSettingStepMantissa = 1;
  }
  if (fieldSettingStepExponentSign) {
    snprintf(mqtt_value, MQTT_VALUE_LEN, "{\"val\":%1.1f, \"min\":%i, \"max\":%i, \"stepsize\":0.%i, \"bits\":\"%i%i\", \"bitsstep\":\"0x%02X\"}", fieldSettingVal, fieldSettingMin, fieldSettingMax, fieldSettingStepMantissa, fieldSettingUnknownBitsVal >> 7, (fieldSettingUnknownBitsVal >> 6) & 0x01, fieldSettingUnknownBitsStep);
  } else {
    snprintf(mqtt_value, MQTT_VALUE_LEN, "{\"val\":%1.0f, \"min\":%i, \"max\":%i, \"stepsize\":%i, \"bits\":\"%i%i\", \"bitsstep\":\"0x%02X\"}", fieldSettingVal, fieldSettingMin, fieldSettingMax, fieldSettingStepMantissa * (fieldSettingStepExponent ? 10 : 1), fieldSettingUnknownBitsVal >> 7, (fieldSettingUnknownBitsVal >> 6) & 0x01, fieldSettingUnknownBitsStep);
  }
  return 1;
}

void field_setting(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, char* mqtt_value) {
  byte FieldNr1 = (packetType + 0x04) & 0x0F;
  byte FieldNr2 = (8 - ((packetType & 0xF0) >> 4)) * 5 + (payloadIndex >> 2);
  uint16_t paramNr = FieldNr1 * 0x0F + FieldNr2;
  // if FSB == 1: common_field_setting does not save history, is only called here for FIELDSTORE functionality, as FIELDKEY breaks if (FSB == 1) before PUB_CONFIG
  common_field_setting(packetSrc, packetType, payloadIndex, payload, paramNr, mqtt_value);
  // store field setting data from 4000[678][0-F] data for later publishing, store value in param-data for 00F039 history
  if (packetSrc == 0x40) {
    byte ppti = 0x39 - PARAM_TP_START;
    uint16_t ptbs = seenstart[ppti] + paramNr;
    uint16_t ptbv = valstart[ppti] + paramNr * parnr_bytes[ppti];
    for (byte i = 0; i < 4; i++) M.paramVal[0][ptbv + i] = payload[payloadIndex - 3 + i] & 0x3F;
    M.paramSeen[0][ptbs >> 3] |= (1 << (ptbs & 0x07));
  }
}

void param_field_setting(byte paramSrc, byte paramPacketType, uint16_t paramNr, byte payloadIndex, byte* payload, char* mqtt_value) {
  if (common_field_setting(paramSrc, paramPacketType, payloadIndex, payload, paramNr, mqtt_value) == 1) {
    // PUB_CONFIG succeeded
    publishEntityParam(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, 4);
  }
}

uint8_t publishFieldSetting(byte paramNr) {
  byte  ppti = 0x39 - PARAM_TP_START;
  uint16_t ptbs = seenstart[ppti] + paramNr;
  uint16_t ptbv = valstart[ppti] + paramNr * parnr_bytes[ppti];
  if (M.paramSeen[0][ptbs >> 3] & (1 << (ptbs & 0x07))) {
    // FSB == 2 forces to output HA config message and entity even if seen before
    switch (common_field_setting(0x00, 0x39, 3, &M.paramVal[0][ptbv], paramNr, mqtt_value)) {
      case 0 : return 0; // PUB_CONFIG failed, retry later
      case 1 : return publishEntityParam(0x00, 0x39, paramNr, 3, &M.paramVal[0][ptbv], mqtt_value, 4);
      case 2 : return 1; // to be skpped, continue with next
      default: return 1; // should not occur
    }
  } else {
    return 1; // not seen, continue with next
  }
}
#endif /* E_SERIES */

// return 0:            use this for all bytes (except for the last byte) which are part of a multi-byte parameter
// BITBASIS (for j==8): this byte is to be treated on a bitbasis (by re-calling this function with j=0..7
//                      switch-statement for j is needed as shown below
//                      indicate for j=0..7: "KEY("KnownParameter"); VALUE_flag8" if bit usage is known
//                      indicate for j=0..7: "UNKNOWN_BIT", mqttTopic will then be set to "Bit-0x[source]-0x[payloadnr]-0x[location]-[bitnr]"
// BITBASIS_UNKNOWN:    shortcut for entire bit-switch statement if bit usage of all bits is unknown
// UNKNOWN_BYTE:        byte function is unknown, mqttTopic will be: "PacketSrc-0xXX-Type-0xXX-Byte-I-Bit-I", value will be hex
// UNKNOWN_BIT:         bit function is unknown, mqttTopic will be: "PacketSrc-0xXX-Type-0xXX-Byte-I-Bit-I", value will be hex
// UNKNOWN_PARAM:       parameter function is unknown, mqttTopic will be "ParamSrc-0xXX-Type-0xXX-Nr-0xXXXX", value will be hex
// VALUE_u0:            "1-byte unsigned integer value at current location payload[i]" no value change detection
// VALUE_u8:            1-byte unsigned integer value at current location payload[i]
// VALUE_s8:            1-byte signed integer value at current location payload[i]
// VALUE_u16:           2-byte unsigned integer value at location payload[i - 1]..payload[i]
// VALUE_s16:           2-byte signed integer value at current location payload[i - 1]..payload[i]
// VALUE_u24:           3-byte unsigned integer value at location payload[i-2]..payload[i]
// VALUE_u32:           4-byte unsigned integer value at location payload[i-3]..payload[i]
// VALUE_u8_add2k:      for 1-byte value (2000+payload[i]) (for year value)
// VALUE_s4abs1c:       for 1-byte value -10..10 where bit 4 is sign and bit 0..3 is value
// VALUE_f8_8_BE        for 2-byte float in f8_8 format (see protocol description, basically s16div256)
// VALUE_f8_8_LE        for 2-byte float in f8_8 format (see protocol description, basically s16div256)
// VALUE_s_f8_8_LE      for 2-byte float in s_f8s8 format (see protocol description, basically (u16div256 * sign))
// VALUE_f8s8_LE        for 2-byte float in f8s8 format (see protocol description, basically (s8 + u8div10))
// VALUE_u8div10        for 1-byte integer to be divided by 10 (used for water pressure in 0.1 bar)
// VALUE_u16div10       for 2-byte integer to be divided by 10 (used for flow in 0.1m/s)
// VALUE_F(value)       for self-calculated float parameter value
// VALUE_u32hex         for 4-byte hex value (used for sw version)
// VALUE_header         for empty payload string

#define VALUE_u8hex              { value_u_hex_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 1);                          return 0; }
#define VALUE_u16hex_LE          { value_u_hex_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 2);                          return 0; }
#define VALUE_u24hex_LE          { value_u_hex_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 3);                          return 0; }
#define VALUE_u32hex_LE          { value_u_hex_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 4);                          return 0; }
#define VALUE_s8                 { value_s8(packetSrc, packetType, payloadIndex, payload, mqtt_value);                                   return 0; }
#define VALUE_s8div10            { value_sdiv10_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 1);                         return 0; }
#define VALUE_s8div100           { value_sdiv100_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 1);                        return 0; }
#define VALUE_s16_LE             { value_s_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 2);                              return 0; }
#define VALUE_s16div10_LE        { value_sdiv10_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 2);                         return 0; }
#define VALUE_s16div100_LE       { value_sdiv100_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 2);                        return 0; }
#define VALUE_u0                 { value_u0(packetSrc, packetType, payloadIndex, payload, mqtt_value);                                   return 0; }
#define VALUE_u8                 { value_u_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 1);                              return 0; }
#define VALUE_u16_LE             { value_u_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 2);                              return 0; }
#define VALUE_u16div1000_LE      { value_udiv1000_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 2);                       return 0; }
#define VALUE_u24_LE             { value_u_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 3);                              return 0; }
#define VALUE_u32_LE             { value_u_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 4);                              return 0; }
#define VALUE_u32div1000_LE      { value_udiv1000_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value, 4);                       return 0; }
#define VALUE_bits(n1, n2)       { value_bits(packetSrc, packetType, payloadIndex, payload, mqtt_value, n1, n2);                         return 0; }
#define VALUE_bits_nopub(n1, n2)       { value_bits_nopub(packetSrc, packetType, payloadIndex, payload, mqtt_value, n1, n2);                         return 0; }
#define VALUE_flag8              { if (haDevice == HA_SENSOR) HADEVICE_BINSENSOR; value_flag8(packetSrc, packetType, payloadIndex, payload, mqtt_value, bitNr); return 0; }
#define VALUE_flag8_inv          { if (haDevice == HA_SENSOR) HADEVICE_BINSENSOR; value_flag8(packetSrc, packetType, payloadIndex, payload, mqtt_value, bitNr, 1); return 0; }
#define UNKNOWN_BIT              { CAT_UNKNOWN; CHECKBIT; if (pubEntity && (haConfig || (EE.outputMode & 0x0100))) unknownBit(packetSrc, packetType, payloadIndex, payload, mqtt_value, bitNr); return 0; }
#define VALUE_u8_add2k           { value_u8_add2k(packetSrc, packetType, payloadIndex, payload, mqtt_value);                             return 0; }
#define VALUE_s4abs1c            { value_s4abs1c(packetSrc, packetType, payloadIndex, payload, mqtt_value);                              return 0; }
#define VALUE_u8div10            { value_u8div10(packetSrc, packetType, payloadIndex, payload, mqtt_value);                              return 0; }
#define VALUE_u16div10_LE        { value_u16div10_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value);                          return 0; }
#define VALUE_u16div100_LE       { value_u16div100_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value);                         return 0; }
#define VALUE_f8_8_BE            { value_f8_8_BE(packetSrc, packetType, payloadIndex, payload, mqtt_value);                              return 0; }
#define VALUE_f8_8_LE            { value_f8_8_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value);                              return 0; }
#define VALUE_s_f8_8_LE          { value_s_f8_8_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value);                            return 0; }
#ifdef MHI_SERIES
#define VALUE_mode(v)            { value_mode(packetSrc, packetType, payloadIndex, payload, mqtt_value, v);                              return 0; }
#endif /* MHI_SERIES */
#define VALUE_S_L(v, l)          { value_s(packetSrc, packetType, payloadIndex, payload, mqtt_value, v, l);                              return 0; }

#define VALUE_S_BIT(v)           { value_s_bit(packetSrc, packetType, payloadIndex, payload, mqtt_value, v, bitNr);                              return 0; }


#define VALUE_F_L(v, l)          { value_f(packetSrc, packetType, payloadIndex, payload, mqtt_value, v, l);                              return 0; }
#define VALUE_f8s8_LE            { value_f8s8_LE(packetSrc, packetType, payloadIndex, payload, mqtt_value);                              return 0; }
#define VALUE_F_L_thr(v, tt, tv) { value_f(packetSrc, packetType, payloadIndex, payload, mqtt_value, v, tt, tv);                         return 0; }
#define VALUE_textString(s)      { value_textString(mqtt_value, s);                                                                      return 0; }
#define VALUE_textStringOnce(s)  { value_textStringOnce(packetSrc, packetType, payloadIndex, payload, mqtt_value, s);                    return 0; }
#define UNKNOWN_BYTE             { CAT_UNKNOWN; CHECK(1); if (pubEntity && (haConfig || (EE.outputMode & 0x0100))) unknownByte(packetSrc, packetType, payloadIndex, payload, mqtt_value); return 0; }
#define VALUE_header             { value_header(packetSrc, packetType, mqtt_value);                                                      return 0; }

#define PARAM_VALUE_u8           { param_value_u_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);           return 0; }
#define PARAM_VALUE_s8           { param_value_s_LE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);           return 0; }
#define PARAM_VALUE_u16_BE       { param_value_u_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);           return 0; }
#define PARAM_VALUE_u24_BE       { param_value_u_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);           return 0; }
#define PARAM_VALUE_u32_BE       { param_value_u_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);           return 0; }
#define PARAM_VALUE_u16div10_BE  { param_value_u16div10_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);    return 0; }
#define PARAM_VALUE_u32div100_BE { param_value_u32div100_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);   return 0; }
#define PARAM_VALUE_s16div10_BE  { param_value_s16div10_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);    return 0; }
#define PARAM_VALUE_u8hex        { param_value_hex_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);         return 0; }
#define PARAM_VALUE_u16hex_BE    { param_value_hex_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);         return 0; }
#define PARAM_VALUE_u24hex_BE    { param_value_hex_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength);         return 0; }
#define PARAM_FIELD_SETTING      { param_field_setting(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value);                        return 0; }

#define UNKNOWN_PARAM            { CAT_UNKNOWN; CHECKPARAM(paramValLength); if (pubEntity && (haConfig || (EE.outputMode & 0x0100))) unknownParam_BE(paramSrc, paramPacketType, paramNr, payloadIndex, payload, mqtt_value, paramValLength); return 0; }

#define UNKNOWN_PARAM8 UNKNOWN_PARAM
#define UNKNOWN_PARAM16 UNKNOWN_PARAM
#define UNKNOWN_PARAM24 UNKNOWN_PARAM
#define UNKNOWN_PARAM32 UNKNOWN_PARAM

#define HANDLE_PARAM(paramValLength)     { handleParam(packetSrc, packetType, payloadIndex, payload, mqtt_value, paramValLength); return 0; }

#define FIELD_SETTING           { field_setting(packetSrc, packetType, payloadIndex, payload, mqtt_value); return 0;}

// BITBASIS returns a byte indicating whic bits of a byte changed, or haven't been seen before
// value of byte and of seen status should not be saved (yet) (saveSeen=0), this is done on bit basis

#ifdef E_SERIES

uint8_t handleParam(byte paramSrc, byte paramPacketType, byte payloadIndex, byte* payload, char* mqtt_value, /*char &cat,*/ byte paramValLength) {
// similar to bytes2keyvalue but using indirect parameter references in payloads
// always returns 0, no bit handling needed

  uint16_t paramNr = (((uint16_t) payload[payloadIndex - paramValLength]) << 8) | payload[payloadIndex - paramValLength - 1];

  switch (paramPacketType) {
    case 0x35 : switch (paramSrc) {
// 35: 8-bit, use M.paramValue8
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
      case 0x00 : // parameters aux -> main
      case 0x40 : // parameters main -> aux
                                                 CAT_SETTING;
                  switch (paramNr) {
        case 0x0000 :                                                                                                       PARAM_KEY("Reboot_Related_Q10-35-00");                                     PARAM_VALUE_u8; //  0x00 0x01 0x03 (quiet related ?)
        case 0x0001 :                                                                                                       PARAM_KEY("Quiet_Level_35");                                               PARAM_VALUE_u8; //  0x00 0x01
        case 0x0003 :                                                                                                       PARAM_KEY("Quiet_Mode_35");                                                PARAM_VALUE_u8;
        case 0x0004 :                                                                                                       PARAM_KEY("Reboot_Related_Q11-35-01");                                     PARAM_VALUE_u8; //  0x00 0x01
        case 0x0008 :                                                                                                       PARAM_KEY("Compressor");                                                   PARAM_VALUE_u8;
        case 0x0035 :                                                                                                       PARAM_KEY("Defrost_Req_Or_Response_Or_Compressor");                        PARAM_VALUE_u8; // = Defrost_Request or Defrost_Response ?
        case 0x0036 :                                                                                                       PARAM_KEY("Defrost_Q_36");                                                 PARAM_VALUE_u8;
        case 0x000A :                                                                                                       PARAM_KEY("Compressor_2");                                                 PARAM_VALUE_u8; // ? = compressor? // was Heatpump_Active_Q0
        case 0x003C :                                                                                                       PARAM_KEY("Heatpump_Active_Q1");                                           PARAM_VALUE_u8; // ? or circ pump? // Heatpump_Active_Q1= (IU_Operation_Mode & 0x04 ?)  // related to System_On_Q and DHW_Demand
        case 0x000E :                                                                                                       PARAM_KEY("Circulation_Pump_2");                                           PARAM_VALUE_u8;
        case 0x0011 : SUBDEVICE("_Unknown");                         HACONFIG; HADEVICE_BINSENSOR;                          PARAM_KEY("DHW_Related_Q");                                                PARAM_VALUE_u8;
        case 0x0012 :                                                                                                       PARAM_KEY("Gasboiler_Active");                                             PARAM_VALUE_u8;
        case 0x0019 :                                                                                                       PARAM_KEY("kWh_Preferential_Mode");                                        PARAM_VALUE_u8;
        case 0x002C : SUBDEVICE("_Unknown");                         HACONFIG; HADEVICE_BINSENSOR;                          PARAM_KEY("Climate_Q_1");                                                  PARAM_VALUE_u8;
        case 0x002D : SUBDEVICE("_Unknown");                         HACONFIG; HADEVICE_BINSENSOR;                          PARAM_KEY("Climate_Q_2");                                                  PARAM_VALUE_u8;
        case 0x002E :                                                                                                       PARAM_KEY("Climate_LWT_2E");                                                PARAM_VALUE_u8;
        case 0x002F :                                                                                                       PARAM_KEY("Climate_LWT_2F");                                                PARAM_VALUE_u8;
        case 0x0030 :                                                                                                       PARAM_KEY("Climate_Room_30");                                               PARAM_VALUE_u8;
        case 0x0031 :                                                                                                       PARAM_KEY("Climate_Room_31");                                               PARAM_VALUE_u8;
        case 0x0032 :                                                                                                       PARAM_KEY("Climate_HC_Auto");                                              PARAM_VALUE_u8;
        case 0x0033 : SUBDEVICE("_Unknown");                         HACONFIG; HADEVICE_BINSENSOR;                          PARAM_KEY("Climate_Mode_Q");                                               PARAM_VALUE_u8; // ?
        case 0x0039 :                                                                                                       PARAM_KEY("Climate_Mode_Heating_Cooling_2");                               PARAM_VALUE_u8; // 1=heating/2=cooling
        case 0x003A :                                                                                                       PARAM_KEY("Climate_Mode_Heating_Cooling_3");                               PARAM_VALUE_u8; // 1=heating/2=cooling
        case 0x0037 :                                                                                                       PARAM_KEY("IU_Operation_Mode");                                            PARAM_VALUE_u8; // 0x00 0x01 0x04 0x05 ?? status depends on DHW on/off, heating on/off, and tapwater-flow yes/no ? /  0x04=DHW_Demand 0x01=heating_on?
        case 0x003F :                                                                                                       PARAM_KEY("DHW_2");                                                        PARAM_VALUE_u8; // DHW on/off setting (main->aux)
        case 0x0040 :                                                                                                       PARAM_KEY("DHW_3");                                                        PARAM_VALUE_u8; // DHW on/off setting (aux->main)
        case 0x0041 :                                                                                                       PARAM_KEY("DHW_4");                                                        PARAM_VALUE_u8; // always 0 on tank-less model?
        case 0x0042 :                                                                                                       PARAM_KEY("DHW_5");                                                        PARAM_VALUE_u8; // always 0 on tank-less model?
        case 0x0047 :                                                                                                       PARAM_KEY("DHW_Boost_1");                                                  PARAM_VALUE_u8; // always 0 on tank-less model?
        case 0x0048 :                                                                                                       PARAM_KEY("DHW_Boost_2");                                                  PARAM_VALUE_u8; // always 0 on tank-less model?
        case 0x0050 :                                                                                                       PARAM_KEY("DHW_Demand");                                                   PARAM_VALUE_u8; // DHW demand (DHW flow sensor)
        case 0x0055 :                                                                                                       PARAM_KEY("Climate_WD_Program_2");                                         PARAM_VALUE_u8; // A.3.1.1.1 Abs+dev / WD+dev / Abs+prog / WD+prog
        case 0x0056 :                                                                                                       PARAM_KEY("Climate_WD_Program_3");                                         PARAM_VALUE_u8; // Abs+dev / WD+dev / Abs+prog / WD+prog // bits 0-1 separately? for Climate_Mode_Wd and Climate_Mode_use_deviation
        case 0x005D :                                                                                                       PARAM_KEY("External_Thermostat_Heating");                                  PARAM_VALUE_u8; // external thermostat relay inputs (X2M-1,2,1a,2a)
        case 0x005E :                                                                                                       PARAM_KEY("External_Thermostat_Cooling");                                  PARAM_VALUE_u8;
        case 0x005F :                                                                                                       PARAM_KEY("External_Thermostat_Heating_Add");                              PARAM_VALUE_u8;
        case 0x0060 :                                                                                                       PARAM_KEY("External_Thermostat_Cooling_Add");                              PARAM_VALUE_u8;
        case 0x009D :                                                                                                       PARAM_KEY("Counter_Schedules");                                            PARAM_VALUE_u8; // increments at scheduled changes
        case 0x013A ... 0x0145 : return 0;                                                                                                                                                                             // characters for product name
        case 0x00A2 : return 0;                                                                                                                                                                                        // useless? sequence counter
        case 0x0013 :                                                                                                       PARAM_KEY("Climate_On_Q");                                                 PARAM_VALUE_u8; //  0x00 0x01 // was Reboot_Related_Q12-35-13
        case 0x0023 :                                                                                                       PARAM_KEY("Reboot_Related_Q13-35-23");                                     PARAM_VALUE_u8; //  0x00 0x01 UI/LCD?
        case 0x0027 :                                                                                                       PARAM_KEY("Reboot_Related_Q14-35-27");                                     PARAM_VALUE_u8; //  0x00 0x01 UI/LCD?
        case 0x004D :                                                                                                       PARAM_KEY("Reboot_Related_Q15-35-4D");                                     PARAM_VALUE_u8; //  0x00 0x01
        case 0x004E :                                                                                                       PARAM_KEY("Reboot_Related_Q15-35-4E");                                     PARAM_VALUE_u8; //
        case 0x005A :                                                                                                       PARAM_KEY("Reboot_Related_Q16-35-5A");                                     PARAM_VALUE_u8; //  0x00 0x01 related to F036-0x0002
        case 0x005C :                                                                                                       PARAM_KEY("Restart_Byte_2");                                               PARAM_VALUE_u8hex; //  0x00 0x7F
        case 0x009B :                                                                                                       PARAM_KEY("Reboot_Related_Q18-35-9B");                                     PARAM_VALUE_u8; //  0x00 0x01 0x02 // RR RT-ext LWT?
        case 0x0021 : // fallthrough
        case 0x0022 : // fallthrough
        case 0x004F : // fallthrough
        case 0x0088 : // fallthrough
        case 0x008D : // fallthrough
        case 0x0090 : // fallthrough // changes on/off when? <= copy of 400012 byte 10 bit 2
        case 0x0093 : // fallthrough
        case 0x0098 : // fallthrough // changes wd+prog -> wd
        case 0x009A : // fallthrough // copy of 0x400020  byte 16
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
        case 0x0122 : UNKNOWN_PARAM8;
        case 0xFFFF : return 0;
        default     : UNKNOWN_PARAM8;
        }
      default   : return 0;
      }

// 36: 16-bit, use M.paramValue8
//
// includes my F036 observed parameters
// and those from from https://github.com/budulinek/Daikin-P1P2---UDP-Gateway/blob/main/Payload-data-write.md
// not sure if they apply to this model:
// 00 room temp setpoint
// 03 DHW setpoint
// 06 LWT setpoint main zone, only in fixed LWT mode
// 08 LWT setpoint deviation main zone, only in weather-dependent LWT mode
// 0B LWT setpoint additional zone, only in fixed LWT mode
// 0D LWT setpoint deviation additional zone, only in weather-dependent LWT mode

    case 0x36 :
                                                                // SETTINGS and MEASUREMENT // Temperatures and system operation
                switch (paramSrc) {
      case 0x00 :
      case 0x40 : HATEMP1;
                  switch (paramNr) {
        case 0x0000 :                            CAT_SETTING;                                                               PARAM_KEY("Room_Heating_Setpoint");                                        PARAM_VALUE_u16div10_BE;
        case 0x0001 :                            CAT_SETTING;                                                               PARAM_KEY("Room_Cooling_Setpoint");                                        PARAM_VALUE_u16div10_BE;
        case 0x0002 :                            CAT_TEMP;                                                                  PARAM_KEY("Temperature_Room_Wall_2");                                      PARAM_VALUE_u16div10_BE; // Temproom1 (reported by main controller)

        case 0x0003 : SUBDEVICE("_DHW");         CAT_SETTING;                                                               PARAM_KEY("Setpoint_DHW_1");                                               PARAM_VALUE_u16div10_BE; // = 0x13-0x40-0
        case 0x0004 : SUBDEVICE("_DHW");         CAT_SETTING;                                                               PARAM_KEY("Setpoint_DHW_2");                                               PARAM_VALUE_u16div10_BE;
        case 0x0005 : SUBDEVICE("_DHW");         CAT_TEMP;                                 HYST_S16_BE(1);                  PARAM_KEY("Temperature_R5T_DHW_Tank");                                     PARAM_VALUE_s16div10_BE; // R5T DHW_tank temp
        case 0x0006 :                            CAT_SETTING;                                                               PARAM_KEY("LWT_Heating_Main");                                             PARAM_VALUE_u16div10_BE; // writable
        case 0x0007 :                            CAT_SETTING;                                                               PARAM_KEY("LWT_Cooling_Main");                                             PARAM_VALUE_u16div10_BE;
        case 0x0008 :                            CAT_SETTING;                                                PRECISION(0);  PARAM_KEY("LWT_Deviation_Heating_Main");                                   PARAM_VALUE_s16div10_BE;
        case 0x0009 :                            CAT_SETTING;                                                PRECISION(0);  PARAM_KEY("LWT_Deviation_Cooling_Main");                                   PARAM_VALUE_s16div10_BE;
        case 0x000A :                            CAT_SETTING;                                                               PARAM_KEY("LWT_Setpoint_Main");                                            PARAM_VALUE_u16div10_BE; // (in 0.1 C, Abs or WD-based)
        case 0x000B :                            CAT_SETTING;                                                               PARAM_KEY("LWT_Heating_Add");                                              PARAM_VALUE_u16div10_BE;
        case 0x000C :                            CAT_SETTING;                                                               PARAM_KEY("LWT_Cooling_Add");                                              PARAM_VALUE_u16div10_BE;
        case 0x000D :                            CAT_SETTING;                                                PRECISION(0);  PARAM_KEY("LWT_Deviation_Heating_Add");                                    PARAM_VALUE_s16div10_BE;
        case 0x000E :                            CAT_SETTING;                                                PRECISION(0);  PARAM_KEY("LWT_Deviation_Cooling_Add");                                    PARAM_VALUE_s16div10_BE;
        case 0x000F :                            CAT_SETTING;                                                               PARAM_KEY("LWT_Setpoint_Add");                                             PARAM_VALUE_u16div10_BE; // (in 0.1 C, Abs or WD-based)
        case 0x0010 : SUBDEVICE("_Unknown");     CAT_SETTING;        HACONFIG; HATEMP1;                                     PARAM_KEY("Setpoint_Room_Q");                                              PARAM_VALUE_u16div10_BE; // 30.0 (~ LWT Main ? Abs ?)
        case 0x0011 :                            CAT_TEMP;                                 HYST_S16_BE(1);                  PARAM_KEY("Temperature_Outside_Unit");                                     PARAM_VALUE_s16div10_BE; // Tempout1  in 0.5 C // ~ 0x11-0x40-5
        case 0x0012 :                            CAT_TEMP;                                 HYST_S16_BE(1);                  PARAM_KEY("Temperature_Outside");                                          PARAM_VALUE_s16div10_BE; // Tempout2 in 0.1 C, from outside unit of from external outside sensor
        case 0x0013 :                            CAT_TEMP;                                 HYST_U16_BE(1);                  PARAM_KEY("Temperature_Room_3");                                           PARAM_VALUE_u16div10_BE; // Temproom2 (copied by Daikin system with minor delay)
        case 0x0014 :                            CAT_TEMP;                                 HYST_U16_BE(1);                  PARAM_KEY("Temperature_RWT");                                              PARAM_VALUE_u16div10_BE; // TempRWT rounded down/different calculus (/255?)?
        case 0x0015 :                            CAT_TEMP;                                 HYST_U16_BE(1);                  PARAM_KEY("Temperature_MWT");                                              PARAM_VALUE_u16div10_BE; // TempMWT
        case 0x0016 :                            CAT_TEMP;                                 HYST_U16_BE(1);                  PARAM_KEY("Temperature_LWT");                                              PARAM_VALUE_u16div10_BE; // TempLWT rounded DOWN (or different calculus than /256; perhaps /255?)
        case 0x0017 :                            CAT_TEMP;                                 HYST_S16_BE(1);                  PARAM_KEY("Temperature_Refrigerant_1");                                    PARAM_VALUE_s16div10_BE; // TempRefr1
        case 0x0018 :                            CAT_TEMP;                                 HYST_S16_BE(1);                  PARAM_KEY("Temperature_Refrigerant_2");                                    PARAM_VALUE_s16div10_BE; // TempRefr2
        case 0x0027 : SUBDEVICE("_DHW");         CAT_SETTING;                              HYST_U16_BE(1);                  PARAM_KEY("Setpoint_DHW_3");                                               PARAM_VALUE_u16div10_BE; // = 0x13-0x40-0 ??  // 0x13-0x40-[1-7] unknown;  = 0x0028-0x0029 ?
        case 0x002A :                            CAT_MEASUREMENT;              HANONE;     HYST_U16_BE(1);                  PARAM_KEY("Flow");                                                         PARAM_VALUE_u16div10_BE; // Flow  0x13-0x40-9
        case 0x002B :                            CAT_SETTING;                  HANONE;                                      PARAM_KEY("SW_Version_Inside_Unit");                                       PARAM_VALUE_u16hex_BE;
        case 0x002C : if (!payload[payloadIndex] && !payload[payloadIndex - 1]) return 0;
                                                 CAT_SETTING;                  HANONE;                                      PARAM_KEY("SW_Version_Outside_Unit");                                      PARAM_VALUE_u16hex_BE;
        case 0x002D :                            CAT_SETTING;                  HANONE;                                      PARAM_KEY("Unknown_P36_002D");                                             PARAM_VALUE_u16hex_BE; // no Temp. value 0016 or 040C. Schedule related ?
        case 0x002E :                            CAT_SETTING;                  HANONE;                                      PARAM_KEY("Unknown_P36_002E");                                             PARAM_VALUE_u16hex_BE; // 0x0000
        case 0xFFFF : return 0;
        default     : UNKNOWN_PARAM16;
        }
      default   : return 0;
      }
    case 0x37 :
                switch (paramSrc) {
      case 0x00 : // 0x01010D 0x010D00 u24hex_BE eboot related?
      case 0x40 :
                  switch (paramNr) {
        case 0xFFFF : return 0;
        default     : UNKNOWN_PARAM24;
        }
      default   : return 0;
      }
    case 0x38 : switch (paramSrc) {
      case 0x00 :                                CAT_COUNTER;
                  switch (paramNr) {
        case 0x0000 :                                                          HAKWH;                                       PARAM_KEY("Electricity_Consumed_Backup1_Heating");                          PARAM_VALUE_u32_BE;
        case 0x0001 :                                                          HAKWH;                                       PARAM_KEY("Electricity_Consumed_Backup1_DHW");                              PARAM_VALUE_u32_BE;
        case 0x0002 :
                      if (!EE.useTotal) {
                        uint32_t v = u_payloadValue_BE(payload + payloadIndex, paramValLength);
                        if (v > M.R.electricityConsumedCompressorHeating) {
                          M.R.electricityConsumedCompressorHeating = v;
                          pseudo0B = 9;
                          if (EE.D13 & 0x01) {
                            EE.electricityConsumedCompressorHeating1 = v;
                            printfTopicS("Set consumption to %i (38)", v);
                            EE_dirty = 1;
                            EE.D13 &= 0xFE;
                            pseudo0F = 9;
                            if (!EE.D13) printfTopicS("Use D5 to save consumption/production counters %i/%i", EE.electricityConsumedCompressorHeating1, EE.energyProducedCompressorHeating1);
                          }
                        }
                      }
                                                                               HAKWH;                                       PARAM_KEY("Electricity_Consumed_Compressor_Heating");                      PARAM_VALUE_u32_BE;
        case 0x0003 :                                                          HAKWH;                                       PARAM_KEY("Electricity_Consumed_Compressor_Cooling");                      PARAM_VALUE_u32_BE;
        case 0x0004 :                                                          HAKWH;                                       PARAM_KEY("Electricity_Consumed_Compressor_DHW");                          PARAM_VALUE_u32_BE;
        case 0x0005 :
                      if (EE.useTotal) {
                        uint32_t v = u_payloadValue_BE(payload + payloadIndex, paramValLength);
                        if (v > M.R.electricityConsumedCompressorHeating) {
                          M.R.electricityConsumedCompressorHeating = v;
                          pseudo0B = 9;
                          if (EE.D13 & 0x01) {
                            EE.electricityConsumedCompressorHeating1 = v;
                            printfTopicS("Set consumption to %i (38)", v);
                            EE_dirty = 1;
                            EE.D13 &= 0xFE;
                            pseudo0F = 9;
                            if (!EE.D13) printfTopicS("Use D5 to save consumption/production counters %i/%i", EE.electricityConsumedCompressorHeating1, EE.energyProducedCompressorHeating1);
                          }
                        }
                      }
                                                                               HAKWH;                                       PARAM_KEY("Electricity_Consumed_Total");                                   PARAM_VALUE_u32_BE;

        case 0x0006 :
                      {
                        uint32_t v = u_payloadValue_BE(payload + payloadIndex, paramValLength);
                        if (v > M.R.energyProducedCompressorHeating) {
                          M.R.energyProducedCompressorHeating = v;
                          pseudo0B = 9;
                          if (EE.D13 & 0x02) {
                            EE.energyProducedCompressorHeating1 = v;
                            printfTopicS("Set production to %i (38)", v);
                            EE_dirty = 1;
                            EE.D13 &= 0xFD;
                            pseudo0F = 9;
                            if (!EE.D13) printfTopicS("Use D5 to save consumption/production counters %i/%i", EE.electricityConsumedCompressorHeating1, EE.energyProducedCompressorHeating1);
                          }
                        }
                      }
                                                                               HAKWH;                                       PARAM_KEY("Energy_Produced_Compressor_Heating");                           PARAM_VALUE_u32_BE;

        case 0x0007 :                                                          HAKWH;                                       PARAM_KEY("Energy_Produced_Compressor_Cooling");                           PARAM_VALUE_u32_BE;
        case 0x0008 :                                                          HAKWH;                                       PARAM_KEY("Energy_Produced_Compressor_DHW");                               PARAM_VALUE_u32_BE;
        case 0x0009 :                                                          HAKWH;                                       PARAM_KEY("Energy_Produced_Compressor_Total");                             PARAM_VALUE_u32_BE;
        case 0x000D :                                                          HAHOURS;                                     PARAM_KEY("Hours_Circulation_Pump");                                       PARAM_VALUE_u32_BE;
        case 0x000E :                                                          HAHOURS;                                     PARAM_KEY("Hours_Compressor_Heating");                                     PARAM_VALUE_u32_BE;
        case 0x000F :                                                          HAHOURS;                                     PARAM_KEY("Hours_Compressor_Cooling");                                     PARAM_VALUE_u32_BE;
        case 0x0010 :                                                          HAHOURS;                                     PARAM_KEY("Hours_Compressor_DHW");                                         PARAM_VALUE_u32_BE;
        case 0x0011 :                                                          HAHOURS;                                     PARAM_KEY("Hours_Backup1_Heating");                                        PARAM_VALUE_u32_BE;
        case 0x0012 :                                                          HAHOURS;                                     PARAM_KEY("Hours_Backup1_DHW");                                            PARAM_VALUE_u32_BE;
        case 0x0013 :                                                          HAHOURS;                                     PARAM_KEY("Hours_Backup2_Heating");                                        PARAM_VALUE_u32_BE;
        case 0x0014 :                                                          HAHOURS;                                     PARAM_KEY("Hours_Backup2_DHW");                                            PARAM_VALUE_u32_BE;
        case 0x0015 :                                                          HAHOURS;                                     PARAM_KEY("Hours_Unknown_Function");                                       PARAM_VALUE_u32_BE;
        case 0x0016 :                                                          HAHOURS;                                     PARAM_KEY("Hours_Drain_Pan_Heater");                                       PARAM_VALUE_u32_BE; // Drain pan heater (BPV/bodemplaat verwarming), thanks to Pim Snoeks for finding this function
        case 0x001A :                                                          HAHOURS;                                     PARAM_KEY("Hours_Gasboiler_Heating");                                      PARAM_VALUE_u32_BE;
        case 0x001B :                                                          HAHOURS;                                     PARAM_KEY("Hours_Gasboiler_DHW");                                          PARAM_VALUE_u32_BE;
        case 0x001C :                                                          HAHOURS;                                     PARAM_KEY("Starts_Compressor");                                            PARAM_VALUE_u32_BE;
        case 0x001D :                                                          HAHOURS;                                     PARAM_KEY("Starts_Gasboiler");                                             PARAM_VALUE_u32_BE;
        case 0x001E :                            CAT_SETTING;                  HACOST;                                      PARAM_KEY("Electricity_Price_2");                                          PARAM_VALUE_u32div100_BE;
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
      case 0x00 : // SRC 2
      case 0x40 : // SRC 3
                  FSB = 0;
                  SUBDEVICE("_FieldSettings");
                  HACONFIG;
                  HADEVICE_SENSOR_VALUE_TEMPLATE("{{value_json.val}}");
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
        case 0x0000 :                                                                                                       PARAM_KEY("Format_24h");                                                   PARAM_VALUE_u8;
        case 0x0031 :                                                                                                       PARAM_KEY("Holiday_Enable");                                               PARAM_VALUE_u8;
        case 0x003B :                                                                                                       PARAM_KEY("Decimal_Delimiter");                                            PARAM_VALUE_u8;
        case 0x003D :                                                                                                       PARAM_KEY("Unit_Flow");                                                    PARAM_VALUE_u8;
        case 0x003F :                                                                                                       PARAM_KEY("Unit_Temperature");                                             PARAM_VALUE_u8;
        case 0x0040 :                                                                                                       PARAM_KEY("Unit_Energy");                                                  PARAM_VALUE_u8;
        case 0x004B :                                                                                                       PARAM_KEY("Unit_DST");                                                     PARAM_VALUE_u8;
        case 0x004C : M.R.quietMode = payload[payloadIndex]; pseudo0D = 9;                                                  PARAM_KEY("Quiet_Mode_UI");                                                PARAM_VALUE_u8; // interface 0=auto 1=always_off 2=on
        case 0x004D : M.R.quietLevel = payload[payloadIndex]; pseudo0D = 9;                                                 PARAM_KEY("Quiet_Level_UI");                                               PARAM_VALUE_u8; // 0=mild .. 2=most silent (different encoding from basic package)
        case 0x004E :                                                                                                       PARAM_KEY("Climate_Heating_Cooling_Auto");                                 PARAM_VALUE_u8; // heating/cooling/auto
        case 0x005B :                                                                                                       PARAM_KEY("Holiday");                                                      PARAM_VALUE_u8;
        case 0x005E :                                                                                                       PARAM_KEY("Schedule_Heating");                                             PARAM_VALUE_u8;
        case 0x005F :                                                                                                       PARAM_KEY("Schedule_Cooling");                                             PARAM_VALUE_u8;
        case 0x0064 :                                                                                                       PARAM_KEY("Schedule_DHW");                                                 PARAM_VALUE_u8;
        case 0x005D :                                                                                                       PARAM_KEY("DHW_Temp_Semidegree");                                          PARAM_VALUE_u8; // semi degrees; 0x00 = -16C; 0x98 = 60C
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
        case      : FIELDKEY("[____]_7.4.1.1_Setpoint_Room_Heating_Comfort");   // step : A.3.2.4, range [3-07 - 3-06]
        case      : FIELDKEY("[____]_7.4.1.2_Setpoint_Room_Cooling_Eco");
        case      : FIELDKEY("[____]_7.4.1.3_Setpoint_Room_Heating_Comfort");   // step A.3.2.4, range [3-09 - 3-08 ]
        case      : FIELDKEY("[____]_7.4.1.4_Setpoint_Room_Cooling_Eco");
        case      : FIELDKEY("[____]_7.4.2.5_Setpoint_Room_Heating_Comfort");   // Deviation -10 .. +10
        case      : FIELDKEY("[____]_7.4.2.6_Setpoint_Room_Cooling_Eco");
        case      : FIELDKEY("[____]_7.4.2.7_Setpoint_Room_Heating_Comfort");
        case      : FIELDKEY("[____]_7.4.2.8_Setpoint_Room_Cooling_Eco");
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
        case      : FIELDKEY("[____]_7.4.2.5_LWT_Main_Deviation_Heating_ComfortSetpoint_Room_Heating_Comfort");   // Deviation -10 .. +10 // cannot access these in WD mode?
        case      : FIELDKEY("[____]_7.4.2.6_Setpoint_Room_Cooling_Eco");
        case      : FIELDKEY("[____]_7.4.2.7_Setpoint_Room_Heating_Comfort");
        case      : FIELDKEY("[____]_7.4.2.8_Setpoint_Room_Cooling_Eco");
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
        case 0x001E : return 0; // electricity price, not needed, use data from main packets
        case 0x001F : return 0; // gas price, not needed, use data from main packets
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
#endif /* SAVESCHEDULE */

uint16_t parameterWritesDone = 0; // # writes done by ATmega; perhaps useful for ESP-side queueing

byte fieldSettingPublishNr = 0xF1; // 0xF1 = ready
#endif /* E_SERIES */

byte bytesbits2keyvalue(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, byte bitNr) {
// A payloadIndex value EMPTY_PAYLOAD indicates an empty payload (used during restart)
// payloadIndex count payload bytes starting at 0 (following Budulinek's suggestion)

#ifdef E_SERIES
#ifdef SAVESCHEDULE
  bool newMem = 0;
  static byte newSched[2];
  static byte weeklyMomentCounter[2];
  static byte dailyMomentsCounter[2];
  static byte dailyMoments[2];
  static byte dayCounter[2];
  static bool isWeeklySchedule[2];
  static bool isSilentSchedule[2];
  static bool isHCSchedule[2];
  static bool isElectricityPriceSchedule[2];
  static uint16_t byteLoc2[2];
#endif /* SAVESCHEDULE */
#endif /* E_SERIES */

  haConfig = 0;

  byte payloadByte = 0;
  byte* payloadPointer = 0;
  if (payloadIndex != EMPTY_PAYLOAD) {
    payloadByte = payload[payloadIndex];
    payloadPointer = &payload[payloadIndex];
  };

  HARESET;
  CAT_UNKNOWN; // cat unknown unless changed below
  HYST_RESET;
  SUBDEVICE("");
  maxOutputFilter = 9; // default all changes visible, unless changed below

#ifdef E_SERIES

  // E_SERIES

  byte src;
  switch (packetSrc) {
    case 0x00 : src = 0;
                break;
    case 0x40 : src = 1;
                break;
    case 0x80 : src = 'A' - '0';
                break;
    default   : src = 'H' - '0';
                break;
  }
  if ((packetType & 0xF0) == 0x30) {
    src += 2;  // 2-7 from/to auxiliary controller, 2-3 for F0
    if (payload[-2] == 0xF1) src += 2; // 4-5 for F1
    if (payload[-2] == 0xFF) src += 4; // 6-7 for F2
  }
  if ((packetType & 0xF8) == 0x08) src += 8;          // 8-9 pseudo-packets ESP / ATmega
  if ((packetType & 0xF8) == 0x00) src = ('B' - '0'); // B boot messages 0x00-0x07
  SRC(src); // set SRC char in mqttTopic
  switch (packetType) {
    // Restart packet types 00-05 (restart procedure 00-05 followed by 20, 60-9F, A1, B1, B8, 21)
    case 0xFF :
                switch (payloadIndex) {
      case  EMPTY_PAYLOAD : KEYHEADER("RestartDummyMsg");
      default   : UNKNOWN_BYTE; // not observed
    }
    case 0x00 :
                switch (payloadIndex) {
      case  EMPTY_PAYLOAD : printfTopicS("Restart phase 0");
                            KEYHEADER("RestartPhase0");
      default   : UNKNOWN_BYTE; // not observed
    }
    case 0x01 :
                switch (payloadIndex) {
      case  EMPTY_PAYLOAD : KEYHEADER("RestartPhase1");
      default   : UNKNOWN_BYTE; // not observed
    }
    case 0x03 :
                pi2 = -1; /// crucial to avoid check/history-saving for 0x80 boot messages
                switch (payloadIndex) {
      case EMPTY_PAYLOAD : KEYHEADER("RestartPhase3");
      case    0 :                                                                                                           KEY("RestartPhase3bitPatternNr");                                          VALUE_u8hex;
      case    1 : // fallthrough
      case    2 : return 0;
      case    3 :                                                                                                           KEY("RestartPhase3bitPattern");                                            VALUE_u24hex_LE;
      default   : UNKNOWN_BYTE; // not observed
    }
    case 0x04 :
                pi2 = -1; /// crucial to avoid check/history-saving for 0x80 boot messages
                switch (payloadIndex) {
      case EMPTY_PAYLOAD : KEYHEADER("RestartPhase4");
      case    0 : // fallthrough
      case    1 : // fallthrough
      case    2 : return 0;
      case    3 :                                                                                                           KEY("RestartPhase4data1");                                                 VALUE_u32hex_LE;
      default   : UNKNOWN_BYTE; // not observed
    }
    case 0x05 :
                pi2 = -1; /// crucial to avoid check/history-saving for 0x80 boot messages
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case EMPTY_PAYLOAD : KEYHEADER("PacketTarget");
        case    0 : // fallthrough
        case    1 : // fallthrough
        case    2 : return 0;
        case    3 :                                                                                                         KEY("RestartPhase4data2");                                                 VALUE_u32hex_LE;
        default   : UNKNOWN_BYTE; // not observed
      }
      case 0x40 : switch (payloadIndex) {
        case    2 : // fallthrough
        case    5 : // fallthrough
        case    8 : // fallthrough
        case   11 : // fallthrough
        case   14 : // fallthrough
        case   17 :                                                                                                         KEY("RestartPhase5data1");                                                 VALUE_u24hex_LE;
        case   19 :                                                                                                         KEY("RestartPhase5data2");                                                 VALUE_u16hex_LE;
        default   : return 0; // 0th and every 3rd byte 01, others 00, likely no info here
      }
    }
    // Main package communication packet types 10-16
    // STARTHERE

    case 0x10 :                                  CAT_SETTING;
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : switch (bitNr) {
          case    8 : bcnt = 27; BITBASIS;
          case    0 : SUBDEVICE("_Mode");
                      HACONFIG;
                      // CHECK(1) already done by BITBASIS
                      CHECKBIT;
                      KEY("Altherma_On"); // was Climate_LWT; in LWT mode, determines on/off (in RT mode, Climate_Room determines on/off)
                      QOS_SWITCH;
                      if (pubHaBit) {
                        HADEVICE_SWITCH;
                        HADEVICE_SWITCH_PAYLOADS("E35002F00 35003100 35002D00", "E35002F01 35003101 35002D01");
                        HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                        PUB_CONFIG;
                      }
                      CHECK_ENTITY;
                      VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case    1 : switch (bitNr) {
          case    8 :
                      if ((M.R.climateModeACH & 0x03) != (payloadByte & 0x03)) {
                        M.R.climateModeACH = (payloadByte & 0x03) | (M.R.climateModeACH & 0xFC); // set bit 1,0 to current CH mode
                        if (payloadByte & 0x03) M.R.climateModeACH = ((payloadByte & 0x03) << 4)  | (M.R.climateModeACH & 0x0F); // store "is/was-heating-request" in 0x30
                        pseudo0D = 9;
                      }
                      bcnt = 1; BITBASIS;
          case    0 : SUBDEVICE("_Mode"); KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_Heating_Request"); // =? Space_CH_Op_or_BPH // was Climate_LWT_OnOff
          case    1 : SUBDEVICE("_Mode"); KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_Cooling_Request");
          case    7 : SUBDEVICE("_Mode");                                      KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_Room");   // determines on/off in RT mode, usually equals _Mode
/*
          case    7 : SUBDEVICE("_Mode");
                      HACONFIG;
                      // CHECK(1) already done by BITBASIS
                      CHECKBIT;
                      KEY("Climate_Room_0"); // was Altherma_ON // was Climate_LWT
                      if (pubHaBit) {
                        HADEVICE_SWITCH;
                        HADEVICE_SWITCH_PAYLOADS("E35002F00 35003100 35002D00", "E35002F01 35003101 35002D01");
                        HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                        PUB_CONFIG;
                      }
                      CHECK_ENTITY;
                      VALUE_flag8;
*/
          case    2 : KEYBIT_PUB_CONFIG_PUB_ENTITY("Reboot_Related_Q10-00-1-2");
          case    3 : KEYBIT_PUB_CONFIG_PUB_ENTITY("Reboot_Related_Q10-00-1-3");
          case    4 : KEYBIT_PUB_CONFIG_PUB_ENTITY("Reboot_Related_Q10-00-1-4");
          case    5 : KEYBIT_PUB_CONFIG_PUB_ENTITY("Reboot_Related_Q10-00-1-5");
          case    6 : KEYBIT_PUB_CONFIG_PUB_ENTITY("Reboot_Related_Q10-00-1-6");
          default   : UNKNOWN_BIT;
        }
        case    2 : switch (bitNr) {
          case    8 : bcnt = 2; BITBASIS;
          case    0 : SUBDEVICE("_DHW"); KEYBIT_PUB_CONFIG_PUB_ENTITY("DHW_Request_19Q");
          default   : UNKNOWN_BIT;
        }
        case    7 : return 0;
        case    8 : HACONFIG;
                    HATEMP1;
                    SUBDEVICE("_Room");
                    QOS_CLIMATE;
                    KEY2_PUB_CONFIG_CHECK_ENTITY("Room_Heating_Setpoint");
                    VALUE_f8s8_LE;
        case    9 : switch (bitNr) {
          case    8 : if (M.R.climateModeACH != (((payloadByte & 0x20) >> 3) | (M.R.climateModeACH & 0xF3))) {
                        pseudo0D = 9;
                        M.R.climateModeACH = (((payloadByte & 0x20) >> 3) | (M.R.climateModeACH & 0xF3));
                      }
                      bcnt = 3; BITBASIS;
                      // bits 0..4 reboot related ?
          case    5 : SUBDEVICE("_Mode");                            HACONFIG; KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_HC_Auto");
          case    6 : // may call 6 and/or 7 in new method; so need to fall-through here:
          case    7 : HACONFIG;
                      CHECKBITS(6, 7);
                      SUBDEVICE("_Quiet");
                      KEY("Quiet_Level");
                      QOS_SELECT;
                      if (pubHa) {
                        HADEVICE_SELECT;
                        HADEVICE_SELECT_OPTIONS("\"Quiet Level 0\",\"Quiet Level 1\",\"Quiet Level 2\",\"Quiet Level 3\"", "'0':'Quiet Level 0','1':'Quiet Level 1','2':'Quiet Level 2','3':'Quiet Level 3'");
                        HADEVICE_SELECT_COMMAND_TEMPLATE("{% set modesL={'Quiet Level 1':1,'Quiet Level 2':2,'Quiet Level 3':3}%} {{   ('E3A004D%02X 3A004C02'|format( ((modesL[value])|int)-1)) if value in modesL.keys() else 'E35000100'  }} ");
                        HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                        PUB_CONFIG;
                      }
                      CHECK_ENTITY;
                      VALUE_bits(6, 7);
          default   : UNKNOWN_BIT;
        }
        case   10 : switch (bitNr) {
          case    8 : bcnt = 4; BITBASIS;
          case    2 : // implied by Quiet_Mode based on 0x3A/0x0D info, but on some models 0x3A/0x0D does not output Quiet_Mode, so output it here (off/on only) too
                      HACONFIG;
                      CHECKBIT;
                      SUBDEVICE("_Quiet");
                      KEY("Quiet_Mode_Active");
                      PUB_CONFIG;
                      CHECK_ENTITY;
                      VALUE_flag8;
          default   : UNKNOWN_BIT;
        }
        case   12 : switch (bitNr) {
          case    8 : bcnt = 5; BITBASIS; // bit 3 reboot related?
          case    3 : KEYBIT_PUB_CONFIG_PUB_ENTITY("Reboot_Related_Q10-00-12-3");
          default   : UNKNOWN_BIT;
        }
        case   13 :                              CAT_UNKNOWN;                                                               KEY1_PUB_CONFIG_CHECK_ENTITY("Reboot_Related_Q10-00-13");                  VALUE_u8hex;
//      case   14 : // returns 0x08 or 0x00 on newer version E systems
        case   15 : return 0;
        case   16 : SUBDEVICE("_Room");
                    HACONFIG;
                    HATEMP1;
                    QOS_CLIMATE;
                    KEY2_PUB_CONFIG_CHECK_ENTITY("Room_Cooling_Setpoint");
                    VALUE_f8s8_LE;
        case   17 : if (!(M.R.useDHW & 0x02)) return 0;
                    SUBDEVICE("_DHW"); // DHW (tank) mode
                    switch (bitNr) {
          case    8 : bcnt = 6; BITBASIS;
          case    1 : SUBDEVICE("_DHW");
                      HACONFIG;
                      // CHECK(1) already done by BITBASIS
                      CHECKBIT;
                      KEY("DHW_Boost"); // was Climate_LWT
                      QOS_SWITCH;
                      if (pubHaBit) {
                        HADEVICE_SWITCH;
                        HADEVICE_SWITCH_PAYLOADS("E35004800", "E35004801");
                        HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                        PUB_CONFIG;
                      }
                      CHECK_ENTITY;
                      VALUE_flag8;
          case    6 :                                                HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("DHW_Related_Q");
          default   : UNKNOWN_BIT;
        }
        case   18 : return 0;
        case   19 : if (!(M.R.useDHW & 0x02)) return 0;
                    CHECK(2);
                    KEY("DHW_Setpoint");
                    HATEMP1;
                    HACONFIG;
                    SUBDEVICE("_DHW");
                    QOS_CLIMATE;
                    if (pubHa) {
                      HADEVICE_CLIMATE;
                      HADEVICE_CLIMATE_TEMPERATURE("S/0/DHW_Setpoint", 30, M.R.DHWsetpointMaxX10 * 0.1, 1)
                      HADEVICE_CLIMATE_MODES("S/1/DHW", "\"off\",\"heat\"", "'0':'off','1':'heat'")
                      if (M.R.useDHW & 0x01) HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R5T_DHW_Tank")
                      HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{{'E360003%04X'|format((value*10)|int)}}");
                      HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0,'heat':1} %}{{'E350040%02X 35003E%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_f8s8_LE; // 8 or 16 bit?
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : switch (bitNr) {
          case  8 : if ((M.R.climateModeACH & 0xC0) != ((payloadByte << 6) & 0xC0)) {
                      M.R.climateModeACH = ((payloadByte << 6) & 0xC0) | (M.R.climateModeACH & 0x3F); // set bit 1,0 to current CH mode
                      pseudo0D = 9;
                    }
                    bcnt = 7; BITBASIS;
          case  0 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_Active_Q4");
          default : UNKNOWN_BIT;
        }
        case    1 : switch (bitNr) {
          case    8 : bcnt = 8; BITBASIS;
          case    7 :                                                                                                       KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_Room_1");
          default   : UNKNOWN_BIT;
        }
        case    2 : switch (bitNr) {
          case    8 : bcnt = 9;
                      if ((payloadByte & 0x03) != M.R.HCmode) {
                        M.R.HCmode = payloadByte & 0x03;
                        UNSEEN_BYTE_00_30_0_CLIMATE_ROOM_HEATING;
                        UNSEEN_BYTE_00_30_1_CLIMATE_ROOM_COOLING;
                        UNSEEN_BYTE_00_30_2_CLIMATE_LWT_HEATING;
                        UNSEEN_BYTE_00_30_3_CLIMATE_LWT_COOLING;
                        UNSEEN_BYTE_00_30_4_CLIMATE_LWT_HEATING_ADD;
                        UNSEEN_BYTE_00_30_5_CLIMATE_LWT_COOLING_ADD;
                      }
                      BITBASIS;
          case    0 : SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_Heating"); // earlier than /0/ in auto mode // was: Valve_Heating
          case    1 : SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_Cooling"); // earlier than /0/ in auto mode // was: Valve_Cooling
          case    4 :                                                                                                       KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_HCA_Changed_Indicator"); // in auto mode, signals 1 until Climate_Mode_Heating /0/ matches above values
          case    5 : SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Valve_Zone_Main");
          case    6 : SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Valve_Zone_Add");
          case    7 : if (!(M.R.useDHW & 0x01)) return 0;
                      SUBDEVICE("_DHW");                             HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Valve_DHW_Tank"); // depends on DHW and on Heatpump/Compressor being active
          default   : UNKNOWN_BIT;
        }
        case    3 : switch (bitNr) {
          case    8 : bcnt = 10; BITBASIS;
          case    0 : if (!(M.R.useDHW & 0x02)) return 0;
                      SUBDEVICE("_DHW");                             HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("DHW"); // follows DHW_Request
          case    4 : if (!(M.R.useDHW & 0x01)) return 0;
                      SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("SHC_Tank");
          default   : UNKNOWN_BIT;
        }
        case    4 : return 0;
        case    5 : if (!(M.R.useDHW & 0x02)) return 0;
                    SUBDEVICE("_DHW");
                    HATEMP1;
                    KEY2_PUB_CONFIG_CHECK_ENTITY("DHW_Setpoint_1");
                    VALUE_f8s8_LE;
                    // bcnt = 11 no longer used here
        case    6 : return 0;
        case    7 : SUBDEVICE("_Room");                                        HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Room_Cooling_Setpoint_2");                   VALUE_f8s8_LE;
        case    8 : return 0;
        case    9 : SUBDEVICE("_Room");                                        HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Room_Heating_Setpoint_2");                   VALUE_f8s8_LE;
        case   10 : switch (bitNr) {
          case    8 : bcnt = 12; BITBASIS;
          case    5 :                                                                                                       KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_HC_Auto");




          case    6 : // may call 6 and/or 7 in new method; so need to fall-through here:
          case    7 : return 0;
                      SUBDEVICE("_Quiet");       CAT_SETTING;                                                               KEYBITS_PUB_CONFIG_PUB_ENTITY(6, 7, "Quiet_Level_1");                  /* VALUE_IN_KEYBITS */ // 2-bit : 01 = silent-level 1; 02= level 2; 03=level 3; and 0 if Quiet_Mode is 0 (off)
          default   : UNKNOWN_BIT; /* perhaps reboot related */
        }
        case   11 : switch (bitNr) {
          case    8 : bcnt = 13; BITBASIS;
          case    2 : return 0;
                      SUBDEVICE("_Quiet");       CAT_SETTING;                                                               KEYBIT_PUB_CONFIG_PUB_ENTITY("Quiet_Mode_1");                           /* VALUE_IN_KEYBIT */
          default   : UNKNOWN_BIT;
        }
        case   12 : SUBDEVICE("_Mode");          CAT_MEASUREMENT;    HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("ErrorCode1");                                VALUE_u8hex; // coding mechanism unknown. 024D2C = HJ-11 08B908 = 89-2 08B90C = 89-3
        case   13 : SUBDEVICE("_Mode");          CAT_MEASUREMENT;    HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("ErrorCode2");                                VALUE_u8hex;
        case   14 : SUBDEVICE("_Mode");          CAT_MEASUREMENT;    HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("ErrorSubCode");                              VALUE_u8hex; // >>2 for Subcode, and what are 2 lsbits ??
/*
        case   15 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Reboot_Related_Q10-40-15");                  VALUE_u8hex;
*/
        case   17 : switch (bitNr) {
          case    8 : bcnt = 14; BITBASIS;
          case    6 : SUBDEVICE("_Unknown");                         HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("BUH1_Q");
          case    1 : M.R.defrostActive = payloadByte & 0x02;
                      SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Defrost_Active");
          default   : UNKNOWN_BIT;
        }
        case   18 : switch (bitNr) {
          case    8 : M.R.compressor = payloadByte & 0x01; bcnt = 15; BITBASIS;
          case    0 : SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Compressor");
          case    3 : SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Circulation_Pump");
          case    5 : SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("BUH_2");
          default   : UNKNOWN_BIT;
        }
        case   19 : switch (bitNr) {
          case  8 : bcnt = 16; BITBASIS;
          case  1 : SUBDEVICE("_Mode");                              HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Gasboiler_Active"); // not gasboiler/BUH? //  : KEYBIT_PUB_CONFIG_PUB_ENTITY("DHWactive1"); // = BUH1?
          case  2 : if (!(M.R.useDHW & 0x02)) return 0;
                    SUBDEVICE("_DHW");                                                                                      KEYBIT_PUB_CONFIG_PUB_ENTITY("DHW_Mode");                         // KEYBIT_PUB_CONFIG_PUB_ENTITY("DHW_Mode_Q");
          default : UNKNOWN_BIT;
        }
        default :             UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x11 :                                  CAT_TEMP; // includes maxOutputFilter = 1;
                switch (packetSrc) { // temperatures+flow
      case 0x00 : switch (payloadIndex) {
        case    0 : return 0;
        case    1 :
                    SUBDEVICE("_Sensors");                           HACONFIG; HATEMP1;    HYST_F8_8_LE(20);                KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_Room_Wall"); {float roomTemp = FN_f8_8_LE(payloadPointer) + EE.RToffset * 0.01; VALUE_F_L(roomTemp, 2);}
        case    7 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Reboot_Related_Q11-00-7");                   VALUE_u8hex;
        // case   8 : // some version E types
        // case   9 : // some version E types
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : return 0;
        case    1 : if (EE.useR1T) return 0;
                    SUBDEVICE("_Sensors");
                    M.R.LWT = FN_f8_8_LE(payloadPointer)  + EE.R2Toffset * 0.01;
                    HACONFIG;
                    HATEMP1;
                    HYST_F8_8_LE(20);
                    KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_R2T_Leaving_Water");
                    VALUE_F_L(M.R.LWT, 2); // R2T
        case    2 : return 0;                         // Domestic hot water temperature (on some models)
        case    3 : if (!(M.R.useDHW & 0x01)) return 0;
                    SUBDEVICE("_DHW");
                    HACONFIG;
                    HATEMP1;
                    HYST_F8_8_LE(20);
                    KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_R5T_DHW_Tank");
                    VALUE_f8_8_LE;   // R5T DHW tank, unconnected on EHYHBX08AAV3?, then reading -40.7/-40.8
        case    4 : return 0;                         // Outside air temperature (low res)
        case    5 : SUBDEVICE("_Sensors");                           HACONFIG; HATEMP1;    HYST_F8_8_LE(20)                 KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_Outside_Unit");                  VALUE_f8_8_LE; // outside temperature outside unit (0.5C) , no RxT reference
        case    6 : return 0;
        case    7 : SUBDEVICE("_Sensors"); M.R.RWT = FN_f8_8_LE(payloadPointer) + EE.R4Toffset * 0.01;
                                                                     HACONFIG; HATEMP1;    HYST_F8_8_LE(20);                KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_R4T_Return_Water");              VALUE_F_L(M.R.RWT, 2); // R4T
        case    8 : return 0;
        case    9 : SUBDEVICE("_Sensors");
                    M.R.MWT = FN_f8_8_LE(payloadPointer) + EE.R1Toffset * 0.01;
                    if (EE.useR1T) M.R.LWT = M.R.MWT;
                    HACONFIG;
                    HATEMP1;
                    HYST_F8_8_LE(20);
                    if (EE.useR1T) {
                      KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_R1T_Leaving_Water");
                    } else {
                      KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_R1T_HP2Gas_Water");
                    }
                    VALUE_F_L(M.R.MWT, 2); // R1T Leaving water temperature on the heat exchanger
        case   10 : return 0;                         // Refrigerant temperature
        case   11 : SUBDEVICE("_Sensors");                           HACONFIG; HATEMP1;    HYST_F8_8_LE(20);                KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_R3T_Refrigerant");               VALUE_f8_8_LE;  // R3T refrigerant temp liquid side
        case   12 : return 0;                         // Room temperature copied with minor delay by Daikin system (perhaps different source in case of ext RT sensor)
        case   13 :
                    SUBDEVICE("_Sensors");                           HACONFIG; HATEMP1;    HYST_F8_8_LE(20);                KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_Room"); { float roomTemp = FN_f8_8_LE(payloadPointer) + EE.RToffset * 0.01; VALUE_F_L(roomTemp, 2); }
        case   14 : return 0;                         // Outside air temperature
        case   15 : SUBDEVICE("_Sensors");                           HACONFIG; HATEMP1;    HYST_F8_8_LE(20);                KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_Outside");                       VALUE_f8_8_LE; // outside air temperature based on outside unit or based or on R6T external sensor
        case   16 : return 0;                         // Unused ?
        case   17 : SUBDEVICE("_Unknown");                           HACONFIG; HATEMP1;    HYST_F8_8_LE(20);                KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_Unused_11_16");                  VALUE_f8_8_LE;
        case   18 : return 0;                         // Outside air temperature
        case   19 : SUBDEVICE("_Unknown");                           HACONFIG; HATEMP1;    HYST_F8_8_LE(20);                KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_Unused_11_18");                  VALUE_f8_8_LE;
        default   : UNKNOWN_BYTE;
      }
      default   : UNKNOWN_BYTE;
    }
    case 0x12 : switch (packetSrc) { // date+time
      case 0x00 :
                                                 CAT_MEASUREMENT; // includes maxOutputFilter = 1;
                  switch (payloadIndex) {
        case    0 : switch (bitNr) {
          case  8 : bcnt = 17; BITBASIS;
          case  1 : return 0; // suppress hourly pulse
                                                                                                                            KEYBIT_PUB_CONFIG_PUB_ENTITY("Hourly_Pulse_0");
          default : UNKNOWN_BIT;
        }
        case    1 : switch (payloadByte) {
                      case 0 : strncpy (timeString1, "Mo", 2); break;
                      case 1 : strncpy (timeString1, "Tu", 2); break;
                      case 2 : strncpy (timeString1, "We", 2); break;
                      case 3 : strncpy (timeString1, "Th", 2); break;
                      case 4 : strncpy (timeString1, "Fr", 2); break;
                      case 5 : strncpy (timeString1, "Sa", 2); break;
                      case 6 : strncpy (timeString1, "Su", 2); break;
                    }
                    timeString2[0] = timeString1[0];
                    timeString2[1] = timeString1[1];
                    return 0;
        case    2 : // Hour
                    timeString1[14] = '0' + (payloadByte / 10);
                    timeString1[15] = '0' + (payloadByte % 10);
                    return 0;
        case    3 : // Minute
                    // Switch to timeString1 if we have not seen a 00Fx31 payload for a few cycli
                    if (M.R.selectTimeString & 0x7F) M.R.selectTimeString --; // cannot be >0x7F so -- is allowed, will not distort 0x80
                    if (payloadByte != M.R.prevMin) {
                      M.R.prevMin = payloadByte;
                      // newMin = 1;
                      timeString1[17] = '0' + (payloadByte / 10);
                      timeString1[18] = '0' + (payloadByte % 10);
                      if (!M.R.selectTimeString) M.R.selectTimeString = 0x80;
                    }
                    return 0;
        case    4 : // Year
                    timeString1[5] = '0' + (payloadByte / 10);
                    timeString1[6] = '0' + (payloadByte % 10);
                    return 0;
        case    5 : // Month
                    timeString1[8] = '0' + (payloadByte / 10);
                    timeString1[9] = '0' + (payloadByte % 10);
                    return 0;
        case    6 : // Day
                    maxOutputFilter = 2;
                    timeString1[11] = '0' + (payloadByte / 10);
                    timeString1[12] = '0' + (payloadByte % 10);
                    if (M.R.selectTimeString & 0x80) {
                      SUBDEVICE("_Mode");
                      M.R.selectTimeString = 0;
                      SRC(9);
                      maxOutputFilter = 2;
                      HACONFIG;
                      CHECK(0);
                      KEY("Date_Time_Daikin");
                      PUB_CONFIG;
                      CHECK_ENTITY;
                      VALUE_textString(timeString1);
                    }
                    return 0;
        case   12 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Reboot_Trigger");                            VALUE_u8hex; // Reboot trigger?
        case   13 : bcnt = 18; BITBASIS_UNKNOWN;
        case   14 : switch (bitNr) {
          case    8 : bcnt = 25; BITBASIS;
          case    6 : SUBDEVICE("_Mode");                                                                                   KEYBIT_PUB_CONFIG_PUB_ENTITY("Defrost_Request");
          default   : UNKNOWN_BIT;
        }
        default   : UNKNOWN_BYTE;
      }
      case 0x40 :
                                                 CAT_SETTING; // PacketSrc 40 PacketType12 system settings and operation
                  switch (payloadIndex) {
        case    1 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Reboot_Related_Q12-40-1");                   VALUE_u8hex; // 0x00 0x40
        case    9 : switch (bitNr) {
          case    8 : bcnt = 28; BITBASIS;
          case    2 : SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("External_Thermostat_Heating"); // 0x35-5D:0x01
          case    1 : SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("External_Thermostat_Cooling"); // 0x35-5E:0x01
          case    0 : SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("External_Thermostat_Heating_Add"); // 0x35-5F:0x01
          default   : UNKNOWN_BIT;
        }
        case   10 : switch (bitNr) {
          case    8 : bcnt = 19; BITBASIS;
          case    7 : SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("External_Thermostat_Cooling_Add"); // 0x35-60:0x01
          case    4 : SUBDEVICE("_Mode");                            HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Preferential_Mode");
//        case    0 : copied to param 0x35 nr 90
          default   : UNKNOWN_BIT;
        }

        case   11 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Restart_Byte");                              VALUE_u8hex;
        case   12 : switch (bitNr) {
          case    8 : bcnt = 20; BITBASIS;
          case    0 : SUBDEVICE("_Unknown");                         HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_On_Q"); // heatpump_enabled??? (preferential related ?)
          case    2 : SUBDEVICE("_Unknown");                         HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_Related_Q");
          case    6 : SUBDEVICE("_Unknown");                         HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_Active_Q"); // status depends both on DHW on/off and heating on/off ? // was System_On_Q
          case    7 : if (!(M.R.useDHW & 0x02)) return 0;
                      SUBDEVICE("_DHW");                             HACONFIG;                                              KEYBIT_PUB_CONFIG_PUB_ENTITY("DHW_Demand");
          default   : UNKNOWN_BIT;
        }
        case   13 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Reboot_Related_Q12-40-13");                  VALUE_u8hex; // 0x00 0x04, or 0x0C during boot?
        case   14 : switch (bitNr) {
          case    8 : bcnt = 26; BITBASIS;
          case    6 : SUBDEVICE("_Mode"); /*                         HACONFIG; */                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Defrost_Response");  // only acknowledges Defrost_Request, no need to show in HA
          default   : UNKNOWN_BIT;
        }
        default : UNKNOWN_BYTE;
      }
      default   : UNKNOWN_BYTE;
    }
    case 0x13 :
                CAT_SETTING; // PacketType13 system settings and operation, and flow measurement
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("UI_Reboot_Related_Q");                       VALUE_u8hex;
        case    2 :
                    switch (bitNr) {
          case    8 : if (M.R.climateMode != (payloadByte & 0xC0)) {
                        M.R.climateMode = payloadByte & 0xC0;
                        pseudo0D = 9;
                        UNSEEN_BYTE_00_30_2_CLIMATE_LWT_HEATING;
                        UNSEEN_BYTE_00_30_3_CLIMATE_LWT_COOLING;
                        UNSEEN_BYTE_00_30_4_CLIMATE_LWT_HEATING_ADD;
                        UNSEEN_BYTE_00_30_5_CLIMATE_LWT_COOLING_ADD;
                      }
                      bcnt = 21; BITBASIS;
          case    7 : // may call 6 and/or 7 in new method; so need to fall-through here:
          case    6 : HACONFIG;
                      CHECKBITS(6, 7);
                      KEY("Program_WD_Abs");
                      QOS_SELECT;
                      if (pubHa) {
                        SUBDEVICE("_Mode");
                        HADEVICE_SELECT;
                        HADEVICE_SELECT_OPTIONS("\"Abs\",\"WD\",\"Abs+prog\",\"WD+prog\"", "'0':'Abs','1':'WD','2':'Abs+prog','3':'WD+prog'");
                        HADEVICE_SELECT_COMMAND_TEMPLATE("{% set modes={'Abs':0,'WD':1,'Abs+prog':2,'WD+prog':3}%}{{'E350056%i'|format(((modes[value])|int) if value in modes.keys() else 0)}}")
                        HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                        PUB_CONFIG;
                      }
                      CHECK_ENTITY;
                      VALUE_bits(6, 7);
          default   : UNKNOWN_BIT;
        }
        default : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : return 0;
        case    1 : if (!(M.R.useDHW & 0x02)) return 0;
                    SUBDEVICE("_DHW");                               HACONFIG; HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("DHW_Setpoint");                              VALUE_f8s8_LE;
        case    3 : switch (bitNr) {
          case    8 : bcnt = 22; BITBASIS;
          case    6 : SUBDEVICE("_Mode");                                                                                   KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_WD_Response");
          case    7 : SUBDEVICE("_Mode");                                                                                   KEYBIT_PUB_CONFIG_PUB_ENTITY("Climate_Program");
          default   : UNKNOWN_BIT;
        }
        case    6 : SUBDEVICE("_Sensors");       CAT_MEASUREMENT;    HACONFIG; HAPRESSURE;                                  KEY1_PUB_CONFIG_CHECK_ENTITY("Water_Pressure");                            VALUE_u8div10;
        case    8 : return 0;
        case    9 : SUBDEVICE("_Sensors"); M.R.Flow = FN_u16div10_LE(payloadPointer);
                                                 CAT_MEASUREMENT;    HACONFIG; HAFLOW;     HYST_U16_LE(1)                   KEY2_PUB_CONFIG_CHECK_ENTITY("Flow");                                      VALUE_u16div10_LE;
        case   10 : return 0;
        case   11 :                                                                                                         KEY2_PUB_CONFIG_CHECK_ENTITY("SW_Version_Inside_Unit");                    VALUE_u16hex_LE;
        case   12 : return 0;
        case   13 : if (!payloadByte && !payload[payloadIndex - 1]) return 0;
                                                                                                                            KEY2_PUB_CONFIG_CHECK_ENTITY("SW_Version_Outside_Unit");                   VALUE_u16hex_LE;
        // case  14 : // some version E types
        // case  15 : // some version E types
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
        case    1 : SUBDEVICE("_LWT");                               HACONFIG; HATEMP1;                 QOS_CLIMATE;        KEY2_PUB_CONFIG_CHECK_ENTITY("Abs_Heating");                               VALUE_f8_8_LE; // or f8s8? // in Abs mode
        case    2 : return 0;
        case    3 : SUBDEVICE("_LWT");                               HACONFIG; HATEMP1;                 QOS_CLIMATE;        KEY2_PUB_CONFIG_CHECK_ENTITY("Abs_Cooling");                               VALUE_f8_8_LE; // or f8s8? // in Abs mode
        case    4 : return 0;
        case    5 : HACONFIG;
                    HATEMP1;
                    SUBDEVICE("_LWT2");
                    if (KEEPCONFIG || EE.haSetup || M.R.numberOfLWTzonesX10) {
                      KEY2_PUB_CONFIG_CHECK_ENTITY("Abs_Heating_Add");
                    } else {
                      KEY2_DEL_CONFIG_CHECK_ENTITY("Abs_Heating_Add");
                    }
                    VALUE_f8_8_LE; // or f8s8? // in Abs mode
        case    6 : return 0;
        case    7 : HACONFIG;
                    HATEMP1;
                    SUBDEVICE("_LWT2");
                    if (KEEPCONFIG || EE.haSetup || M.R.numberOfLWTzonesX10) {
                      KEY2_PUB_CONFIG_CHECK_ENTITY("Abs_Cooling_Add");
                    } else {
                      KEY2_DEL_CONFIG_CHECK_ENTITY("Abs_Cooling_Add");
                    }
                    VALUE_f8_8_LE; // or f8s8? // in Abs mode
        case    8 : SUBDEVICE("_LWT");                               HACONFIG; HATEMP1;                 QOS_CLIMATE;        KEY1_PUB_CONFIG_CHECK_ENTITY("Deviation_Heating");                         VALUE_s4abs1c;
        case    9 : SUBDEVICE("_LWT");                               HACONFIG; HATEMP1;                 QOS_CLIMATE;        KEY1_PUB_CONFIG_CHECK_ENTITY("Deviation_Cooling");                         VALUE_s4abs1c; // guess
        case   10 :
                    SUBDEVICE("_LWT2");   
                    HACONFIG;
                    HATEMP1;
                    QOS_CLIMATE;
                    if (KEEPCONFIG || EE.haSetup || M.R.numberOfLWTzonesX10) {
                      KEY1_PUB_CONFIG_CHECK_ENTITY("Deviation_Heating_Add");
                    } else {
                      KEY1_DEL_CONFIG_CHECK_ENTITY("Deviation_Heating_Add");
                    }
                    VALUE_s4abs1c;
        case   11 :
                    SUBDEVICE("_LWT2");   
                    HACONFIG;
                    HATEMP1;
                    QOS_CLIMATE;
                    if (KEEPCONFIG || EE.haSetup || M.R.numberOfLWTzonesX10) {
                      KEY1_PUB_CONFIG_CHECK_ENTITY("Deviation_Cooling_Add");
                    } else {
                      KEY1_DEL_CONFIG_CHECK_ENTITY("Deviation_Cooling_Add");
                    }
                    VALUE_s4abs1c; // format guess
        case   12 : // first package 37 instead of 00 (55 = initial target temp?)
        case   13 :
        case   14 :
        default :   UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) { // no need to output any of these, duplicates from above
        case    0 : return 0;
        case    1 :                                                            HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Abs_Heating_1");                             VALUE_f8_8_LE;
        case    2 : return 0;
        case    3 :                                                            HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Abs_Cooling_1");                             VALUE_f8_8_LE;
        case    4 : return 0;
        case    5 :                                                            HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Abs_Heating_Add_1");                         VALUE_f8_8_LE;
        case    6 : return 0;
        case    7 :                                                            HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Abs_Cooling_Add_1");                         VALUE_f8_8_LE;
        case    8 :                                                            HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("Deviation_Heating_Main_1");                  VALUE_s4abs1c;
        case    9 :                                                            HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("Deviation_Cooling_Main_1");                  VALUE_s4abs1c;
        case   10 :                                                            HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("Deviation_Heating_Add_1");                   VALUE_s4abs1c;
        case   11 :                                                            HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("Deviation_Cooling_Add_1");                   VALUE_s4abs1c;
        case   15 : return 0;
        case   16 : /* if (payloadByte || payload[payloadIndex - 1]) */
                    { SUBDEVICE("_LWT");                             HACONFIG; HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("LWT_Setpoint");                              VALUE_f8s8_LE; }
                    return 0; // 2.4, too low, or cooling?
        case   17 : return 0;
        case   18 : /* if (payloadByte || payload[payloadIndex - 1]) */
                    HACONFIG;
                    SUBDEVICE("_LWT2");   
                    HATEMP1;
                    if (KEEPCONFIG || EE.haSetup || M.R.numberOfLWTzonesX10) {
                      KEY2_PUB_CONFIG_CHECK_ENTITY("LWT_Setpoint_Add");
                    } else {
                      KEY2_DEL_CONFIG_CHECK_ENTITY("LWT_Setpoint_Add");
                    }
                    VALUE_f8s8_LE;
                    return 0;
        default :   UNKNOWN_BYTE;
      }
      default   :   UNKNOWN_BYTE;
    }
    case 0x15 :
                                                 CAT_TEMP  ; // PacketType15 system settings and operation
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : UNKNOWN_BYTE; // 3rd byte of Electricity_Price ?, change only via field settings
        case    1 : return 0;
        case    2 : SUBDEVICE("_Prices");        CAT_SETTING;        HACONFIG; HACOST;                                      KEY2_PUB_CONFIG_CHECK_ENTITY("Electricity");                               VALUE_u16div100_LE; // 16 or 24 bit, changes with schedule
        case    3 : UNKNOWN_BYTE; // 3rd byte of Gas_Price ?
        case    4 : return 0;
        case    5 : SUBDEVICE("_Prices");
                    HACONFIG;
                    HACOST;
                    CAT_SETTING;
                    CHECK(2);
                    KEY("Gas");
                    PUB_CONFIG; // first as sensor, then as numberbox, so HARESET2 to restart
                    HARESET2;
                    KEY("Gas");
                    QOS_NUMBER;
                    if (pubHa) {
                      HADEVICE_NUMBER;
                      HADEVICE_NUMBER_RANGE(0.01, 990, 0.01);
                      HADEVICE_NUMBER_MODE("box");
                      HADEVICE_NUMBER_COMMAND_TEMPLATE("{{'E3D001F%04X'|format((value*100)|int)}}");
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      PUB_CONFIG_COMMON_NAME("Gas_Number");
                    }
                    CHECK_ENTITY;
                    VALUE_u16div100_LE; // 16 or 24 bit?
        // case   6 : // some version E types // TODO byte 6 is param-nr 0x00-0x19, byte 7-8 is value?
        // case   7 : // some version E types
        // case   8 : // some version E types
        default :   UNKNOWN_BYTE;
      }




      case 0x40 : switch (payloadIndex) {
        case    0 : return 0;
        case    1 : SUBDEVICE("_Unknown");                           HACONFIG; HATEMP1;    HYST_F8_8_LE(20)                 KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_Unused_15_0");                   VALUE_f8_8_LE;
        case    2 : return 0;
        case    3 : SUBDEVICE("_Sensors");                           HACONFIG; HATEMP1;    HYST_F8_8_LE(20)                 KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_Refrigerant_2");                 VALUE_f8_8_LE;       // =? Brine_inlet_temp
        case    4 : return 0;
        case    5 : SUBDEVICE("_Unknown");                           HACONFIG; HATEMP1;    HYST_F8_8_LE(25)                 KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_Refrigerant_3_Q");               VALUE_f8_8_LE;
        default :             UNKNOWN_BYTE;
      }
      default :               UNKNOWN_BYTE;
    }
    case 0x16 : return 0; // reintroduce 0x16 decoding
    case 0x20 ... 0x21 : switch (packetSrc) {                   //  0x20 0x21 sensor-related?
      case 0x00 : UNKNOWN_BYTE; // return 0;
      case 0x40 : switch (payloadIndex) {
        case  0 : // 0x00 0x01
        case  2 : // 0x01 0x02 (related to Pref / cooling / ??)
        case 14 : // 0x00 or 0x3C after restart
        case 16 : // 0x00 or 0x4B after restart, copied to param 0x35 0x009A
        default : UNKNOWN_BYTE;
      }
      default   : UNKNOWN_BYTE; // 0x20 1-byte payload, currently not stored in history; 0x20 empty poayload
    }
    case 0x60 ... 0x8F : switch (packetSrc) {                   // field setting
      case 0x00 : return 0; // UI or bridge writing new field settings, ignore
      case 0x40 : switch (payloadIndex) {
        case 19        : if (packetType == 0x8F) { // last packet of field setting information
                           if (!throttleValue) { // if not throttling, start output field settings
                             printfTopicS("Not throttling, received field settings, so start output field settings");
                             fieldSettingPublishNr = 0;
                           }
                         }
                         // fallthrough
        case  3        : // fallthrough
        case  7        : // fallthrough
        case 11        : // fallthrough
        case 15        : FSB = 1;
                         FIELD_SETTING; // only to store history data, not to output any data over mqtt yet
        case  0 ...  2 : // fallthrough
        case  4 ...  6 : // fallthrough
        case  8 ... 10 : // fallthrough
        case 12 ... 14 : // fallthrough
        case 16 ... 18 : return 0;
        default : UNKNOWN_BYTE;
      }
    }

// 0x90-0x9F unknown restart info?
    case 0x90 ... 0x9F : switch (packetSrc) {
      case 0x00 : UNKNOWN_BYTE;
      case 0x40 : UNKNOWN_BYTE;
      default : UNKNOWN_BYTE;
    }
    case 0xA1 : return 0; // ignore for now, name of outside unit
    case 0xB1 : return 0; // ignore for now, name of inside unit
    case 0xB8 : // SUBDEVICE("_counters");
                HACONFIG;
                CAT_COUNTER;
                switch (packetSrc) {
      case 0x00 :             return 0;                         // no info in 0x00 payloads regardless of subtype
      case 0x40 : SUBDEVICE("_Meters");
                  switch (payload[0]) {
        case 0x00 : switch (payloadIndex) {
          case  0 : return 0; // subtype
          case  1 : return 0;
          case  2 : return 0;
          case  3 :                                                            HAKWH;                                       KEY3_PUB_CONFIG_CHECK_ENTITY("Electricity_Consumed_Backup_Heating");       VALUE_u24_LE; // electricity used for room heating, backup heater
          case  4 : return 0;
          case  5 : return 0;
          case  6 : if (!(M.R.useDHW & 0x02)) return 0;
                                                                               HAKWH;                                       KEY3_PUB_CONFIG_CHECK_ENTITY("Electricity_Consumed_Backup_DHW");           VALUE_u24_LE; // electricity used for DHW, backup heater
          case  7 : return 0;
          case  8 : return 0;
          case  9 : if (!EE.useTotal) {
                      uint32_t v = u_payloadValue_LE(payload + payloadIndex, 3);
                      if (v > M.R.electricityConsumedCompressorHeating) {
                        M.R.electricityConsumedCompressorHeating = v;
                        pseudo0B = 9;
                        if (EE.D13 & 0x01) {
                          EE.electricityConsumedCompressorHeating1 = v;
                          printfTopicS("Set consumption to %i (B8)", v);
                          EE_dirty = 1;
                          EE.D13 &= 0xFE;
                          pseudo0F = 9;
                          if (!EE.D13) printfTopicS("Use D5 to save consumption/production counters %i/%i", EE.electricityConsumedCompressorHeating1, EE.energyProducedCompressorHeating1);
                        }
                      }
                    }
                                                                               HAKWH;                                       KEY3_PUB_CONFIG_CHECK_ENTITY("Electricity_Consumed_Compressor_Heating");   VALUE_u24_LE; // electricity used for room heating, compressor
          case 10 : return 0;
          case 11 : return 0;
          case 12 :                                                            HAKWH;                                       KEY3_PUB_CONFIG_CHECK_ENTITY("Electricity_Consumed_Compressor_Cooling");   VALUE_u24_LE; // electricity used for cooling, compressor
          case 13 : return 0;
          case 14 : return 0;
          case 15 : if (!(M.R.useDHW & 0x02)) return 0;
                                                                               HAKWH;                                       KEY3_PUB_CONFIG_CHECK_ENTITY("Electricity_Consumed_Compressor_DHW");       VALUE_u24_LE; // eletricity used for DHW, compressor
          case 16 : return 0;
          case 17 : return 0;
          case 18 :
                    if (EE.useTotal) {
                      uint32_t v = u_payloadValue_LE(payload + payloadIndex, 3);
                      if (v > M.R.electricityConsumedCompressorHeating) {
                        M.R.electricityConsumedCompressorHeating = v;
                        pseudo0B = 9;
                        if (EE.D13 & 0x01) {
                          EE.electricityConsumedCompressorHeating1 = v;
                          printfTopicS("Set consumption to %i (B8)", v);
                          EE_dirty = 1;
                          EE.D13 &= 0xFE;
                          pseudo0F = 9;
                          if (!EE.D13) printfTopicS("Use D5 to save consumption/production counters %i/%i", EE.electricityConsumedCompressorHeating1, EE.energyProducedCompressorHeating1);
                        }
                      }
                    }
                                                                               HAKWH;                                       KEY3_PUB_CONFIG_CHECK_ENTITY("Electricity_Consumed_Total");                VALUE_u24_LE; // electricity used, total
          default : UNKNOWN_BYTE;
        }
        case 0x01 : switch (payloadIndex) { // payload B8 subtype 01
          case  0 : return 0; // subtype
          case  1 : return 0;
          case  2 : return 0;
          case  3 : { uint32_t v = u_payloadValue_LE(payload + payloadIndex, 3);
                      if (v > M.R.energyProducedCompressorHeating) {
                        M.R.energyProducedCompressorHeating = v;
                        pseudo0B = 9;
                        if (EE.D13 & 0x02) {
                          EE.energyProducedCompressorHeating1 = v;
                          printfTopicS("Set production to %i (B8)", v);
                          EE_dirty = 1;
                          EE.D13 &= 0xFD;
                          pseudo0F = 9;
                          if (!EE.D13) printfTopicS("Use D5 to save consumption/production counters %i/%i", EE.electricityConsumedCompressorHeating1, EE.energyProducedCompressorHeating1);
                        }
                      }
                    }
                                                                               HAKWH;                                       KEY3_PUB_CONFIG_CHECK_ENTITY("Energy_Produced_Compressor_Heating");        VALUE_u24_LE; // energy produced for room heating // compressor or heatpump including BUH?
          case  4 : return 0;
          case  5 : return 0;
          case  6 :                                                            HAKWH;                                       KEY3_PUB_CONFIG_CHECK_ENTITY("Energy_Produced_Compressor_Cooling");        VALUE_u24_LE; // energy produced when cooling
          case  7 : return 0;
          case  8 : return 0;
          case  9 : if (!(M.R.useDHW & 0x02)) return 0;
                                                                               HAKWH;                                       KEY3_PUB_CONFIG_CHECK_ENTITY("Energy_Produced_Compressor_DHW");            VALUE_u24_LE; // energy produced for DHW
          case 10 : return 0;
          case 11 : return 0;
          case 12 :                                                            HAKWH;                                       KEY3_PUB_CONFIG_CHECK_ENTITY("Energy_Produced_Compressor_Total");          VALUE_u24_LE; // energy produced total
          default : UNKNOWN_BYTE;
        }
        case 0x02 : switch (payloadIndex) { // payload B8 subtype 02
          case  0 :           return 0; // subtype
          case  1 :           return 0;
          case  2 :           return 0;
          case  3 :                                                            HAHOURS;                                     KEY3_PUB_CONFIG_CHECK_ENTITY("Hours_Circulation_Pump");                    VALUE_u24_LE;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 :                                                            HAHOURS;                                     KEY3_PUB_CONFIG_CHECK_ENTITY("Hours_Compressor_Heating");                  VALUE_u24_LE;
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :                                                            HAHOURS;                                     KEY3_PUB_CONFIG_CHECK_ENTITY("Hours_Compressor_Cooling");                  VALUE_u24_LE;
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 : if (!(M.R.useDHW & 0x02)) return 0;
                                                                               HAHOURS;                                     KEY3_PUB_CONFIG_CHECK_ENTITY("Hours_Compressor_DHW");                      VALUE_u24_LE;
          default :           UNKNOWN_BYTE;
        }
        case 0x03 : switch (payloadIndex) { // payload B8 subtype 03
          case  0 :           return 0; // subtype
          case  1 :           return 0;
          case  2 :           return 0;
          case  3 :                                                            HAHOURS;                                     KEY3_PUB_CONFIG_CHECK_ENTITY("Hours_Backup1_Heating");                     VALUE_u24_LE;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 : if (!(M.R.useDHW & 0x02)) return 0;
                                                                               HAHOURS;                                     KEY3_PUB_CONFIG_CHECK_ENTITY("Hours_Backup1_DHW");                         VALUE_u24_LE;
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :                                                            HAHOURS;                                     KEY3_PUB_CONFIG_CHECK_ENTITY("Hours_Backup2_Heating");                     VALUE_u24_LE;
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 : if (!(M.R.useDHW & 0x02)) return 0;
                                                                               HAHOURS;                                     KEY3_PUB_CONFIG_CHECK_ENTITY("Hours_Backup2_DHW");                         VALUE_u24_LE;
          case 13 :           return 0;
          case 14 :           return 0;
          case 15 :                                                            HAHOURS;                                     KEY3_PUB_CONFIG_CHECK_ENTITY("Hours_Booster");                             VALUE_u24_LE; // DHW Booster Heater
          case 16 :           return 0;
          case 17 :           return 0;
          case 18 :                                                            HAHOURS;                                     KEY3_PUB_CONFIG_CHECK_ENTITY("Hours_Drain_Pan_Heater");                    VALUE_u24_LE; // Drain pan heater (BPV/bodemplaat verwarming), thanks to Pim Snoeks for finding this function
          default :           UNKNOWN_BYTE;
        }
        case 0x04 : switch (payloadIndex) {
                            // payload B8 subtype 04
          case  0 :           return 0; // subtype
          case  1 :           return 0;
          case  2 :           return 0;
          case  3 :                                                                                                         KEY3_PUB_CONFIG_CHECK_ENTITY("Counter_Unknown_00");                        VALUE_u24_LE;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 :                                                                                                         KEY3_PUB_CONFIG_CHECK_ENTITY("Counter_Unknown_01");                        VALUE_u24_LE;
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :                                                                                                         KEY3_PUB_CONFIG_CHECK_ENTITY("Counter_Unknown_02");                        VALUE_u24_LE;
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 :                                                                                                         KEY3_PUB_CONFIG_CHECK_ENTITY("Starts_Compressor");                         VALUE_u24_LE;
          default :           UNKNOWN_BYTE;
        }
        case 0x05 : switch (payloadIndex) { // payload B8 subtype 05
          case  0 :           return 0; // subtype
          case  1 :           return 0;
          case  2 :           return 0;
          case  3 :                                                            HAHOURS;                                     KEY3_PUB_CONFIG_CHECK_ENTITY("Hours_Gasboiler_Heating");                   VALUE_u24_LE;
          case  4 :           return 0;
          case  5 :           return 0;
          case  6 : if (!(M.R.useDHW & 0x02)) return 0;
                                                                               HAHOURS;                                     KEY3_PUB_CONFIG_CHECK_ENTITY("Hours_Gasboiler_DHW");                       VALUE_u24_LE;
          case  7 :           return 0;
          case  8 :           return 0;
          case  9 :                                                                                                         KEY3_PUB_CONFIG_CHECK_ENTITY("Counter_Gasboiler_Heating");                 VALUE_u24_LE; // older models report 0
          case 10 :           return 0;
          case 11 :           return 0;
          case 12 : if (!(M.R.useDHW & 0x02)) return 0;
                                                                                                                            KEY3_PUB_CONFIG_CHECK_ENTITY("Counter_Gasboiler_DHW");                     VALUE_u24_LE; // older models report 0
          case 13 :           return 0;
          case 14 :           return 0;
          case 15 :                                                                                                         KEY3_PUB_CONFIG_CHECK_ENTITY("Starts_Gasboiler");                          VALUE_u24_LE;
          case 16 :           return 0;
          case 17 :           return 0;
          case 18 :                                                                                                         KEY3_PUB_CONFIG_CHECK_ENTITY("Counter_Gasboiler_Total");                   VALUE_u24_LE; // older models report 0
          default :           UNKNOWN_BYTE;
        }
        default :             UNKNOWN_BYTE;
      }
      default :               UNKNOWN_BYTE;
    }
    // Packet types 0x3x from main controller (0x00) and from auxiliary controller(s) (0xF0 / 0xF1 / 0xFF)
    case 0x30 : switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case  0 : // bytes in 0x30 message do not seem to contain useful information but packet is present each cycle (in E, not in F)
                  // we use this to do some calculations once per package cycle
                  // and set up climate integrations
                  //
                  // heating(/cooling) energy produced by heat pump and gas boiler / based on 4182 J/kg K, 1 kg/l
                  {
                    if ((espUptime - ePowerTime > POWER_TIME_OUT) && ePowerAvailable) {
                      ePowerAvailable = false;
                      ePower = 0;
                    }
                    if ((espUptime - eTotalTime > ENERGY_TIME_OUT) && eTotalAvailable) {
                      eTotalAvailable = false;
                    }

                    if ((espUptime - bPowerTime > POWER_TIME_OUT)  && bPowerAvailable) {
                      bPowerAvailable = false;
                      bPower = 0;
                    }
                    if ((espUptime - bTotalTime > ENERGY_TIME_OUT)  && bTotalAvailable) {
                      bTotalAvailable = false;
                    }
                    float floatPower = (M.R.LWT - M.R.MWT) * M.R.Flow * 69.7;
                    if (floatPower > 32767) {
                      M.R.power1 = 32767;
                    } else if (floatPower < -32768) {
                      M.R.power1 = -32768;
                    } else {
                      M.R.power1 = floatPower;
                    }
                    floatPower = (M.R.MWT - M.R.RWT) * M.R.Flow * 69.7;
                    if (floatPower > 32767) {
                      M.R.power2 = 32767;
                    } else if (floatPower < -32768) {
                      M.R.power2 = -32768;
                    } else {
                      M.R.power2 = floatPower;
                    }
                    if (ePowerAvailable && (ePower > 0) && M.R.compressor && !M.R.defrostActive) {
                      M.R.COP_realtime = (1000 * floatPower) / ePower;
                      if (M.R.COP_realtime > 10000) M.R.COP_realtime = 10000;
                      if (M.R.COP_realtime < 0) M.R.COP_realtime = 0;
                    } else {
                      M.R.COP_realtime = 0;
                    }
                  }
                  // RT heating
                  HACONFIG;
                  CHECK(1);
                  QOS_CLIMATE;
                  if (pubHa) {
                    HATEMP1;
                    SUBDEVICE("_Room");
                    HADEVICE_CLIMATE;
                    if (KEEPCONFIG || EE.haSetup || M.R.controlRTX10) { // RT mode (both for 1 RT-ext and 2 RT, not for 0 LWT)
                      KEY("Room_Heating"); // heating
                      HADEVICE_CLIMATE_TEMPERATURE("S/0/Room_Heating_Setpoint", M.R.roomTempHeatingMinX10 * 0.1, M.R.roomTempHeatingMaxX10 * 0.1, 0.5)
                      HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_Room")
                      if (heatingMode || EE.haSetup || !(M.R.first030 & 0x01)) {
                        HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\",\"heat\"", "'0':'off','1':'heat'")
                        HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0,'heat':1} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                        if (!heatingMode) espUptime030 = espUptime;
                      } else {
                        HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\"", "'0':'off','1':'off'")
                        HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                      }
                      HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{{'E360000%04X'|format((value*10)|int)}}");
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      HADEVICE_AVAILABILITY("C\/9\/RT", 1, 0);
                      M.R.first030 |= 0x01;
                      PUB_CONFIG;
                    } else {
                      KEY("Room_Heating"); // heating
                      DEL_CONFIG;
                    }
                  }
                  VALUE_u0; // ensure Seen is set
          case 1 : // RT cooling
                  HACONFIG;
                  CHECK(1);
                  QOS_CLIMATE;
                  if (pubHa) {
                    HATEMP1;
                    SUBDEVICE("_Room");
                    HADEVICE_CLIMATE;
                    if (KEEPCONFIG || EE.haSetup || (!M.R.heatingOnlyX10 && M.R.controlRTX10)) { // RT mode (both for 1 RT-ext and 2 RT, not for 0 LWT)
                      KEY("Room_Cooling");
                      HADEVICE_CLIMATE_TEMPERATURE("S/0/Room_Cooling_Setpoint", M.R.roomTempCoolingMinX10 * 0.1, M.R.roomTempCoolingMaxX10 * 0.1, 0.5)
                      HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_Room")
                      if (coolingMode || EE.haSetup || !(M.R.first030 & 0x02)) {
                        HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\",\"cool\"", "'0':'off','1':'cool'")
                        HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0,'cool':1} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                        if (!coolingMode) espUptime030 = espUptime;
                      } else {
                        HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\"", "'0':'off','1':'off'")
                        HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                      }
                      HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{{'E360001%04X'|format((value*10)|int)}}");
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      HADEVICE_AVAILABILITY("C\/9\/RT", 1, 0);
                      HADEVICE_AVAILABILITY("S\/9\/Heating_Only", 0, 1);
                      M.R.first030 |= 0x02;
                      PUB_CONFIG;
                    } else {
                      KEY("Room_Cooling");
                      DEL_CONFIG;
                    }
                  }
                  VALUE_u0; // ensure Seen is set
          case 2 : // LWT_heating
                  HACONFIG;
                  CHECK(1);
                  QOS_CLIMATE;
                  if (pubHa) {
                    HATEMP1;
                    SUBDEVICE("_LWT");
                    HADEVICE_CLIMATE;
                    if (1 || !M.R.controlRTX10) { // RT mode (both for 1 RT-ext and 2 RT, not for 0 LWT) // 1|| as these seem to work even in RT mode
                      if (M.R.climateMode & 0x40) { // WD
                        KEY("Deviation_Heating");
                        HADEVICE_CLIMATE_TEMPERATURE("S/0/Deviation_Heating", -10, 10, 1)
                        if (EE.useR1T) {
                          HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R1T_Leaving_Water");
                        } else {
                          HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R2T_Leaving_Water");
                        }
                        if (heatingMode || EE.haSetup || !(M.R.first030 & 0x04)) {
                          HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\",\"heat\"", "'0':'off','1':'heat'")
                          HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0,'heat':1} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                          if (!heatingMode) espUptime030 = espUptime;
                        } else {
                          HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\"", "'0':'off','1':'off'")
                          HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                        }
                        HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{%if(value<0)%}{{'E360008%04X'|format((65536+value*10)|int)}}{%else%}{{'E360008%04X'|format((value*10)|int)}}{%endif%}")
                        HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                        M.R.first030 |= 0x04;
                        PUB_CONFIG_COMMON_NAME("LWT_Heating");
                      } else { // Abs
                        KEY("Abs_Heating");
                        HADEVICE_CLIMATE_TEMPERATURE("S/0/Abs_Heating", M.R.LWTheatMainMinX10 * 0.1, M.R.LWTheatMainMaxX10 * 0.1, 1)
                        if (EE.useR1T) {
                          HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R1T_Leaving_Water");
                        } else {
                          HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R2T_Leaving_Water");
                        }
                        if (heatingMode || EE.haSetup || !(M.R.first030 & 0x04)) {
                          HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\",\"heat\"", "'0':'off','1':'heat'")
                          HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0,'heat':1} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                          if (!heatingMode) espUptime030 = espUptime;
                        } else {
                          HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\"", "'0':'off','1':'off'")
                          HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                        }
                        HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{{'E360006%04X'|format((value*10)|int)}}");
                        HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                        M.R.first030 |= 0x04;
                        PUB_CONFIG_COMMON_NAME("LWT_Heating");
                      }
                    }
                  }
                  VALUE_u0; // ensure Seen is set
                  return 0;
          case 3 : // LWT cooling main zone
                  HACONFIG;
                  CHECK(1);
                  QOS_CLIMATE;
                  if (pubHa) {
                    HATEMP1;
                    SUBDEVICE("_LWT");
                    HADEVICE_CLIMATE;
                    if (EE.haSetup || !M.R.heatingOnlyX10) { // if (1 || !M.R.controlRTX10)  // RT mode (both for 1 RT-ext and 2 RT, not for 0 LWT) // 1|| as these seem to work even in RT mode
                      if (M.R.climateMode & 0x40) { // WD
                        KEY("Deviation_Cooling");
                        HADEVICE_CLIMATE_TEMPERATURE("S/0/Deviation_Cooling", -10, 10, 1)
                        if (EE.useR1T) {
                          HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R1T_Leaving_Water");
                        } else {
                          HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R2T_Leaving_Water");
                        }
                        if (coolingMode || EE.haSetup || !(M.R.first030 & 0x08)) {
                          HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\",\"cool\"", "'0':'off','1':'cool'")
                          HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0,'cool':1} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                          if (!coolingMode) espUptime030 = espUptime;
                        } else {
                          HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\"", "'0':'off','1':'off'")
                          HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                        }
                        HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{%if(value<0)%}{{'E360009%04X'|format((65536+value*10)|int)}}{%else%}{{'E360009%04X'|format((value*10)|int)}}{%endif%}")
                        HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                        HADEVICE_AVAILABILITY("S\/9\/Heating_Only", 0, 1);
                        M.R.first030 |= 0x08;
                        PUB_CONFIG_COMMON_NAME("LWT_Cooling");
                      } else { // Abs
                        KEY("Abs_Cooling");
                        HADEVICE_CLIMATE_TEMPERATURE("S/0/Abs_Cooling", M.R.LWTcoolMainMinX10 * 0.1, M.R.LWTcoolMainMaxX10 * 0.1, 1)
                        if (EE.useR1T) {
                          HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R1T_Leaving_Water");
                        } else {
                          HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R2T_Leaving_Water");
                        }
                        if (coolingMode || EE.haSetup || !(M.R.first030 & 0x08)) {
                          HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\",\"cool\"", "'0':'off','1':'cool'")
                          HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0,'cool':1} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                          if (!coolingMode) espUptime030 = espUptime;
                        } else {
                          HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\"", "'0':'off','1':'off'")
                          HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                        }
                        HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{{'E360007%04X'|format((value*10)|int)}}");
                        HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                        HADEVICE_AVAILABILITY("S\/9\/Heating_Only", 0, 1);
                        M.R.first030 |= 0x08;
                        PUB_CONFIG_COMMON_NAME("LWT_Cooling");
                      }
                    } else {
                      // delete?
                    }
                  }
                  VALUE_u0; // ensure Seen is set
                  return 0;
          case 4 : // LWT_heating add zone
                  HACONFIG;
                  CHECK(1);
                  QOS_CLIMATE;
                  if (pubHa) {
                    HATEMP1;
                    SUBDEVICE("_LWT2");   
                    HADEVICE_CLIMATE;
                    if (KEEPCONFIG || M.R.numberOfLWTzonesX10) {
                      if (1 || EE.haSetup || !M.R.controlRTX10) { // RT mode (both for 1 RT-ext and 2 RT, not for 0 LWT) // 1|| as these seem to work even in RT mode
                        if (M.R.climateMode & 0x40) { // WD
                          KEY("Deviation_Heating_Add");
                          HADEVICE_CLIMATE_TEMPERATURE("S/0/Deviation_Heating_Add", -10, 10, 1)
                          HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R2T_Leaving_Water")
                          if (heatingMode || EE.haSetup || !(M.R.first030 & 0x10)) {
                            HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\",\"heat\"", "'0':'off','1':'heat'")
                            HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0,'heat':1} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                            if (!heatingMode) espUptime030 = espUptime;
                          } else {
                            HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\"", "'0':'off','1':'off'")
                            HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                          }
                          HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{%if(value<0)%}{{'E36000D%04X'|format((65536+value*10)|int)}}{%else%}{{'E36000D%04X'|format((value*10)|int)}}{%endif%}")
                          HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                          HADEVICE_AVAILABILITY("C\/9\/Number_Of_Zones", 2, 1);
                          M.R.first030 |= 0x10;
                          PUB_CONFIG_COMMON_NAME("LWT_Heating_Add");
                        } else { // Abs
                          KEY("Abs_Heating_Add");
                          HADEVICE_CLIMATE_TEMPERATURE("S/0/Abs_Heating_Add", M.R.LWTheatAddMinX10 * 0.1, M.R.LWTheatAddMaxX10 * 0.1, 1)
                          if (EE.useR1T) {
                            HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R1T_Leaving_Water");
                          } else {
                            HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R2T_Leaving_Water");
                          }
                          if (heatingMode || EE.haSetup || !(M.R.first030 & 0x10)) {
                            HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\",\"heat\"", "'0':'off','1':'heat'")
                            HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0,'heat':1} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                            if (!heatingMode) espUptime030 = espUptime;
                          } else {
                            HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\"", "'0':'off','1':'off'")
                            HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                          }
                          HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{{'E36000B%04X'|format((value*10)|int)}}");
                          HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                          HADEVICE_AVAILABILITY("C\/9\/Number_Of_Zones", 2, 1);
                          M.R.first030 |= 0x10;
                          PUB_CONFIG_COMMON_NAME("LWT_Heating_Add");
                        }
                      }
                    } else {
                      KEY("Heating_Add_NA");
                      DEL_CONFIG_COMMON_NAME("LWT_Heating_Add");
                    }
                  }
                  VALUE_u0; // ensure Seen is set
                  return 0;
          case 5 : // LWT cooling add zone
                  HACONFIG;
                  CHECK(1);
                  QOS_CLIMATE;
                  if (pubHa) {
                    HATEMP1;
                    SUBDEVICE("_LWT2");   
                    HADEVICE_CLIMATE;
                    if (KEEPCONFIG || EE.haSetup || (!M.R.heatingOnlyX10 && M.R.numberOfLWTzonesX10)) {
                      //if (1 || !M.R.controlRTX10) { // RT mode (both for 1 RT-ext and 2 RT, not for 0 LWT) // 1|| as these seem to work even in RT mode
                        if (M.R.climateMode & 0x40) { // WD
                          KEY("Deviation_Cooling_Add");
                          HADEVICE_CLIMATE_TEMPERATURE("S/0/Deviation_Cooling_Add", -10, 10, 1)
                          if (EE.useR1T) {
                            HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R1T_Leaving_Water");
                          } else {
                            HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R2T_Leaving_Water");
                          }
                          if (coolingMode || EE.haSetup || !(M.R.first030 & 0x20)) {
                            HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\",\"cool\"", "'0':'off','1':'cool'")
                            HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0,'cool':1} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                            if (!coolingMode) espUptime030 = espUptime;
                          } else {
                            HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\"", "'0':'off','1':'off'")
                            HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                          }
                          HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{%if(value<0)%}{{'E36000E%04X'|format((65536+value*10)|int)}}{%else%}{{'E36000E%04X'|format((value*10)|int)}}{%endif%}")
                          HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                          HADEVICE_AVAILABILITY("C\/9\/Number_Of_Zones", 2, 1);
                          HADEVICE_AVAILABILITY("S\/9\/Heating_Only", 0, 1);
                          M.R.first030 |= 0x20;
                          PUB_CONFIG_COMMON_NAME("LWT_Cooling_Add");
                        } else { // Abs
                          KEY("Abs_Cooling_Add");
                          HADEVICE_CLIMATE_TEMPERATURE("S/0/Abs_Cooling_Add", M.R.LWTcoolAddMinX10 * 0.1, M.R.LWTcoolAddMaxX10 * 0.1, 1)
                          if (EE.useR1T) {
                            HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R1T_Leaving_Water");
                          } else {
                            HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/1/Temperature_R2T_Leaving_Water");
                          }
                          if (coolingMode || EE.haSetup || !(M.R.first030 & 0x20)) {
                            HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\",\"cool\"", "'0':'off','1':'cool'")
                            HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0,'cool':1} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                            if (!coolingMode) espUptime030 = espUptime;
                          } else {
                            HADEVICE_CLIMATE_MODES("S/0/Altherma_On", "\"off\"", "'0':'off','1':'off'")
                            HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0} %}{{'E35002F%02X 350031%02X 35002D%02X'|format((modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0, (modes[value]|int) if value in modes.keys() else 0)}}");
                          }
                          HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{{'E36000C%04X'|format((value*10)|int)}}");
                          HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                          HADEVICE_AVAILABILITY("C\/9\/Number_Of_Zones", 2, 1);
                          HADEVICE_AVAILABILITY("S\/9\/Heating_Only", 0, 1);
                          M.R.first030 |= 0x20;
                          PUB_CONFIG_COMMON_NAME("LWT_Cooling_Add");
                        }
                      //}
                    } else {
                      KEY("Cooling_Add_NA");
                      DEL_CONFIG_COMMON_NAME("LWT_Cooling_Add");
                    }
                  }
                  VALUE_u0; // ensure Seen is set
                  return 0;
        case  6 : // internal calculations
                  { byte fch = (M.R.first030 ^ M.R.first030a);
                    if (fch && (espUptime > espUptime030 + 15)) {
                      if (fch & 0x01) {
                        UNSEEN_BYTE_00_30_0_CLIMATE_ROOM_HEATING;
                        M.R.first030a |= 0x01;
                        //printfTopicS("030-01 ready");
                      }
                      if (fch & 0x02) {
                        UNSEEN_BYTE_00_30_1_CLIMATE_ROOM_COOLING;
                        M.R.first030a |= 0x02;
                        //printfTopicS("030-02 ready");
                      }
                      if (fch & 0x04) {
                        UNSEEN_BYTE_00_30_2_CLIMATE_LWT_HEATING;
                        M.R.first030a |= 0x04;
                        //printfTopicS("030-04 ready");
                      }
                      if (fch & 0x08) {
                        UNSEEN_BYTE_00_30_3_CLIMATE_LWT_COOLING;
                        M.R.first030a |= 0x08;
                        //printfTopicS("030-08 ready");
                      }
                      if (fch & 0x10) {
                        UNSEEN_BYTE_00_30_4_CLIMATE_LWT_HEATING_ADD;
                        M.R.first030a |= 0x10;
                        //printfTopicS("030-10 ready");
                      }
                      if (fch & 0x20) {
                        UNSEEN_BYTE_00_30_5_CLIMATE_LWT_COOLING_ADD;
                        M.R.first030a |= 0x20;
                        //printfTopicS("030-20 ready");
                      }
                    }
                  }
                  // field setting publication
                  if (fieldSettingPublishNr < 0xF0) {
                    HACONFIG;
                    FSB = 2;
                    SUBDEVICE("_FieldSettings");
                    HACONFIG;
                    HADEVICE_SENSOR_VALUE_TEMPLATE("{{value_json.val}}");
                    if (publishFieldSetting(fieldSettingPublishNr)) fieldSettingPublishNr++;
                    return 0;
                  } else if (fieldSettingPublishNr == 0xF0) {
                    fieldSettingPublishNr++;
                    printfTopicS("Field setting output ready");
                  }
                  return 0;
        default : return 0;
      }
      case 0x40 : switch (payloadIndex) {
        default : return 0;
      }
      default : UNKNOWN_BYTE;
    }
    case 0x31 :                                  CAT_MEASUREMENT; // PacketType B8 system settings and operation
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case  0 :             bcnt = 23; BITBASIS_UNKNOWN;
        case  1 :                                CAT_UNKNOWN;                                                               KEYBIT_PUB_CONFIG_PUB_ENTITY("F031_1");
        case  2 :                                CAT_UNKNOWN;                                                               KEYBIT_PUB_CONFIG_PUB_ENTITY("F031_2");
        case 3: switch (bitNr) {
          case 8: bcnt = 29; BITBASIS;
/*
          case 0:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Z_0031_3_0_Q_Always1");
          case 1:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Z_0031_3_1_Q_Always0");
          case 2:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Z_0031_3_2_Q_Always0");
          case 3:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Z_0031_3_3_Q_Always0");
          case 4:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Aux_LCD_Req_2");
          case 5:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Aux_LCD_Ack");
          case 6:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Aux_LCD_Light_2");
*/
          case 7: HACONFIG; SUBDEVICE("_UI"); CAT_SETTING;                                                                  KEYBIT_PUB_CONFIG_PUB_ENTITY("Main_LCD_Light");
          default: return 0;
        }
        case 4:
                switch (bitNr) {
          case 8: bcnt = 30; BITBASIS;
/*
          case 0:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Aux_Installer_Req_2");
          case 1:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Aux_Installer_Ack");
          case 2:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY_INV("Aux_Advanced_2");
          case 3:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Aux_Installer_2");
          case 4:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY_INV("Aux_Installer2_2");
*/
          case 5: HACONFIG; SUBDEVICE("_UI"); CAT_SETTING;                                                                  KEYBIT_PUB_CONFIG_PUB_ENTITY_INV("Main_Advanced");
          case 6: HACONFIG; SUBDEVICE("_UI"); CAT_SETTING;                                                                  KEYBIT_PUB_CONFIG_PUB_ENTITY("Main_Installer");
/*
          case 7:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY_INV("Main_Installer2");
*/
          default: return 0;
        }
/*
        case  5 : switch (bitNr) {
          case    8 : bcnt = 24; BITBASIS;
          case    6 : return 0; // suppress hourly pulse reporting
                                                 CAT_SETTING;                                                               KEYBIT_PUB_CONFIG_PUB_ENTITY("Hourly_Pulse_1");
          case    2 : return 0; // suppress error messages caused by inserted messages and counter requests
                                                 CAT_SETTING;                                                               KEYBIT_PUB_CONFIG_PUB_ENTITY("Protocol_Missing_F0_Reply_Detected");
          case    0 :                            CAT_UNKNOWN;                                                               KEYBIT_PUB_CONFIG_PUB_ENTITY("Reboot_Related_Q31-00-5-0");
          default   : UNKNOWN_BIT;
        }
*/
        case  6 :             // year
                              if (EE.minuteTimeStamp) return 0;
                              M.R.selectTimeString = 4;
                              // Prefer use of timeString2,
                              // unless we are not seeing these
                              timeString2[5] = '0' + (payloadByte / 10);
                              timeString2[6] = '0' + (payloadByte % 10);
                              return 0;

        case  7 :             // month
                              if (EE.minuteTimeStamp) return 0;
                              timeString2[8] = '0' + (payloadByte / 10);
                              timeString2[9] = '0' + (payloadByte % 10);
                              return 0;

        case  8 :             // day
                              if (EE.minuteTimeStamp) return 0;
                              timeString2[11] = '0' + (payloadByte / 10);
                              timeString2[12] = '0' + (payloadByte % 10);
                              return 0;

        case  9 :             // hours
                              if (EE.minuteTimeStamp) return 0;
                              timeString2[14] = '0' + (payloadByte / 10);
                              timeString2[15] = '0' + (payloadByte % 10);
                              return 0;
        case 10 :             // minutes
                              if (EE.minuteTimeStamp) return 0;
                              timeString2[17] = '0' + (payloadByte / 10);
                              timeString2[18] = '0' + (payloadByte % 10);
                              return 0;
        case 11 :             // seconds
                              if (EE.minuteTimeStamp) return 0;
                              timeString2[20] = '0' + (payloadByte / 10);
                              timeString2[21] = '0' + (payloadByte % 10);
                              if (payloadByte != M.R.prevSec)
                              {
                                SUBDEVICE("_Mode");
                                M.R.prevSec = payloadByte;
                                SRC(9);
                                HACONFIG;
                                maxOutputFilter = 2;
                                CHECK(0);
                                KEY("Date_Time_Daikin");
                                PUB_CONFIG;
                                CHECK_ENTITY;
                                VALUE_textString(timeString2);
                              }
                              return 0;
        default:              return 0;
      }
      case 0x40 : switch (payloadIndex) {
        case 3: switch (bitNr) {
          case 8: bcnt = 31; BITBASIS;
/*
          case 0:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Z_4031_3_0_Q_Always0");
          case 1:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Z_4031_3_1_Q_Always0");
          case 2:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Z_4031_3_2_Q_Always0");
          case 3:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Z_4031_3_3_Q_Always0");
          case 4:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Aux_LCD_Req");
          case 5:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Aux_LCD_Ack_2");
*/
          case 6: HACONFIG; SUBDEVICE("_UI"); CAT_SETTING;                                                                  KEYBIT_PUB_CONFIG_PUB_ENTITY("Aux_LCD_Light");
/*
          case 7:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Main_LCD_Light_2");
*/
          default: return 0;
        }
        case 4: switch (bitNr) {
          case 8: bcnt = 32; BITBASIS;
/*
          case 0:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Aux_Installer_Req");
          case 1:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Aux_Installer_Ack_2");
*/
          case 2: HACONFIG; SUBDEVICE("_UI"); CAT_SETTING;                                                                  KEYBIT_PUB_CONFIG_PUB_ENTITY_INV("Aux_Advanced");
          case 3: HACONFIG; SUBDEVICE("_UI"); CAT_SETTING;                                                                  KEYBIT_PUB_CONFIG_PUB_ENTITY("Aux_Installer");
/*
          case 4:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY_INV("Aux_Installer2");
          case 5:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY_INV("Main_Advanced_2");
          case 6:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Main_Installer_2");
          case 7:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY_INV("Main_Installer2_2");
*/
          default: return 0;
        }
        //case 5 :  SUBDEVICE("_31"); HACONFIG; KEY1_PUB_CONFIG_CHECK_ENTITY("4031-5"); VALUE_u8hex; // always 0x05 ??
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
    case 0x3A : CAT_SETTING;
                SUBDEVICE("_35-3A");
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
    case 0x3B : SUBDEVICE("_36-3B");
                switch (packetSrc) {
      case 0x00 : // fallthrough
      case 0x40 : switch (payloadIndex) {
        case    3 : // fallthrough
        case    7 : // fallthrough
        case   11 : // fallthrough
        case   15 : // fallthrough
        case   19 :             HANDLE_PARAM(2); // 16-bit parameter handling
        default   : return 0; // nothing to do
      }
      default :               return 0; // unknown packetSrc
    }
    case 0x37 : // fallthrough 24-bit
    case 0x3C : SUBDEVICE("_37-3C");
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
    case 0x3D : SUBDEVICE("_38-39-3D");
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
                      mqtt_value_schedule[PS][0] = ''';
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
#else /* SAVESCHEDULE */
    case 0x3E : return 0;
#endif /* SAVESCHEDULE */
    // PSEUDO_PACKETS_SYSTEM

    case 0x0B : SRC(9);
                CAT_PSEUDO_SYSTEM;
                maxOutputFilter = 3;
                switch (packetSrc) {
      case 0x40 : switch (payloadIndex) {
        case  0 : return 0;
        case  1 : SUBDEVICE("_COP");                                 HACONFIG; HACOP;      HYST_U16_LE(50);                 KEY2_PUB_CONFIG_CHECK_ENTITY("COP_Before_Bridge");                              VALUE_u16div1000_LE;
        case  2 : return 0;
        case  3 : SUBDEVICE("_COP");                                 HACONFIG; HACOP;      HYST_U16_LE(50);                 KEY2_PUB_CONFIG_CHECK_ENTITY("COP_After_Bridge");                               VALUE_u16div1000_LE;
        case  4 : return 0;
        case  5 : SUBDEVICE("_COP");                                 HACONFIG; HACOP;      HYST_U16_LE(50);                 KEY2_PUB_CONFIG_CHECK_ENTITY("COP_Lifetime");                              VALUE_u16div1000_LE;
        default : return 0;
      }
      default : UNKNOWN_BYTE;
    }

    case 0x0C : SRC(9);
                CAT_PSEUDO_SYSTEM;
                maxOutputFilter = 3;
                switch (packetSrc) {
      case 0x40 : switch (payloadIndex) {
        case  0 : return 0;
        case  1 : if (EE.useR1T) return 0;
                  SUBDEVICE("_Power");                               HACONFIG; HAPOWER;    HYST_S16_LE(15);                 KEY2_PUB_CONFIG_CHECK_ENTITY("Production_Gasboiler");                      VALUE_s16_LE;
        case  2 : return 0;
        case  3 : SUBDEVICE("_Power");                               HACONFIG; HAPOWER;    HYST_S16_LE(15);                 KEY2_PUB_CONFIG_CHECK_ENTITY("Production_Heatpump");                       VALUE_s16_LE;
        case  4 : return 0;
        case  5 : SUBDEVICE("_Power");                               HACONFIG; HAPOWER;    HYST_S16_LE(20);                 KEY2_PUB_CONFIG_CHECK_ENTITY("Consumption_Heatpump");                      VALUE_u16_LE;
        case  6 : return 0;
        case  7 : return 0;
        case  8 : return 0;
        case  9 : SUBDEVICE("_Meters");                              HACONFIG; HAKWH;      HYST_U32_LE(10);                 KEY4_PUB_CONFIG_CHECK_ENTITY("Electricity_Consumed_Heatpump_External");    VALUE_u32div1000_LE;
        case 10 : return 0;
        case 11 : SUBDEVICE("_COP");                                 HACONFIG; HACOP;      HYST_U16_LE(5);                  KEY2_PUB_CONFIG_CHECK_ENTITY("COP_Realtime");                              VALUE_u16div1000_LE;
        case 12 : return 0;
        case 13 : SUBDEVICE("_Power");                               HACONFIG; HAPOWER;    HYST_S16_LE(20);                 KEY2_PUB_CONFIG_CHECK_ENTITY("Consumption_BUH");                           VALUE_u16_LE;
        case 14 : return 0;
        case 15 : return 0;
        case 16 : return 0;
        case 17 : SUBDEVICE("_Meters");                              HACONFIG; HAKWH;      HYST_S16_LE(20)                  KEY2_PUB_CONFIG_CHECK_ENTITY("Gas_Consumed");                              VALUE_u32div1000_LE; // Consumed BUH/gas in kWh
        default : return 0;
      }
      default : UNKNOWN_BYTE;
    }

    case 0x0D : CAT_PSEUDO_SYSTEM;
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        default   : return 0;
      }
      case 0x40 : switch (payloadIndex) {
        case    0   : if (!EE.haSetup && M.R.heatingOnlyX10) return 0;
                    HACONFIG;
                    CHECK(1);
                    KEY("Heating_Cooling_Auto");
                    QOS_SELECT;
                    if (pubHa) {
                      SUBDEVICE("_Mode");
                      HADEVICE_SELECT;
                      HADEVICE_SELECT_OPTIONS("\"Heating\",\"Cooling\",\"Auto\"","'0':'Heating','1':'Cooling','2':'Auto'");
                      HADEVICE_SELECT_COMMAND_TEMPLATE("{% set modes={'Heating':0,'Cooling':1,'Auto':2}%}{{'E3A004E%02X'|format((modes[value]|int) if value in modes.keys() else 'Heating')}}");
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case    1 : HACONFIG;
                    CHECK(1);
                    KEY("RT_LWT");
                    QOS_SELECT;
                    if (pubHa) {
                      SUBDEVICE("_FieldSettings");
                      HADEVICE_SELECT;
                      HADEVICE_SELECT_OPTIONS("\"RT\",\"RT-ext\",\"LWT\"", "'2':'RT','1':'RT-ext','0':'LWT'");
                      HADEVICE_SELECT_COMMAND_TEMPLATE("{% set modes={'RT':2,'RT-ext':1,'LWT':0}%}{{'E3900BB090002%02X'|format(((modes[value])|int) if value in modes.keys() else 0)}}")
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case    2 : // not for HA, but do output entity (but not config), so include HACONFIG, only for availability purposes
                    HACONFIG;
                    CHECK(1);
                    KEY("RT");
                    CHECK_ENTITY;
                    QOS_AVAILABILITY;
                    VALUE_u8;
        case    3 : HACONFIG;
                    CHECK(1);
                    KEY("RT_Modulation");
                    QOS_SELECT;
                    if (pubHa) {
                      SUBDEVICE("_FieldSettings");
                      HADEVICE_SELECT;
                      HADEVICE_SELECT_OPTIONS("\"No modulation\",\"Modulation\"", "'0':'No modulation','1':'Modulation'");
                      HADEVICE_SELECT_COMMAND_TEMPLATE("{% set modes={'No modulation':0,'Modulation':1}%}{{'E39007D090002%02X'|format(((modes[value])|int) if value in modes.keys() else 0)}}")
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      HADEVICE_AVAILABILITY("C\/9\/RT", 1, 0);
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case    4 : HACONFIG;
                    CHECK(1);
                    KEY("RT_Modulation_Max");
                    QOS_NUMBER;
                    if (pubHa) {
                      SUBDEVICE("_FieldSettings");
                      HADEVICE_NUMBER;
                      HADEVICE_NUMBER_RANGE(0, 10, 1);
                      HADEVICE_NUMBER_MODE("box");
                      HADEVICE_NUMBER_COMMAND_TEMPLATE("{{'E39007E090002%02X'|format(value|int)}}");
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      HADEVICE_AVAILABILITY("C\/9\/RT", 1, 0);
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case    5 : HACONFIG;
                    CHECK(1);
                    KEY("Number_Of_Zones");
                    QOS_SELECT; // and QOS_AVAILABILITY
                    if (pubHa) {
                      SUBDEVICE("_FieldSettings");
                      HADEVICE_SELECT;
                      HADEVICE_SELECT_OPTIONS("\"1 LWT zone\",\"2 LWT zones\"", "'1':'1 LWT zone','2':'2 LWT zones'");
                      HADEVICE_SELECT_COMMAND_TEMPLATE("{% set modes={'1 LWT zone':0,'2 LWT zones':1}%}{{'E39006B090002%02X'|format(((modes[value])|int) if value in modes.keys() else 0)}}")
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case    6 :                                                                                                           KEY1_PUB_CONFIG_CHECK_ENTITY("Electricity_Consumed_Heatpump_Available");   VALUE_u8;
        case    7 :                                                                                                           KEY1_PUB_CONFIG_CHECK_ENTITY("Electricity_Consumed_BUH_Available");        VALUE_u8;
        case    8 : HACONFIG;
                    CHECK(1);
                    KEY("Overshoot");
                    QOS_NUMBER;
                    if (pubHa) {
                      SUBDEVICE("_FieldSettings");
                      HADEVICE_NUMBER;
                      HADEVICE_NUMBER_RANGE(1, 4, 1);
                      HADEVICE_NUMBER_MODE("box");
                      HADEVICE_NUMBER_COMMAND_TEMPLATE("{{'E39008B090002%02X'|format(value|int)}}");
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case    9 : HACONFIG;
                    CHECK(1);
                    SUBDEVICE("_Quiet");
                    KEY("Quiet_Mode");
                    QOS_SELECT;
                    if (pubHa) {
                      HADEVICE_SELECT;
                      HADEVICE_SELECT_OPTIONS("\"Auto\",\"Always off\",\"On\"", "'0':'Auto','1':'Always off','2':'On'");
                      HADEVICE_SELECT_COMMAND_TEMPLATE("{% set modes={'Auto':0,'Always off':1,'On':2}%}{{'E3A004C%i'|format(((modes[value])|int) if value in modes.keys() else 0)}}")
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case   10 : HACONFIG;
                    CHECK(1);
                    SUBDEVICE("_Quiet");
                    KEY("Quiet_Level_When_On");
                    QOS_SELECT;
                    if (pubHa) {
                      HADEVICE_SELECT;
                      HADEVICE_SELECT_OPTIONS("\"Level 1\",\"Level 2\",\"Level 3\"", "'0':'Level 1','1':'Level 2','2':' Level 3'");
                      HADEVICE_SELECT_COMMAND_TEMPLATE("{% set modes={'Level 1':0,'Level 2':1,'Level 3':2}%}{{'E3A004D%i'|format(((modes[value])|int) if value in modes.keys() else 0)}}")
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        default   : return 0;
      }
      default   : return 0;
    }
#include "P1P2_Pseudo.h"
    default   : UNKNOWN_BYTE // unknown PacketByte
  }

#elif defined W_SERIES

  // W_SERIES

  byte src;
  switch (packetSrc) {
    case 0x00 : src = 0;
                break;
    case 0x40 : src = 1;
                break;
    default   : src = 2;
                break;
  }
  SRC(src); // set SRC char in mqttTopic
  switch (packetType) {
    // W_SERIES only pseudo packets
    case 0x0C :
                haConfig = 0;
                switch (packetSrc) {
      case 0x00 : return 0;
      case 0x40 : SRC(9);
                  switch (payloadIndex) {
        case    0 : return 0;
        case    1 : if (!electricityMeterDataValid) return 0;
                                                                     HACONFIG; HAPOWER;                                     KEY("Electricity_Power");                                                 VALUE_u16_LE;
        case    2 : return 0;
        case    3 : return 0;
        case    4 : return 0;
        case    5 : if (!electricityMeterDataValid) return 0;
                                                                     HACONFIG; HAKWH;                                       KEY("Electricity_Total");                                                  VALUE_u32_LE;
        default   : return 0;
      }
      default   : return 0;
    }
#include "P1P2_Pseudo.h"
    default : UNKNOWN_BYTE // unknown PacketByte
  }

#elif defined F_SERIES

  byte src;
  static byte C1_subtype = 0;

  switch (packetSrc) {
    case 0x00 : src = 0;
                break;
    case 0x40 : src = 1;
                break;
    case 0x80 : src = 'A' - '0';
                break;
    default   : src = 'H' - '0';
                break;
  }
  if ((packetType & 0xF0) == 0x30) {
    src += 2;  // 2-7 from/to auxiliary controller, 2-3 for F0
    if (payload[-2] == 0xF1) src += 2; // 4-5 for F1
    if (payload[-2] == 0xFF) src += 4; // 6-7 for F2
  }
  if ((packetType & 0xF8) == 0x08) src += 8;          // 8-9 pseudo-packets ESP / ATmega
  if ((packetType & 0xF8) == 0x00) src = ('B' - '0'); // B boot messages 0x00-0x07
  SRC(src); // set SRC char in mqttTopic

  switch (packetType) {
    // Restart packet types 00-05 (restart procedure 00-05 followed by 20, 60-9F, A1, B1, B8, 21)
    // Thermistor package
    // 0DAC = 13 // other 085F
    // 145A = 20 // other 2A98
    case 0x00 ... 0x07 : return 0; // for now
    case 0xA3 : SUBDEVICE("_Thermistors");
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
                                                 CAT_TEMP;           HACONFIG; HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Th1");                                       VALUE_s_f8_8_LE;
        case    5 : if (payload[payloadIndex - 2] & 0x80) return 0;
                                                 CAT_TEMP;           HACONFIG; HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Th2");                                       VALUE_s_f8_8_LE;
        case    8 : if (payload[payloadIndex - 2] & 0x80) return 0;
                                                 CAT_TEMP;           HACONFIG; HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Th3");                                       VALUE_s_f8_8_LE;
        case   11 : if (payload[payloadIndex - 2] & 0x80) return 0;
                                                 CAT_TEMP;           HACONFIG; HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Th4");                                       VALUE_s_f8_8_LE;
        case   14 : if (payload[payloadIndex - 2] & 0x80) return 0;
                                                 CAT_TEMP;           HACONFIG; HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Th5");                                       VALUE_s_f8_8_LE;
        case   17 : if (payload[payloadIndex - 2] & 0x80) return 0;
                                                 CAT_TEMP;           HACONFIG; HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Th6");                                       VALUE_s_f8_8_LE;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    // Main package communication packet types 10-16
    case 0x10 :
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Power_Request");                             VALUE_u8;
        case    1 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Target_Operating_Mode_0");                   VALUE_u8;
        case    2 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Actual_Operating_Mode_0");                   VALUE_u8;
        case    3 : SUBDEVICE("_Setpoints");     CAT_TEMP;           HACONFIG; HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("Setpoint_Temperature");                      VALUE_u8;
        case    4 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_000010_4");                          VALUE_u8;
        case    5 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Fan_Speed");                                 VALUE_u8;
        case    6 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_000010_6");                          VALUE_u8;
        case    7 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_000010_7");                          VALUE_u8;
        case    8 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_000010_8");                          VALUE_u8;
        case    9 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_000010_9");                          VALUE_u8;
        case   10 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_000010_10");                         VALUE_u8;
        case   11 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_000010_11");                         VALUE_u8;
        case   12 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_000010_12");                         VALUE_u8;
        case   13 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("System_Status");                             VALUE_u8;
        case   14 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_000010_14");                         VALUE_u8;
        case   15 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_000010_15");                         VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Power");                                     VALUE_u8;
        case    1 : SUBDEVICE("_Unknown");       CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400010_1");                          VALUE_u8;
        case    2 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Target_Operating_Mode_1");                   VALUE_u8;
        case    3 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Actual_Operating_Mode_1");                   VALUE_u8;
        case    4 : SUBDEVICE("_Setpoints");     CAT_TEMP;           HACONFIG; HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("Setpoint_Temperature_1");                    VALUE_u8;
        case    5 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400010_5");                          VALUE_u8;
        case    6 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Fan_Speed_1");                               VALUE_u8;
        case    7 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400010_7");                          VALUE_u8;
        case    8 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400010_8");                          VALUE_u8;
        case    9 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400010_9");                          VALUE_u8;
        case   10 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400010_10");                         VALUE_u8;
        case   11 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400010_11");                         VALUE_u8;
        case   12 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400010_12");                         VALUE_u8;
        case   13 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400010_13");                         VALUE_u8;
        case   14 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("System_Status_1");                           VALUE_u8;
        case   15 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400010_15");                         VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x11 :
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_000011_0");                          VALUE_u8;
        case    1 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_000011_1");                          VALUE_u8;
        case    2 : return 0;
        case    3 : SUBDEVICE("_Filter");        CAT_SETTING;        HACONFIG;                                              KEY2_PUB_CONFIG_CHECK_ENTITY("Filter_Related");                            VALUE_u16_LE;
        case    4 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_000011_4");                          VALUE_u8;
        case    5 : return 0;
        case    6 : SUBDEVICE("_Sensors");       CAT_TEMP;           HACONFIG; HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_Room");                          VALUE_f8_8_LE;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400011_0");                          VALUE_u8;
        case    1 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400011_1");                          VALUE_u8;
        case    2 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400011_2");                          VALUE_u8;
        case    3 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400011_3");                          VALUE_u8;
        case    4 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400011_4");                          VALUE_u8;
        case    5 : return 0;
        case    6 : SUBDEVICE("_Sensors");       CAT_TEMP;           HACONFIG; HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_Inside_Air_Intake");             VALUE_f8_8_LE;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x18 :
                switch (packetSrc) {
      case 0x80 : switch (payloadIndex) {
        case    0 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Heatpump_On");                               VALUE_u8;
        case    1 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_800018_1");                          VALUE_u8;
        case    2 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_800018_2");                          VALUE_u8;
        case    3 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_800018_3");                          VALUE_u8;
        case    4 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_800018_4");                          VALUE_u8;
        case    5 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_800018_5");                          VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x1F : SUBDEVICE("_Unknown");
                switch (packetSrc) {
      case 0x40 : switch (payloadIndex) {
        case    0 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40001F_0_PercQ");                    VALUE_u8;
        case    1 : return 0;
        case    2 :                                                  HACONFIG;                                              KEY2_PUB_CONFIG_CHECK_ENTITY("Unknown_40001F_1-2_Temp1Q");                 VALUE_f8_8_BE;
        case    3 : return 0;
        case    4 :                                                  HACONFIG;                                              KEY2_PUB_CONFIG_CHECK_ENTITY("Unknown_40001F_3-4_Temp2Q");                 VALUE_f8_8_BE;
        case    5 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40001F_5_Temp3Q");                   VALUE_u8;
        case    6 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40001F_6_sameas_5");                 VALUE_u8;
        case    7 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40001F_7_CounterQ");                 VALUE_u8;
        case   11 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40001F_11_Temp4Q");                  VALUE_u8;
        case   13 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40001F_13_Temp5Q");                  VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x20 : SUBDEVICE("_Unknown");
                switch (packetSrc) {
      case 0x40 : switch (payloadIndex) {
        case    0 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_0");                          VALUE_u8;
        case    1 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_1");                          VALUE_u8;
        case    2 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_2");                          VALUE_u8;
        case    3 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_3");                          VALUE_u8;
        case    4 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_4");                          VALUE_u8;
        case    5 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_5");                          VALUE_u8;
        case    6 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_6");                          VALUE_u8;
        case    7 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_7");                          VALUE_u8;
        case    8 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_8");                          VALUE_u8;
        case    9 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_9");                          VALUE_u8;
        case   10 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_10");                         VALUE_u8;
        case   11 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_11");                         VALUE_u8;
        case   12 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_12");                         VALUE_u8;
        case   13 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_13");                         VALUE_u8;
        case   14 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_14");                         VALUE_u8;
        case   15 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_15");                         VALUE_u8;
        case   16 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_16");                         VALUE_u8;
        case   17 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_17");                         VALUE_u8;
        case   18 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_18");                         VALUE_u8;
        case   19 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_400020_19");                         VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x30 : SUBDEVICE("_Unknown");
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_0");                          VALUE_u8;
        case    1 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_1");                          VALUE_u8;
        case    2 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_2");                          VALUE_u8;
        case    3 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_3");                          VALUE_u8;
        case    4 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_4");                          VALUE_u8;
        case    5 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_5");                          VALUE_u8;
        case    6 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_6");                          VALUE_u8;
        case    7 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_7");                          VALUE_u8;
        case    8 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_8");                          VALUE_u8;
        case    9 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_9");                          VALUE_u8;
        case   10 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_10");                         VALUE_u8;
        case   11 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_11");                         VALUE_u8;
        case   12 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_12");                         VALUE_u8;
        case   13 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_13");                         VALUE_u8;
        case   14 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_14");                         VALUE_u8;
        case   15 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_15");                         VALUE_u8;
        case   16 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_16");                         VALUE_u8;
        case   17 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_17");                         VALUE_u8;
        case   18 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_18");                         VALUE_u8;
        case   19 :                                                  HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F030_19");                         VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
/*
 *  case 0x37 : // FDYQ180MV1 zone names TBD
 */

// for 10/11 0x38 FDY/FXMQ = BCL / LPA
// for 12    0x3B FDYQ = M
//
// 00F038/3B (thermostat)
// C/H
// 0   power off/on               0-3 (2/3 signaling bit)  <== combined with target operation mode  P1P2/P/P1P2MQTT/bridge0/S/2/Power 0
// 0a  mode_HA                                                                                      (combi: S/2/Target_Operating_Mode_HA 0)
// 1   ?
// 2   target operating mode      60-67                    <== combined with power off/on           P1P2/P/P1P2MQTT/bridge0/S/2/Target_Operating_Mode 60
// 3   actual operating mode      0-2 (fan/heat/cool)
// 4/8 setpoint temperature                                <===========                             P1P2/P/P1P2MQTT/bridge0/T/2/Setpoint_Cooling 0   and _Heating
// 6/10 fan speed low/medium/high                          <===========                                                     T/2/Fanspeed_Cooling     and _Heating
//
// 40F038/3B (aux controller)
// C/H
//  0  off/on 0-1
//  1  target operating mode fan/heat/cool/auto/?/?/?/dry  60-67     (combined: 0 or 60-67)
// 2/6 setpoint                                            14-28
// 4/8 fan speed low/medium/high                           17/49/81
//
// HA
// fan mode auto low medium high
//     mode auto off cool heat dry fan_only
//
// 000010
// 0 off/on
// 1 mode 60-67
// 2 mode fan/heat/cool
// 3 setpoint
// 5 fan speed low high
//
// 400010
// 0 off/on
// 2 mode 60-67
// 3 mode fan/heat/cool
// 4 setpoint
// 6 fan speed low high


   case 0x38 :                                                              // FDY125LV1 operation mode, added bytes 16-19 for FXMQ and FTQ
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : HACONFIG;
                    CAT_SETTING;
                    SUBDEVICE("_Mode");
                    CHECK(1);
                    KEY("Power");
                    QOS_SWITCH;
                    if (pubHa) {
                      HADEVICE_SWITCH;
                      HADEVICE_SWITCH_PAYLOADS("F380000", "F380001");
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case    1 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F038_1");                          VALUE_u8;
        case    2 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                           QOS_CLIMATE;       KEY1_PUB_CONFIG_CHECK_ENTITY("Target_Operating_Mode");                     VALUE_u8;
        case    3 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Actual_Operating_Mode");                     VALUE_u8;
        case    4 : // Setpoint_Cooling
                    HACONFIG;
                    CAT_SETTING;
                    HATEMP1;
                    SUBDEVICE("_Setpoints");
                    CHECK(1);
                    KEY("Setpoint_Cooling");
                    QOS_CLIMATE;
                    if (pubHa) {
                      HADEVICE_CLIMATE;
                      // temps, byte 6 in 40F038
                      HADEVICE_CLIMATE_TEMPERATURE("S/2/Setpoint_Cooling", EE.setpointCoolingMin, EE.setpointCoolingMax, 1); // only visible in L1(/L5) mode, not in L0 mode // other values in cooling mode?
                      HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/0/Temperature_Room");
                      HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{{'F3802%02X'|format(value|int)}}");
                      // modes byte 0 (off/on) and 1 (mode) in 40F038
                      //
#ifdef Use_HA_and_off
                      HADEVICE_CLIMATE_MODES("S/2/Target_Operating_Mode_HA", "\"off\",\"auto\",\"heat\",\"cool\",\"dry\",\"fan_only\"","'0':'off','96':'fan_only','97':'heat','98':'cool','99':'auto','103':'dry'");
                      HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modesM={'off':0,'auto':1,'heat':1,'cool':1,'dry':1,'fan_only':1} %}{% set modes={'off':0,'auto':99,'heat':97,'cool':98,'dry':103,'fan_only':96} %}{{('F380001 3801%02X'|format((modes[value]|int) if value in modes.keys() else 0)) if (modesM[value]) else 'F380000'}}");
#else
                      HADEVICE_CLIMATE_MODES("S/2/Target_Operating_Mode", "\"auto\",\"heat\",\"cool\",\"dry\",\"fan_only\"","'96':'fan_only','97':'heat','98':'cool','99':'auto','103':'dry'");
                      HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'auto':99,'heat':97,'cool':98,'dry':103,'fan_only':96} %}{{'F3801%02X'|format((modes[value]|int) if value in modes.keys() else 99)}}");
#endif
                      // fan_modes, fan speed heating, byte 8 in 40F038
                      HADEVICE_CLIMATE_FAN_MODES("S/2/Fan_Speed_Cooling", "\"low\",\"medium\",\"high\"", "'17':'low','49':'medium','81':'high'"); // some models perhaps also auto mode?
                      HADEVICE_CLIMATE_FAN_MODE_COMMAND_TEMPLATE("{% set modes={'low':17,'medium':49,'high':81} %} {{'F3804%02X'|format((modes[value]|int) if value in modes.keys() else 17)}} ");
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case    5 : SUBDEVICE("_Unknown");       CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Error_Setting_1_Q");                         VALUE_u8;
        case    6 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG;                        QOS_CLIMATE;          KEY1_PUB_CONFIG_CHECK_ENTITY("Fan_Speed_Cooling");                         VALUE_u8;
        case    7 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F038_7");                          VALUE_u8;
        case    8 : // Setpoint_Heating
                    HACONFIG;
                    CAT_SETTING;
                    HATEMP1;
                    SUBDEVICE("_Setpoints");
                    CHECK(1);
                    KEY("Setpoint_Heating");
                    QOS_CLIMATE;
                    if (pubHa) {
                      HADEVICE_CLIMATE;
                      // temps, byte 6 in 40F038
                      HADEVICE_CLIMATE_TEMPERATURE("S/2/Setpoint_Heating", EE.setpointHeatingMin, EE.setpointHeatingMax, 1); // only visible in L1(/L5) mode, not in L0 mode
                      HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/0/Temperature_Room");
                      HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{{'F3806%02X'|format(value|int)}}");
                      // modes byte 0 (off/on) and 1 (mode) in 40F038
#ifdef Use_HA_and_off
                      HADEVICE_CLIMATE_MODES("S/2/Target_Operating_Mode_HA", "\"off\",\"auto\",\"heat\",\"cool\",\"dry\",\"fan_only\"","'0':'off','96':'fan_only','97':'heat','98':'cool','99':'auto','103':'dry'");
                      HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modesM={'off':0,'auto':1,'heat':1,'cool':1,'dry':1,'fan_only':1} %}{% set modes={'off':0,'auto':99,'heat':97,'cool':98,'dry':103,'fan_only':96} %}{{('F380001 3801%02X'|format((modes[value]|int) if value in modes.keys() else 0)) if (modesM[value]) else 'F380000'}}");
#else
                      HADEVICE_CLIMATE_MODES("S/2/Target_Operating_Mode", "\"auto\",\"heat\",\"cool\",\"dry\",\"fan_only\"","'96':'fan_only','97':'heat','98':'cool','99':'auto','103':'dry'");
                      HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'auto':99,'heat':97,'cool':98,'dry':103,'fan_only':96} %}{{'F3801%02X'|format((modes[value]|int) if value in modes.keys() else 99)}}");
#endif
                      // fan_modes, fan speed heating, byte 8 in 40F038
                      HADEVICE_CLIMATE_FAN_MODES("S/2/Fan_Speed_Heating", "\"low\",\"medium\",\"high\"", "'17':'low','49':'medium','81':'high'"); // some models perhaps also auto mode?
                      HADEVICE_CLIMATE_FAN_MODE_COMMAND_TEMPLATE("{% set modes={'low':17,'medium':49,'high':81} %} {{'F3808%02X'|format((modes[value]|int) if value in modes.keys() else 17)}} ");
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case    9 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Error_Setting_2_Q");                         VALUE_u8;
        case   10 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG;                         QOS_CLIMATE;         KEY1_PUB_CONFIG_CHECK_ENTITY("Fan_Speed_Heating");                         VALUE_u8;
        case   11 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F038_11");                         VALUE_u8;
        case   12 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F038_12");                         VALUE_u8;
        case   13 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F038_13");                         VALUE_u8;
        case   14 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Clear_Error_Code");                          VALUE_u8;
        case   15 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F038_15");                         VALUE_u8;
        case   16 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F038_16");                         VALUE_u8;
        case   17 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F038_17");                         VALUE_u8;
        case   18 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F038_18");                         VALUE_u8;
        case   19 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F038_19");                         VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Power");                                     VALUE_u8;
        case    1 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Target_Operating_Mode_3");                   VALUE_u8;
        case    2 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG; HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("Setpoint_Cooling_3");                        VALUE_u8;
        case    3 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Error_Setting_1_Q");                         VALUE_u8;
        case    4 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Fan_Speed_Cooling_1");                       VALUE_u8;
        case    5 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F038_5");                          VALUE_u8;
        case    6 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG; HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("Setpoint_Heating_3");                        VALUE_u8;
        case    7 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Error_Setting_2_Q");                         VALUE_u8;
        case    8 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Fan_Speed_Heating_1");                       VALUE_u8;
        case    9 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F038_9");                          VALUE_u8;
        case   10 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F038_10");                         VALUE_u8;
        case   11 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Clear_Error_Code");                          VALUE_u8;
        case   12 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Clear_Error_Code");                          VALUE_u8;
        case   13 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F038_13");                         VALUE_u8;
        case   14 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Fan_Mode_Q");                                VALUE_u8;
        case   15 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F038_15");                         VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x39 :                                                            // FDY125LV1 filter?
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F039_0");                          VALUE_u8;
        case    1 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F039_1");                          VALUE_u8;
        case    2 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F039_2");                          VALUE_u8;
        case    3 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F039_3");                          VALUE_u8;
        case    4 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F039_4");                          VALUE_u8;
        case    5 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F039_5");                          VALUE_u8;
        case    6 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F039_6");                          VALUE_u8;
        case    7 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F039_7");                          VALUE_u8;
        case    8 : return 0;
        case    9 : SUBDEVICE("_Filter");        CAT_SETTING;        HACONFIG;                                              KEY2_PUB_CONFIG_CHECK_ENTITY("Filter_Related");                            VALUE_u16hex_LE;
        case   10 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F039_10");                         VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : SUBDEVICE("_Filter");        CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Filter_Alarm_Reset_Q");                      VALUE_u8;
        case    1 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F039_1");                          VALUE_u8;
        case    2 : return 0;
        case    3 : SUBDEVICE("_Filter");        CAT_SETTING;        HACONFIG;                                              KEY2_PUB_CONFIG_CHECK_ENTITY("Filter_Related");                            VALUE_u16hex_LE;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x3B :                                                           // FDYQ180MV1 operation mode
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : HACONFIG;
                    CAT_SETTING;
                    SUBDEVICE("_Mode");
                    CHECK(1);
                    KEY("Power");
                    QOS_SWITCH;
                    if (pubHa) {
                      HADEVICE_SWITCH;
                      HADEVICE_SWITCH_PAYLOADS("F3B0000", "F3B0001");
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;                                                                                                                                                                      VALUE_u8;
                    VALUE_u8;
        case    1 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03B_1");                          VALUE_u8;
        case    2 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                          QOS_CLIMATE;        KEY1_PUB_CONFIG_CHECK_ENTITY("Target_Operating_Mode");                     VALUE_u8;
        case    3 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Actual_Operating_Mode");                     VALUE_u8;
        case    4 : // Setpoint_Cooling
                    HACONFIG;
                    CAT_SETTING;
                    HATEMP1;
                    SUBDEVICE("_Setpoints");
                    CHECK(1);
                    KEY("Setpoint_Cooling");
                    QOS_CLIMATE;
                    if (pubHa) {
                      HADEVICE_CLIMATE;
                      // temps, byte 6 in 40F038
                      HADEVICE_CLIMATE_TEMPERATURE("S/2/Setpoint_Cooling", EE.setpointCoolingMin, EE.setpointCoolingMax, 1); // only visible in L1(/L5) mode, not in L0 mode // other values in cooling mode?
                      HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/0/Temperature_Room");
                      HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{{'F3B02%02X'|format(value|int)}}");
                      // modes byte 0 (off/on) and 1 (mode) in 40F03B
#ifdef Use_HA_and_off
                      HADEVICE_CLIMATE_MODES("S/2/Target_Operating_Mode_HA", "\"off\",\"auto\",\"heat\",\"cool\",\"dry\",\"fan_only\"","'0':'off','96':'fan_only','97':'heat','98':'cool','99':'auto','103':'dry'");
                      HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modesM={'off':0,'auto':1,'heat':1,'cool':1,'dry':1,'fan_only':1} %}{% set modes={'off':0,'auto':99,'heat':97,'cool':98,'dry':103,'fan_only':96} %}{{('F3B0001 3801%02X'|format((modes[value]|int) if value in modes.keys() else 0)) if (modesM[value]) else 'F3B0000'}}");
#else
                      HADEVICE_CLIMATE_MODES("S/2/Target_Operating_Mode", "\"auto\",\"heat\",\"cool\",\"dry\",\"fan_only\"","'96':'fan_only','97':'heat','98':'cool','99':'auto','103':'dry'");
                      HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'auto':99,'heat':97,'cool':98,'dry':103,'fan_only':96} %}{{'F3B01%02X'|format((modes[value]|int) if value in modes.keys() else 99)}}");
#endif
                      // fan_modes, fan speed heating, byte 8 in 40F03B
                      HADEVICE_CLIMATE_FAN_MODES("S/2/Fan_Speed_Cooling", "\"low\",\"medium\",\"high\"", "'17':'low','49':'medium','81':'high'"); // some models perhaps also auto mode?
                      HADEVICE_CLIMATE_FAN_MODE_COMMAND_TEMPLATE("{% set modes={'low':17,'medium':49,'high':81} %} {{'F3B04%02X'|format((modes[value]|int) if value in modes.keys() else 17)}} ");
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case    5 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Error_Setting_1_Q");                         VALUE_u8;
        case    6 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG;                          QOS_CLIMATE;        KEY1_PUB_CONFIG_CHECK_ENTITY("Fan_Speed_Cooling");                         VALUE_u8;
        case    7 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03B_7");                          VALUE_u8;
        case    8 : // Setpoint_Heating
                    HACONFIG;
                    CAT_SETTING;
                    HATEMP1;
                    SUBDEVICE("_Setpoints");
                    CHECK(1);
                    KEY("Setpoint_Heating");
                    QOS_CLIMATE;
                    if (pubHa) {
                      HADEVICE_CLIMATE;
                      // temps, byte 6 in 40F038
                      HADEVICE_CLIMATE_TEMPERATURE("S/2/Setpoint_Heating", EE.setpointHeatingMin, EE.setpointHeatingMax, 1); // only visible in L1(/L5) mode, not in L0 mode
                      HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/0/Temperature_Room");
                      HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{{'F3B06%02X'|format(value|int)}}");
                      // modes byte 0 (off/on) and 1 (mode) in 40F03B
#ifdef Use_HA_and_off
                      HADEVICE_CLIMATE_MODES("S/2/Target_Operating_Mode_HA", "\"off\",\"auto\",\"heat\",\"cool\",\"dry\",\"fan_only\"","'0':'off','96':'fan_only','97':'heat','98':'cool','99':'auto','103':'dry'");
                      HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modesM={'off':0,'auto':1,'heat':1,'cool':1,'dry':1,'fan_only':1} %}{% set modes={'off':0,'auto':99,'heat':97,'cool':98,'dry':103,'fan_only':96} %}{{('F3B0001 3801%02X'|format((modes[value]|int) if value in modes.keys() else 0)) if (modesM[value]) else 'F3B0000'}}");
#else
                      HADEVICE_CLIMATE_MODES("S/2/Target_Operating_Mode", "\"auto\",\"heat\",\"cool\",\"dry\",\"fan_only\"","'96':'fan_only','97':'heat','98':'cool','99':'auto','103':'dry'");
                      HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'auto':99,'heat':97,'cool':98,'dry':103,'fan_only':96} %}{{'F3B01%02X'|format((modes[value]|int) if value in modes.keys() else 99)}}");
#endif
                      // fan_modes, fan speed heating, byte 8 in 40F03B
                      HADEVICE_CLIMATE_FAN_MODES("S/2/Fan_Speed_Heating", "\"low\",\"medium\",\"high\"", "'17':'low','49':'medium','81':'high'"); // some models perhaps also auto mode?
                      HADEVICE_CLIMATE_FAN_MODE_COMMAND_TEMPLATE("{% set modes={'low':17,'medium':49,'high':81} %} {{'F3B08%02X'|format((modes[value]|int) if value in modes.keys() else 17)}} ");
                      HADEVICE_AVAILABILITY("A\/8\/Control_Function", 1, 0);
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case    9 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Error_Setting_2_Q");                         VALUE_u8;
        case   10 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG;                            QOS_CLIMATE;      KEY1_PUB_CONFIG_CHECK_ENTITY("Fan_Speed_Heating");                         VALUE_u8;
        case   11 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03B_11");                         VALUE_u8;
        case   12 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03B_12");                         VALUE_u8;
        case   13 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03B_13");                         VALUE_u8;
        case   14 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Clear_Error_Code");                          VALUE_u8;
        case   15 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03B_15");                         VALUE_u8;
        case   16 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03B_16");                         VALUE_u8;
        case   17 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Zone_Status");                               VALUE_u8;
        case   18 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Fan_Mode");                                  VALUE_u8;
        case   19 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03B_14");                         VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 :                              CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Power");                                     VALUE_u8;
        case    1 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Target_Operating_Mode_3");                   VALUE_u8;
        case    2 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG; HATEMP1;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("Setpoint_Cooling_3");                        VALUE_u8;
        case    3 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Error_Setting_1_Q");                         VALUE_u8;
        case    4 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Fan_Speed_Cooling_1");                       VALUE_u8;
        case    5 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F03B_5");                          VALUE_u8;
        case    6 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG; HATEMP1;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("Setpoint_Heating_3");                        VALUE_u8;
        case    7 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Error_Setting_2_Q");                         VALUE_u8;
        case    8 : SUBDEVICE("_Setpoints");     CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Fan_Speed_Heating_1");                       VALUE_u8;
        case    9 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F03B_9");                          VALUE_u8;
        case   10 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F03B_10");                         VALUE_u8;
        case   11 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F03B_11");                         VALUE_u8;
        case   12 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F03B_12");                         VALUE_u8;
        case   13 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F03B_13");                         VALUE_u8;
        case   14 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F03B_14");                         VALUE_u8;
        case   15 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F03B_15");                         VALUE_u8;
        case   16 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Zone_Status");                               VALUE_u8;
        case   17 : SUBDEVICE("_Mode");          CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Fan_Mode");                                  VALUE_u8;
        case   18 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F03B_18");                         VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x3C :                                                            // FDYQ180MV1 filter
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03C_0");                          VALUE_u8;
        case    1 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03C_1");                          VALUE_u8;
        case    2 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03C_2");                          VALUE_u8;
        case    3 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03C_3");                          VALUE_u8;
        case    4 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03C_4");                          VALUE_u8;
        case    5 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03C_5");                          VALUE_u8;
        case    6 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03C_6");                          VALUE_u8;
        case    7 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03C_7");                          VALUE_u8;
        case    8 : return 0;
        case    9 : SUBDEVICE("_Filter");        CAT_SETTING;        HACONFIG;                                              KEY2_PUB_CONFIG_CHECK_ENTITY("Filter_Related");                            VALUE_u16hex_LE;
        case   10 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03C_10");                         VALUE_u8;
        case   11 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_00F03C_11");                         VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : SUBDEVICE("_Filter");        CAT_SETTING;        HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Filter_Alarm_Reset_Q");                      VALUE_u8;
        case    1 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_40F03C_1");                          VALUE_u8;
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0xC1 :                                                            // FDY125LV1 service mode
                switch (packetSrc) {
      case 0x40 : switch (payloadIndex) {
        case    0 : C1_subtype = payloadByte; break;
        case    1 : SUBDEVICE("_Unknown");                           HACONFIG;                                              KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown_0000C1_1");                          VALUE_u8;
        case    2 : return 0;
        case    3 : switch (C1_subtype) {
          case   1 : SUBDEVICE("_Sensors");      CAT_TEMP;           HACONFIG; HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_Air_Intake");                    VALUE_f8_8_LE;
          case   2 : SUBDEVICE("_Sensors");      CAT_TEMP;           HACONFIG; HATEMP1;                                     KEY2_PUB_CONFIG_CHECK_ENTITY("Temperature_Heat_Exchanger");                VALUE_f8_8_LE;
          default  : UNKNOWN_BYTE;
        }
        default   : UNKNOWN_BYTE;
      }
      default   :               UNKNOWN_BYTE;
    }
    case 0x0C : SRC(9);                          CAT_PSEUDO_SYSTEM;
                switch (packetSrc) {
      case 0x40 : switch (payloadIndex) {
        default : return 0;
      }
      default : UNKNOWN_BYTE;
    }
#include "P1P2_Pseudo.h"
    default : UNKNOWN_BYTE // unknown PacketByte
  }

  return 0;

#elif defined H_SERIES /* *_SERIES */

  byte src;
  switch (packetSrc) {
    case 0x21 : src = 2; break; // indoor unit info
    case 0x89 : src = 8; break; // IU and OU system info
    case 0x8A : src = 7; break; // ?
    case 0x41 : src = 4; break; // sender is Hitachi remote control or Airzone
    case 0x00 : src = 0; break; // pseudopacket, ATmega
    case 0x40 : src = 1; break; // pseudopacket, ESP
    case 0x79 : src = 5; break; // HiBox AHP-SMB-01 or ATW-TAG-02 ?
    case 0xFF : src = 6; break; // HiBox AHP-SMB-01 or ATW-TAG-02 ?
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

  HACONFIG;
  switch (packetSrc) {
    // 0x89 : sender is certainly the indoor unit providing data from the system
    //        including data from the outdoor unit
    case 0x89: switch (packetType) {
      case 0x29: switch (payload[4])  { // payload[4] can be 0xE1..0xE5, currently decode only 0xE2
        case 0xE2: switch (EE.hitachiModel) {
          case 1 : switch (payloadIndex) {
            case  6:                               CAT_MEASUREMENT;              HAPERCENT;                                   KEY1_PUB_CONFIG_CHECK_ENTITY("OUExpansionValve");                          VALUE_u8;
            case  7:                               CAT_MEASUREMENT;              HAPERCENT;                                   KEY1_PUB_CONFIG_CHECK_ENTITY("IUExpansionValve");                          VALUE_u8;
            case  8:                               CAT_MEASUREMENT;              HAFREQ;                                      KEY1_PUB_CONFIG_CHECK_ENTITY("TargetCompressorFrequency");                 VALUE_u8;
            case  9:                               CAT_MEASUREMENT;                                                           KEY1_PUB_CONFIG_CHECK_ENTITY("ControlCircuitRunStop");                     VALUE_u8;
            case 10:                               CAT_MEASUREMENT;                                                           KEY1_PUB_CONFIG_CHECK_ENTITY("HeatpumpIntensity");                         VALUE_u8;
            case 14:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("IUAirInletTemperature");                     VALUE_s8;
            case 15:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("IUAirOutletTemperature");                    VALUE_s8;
            case 20:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("OUHeatExchangerTemperatureOutput");          VALUE_s8;
            case 21:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("IUGasPipeTemperature");                      VALUE_s8;
            case 22:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("IULiquidPipeTemperature");                   VALUE_s8;
            case 23:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("OutdoorAirTemperature");                     VALUE_s8;
            case 24:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("TempQ");                                     VALUE_s8;
            case 25:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("CompressorTemperature");                     VALUE_s8;
            case 26:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("TemperatureEvaporator");                     VALUE_s8;
            case 29:                               CAT_SETTING;                  HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("TemperatureSetting");                        VALUE_s8;
            case 37: return 0; // do not report checksum
            default  : UNKNOWN_BYTE;
          }
          case 2 : switch (payloadIndex) {
            case  7:                               CAT_MEASUREMENT;              HAPERCENT;                                   KEY1_PUB_CONFIG_CHECK_ENTITY("InsideRegulatorOpening");         VALUE_u8;
            case  8:                               CAT_MEASUREMENT;              HAPERCENT;                                   KEY1_PUB_CONFIG_CHECK_ENTITY("OutsideRegulatorOpening");        VALUE_u8;
            case  9:                               CAT_MEASUREMENT;              HAFREQ;                                      KEY1_PUB_CONFIG_CHECK_ENTITY("CompressorFrequency");            VALUE_u8;
            case 11:                               CAT_MEASUREMENT;              HACURRENT;                                   KEY1_PUB_CONFIG_CHECK_ENTITY("HeatpumpIntensity");              VALUE_u8;
            case 15:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("WaterINTemperature");             VALUE_s8;
            case 16:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("WaterOUTTemperature");            VALUE_s8;
            case 21:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("ExchangerOutputTemperature");     VALUE_s8;
            case 22:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("GasTemperature");                 VALUE_s8;
            case 23:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("LiquidTemperature");              VALUE_s8;
            case 24:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("ExternalSensorTemperature");      VALUE_s8;
            case 26:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("CompressorDischargeTemperature"); VALUE_s8;
            case 27:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("EvaporatorTemperature");          VALUE_s8;
            case 37: return 0; // do not report checksum
            default  : UNKNOWN_BYTE;
          }
          default  : UNKNOWN_BYTE; // unknown hitachiModel
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
/*
        case 0x06 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--06");                          VALUE_s8; // useless
*/
        case 0x07:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("IUAirInletTemperature");                     VALUE_s8;
        case 0x08:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("IUAirOutletTemperature");                    VALUE_s8;
        case 0x09:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("IULiquidPipeTemperature");                   VALUE_s8;
        case 0x0A:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("IURemoteSensorAirTemperature");              VALUE_s8;
        case 0x0B:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("OutdoorAirTemperature");                     VALUE_s8;
        case 0x0C:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("IUGasPipeTemperature");                      VALUE_s8;
        case 0x0D:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("OUHeatExchangerTemperature1");               VALUE_s8;
        case 0x0E:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("OUHeatExchangerTemperature2");               VALUE_s8;
        case 0x0F:                               CAT_TEMP;                     HATEMP0;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("CompressorTemperature");                     VALUE_s8;
        case 0x10:                               CAT_MEASUREMENT;                                                           KEY1_PUB_CONFIG_CHECK_ENTITY("HighPressure");                              VALUE_u8;
        case 0x11:                               CAT_MEASUREMENT;                                                           KEY1_PUB_CONFIG_CHECK_ENTITY("LowPressure_x10");                           VALUE_u8;
        case 0x12:                               CAT_MEASUREMENT;              HAFREQ;                                      KEY1_PUB_CONFIG_CHECK_ENTITY("TargetCompressorFrequency");                 VALUE_u8;
        case 0x13:                               CAT_MEASUREMENT;              HAFREQ;                                      KEY1_PUB_CONFIG_CHECK_ENTITY("CompressorFrequency");                       VALUE_u8;
        case 0x14:                               CAT_MEASUREMENT;              HAPERCENT;                                   KEY1_PUB_CONFIG_CHECK_ENTITY("IUExpansionValve");                          VALUE_u8;
        case 0x15:                               CAT_MEASUREMENT;              HAPERCENT;                                   KEY1_PUB_CONFIG_CHECK_ENTITY("OUExpansionValve");                          VALUE_u8;
/*
        case 0x16:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--16");                          VALUE_u8; // useless
        case 0x17:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--17");                          VALUE_u8; // useless
*/
        case 0x18:                               CAT_MEASUREMENT;              HACURRENT;                                   KEY1_PUB_CONFIG_CHECK_ENTITY("CompressorCurrent");                         VALUE_u8; // ?
/*
        case 0x19:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--19");                          VALUE_u8; // useless
        case 0x1A:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--1A");                          VALUE_u8; // useless
        case 0x1B:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--1B");                          VALUE_u8; // useless
        case 0x1C:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--1C");                          VALUE_u8; // useless
        case 0x1D:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--1D");                          VALUE_u8; // useless
        case 0x1E:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--1E");                          VALUE_u8; // useless
        case 0x1F:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--1F");                          VALUE_u8; // useless
        case 0x20:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--20");                          VALUE_u8; // useless
*/
        case 0x21: switch (bitNr) {
          case 8: bcnt = 9; BITBASIS;
          case 0:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-21-0");
          case 1:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-21-1-OUnitOn");
          case 2:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-21-2");
          case 3:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-21-3-CompressorOn");
          case 4:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-21-4");
          case 5:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-21-5");
          case 6:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-21-6");
          case 7:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-21-7-OUStarting");
          default: UNKNOWN_BIT;
        }
        case 0x22: switch (bitNr) {
          case 8: bcnt = 1; BITBASIS;
          case 0:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-22-0");
          case 1:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-22-1");
          case 2:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-22-2");
          case 3:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-22-3");
          case 4:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-22-4");
          case 5:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-22-5");
          case 6:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-22-6");
          case 7:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("892D-22-7");
          default: UNKNOWN_BIT;
        }
        case 0x23 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--23");                          VALUE_u8; // useless
        case 0x24 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--24");                          VALUE_u8; // useless
        case 0x25 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--25");                          VALUE_u8; // useless
        case 0x26 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--26");                          VALUE_u8; // useless
        case 0x27 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--27");                          VALUE_u8; // useless
        case 0x28 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-892D--28");                          VALUE_u8; // useless
        case 0x29: return 0; // do not report checksum
        default  : UNKNOWN_BYTE;
      }
    case 0x27: // type 2 of message
                                                 CAT_MEASUREMENT;
                 switch (payloadIndex) {
/*
        case 0x06 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--06");                          VALUE_u8; // useless
*/
        case 0x07: // 8 bit byte : 65 HEAT, 64 HEAT STOP, 33 VENTIL, 67 DEFROST
                   switch (bitNr) {
          case 8: bcnt = 2; BITBASIS;
          case 0:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-07-0-OUnitOn");
          case 1:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-07-1-DEFROST");
          case 2:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-07-2");
          case 3:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-07-3");
          case 4:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-07-4");
          case 5:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-07-5-VentilMode");
          case 6:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-07-6-HeatMode");
          case 7:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-07-7");
          default: UNKNOWN_BIT;
        }
        case 0x08: switch (bitNr) {
          case 8: bcnt = 3; BITBASIS;
          case 0:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-08-0");
          case 1:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-08-1");
          case 2:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-08-2");
          case 3:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-08-3");
          case 4:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-08-4");
          case 5:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-08-5");
          case 6:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-08-6");
          case 7:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-08-7");
          default: UNKNOWN_BIT;
        }
        case 0x09:                               CAT_TEMP;                     HATEMP1;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("TemperatureSetpoint");                       VALUE_u8;
/*
        case 0x0A :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("8927--0A-clock");                            VALUE_u8; // 8 bit byte : 129 / 193 each 30s
        case 0x0B :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--0B");                          VALUE_u8; // useless
        case 0x0C :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--0C");                          VALUE_u8; // useless
        case 0x0D :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--0D");                          VALUE_u8; // useless
        case 0x0E :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--0E");                          VALUE_u8; // useless
        case 0x0F :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--0F");                          VALUE_u8; // useless
        case 0x10 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--10");                          VALUE_u8; // useless
        case 0x11 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--11");                          VALUE_u8; // useless
        case 0x12 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--12");                          VALUE_u8; // useless
        case 0x13 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--13");                          VALUE_u8; // useless
        case 0x14 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--14");                          VALUE_u8; // useless
        case 0x15 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--15");                          VALUE_u8; // useless
*/
        case 0x16:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("8927--16-unsure");                           VALUE_u8; // ?
/*
        case 0x17 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--17");                          VALUE_u8; // useless
        case 0x18 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--18");                          VALUE_u8; // useless
        case 0x19 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--19");                          VALUE_u8; // useless
        case 0x1A :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--1A");                          VALUE_u8; // useless
        case 0x1B :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-8927--1B");                          VALUE_u8; // useless
*/
        case 0x1C: switch (bitNr) {
          case 8: bcnt = 4; BITBASIS;
          case 0:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-1C-0");
          case 1:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-1C-1-PREHEAT");
          case 2:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-1C-2");
          case 3:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-1C-3");
          case 4:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-1C-4");
          case 5:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-1C-5");
          case 6:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-1C-6");
          case 7:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("8927-1C-7");
          default: UNKNOWN_BIT;
        }
        case 0x23: return 0; // do not report checksum
        default  : UNKNOWN_BYTE;
      }
      default  : UNKNOWN_BYTE; // unknown packet type
    }
    // 0x21 : sender is certainly the indoor unit
    case 0x21: switch (packetType) {
      case 0x1C: switch (payloadIndex) { // new v0.9.51
        case   7 : switch (bitNr) {
          case   8 : bcnt = 10; BITBASIS;
          case   0 : HACONFIG;                                                                                                KEYBIT_PUB_CONFIG_PUB_ENTITY("Power_On");
          default: UNKNOWN_BIT;
        }
        case   8 : switch (bitNr) {
          case   8 : bcnt = 11; BITBASIS;
          case 1 ... 2 : HACONFIG;                                                                                            KEYBITS_PUB_CONFIG_PUB_ENTITY(1, 2, "Fanmode"); // 01 = high, 02 = medium, 03 = low
          default: UNKNOWN_BIT;
        }
        case    9 : HACONFIG;                                                                                                 KEY1_PUB_CONFIG_CHECK_ENTITY("Temperature");   VALUE_u8;
        // perhaps case 0x18 : return 0; // do not report checksum
        default   : UNKNOWN_BYTE;
      }
      case 0x29: switch (payload[4])  { // payload[4] can be 0xF1..0xF5, currently do not decode
        default: switch (payloadIndex) {
          case 0x25: return 0; // do not report checksum
          default  : UNKNOWN_BYTE;
        }
      }
      // type 1 of message has length 0x12
      case 0x12:                                 CAT_SETTING;
                 switch (payloadIndex) {
/*
        case 0x06 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-2112--06");                          VALUE_flag8;
*/
        case 0x07: // AC MODE 195 HEAT, 192 HEAT STOP, 160 VENTIL STOP, 163 VENTIL
                   switch (bitNr) {
          case 8: bcnt = 5; BITBASIS;
          case 0:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("ACMode0UnitOn");
          case 1:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("ACMode1UnitOn");
          case 2:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("ACMode2");
          case 3:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("ACMode3");
          case 4:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("ACMode4");
          case 5:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("ACMode5Ventil");
          case 6:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("ACMode6Heat");
          case 7:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("ACMode7");
          default: UNKNOWN_BIT;
        }
        case 0x08: // VENTILATION 8 bit byte : 8 LOW, 4 MED, 2 HIGH
                   switch (bitNr) {
          case 8: bcnt = 6; BITBASIS;
          case 1:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("VentilHighOn");
          case 2:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("VentilMedOn");
          case 3:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("VentilLowOn");
          default: UNKNOWN_BIT;
        }
        case 0x09:                               CAT_TEMP;                     HATEMP1;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("TemperatureSetpoint");                       VALUE_u8;
/*
        case 0x0A :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-2112--0A");                          VALUE_u8; // useless
*/
        case 0x0B:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-2112--0B");                          VALUE_u8; // ?
/*
        case 0x0C :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-2112--0C");                          VALUE_u8; // useless
        case 0x0D :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-2112--0D");                          VALUE_u8; // useless
*/
        case 0x0E: return 0; // do not report checksum
        default  : UNKNOWN_BYTE;
      }
      /*
      // type 2 of message has length 0x0B, no information found in these messages
      case 0x0B :                                CAT_SETTING;
                  switch (payloadIndex) {
        case 0x06 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-210B--06");                          VALUE_u8; // ?
        case 0x06 :                                                                                                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-210B--06");                          VALUE_u8; // ?
        default   : UNKNOWN_BYTE;
      }
      */
      default  : UNKNOWN_BYTE; // return 0; // unknown packet type
    }
    // 0x41 : sender is Hitachi remote control or Airzone
    case 0x41: switch (packetType) {
      // type 1 of message has length 0x18
      case 0x18:                                 CAT_SETTING;
                 switch (payloadIndex) {
        // ********************
        // ***** Here we process all data as they are useful to send a remote command
        // ********************
        case 0x00:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ00");                              VALUE_u8; // useless
        case 0x01:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ01");                              VALUE_u8; // useless
        case 0x02:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ02");                              VALUE_u8; // useless
        case 0x03:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ03");                              VALUE_u8; // useless
        case 0x04:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ04");                              VALUE_u8; // useless
        case 0x05:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ05");                              VALUE_u8; // useless
        case 0x06:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ06");                              VALUE_u8; // useless
        case 0x07: switch (bitNr) {
          case 8: bcnt = 7; BITBASIS;
          case 0:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("SetACMode0UnitOn");
          case 1:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("SetACMode1UnitOn");
          case 2:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("SetACMode2");
          case 3:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("SetACMode3");
          case 4:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("SetACMode4");
          case 5:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("SetACMode5Ventil");
          case 6:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("SetACMode6Heat");
          case 7:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("SetACMode7");
          default: UNKNOWN_BIT;
        }
        case 0x08: switch (bitNr) {
          case 8: bcnt = 8; BITBASIS;
          case 1:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("SetVentilHighOn");
          case 2:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("SetVentilMedOn");
          case 3:                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("SetVentilLowOn");
          default: UNKNOWN_BIT;
        }
        case 0x09:                               CAT_TEMP;                     HATEMP1;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("SetTemperatureSetpoint");                    VALUE_u8;
        case 0x0A:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ0A");                              VALUE_u8; // useless
        case 0x0B:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ0B");                              VALUE_u8; // useless
        case 0x0C:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ0C");                              VALUE_u8; // useless
        case 0x0D:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ0D");                              VALUE_u8; // useless
        case 0x0E:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ0E");                              VALUE_u8; // useless
        case 0x0F:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ0F");                              VALUE_u8; // useless
        case 0x10:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ10");                              VALUE_u8; // useless
        case 0x11:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ11");                              VALUE_u8; // useless
        case 0x12:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ12");                              VALUE_u8; // useless
        case 0x13:                                                                                                          KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-AZ13");                              VALUE_u8; // useless
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
      default: UNKNOWN_BYTE; // unknown packet type
    }
    // 0x8A : sender is ?
    case 0x8A: switch (packetType) {
      case 0x29: switch (payload[4])  { // payload[4] can be 0xF1..0xF5, seen F1 only, currently decode only 0xE2
        case 0xF1: switch (payloadIndex) {
          // perhaps case 0x25: return 0; // do not report checksum
          default : UNKNOWN_BYTE;
        }
        default  : return 0;
      }
      default: UNKNOWN_BYTE; // unknown packet type
    }
    // 0x79 : sender is ?
    // new in v0.9.55
    case 0x79: switch (packetType) {
      case 0x2E: switch (payloadIndex) {
        // perhaps case 0x2A: return 0; // do not report checksum
        default : UNKNOWN_BYTE;
      }
      default  : return 0;
    }
    // 0xFF : sender is ?
    // new in v0.9.55
    case 0xFF: switch (packetType) {
      case 0x23: switch (payloadIndex) {
        // perhaps case 0x1F: return 0; // do not report checksum
        default : UNKNOWN_BYTE;
      }
      default  : return 0;
    }
    // new in v0.9.45 / v0.9.55
    case 0x19: switch (packetType) {
      case 0x09: switch (payloadIndex) {
        // perhaps case 0x05: return 0; // do not report checksum
        default : UNKNOWN_BYTE;
      }
      case 0x0A: switch (payloadIndex) {
        case  5  : HACONFIG; CAT_TEMP;        HATEMP0;   KEY1_PUB_CONFIG_CHECK_ENTITY("Tgas");                         VALUE_u8; // Temperature_Gas
        case  6  : HACONFIG; CAT_TEMP;        HATEMP0;   KEY1_PUB_CONFIG_CHECK_ENTITY("Tliq");                         VALUE_u8; // Temperature_liquid // so this is not a checksum
        cefault  : UNKNOWN_BYTE;
      }
      case 0x10: UNKNOWN_BYTE;
      default: return 0;
    }
    case 0x23: switch (packetType) {
      case 0x0A: UNKNOWN_BYTE;
      case 0x1C: switch (payloadIndex) {
        case 14  : HACONFIG; CAT_MEASUREMENT; HAFREQ;    KEY1_PUB_CONFIG_CHECK_ENTITY("Freq");                         VALUE_u8; // Inverter_Operation_Frequency
        case  6  : HACONFIG; CAT_TEMP;        HATEMP0;   KEY1_PUB_CONFIG_CHECK_ENTITY("Ta");                           VALUE_s8; // Ambient_Temperature
        case  8  : HACONFIG; CAT_TEMP;        HATEMP0;   KEY1_PUB_CONFIG_CHECK_ENTITY("Te");                           VALUE_s8; // Evaporator_Gas_Temperature
        case 10  : HACONFIG; CAT_TEMP;        HATEMP0;   KEY1_PUB_CONFIG_CHECK_ENTITY("Td");                           VALUE_s8; // Discharge_Gas_Temperature
        case 12  : HACONFIG; CAT_MEASUREMENT; HAPRESSURE;KEY1_PUB_CONFIG_CHECK_ENTITY("Pd");                           VALUE_u8div10; // Discharge Pressure in MPa (div by 10)
        case 15  : HACONFIG; CAT_MEASUREMENT; HACURRENT; KEY1_PUB_CONFIG_CHECK_ENTITY("Curr");                         VALUE_u8; // Compressor_Current
        case 16  : HACONFIG; CAT_MEASUREMENT; HAPERCENT; KEY1_PUB_CONFIG_CHECK_ENTITY("Evo");                          VALUE_u8; // Output_Expansion_Valve_Open
        default  : UNKNOWN_BYTE;
      }
      default: return 0;
    }
    case 0x29: switch (packetType) {
      case 0x09: UNKNOWN_BYTE;
      default: return 0;
    }
    case 0x49: switch (packetType) {
      case 0x09: UNKNOWN_BYTE;
      case 0x23: switch (payloadIndex) {
        case  8  : HACONFIG; CAT_SETTING;     HATEMP0;   KEY1_PUB_CONFIG_CHECK_ENTITY("Tset");                         VALUE_u8; // Temperature_Target
        default  : UNKNOWN_BYTE;
      }
      case 0x30: switch (payloadIndex) {
        case  7 : HACONFIG; CAT_TEMP;        HATEMP0;    KEY1_PUB_CONFIG_CHECK_ENTITY("Twi");                          VALUE_s8; // Water_Inlet_Temperature
        case  8 : HACONFIG; CAT_TEMP;        HATEMP0;    KEY1_PUB_CONFIG_CHECK_ENTITY("Two");                          VALUE_s8; // Water_Outlet_Temperature
        case 11 : HACONFIG; CAT_TEMP;        HATEMP0;    KEY1_PUB_CONFIG_CHECK_ENTITY("TwoHP");                        VALUE_s8; // Water_Outlet_Heat_Pump_Temperature
        case 16 : HACONFIG; CAT_TEMP;        HATEMP0;    KEY1_PUB_CONFIG_CHECK_ENTITY("TaAv");                         VALUE_s8; // Ambient_Average_Temperature
        case 20 : HACONFIG; CAT_MEASUREMENT; HAPERCENT;  KEY1_PUB_CONFIG_CHECK_ENTITY("Evi");                          VALUE_u8; // Indoor_Expansion_Valve_Open
        case 30 : HACONFIG; CAT_MEASUREMENT; HAPERCENT;  KEY1_PUB_CONFIG_CHECK_ENTITY("HPWP");                         VALUE_u8; // Heat_Pump_Water_Pump_Speed
        default : UNKNOWN_BYTE;
      }
      default: return 0;
    }
    default: break; // do nothing
  }
  // restart switch as pseudotypes 00000B 40000B coincide with H-link 21000B ; Src/Type reversed
  switch (packetType) {
#include "P1P2_Pseudo.h"
    default: UNKNOWN_BYTE; // break; // do nothing
  }

#elif defined MHI_SERIES

  switch (packetSrc) {
    case 0x00          : SRC(0); break; // internal unit
    case 0x01          : SRC(1); break; // internal unit
    case 0x80          : SRC(2); break; // internal unit
    case 0x81          : SRC(3); break; // external unit
    case 0x02 ... 0x07 : SRC(4); break; // RC (02..07 is sequence counter?)
    default            : SRC(9); break; // unknown ?
  }

  // P1/P2: Main/Peripheral; peripheral-address; packet-type
  // MHI:   address;         data
  // pseudo: FF              00                  08..0F
  //  if ((packetSrc >= 0x00) && (packetSrc <= 0x7F)) packetSrc = 0x01;

  HACONFIG;
  switch (packetSrc) {
    case 0x00 ... 0x01 : // internal unit
    case 0x80 ... 0x81 : // external unit
    case 0x02 ... 0x07 : CAT_SETTING; // RC
                         switch (payloadIndex) {
      case  0 :                                                                                                             KEY1_PUB_CONFIG_CHECK_ENTITY("Byte1-status");                              VALUE_u8hex;
      case  1 : switch (bitNr) { // Mode (Swing, Mode, Power)
        case  8 : switch (packetSrc) {
                    case 0x00 ... 0x07 : bcnt = packetSrc; break;
                    case 0x80 ... 0x81 : bcnt = 0x08 + (packetSrc - 0x80); break;
                    default            : /* should not happen */; break;
                  }
                  BITBASIS;
        case  6 :                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Swing");
        case  3 ... 4 : // only call 3 and/or 4 in new method; so need to fall-through here:
        case  2 :                                                                                                           KEYBITS_PUB_CONFIG_PUB_ENTITY(2, 4, "Mode");// 000 = Auto / 001 = Dry / 010 = Cool / 011 = Fan / 100 = Heat
        case  0 :                                                                                                           KEYBIT_PUB_CONFIG_PUB_ENTITY("Power");
        default : UNKNOWN_BIT;

      }
      case  2 : CAT_SETTING;
                switch (bitNr) { // Fan_Speed (Vane, Speed)
        case  8 : switch (packetSrc) {
                    case 0x00 ... 0x07 : bcnt = 0x0A + packetSrc; break;
                    case 0x80 ... 0x81 : bcnt = 0x12 + (packetSrc - 0x80); break;
                    default            : /* should not happen */; break;
                  }
                  BITBASIS;
        case  5 : return 0;
        case  3 : // may  call 3 and/oor 4 in new method; so need to fall-through here:
        case  4 :                                                                                                           KEYBITS_PUB_CONFIG_CHECK_ENTITY(3, 4, "Vane");                             VALUE_S_L(((payloadByte & 0x30) >> 4) + 1, 1); // 00 = 1 .. 11 = 4
        //case  1 ... 2 : return 0;
        case 1 ... 2 : // may call 0, 1, and/or 2 in new method; so need to fall-through here:
        case  0 :                                                                                                           KEYBITS_PUB_CONFIG_CHECK_ENTITY(0, 2, "Fan_Speed");                        VALUE_S_L((payloadByte & 0x07) + 1, 1);
        default : UNKNOWN_BIT;
      }
      case  3 :                                  CAT_SETTING;                  HATEMP1;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("Set_Temp");                                  VALUE_F_L((payloadByte - 0x80) * 0.5, 1); // OK, aligned with RC.
      case  4 :                                  CAT_TEMP;                     HATEMP1;                                     KEY1_PUB_CONFIG_CHECK_ENTITY("RC_Indoor_Temp");                            VALUE_F_L((payloadByte - 0x3D) * 0.25, 1);
      case  5 :                                                                                                             KEY1_PUB_CONFIG_CHECK_ENTITY("Byte6-status");                              VALUE_u8hex;
      case  6 :                                                                                                             KEY1_PUB_CONFIG_CHECK_ENTITY("Byte7-status");                              VALUE_u8hex;
      case  7 :                                                                                                             KEY1_PUB_CONFIG_CHECK_ENTITY("RC_Mode");                                   VALUE_u8hex; // 80 in standard mode, 40 when loading or showing I/U data; C0 when loading or showing O/U data
      case  8 :                                                                                                             KEY1_PUB_CONFIG_CHECK_ENTITY("Byte9-status");                              VALUE_u8hex;
      case  9 :                                                                                                             KEY1_PUB_CONFIG_CHECK_ENTITY("Byte10-defrost");                            VALUE_u8hex;
      case 10 :                                                                                                             KEY1_PUB_CONFIG_CHECK_ENTITY("Byte11-status");                             VALUE_u8hex;
      case 11 :                                                                                                             KEY1_PUB_CONFIG_CHECK_ENTITY("Byte12-status");                             VALUE_u8hex;
      case 12 :                                                                                                             KEY1_PUB_CONFIG_CHECK_ENTITY("Byte13-status");                             VALUE_u8hex;
      case 14 : return 0; // frame CRC
      default : UNKNOWN_BYTE;
    }
    default   : switch (payloadIndex) {
      case 14 : return 0; // frame CRC
      default : UNKNOWN_BYTE;
    }
  }

#else /* *_SERIES */

#error no series defined

#endif /* *_SERIES */

  return 0;
}

void unSeen() {
#ifdef E_SERIES
  UNSEEN_BYTE_00_30_0_CLIMATE_ROOM_HEATING;
  UNSEEN_BYTE_00_30_1_CLIMATE_ROOM_COOLING;
  UNSEEN_BYTE_00_30_2_CLIMATE_LWT_HEATING;
  UNSEEN_BYTE_00_30_3_CLIMATE_LWT_COOLING;
  UNSEEN_BYTE_00_30_4_CLIMATE_LWT_HEATING_ADD;
  UNSEEN_BYTE_00_30_5_CLIMATE_LWT_COOLING_ADD;
  UNSEEN_BYTE_00_10_19_CLIMATE_DHW;
  UNSEEN_BYTE_40_0F_10_HEATING_ONLY;
#endif /* E_SERIES */
}

byte bits2keyvalue(byte packetSrc, byte packetType, byte payloadIndex, byte* payload, byte j) {
  byte b = bytesbits2keyvalue(packetSrc, packetType, payloadIndex, payload, j) ;
  return b;
}

byte bytes2keyvalue(byte packetSrc, byte packetType, byte payloadIndex, byte* payload) {
  return bits2keyvalue(packetSrc, packetType, payloadIndex, payload, 8) ;
}

#endif /* P1P2_ParameterConversion */
