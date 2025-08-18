/* P1P2_Hitachi_model_3.h
 *
 * Copyright (c) 2019-2024 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * handles Hitachi packets for model 3
 *
 * For Hitachi ducted Unit. Interface is connected on the H-Link bus of the Indoor Unit <<==>> remote control.
 * IU : RPI 4.0 FSN4E (2010)
 * OU : RAS 4 HVRNE (2010)
 * Remote control : PC-ARFP1E (2020) + PC-ARFG2-E (2024)
 * 
 * with only 1 remote control :
    * 
    * 21 1F 0A 0201 FFFFFFFF 16 <- broadcast from remote control, no one responds
    * 
    * 21 00 1C 0201 01010101 01 A004191083822210010004200102200600 80 <- remote control 1 who advertises IU 1
    * 89 06 <- UI ack
    * 89 00 2D 0101 01010101 01 200419000006010082221001000420010220000000000000131E0200000000000400 AB <- IU 1 who advertises remote control 1
    * 21 06 <- remote control ack
 * 
 * 
 * with 2 remote controls :
    * 
    * 21 1F 0A 0201 FFFFFFFF 16 <- RC 1 broadcast
    * 41 1F 0A 0201 01020102 16 <- RC 1 asks for RC 2
    * 41 06 <- RC 2 ack
    * 
    * 21 1F 0A 0202 FFFFFFFF 15 <- RC 2 broadcast
    * 41 1F 0A 0202 01010101 15 <- RC 2 asks for RC 1
    * 41 06 <- RC 1 ack
    * 
    * Communication with IU is the same as with 1 remote control, but the both RC does. Ex with second RC 2 (who asks parameters from IU in addition) :
    * 21 00 1C 0202 01010101 01 A0041A107F8222100100042001022006F0 8C <- RC 2 who advertises IU 1
    * 89 06 <- IU ack
    * 21 00 0B 0202 01010101 05 0E <- RC 2 who asks IU for parameters
    * 89 06 <- IU ack
    * 89 00 2D 0101 01020102 01 20041A000006010082221001000420010220000000000000131E0200000000000400 A8 <- IU 1 who advertises RC "2
    * 21 06 <- RC 2 ack
    * 89 00 2D 0101 01020102 06 171615C21916178019808000000200FFFF0009630000200000EC0000000000000000 D8 <- IU 1 who advertises RC 2 
    * 21 06 <- RC 2 ack
    * 
    * When a RC has a user input change, it first sends the update to the other RC, then both RCs send the update to the IU :
    * 41 00 1E 0201 01020102 01 A0041A1C8300015100000000000000000000FF 92 <- RC 1 who advertises RC 2
    * 41 06 <- RC 2 ack
 * 
 * 
 * Trames format :
    * 1st byte : 21 if RC speaks to IU, 41 if RC speaks to another RC, 89 if IU speaks to RC
    * 2nd byte : 00 for data packet, 1F for discovery packet, and 06 for ack packet
    * 3rd byte : length of the packet (including 1st and 2nd byte)
    * 4th and 5th byte : seems to be the emitter ID, 0201 for first RC, 0202 for second RC, 0101 for IU
    * 6th to 9th byte : seems to be the destination ID, 01010101 for IU, 01010101 for first RC, 01020102 for second RC
    * 10th byte (for data packets only) : packet type (IU has 2 types 0x01 and 0x06, payloads are different and RC sends data with 0x01 and asks with 0x05)
    * last byte (except for ack packets) : checksum (XOR of all bytes except the first one)
 * 
 * 
 * Payload of 21 00 1C xxxx xxxxxxxx 01 and 89 00 2D xxxx xxxxxxxx 01 :
    * 1st byte : | ? | heat | ventil | dehum | clim | ? | defrost ? (for 89002D only) | power |
    * 2nd byte : | ? | OUnitOn (for 89002D only) | OUnitStby (for 89002D only) | ? | fan low | fan medium | fan high | ? |
    * 3th byte : temperature setpoint (unsigned 8 bits)
    * 5th byte (only for 21 00 1C ...) : temperature from remote control (unsigned_value / 3 - 20 = Temp (°C))
    * 6th byte (only for 89 00 2D ...) : unknown (changes between 0x00 and 0x18)
    * 6th byte (9th on 89 00 2D) : | B3 | ? | B2 | ? | D1 | ? | B1 | ? |
    * 7th byte (10th on 89 00 2D) : | ? | ? | D3 | B5 | B4=4 | B4=3 | B4=2 | B4=1 |
    * 8th byte (11th on 89 00 2D) : | B9 | C4 | B6 | C8=1 ou 2 | B7 | B8 | C6 | ? |
    * 9th byte (12th on 89 00 2D) : | seems to be set momentarily when parameters are changed | E5 | E4=2 | E4=1 | E3 | E2 | E1=1 ou 2 | E1=2 (sometimes if E1=0) |
    * 10th byte (13th on 89 00 2D) : | ? | C7 | C8=2 | C5=2 | C5=1 | Cb | ? | ? |
 * 
 * 
 * Payload of 89 00 2D xxxx xxxxxxxx 06 (need 21000B020101010101050D to be send before IU responds with it - can be done by the main RC if option P4 is set to 1) :
    * 1st byte : IUAirInletTemperature (signed 8 bits)
    * 2nd byte : IUAirOutletTemperature (signed 8 bits)
    * 3rd byte : IULiquidPipeTemperature (signed 8 bits)
    * 4th byte : IURemoteSensorAirTemperature (signed 8 bits)
    * 5th byte : OutdoorAirTemperature (signed 8 bits)
    * 6th byte : IUGasPipeTemperature (signed 8 bits)
    * 7th byte : OUHeatExchangerTemperature1 (signed 8 bits)
    * 8th byte : OUHeatExchangerTemperature2 (signed 8 bits)
    * 9th byte : CompressorTemperature (signed 8 bits)
    * 12th byte : CompressorControl
    * 13th byte : CompressorFrequency
    * 14th byte : IUExpansionValve
    * 15th byte : OUExpansionValve
    * 18th byte : CompressorCurrent
    * 19th byte : NumberAbnormality (unsigned 8 bits)
    * 20th byte : NumberPowerFailure (unsigned 8 bits)
    * 21st byte : NumberAbnormalTransmission (unsigned 8 bits) TBC ?
    * 22nd byte : NumberInverterTripping (unsigned 8 bits) TBC ?
    * 27th byte : | unknown | ? | ? | CompressorOn | ? | ? | IUnitOn | ? |
    * 28th byte : | ? | ? | ? | ? | ? | ? | unknown | unknown |
 * 
 * 
 * Payload of 41 00 1E xxxx xxxxxxxx 01 :
    * first 3 bytes : seems to be the same as 21 00 1C
    * 5th byte : same as 21 00 1C
    * 8th byte : | ? | ? | ? | ? | ? | eco mode bit 2 | eco mode bit 1 | ? | (eco mode : 0 = off, 1 = low, 2 = mid, 3 = high, only for read, cannot be set by an another RC on bus)
    * 9th to 18th byte : 10 bytes of 0x00
        * 16th byte to 0x01 seems to force the destinated remote control to send a 21 00 1C immedialtely to IU (even if any parameter has changed)
    * one more byte than 21 00 1C : 0xFF
 * 
 * 
 * Connection of a new remote control :
    * new RC sends a normal 41 00 1E packet but with FF FF FF FF as destination ID
    * main RC responds with 41 F1 0C 02 01 01 01 01 01 01  02   FD <- seems 0x02 as payload is the ID for the new RC
    * follow by an ack 41 06 from new RC then :
    * 41 E2 0A 02 02 01 01 01 01 E8
    * 41 06
    * 41 F2 0A 02 01 01 02 01 02 FB
    * 41 06
 * 
 * 
 * version history
 * 20250505 v0.9.57 creation
 *
 */

#define OTHER_DATA(key, start, len) { \
  HACONFIG; CAT_SETTING; if (bitNr == 8) { CHECK(1); } else { CHECKBIT; } KEY(key); \
  if (pubHa) { \
    if (payloadIndex) { if (bitNr == 8) { VALUE_byte_nopub; } else { VALUE_bits_nopub; } } \
    else { HADEVICE_SENSOR; PUB_CONFIG; } \
  } \
  CHECK_ENTITY; \
  char* p = mqtt_value_text_buffer; \
  p += snprintf(p, MQTT_VALUE_LEN, "%s", start); \
  for (int i = 0; i < len - 4; i++) { \
    p += snprintf(p, MQTT_VALUE_LEN - (p - mqtt_value_text_buffer), " %02X", payload[i]); \
    if (i == payloadIndex && !(pubHa)) p += snprintf(p, MQTT_VALUE_LEN - (p - mqtt_value_text_buffer), "(%i)", bitNr); \
  } \
  if (bitNr == 8) { VALUE_byte_textStringOnce(mqtt_value_text_buffer); } \
  else { VALUE_bits_textStringOnce(mqtt_value_text_buffer); } \
}

byte src = 9; // default source is unknown
switch (packetSrc) {
  case 0x00 : src = 0; break; // pseudopacket, ATmega
  case 0x40 : src = 1; break; // pseudopacket, ESP
  case 0x21 : src = 2; SUBDEVICE("_RC"); break; // a remote control to IU
  case 0x89 : src = 8; SUBDEVICE("_UI"); break; // IU to a remote control
  case 0x41 :
    if (payload[0x01] == 0x01) {
      src = 2; // main remote control to p1p2 monitor
      SUBDEVICE("_RC");
    } break;
}
SRC(src); // set char in mqtt_key prefix

switch (packetSrc) {
  case 0x21:
    if (packetType == 0x1C) {
      if (readyToWrite == 1) { readyToWrite = 2; } // ready to write again
      else if (readyToWrite == 0) { readyToWrite = 1; } // no echo received but if two 21001C are received, the writing probably not happen
      switch (payloadIndex) {
        case 0x07:
          switch (bitNr) {
            case 8: bcnt = 1; BITBASIS;
            case 0:
            case 3 ... 6:
              HACONFIG;
              CAT_SETTING;
              CHECKBIT;
              QOS_CLIMATE;
              if (!(bitNr) && pubHa) {
                KEY("RemoteControl");
                HADEVICE_CLIMATE;
                HADEVICE_CLIMATE_TEMPERATURE_CURRENT("T/2/TemperatureRC");
                HADEVICE_CLIMATE_TEMPERATURE("S/2/TemperatureSetpoint", 17, 30, 1);
                HADEVICE_CLIMATE_TEMPERATURE_COMMAND("{{'Z01%02X'|format(value|int)}}");
                HADEVICE_CLIMATE_MODES("S/2/Mode", "\"off\",\"cool\",\"dry\",\"fan_only\",\"heat\"", "'0':'off','3':'cool','4':'dry','5':'fan_only','6':'heat'");
                HADEVICE_CLIMATE_MODE_COMMAND_TEMPLATE("{% set modes={'off':0,'cool':3,'dry':4,'fan_only':5,'heat':6} %} {{'Z02%02X'|format(modes[value]|int)}}");
                HADEVICE_CLIMATE_MODE_POWER_COMMAND_TEMPLATE("{{'Z03%02X'|format(1 if value == 'ON' else 0)}}");
                HADEVICE_CLIMATE_FAN_MODES("S/2/FanForce", "\"low\",\"medium\",\"high\"", "'1':'high','2':'medium','3':'low'");
                HADEVICE_CLIMATE_FAN_MODE_COMMAND_TEMPLATE("{% set modes={'high':1,'medium':2,'low':3} %} {{'Z04%02X'|format(modes[value]|int)}}");
                PUB_CONFIG;
              }
              KEY("Mode");
              CHECK_ENTITY;
              if (bitNr != 0) {
                if ((!(payload[0x07] & 0x01)) || (!((payload[0x07] >> bitNr) & 0x01))) {
                  VALUE_bits_nopub;
                }
              } else {
                if (payload[0x07] & 0x01) {
                  for (int i = 3; i <= 6; ++i) {
                    if ((payload[0x07] >> i) & 0x01) {
                      VALUE_S_BIT(i);
                    }
                  }
                }
              }
              VALUE_S_BIT(bitNr);
          } break;
        case 0x08:
          switch (bitNr) {
            case 8: bcnt = 2; BITBASIS;
            case 1 ... 3 :    HACONFIG;      CAT_SETTING;                         CHECKBIT;         QOS_CLIMATE;        KEY("FanForce");              CHECK_ENTITY;           if ((payload[0x08] >> bitNr) & 0x01) { VALUE_S_BIT(bitNr); } else { VALUE_bits_nopub; }
          } break;
        case 0x09:            HACONFIG;      CAT_SETTING;        HATEMP0;         CHECK(1);         QOS_CLIMATE;        KEY("TemperatureSetpoint");   CHECK_ENTITY;           VALUE_u8;
        case 0x0B:            HACONFIG;      CAT_TEMP;           HATEMP1;         CHECK(1);         QOS_CLIMATE;        KEY("TemperatureRC");         CHECK_ENTITY;           VALUE_F_L((*(uint8_t *)&payload[0x0B]) / 3.0f - 20.0f, 1); // retro-measured : 0,3387 * unsigned_value - 20,462 = Temp (°C), supposedly Hitachi programmed it as : unsigned_value / 3 - 20 = Temp (°C)
        case 0x0C ... 0x10: return 0; // already handled by 89 00 2D / 01
      }
      OTHER_DATA("OtherData-21001C", "21 00 1C", 0x1C);
    } else if (packetDst == 0x00 && packetType == 0x0B && payload[0x06] == 0x05) { // RC ask for IU parameters
      return 0;
    } else if (packetDst == 0x1F && packetType == 0x0A) { // discovery packet
      return 0;
    } break;
  case 0x89:
    if (packetType == 0x2D) {
      switch (payload[0x06]) {
        case 0x06:
          switch (payloadIndex) {
            case 0x07:       HACONFIG;      CAT_TEMP;           HATEMP0;         KEY1_PUB_CONFIG_CHECK_ENTITY("IUAirInletTemperature");                                      VALUE_s8; // b2
            case 0x08:       HACONFIG;      CAT_TEMP;           HATEMP0;         KEY1_PUB_CONFIG_CHECK_ENTITY("IUAirOutletTemperature");                                     VALUE_s8; // b3
            case 0x09:       HACONFIG;      CAT_TEMP;           HATEMP0;         KEY1_PUB_CONFIG_CHECK_ENTITY("IULiquidPipeTemperature");                                    VALUE_s8; // b4
            case 0x0A:       HACONFIG;      CAT_TEMP;           HATEMP0;         KEY1_PUB_CONFIG_CHECK_ENTITY("IURemoteSensorAirTemperature");                               VALUE_s8; // b5
            case 0x0B:       HACONFIG;      CAT_TEMP;           HATEMP0;         KEY1_PUB_CONFIG_CHECK_ENTITY("OutdoorAirTemperature");                                      VALUE_s8; // b6
            case 0x0C:       HACONFIG;      CAT_TEMP;           HATEMP0;         KEY1_PUB_CONFIG_CHECK_ENTITY("IUGasPipeTemperature");                                       VALUE_s8; // b7
            case 0x0D:       HACONFIG;      CAT_TEMP;           HATEMP0;         KEY1_PUB_CONFIG_CHECK_ENTITY("OUHeatExchangerTemperature1");                                VALUE_s8; // b8
            case 0x0E:       HACONFIG;      CAT_TEMP;           HATEMP0;         KEY1_PUB_CONFIG_CHECK_ENTITY("OUHeatExchangerTemperature2");                                VALUE_s8; // b9
            case 0x0F:       HACONFIG;      CAT_TEMP;           HATEMP0;         KEY1_PUB_CONFIG_CHECK_ENTITY("CompressorTemperature");                                      VALUE_s8; // bA
            case 0x12:       HACONFIG;      CAT_SETTING;        HAFREQ;          KEY1_PUB_CONFIG_CHECK_ENTITY("CompressorControl");                                          VALUE_u8; // h3
            case 0x13:       HACONFIG;      CAT_SETTING;        HAFREQ;          KEY1_PUB_CONFIG_CHECK_ENTITY("CompressorFrequency");                                        VALUE_u8; // h4
            case 0x14:       HACONFIG;      CAT_SETTING;        HAPERCENT;       KEY1_PUB_CONFIG_CHECK_ENTITY("IUExpansionValve");                                           VALUE_u8; // L1
            case 0x15:       HACONFIG;      CAT_SETTING;        HAPERCENT;       KEY1_PUB_CONFIG_CHECK_ENTITY("OUExpansionValve");                                           VALUE_u8; // L2
            case 0x18:       HACONFIG;      CAT_SETTING;        HACURRENT;       KEY1_PUB_CONFIG_CHECK_ENTITY("CompressorCurrent");                                          VALUE_u8; // P1
            case 0x19:       HACONFIG;      CAT_MEASUREMENT;                     KEY1_PUB_CONFIG_CHECK_ENTITY("NumberAbnormality");                                          VALUE_u8; // E1
            case 0x1A:       HACONFIG;      CAT_MEASUREMENT;                     KEY1_PUB_CONFIG_CHECK_ENTITY("NumberPowerFailure");                                         VALUE_u8; // E2
            case 0x1B:       HACONFIG;      CAT_MEASUREMENT;                     KEY1_PUB_CONFIG_CHECK_ENTITY("NumberAbnormalTransmission");                                 VALUE_u8; // E3 TBC ?
            case 0x1C:       HACONFIG;      CAT_MEASUREMENT;                     KEY1_PUB_CONFIG_CHECK_ENTITY("NumberInverterTripping");                                     VALUE_u8; // E4 TBC ?
            case 0x21:
              switch (bitNr) {
                case 8: bcnt = 13; BITBASIS;
                case 1:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("IUnitOn");
                case 4:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("CompressorOn");
                case 7:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Unknown-89002D-06-27th-7"); // UI drain pomp ?
              } break;
            case 0x22:
              switch (bitNr) {
                case 8: bcnt = 14; BITBASIS;
                case 0:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Unknown-89002D-06-28th-1"); // clim = 1, ventil-off = 0
                case 1:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Unknown-89002D-06-28th-2"); // end of cycle for compressor (oil equalization ?)
              } break;
          }
          OTHER_DATA("OtherData-89002D-06", "89 00 2D", 0x2D);
        case 0x01:
          switch (payloadIndex) {
            case 0x07:
              switch (bitNr) {
                case 8: bcnt = 3; BITBASIS;
                case 0:      HACONFIG;      CAT_SETTING;        HAPOWER;         KEYBIT_PUB_CONFIG_PUB_ENTITY("Power");
                case 1:      HACONFIG;      CAT_MEASUREMENT;                     KEYBIT_PUB_CONFIG_PUB_ENTITY("Defrost");
                case 3:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("ModeCool");
                case 4:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("ModeDry");
                case 5:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("ModeFanOnly");
                case 6:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("ModeHeat");
              } break;
            case 0x08:
              switch (bitNr) {
                case 8: bcnt = 4; BITBASIS;
                case 1:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("FanForceHigh");
                case 2:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("FanForceMedium");
                case 3:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("FanForceLow");
                case 5:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("OUnitStby");
                case 6:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("OUnitOn");
              } break;
            case 0x09:       HACONFIG;      CAT_SETTING;        HATEMP0;         KEY1_PUB_CONFIG_CHECK_ENTITY("TemperatureSetpoint");                                        VALUE_u8;
            case 0x0C:       HACONFIG;      CAT_SETTING;                         KEY1_PUB_CONFIG_CHECK_ENTITY("Unknown-89002D-01-6th");                                      VALUE_u8;
            case 0x0F:
              switch (bitNr) {
                case 8: bcnt = 5; BITBASIS;
                case 1:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-B1");
                case 3:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-D1");
                case 5:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-B2");
                case 7:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-B3");
              } break;
            case 0x10:
              switch (bitNr) {
                case 8: bcnt = 6; BITBASIS;
                case 0 ... 3:
                  HACONFIG; CAT_SETTING; HADEVICE_SENSOR; HANONE; PRECISION(0); CHECKBIT; KEY("Option-B4");
                  if (bitNr == 0) PUB_CONFIG;
                  CHECK_ENTITY;
                  if (!(bitNr) && !(payload[0x10] & 0x0F)) {
                    VALUE_S_BIT(0);
                  }
                  if ((payload[0x10] >> bitNr) & 0x01) {
                    VALUE_S_BIT(bitNr + 1);
                  }
                  VALUE_bits_nopub;
                case 4:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-B5");
                case 5:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-D3");
              } break;
            case 0x11:
              switch (bitNr) {
                case 8: bcnt = 7; BITBASIS;
                case 1:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-C6");
                case 2:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-B8");
                case 3:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-B7");
                case 4:
                  HACONFIG; CAT_SETTING; HADEVICE_SENSOR; HANONE; PRECISION(0); CHECKBIT; KEY("Option-C8"); PUB_CONFIG; CHECK_ENTITY;
                  if (!((payload[0x11] >> 4) & 0x01)) VALUE_S_BIT(0);
                  if ((payload[0x13] >> 5) & 0x01) VALUE_S_BIT(2);
                  VALUE_S_BIT(1);
                case 5:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-B6");
                case 6:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-C4");
                case 7:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-B9");
              }  break;
            case 0x12:
              switch (bitNr) {
                case 8: bcnt = 8; BITBASIS;
                case 0 ... 1:
                  HACONFIG; CAT_SETTING; HADEVICE_SENSOR; HANONE; PRECISION(0); CHECKBIT; KEY("Option-E1");
                  if (bitNr == 0) PUB_CONFIG;
                  CHECK_ENTITY;
                  if (!((payload[0x12] >> 1) & 0x01)) VALUE_S_BIT(0);
                  if (payload[0x12] & 0x01) VALUE_S_BIT(2);
                  VALUE_S_BIT(1);
                case 2:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-E2");
                case 3:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-E3");
                case 4 ... 5:
                  HACONFIG; CAT_SETTING; HADEVICE_SENSOR; HANONE; PRECISION(0); CHECKBIT; KEY("Option-E4");
                  if (bitNr == 4) PUB_CONFIG;
                  CHECK_ENTITY;
                  if ((payload[0x12] >> 4) & 0x01) VALUE_S_BIT(1);
                  if ((payload[0x12] >> 5) & 0x01) VALUE_S_BIT(2);
                  VALUE_S_BIT(0);
                case 6:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-E5");
                case 7:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-updated");
              } break;
            case 0x13:
              switch (bitNr) {
                case 8: bcnt = 9; BITBASIS;
                case 2:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-Cb");
                case 3 ... 4:
                  HACONFIG; CAT_SETTING; HADEVICE_SENSOR; HANONE; PRECISION(0); CHECKBIT; KEY("Option-C5");
                  if (bitNr == 3) PUB_CONFIG;
                  CHECK_ENTITY;
                  if ((payload[0x13] >> 3) & 0x01) VALUE_S_BIT(1);
                  if ((payload[0x13] >> 4) & 0x01) VALUE_S_BIT(2);
                  VALUE_S_BIT(0);
                case 5:
                  HACONFIG; CAT_SETTING; HADEVICE_SENSOR; HANONE; PRECISION(0); CHECKBIT; KEY("Option-C8"); CHECK_ENTITY;
                  if (!((payload[0x11] >> 4) & 0x01)) VALUE_S_BIT(0);
                  if ((payload[0x13] >> 5) & 0x01) VALUE_S_BIT(2);
                  VALUE_S_BIT(1);
                case 6:      HACONFIG;      CAT_SETTING;                         KEYBIT_PUB_CONFIG_PUB_ENTITY("Option-C7");
              }
          }
          OTHER_DATA("OtherData-89002D-01", "89 00 2D", 0x2D);
      }
    } break;
  case 0x41:
    if (packetType == 0x1E && payload[0x01] == 0x01) { // emmiter is the physical remote control
      switch (payloadIndex) {
        case 0x07:
          switch (bitNr) {
            case 8: bcnt = 10; BITBASIS;
            case 0:
            case 3 ... 6: return 0;
          } break;
        case 0x08:
          switch (bitNr) {
            case 8: bcnt = 11; BITBASIS;
            case 1 ... 3:  return 0;
          } break;
        case 0x09:
        case 0x0B: return 0;
        case 0x0E:
          switch (bitNr) {
            case 8: bcnt = 12; BITBASIS;
            case 1 ... 2:  HACONFIG;      CAT_SETTING;                         CHECKBIT;         QOS_CLIMATE;        KEY("EcoMode");              CHECK_ENTITY;
              HACONFIG;
              CAT_SETTING;
              CHECKBIT;
              KEY("EcoMode");
              if ((bitNr == 1) && pubHa) {
                HADEVICE_ENUM;
                HAECOMODE;
                HADEVICE_SELECT_OPTIONS("\"off\",\"low\",\"medium\",\"high\"", "'0':'off','1':'low','2':'medium','3':'high'");
                PUB_CONFIG;
              }
              CHECK_ENTITY;
              VALUE_S_BIT((payload[0x0E] >> 1) & 0x03);
          }
      }
      OTHER_DATA("OtherData-41001E-01", "41 00 1E", 0x1E);
    } else if (packetType == 0x1E && payload[0x01] == 0x02) { // emmiter is p1p2 monitor
      if (readyToWrite == 0) { readyToWrite = 1; } // echo received
      return 0;
    } else if (packetDst == 0x1F && packetType == 0x0A) { // discovery packet
      return 0;
    }
}

if (!(payloadIndex) && !((packetSrc == 0x00 || packetSrc == 0x40) && packetDst == 0x00)) { // if not a pseudo packet and only one time per packet
  src = 9; // reset source to unknown
  SRC(src);
  SUBDEVICE("_Bridge")

  HACONFIG; CAT_SETTING; KEY("OtherPacket");
  if (pubHa) { HADEVICE_SENSOR; PUB_CONFIG; }
  char* p = mqtt_value_text_buffer;
  p += snprintf(p, MQTT_VALUE_LEN, "%02X", packetSrc);
  p += snprintf(p, MQTT_VALUE_LEN - (p - mqtt_value_text_buffer), " %02X", packetDst);
  p += snprintf(p, MQTT_VALUE_LEN - (p - mqtt_value_text_buffer), " %02X", packetType);
  for (int i = 0; i < packetType - 4; i++) {
    p += snprintf(p, MQTT_VALUE_LEN - (p - mqtt_value_text_buffer), " %02X", payload[i]);
  }
  VALUE_byte_textStringOnce(mqtt_value_text_buffer);
}