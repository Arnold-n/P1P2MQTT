/* P1P2_Hitachi_model_1_2.h
 *
 * Copyright (c) 2019-2024 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * handles Hitachi packets for model 1 and 2
 *
 * version history
 * 20250505 v0.9.57 extract existing code from P1P2_ParameterConversion.h
 *
 */

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
      case 0xE2:
# if HITACHI_MODEL == 1
        switch (payloadIndex) {
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
          default  : UNKNOWN_BYTE;
        }
# elif HITACHI_MODEL == 2
        switch (payloadIndex) {
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
          default  : UNKNOWN_BYTE;
        }
# else /* HITACHI_MODEL */
        UNKNOWN_BYTE; // unknown hitachiModel
# endif /* HITACHI_MODEL */
      default: UNKNOWN_BYTE; // unknown payload[4]
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
      default   : UNKNOWN_BYTE;
    }
    case 0x29: switch (payload[4])  { // payload[4] can be 0xF1..0xF5, currently do not decode
      default: UNKNOWN_BYTE; // unknown payload[4]
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
      default: UNKNOWN_BYTE;
    }
    case 0x29: switch (payload[4])  { // payload[4] can be 0xE1..0xE5, seen E1 only, currently decode only 0xE2
      case 0xE1: UNKNOWN_BYTE;
      default  : return 0;
    }
    default: UNKNOWN_BYTE; // unknown packet type
  }
  // 0x8A : sender is ?
  case 0x8A: switch (packetType) {
    case 0x29: switch (payload[4])  { // payload[4] can be 0xF1..0xF5, seen F1 only, currently decode only 0xE2
      case 0xF1: UNKNOWN_BYTE;
      default  : return 0;
    }
    default: UNKNOWN_BYTE; // unknown packet type
  }
  // 0x79 : sender is ?
  // new in v0.9.55
  case 0x79: switch (packetType) {
    case 0x2E: UNKNOWN_BYTE;
    default  : return 0;
  }
  // 0xFF : sender is ?
  // new in v0.9.55
  case 0xFF: switch (packetType) {
    case 0x23: UNKNOWN_BYTE;
    default  : return 0;
  }
  // new in v0.9.45 / v0.9.55
  case 0x19: switch (packetType) {
    case 0x09: UNKNOWN_BYTE;
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