/* P1P2_Pseudo.h
 *
 * Copyright (c) 2019-2024 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * handles only 0E/0F pseudo-packets
 *
 * version history
 * 20240605 v0.9.53 V_Interface entity is voltage
 * 20240519 v0.9.49 hysteresis-check for V_Interface
 * 20240515 v0.9.46 separated common pseudo code handling into P1P2_Pseudo.h
 *
 */

    case 0x0E : CAT_PSEUDO;
                SUBDEVICE("_Bridge");
                haConfig = 0;
                switch (packetSrc) {
      case 0x00 : HAENTITYCATEGORY_DIAGNOSTIC;
                  switch (payloadIndex) {
        case    0 : KEY1_PUB_CONFIG_CHECK_ENTITY("ATmega_Major_Version");                                                                                                 VALUE_u8;
        case    1 : KEY1_PUB_CONFIG_CHECK_ENTITY("ATmega_Minor_Version");                                                                                                 VALUE_u8;
        case    2 : KEY1_PUB_CONFIG_CHECK_ENTITY("ATmega_Patch_Version");                                                                                                 VALUE_u8;
        case    3 : KEY1_PUB_CONFIG_CHECK_ENTITY("P1P2_ESP_Interface_hwID_ATmega");                                                                                       VALUE_u8hex;
        case    4 : KEY1_PUB_CONFIG_CHECK_ENTITY("Brand");                                                                                                                VALUE_u8;
#ifdef F_SERIES
        case    5 : HACONFIG;
                    CHECK(1);
                    KEY("Model");
                    QOS_SELECT;
                    if (pubHa) {
                      SUBDEVICE("_Bridge");
                      HADEVICE_SELECT;
                      HADEVICE_SELECT_OPTIONS("\"F-series 1 generic\",\"F-series 10 (B C L)\",\"F-series 11 (LA P PA A)\",\"F-series 12 (M)\"", "'1':'F-series 1 generic','10':'F-series 10 (B C L)','11':'F-series 11 (LA P PA A)','12':'F-series 12 (M)'");
                      HADEVICE_SELECT_COMMAND_TEMPLATE("{% set modes={'F-series 1 generic':1,'F-series 10 (B C L)':10,'F-series 11 (LA P PA A)':11,'F-series 12 (M)':12}%}{{'M%i'|format(((modes[value])|int) if value in modes.keys() else 1)}}")
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
#else
        case    5 : KEY1_PUB_CONFIG_CHECK_ENTITY("Model");                                                                                                                VALUE_u8;
#endif
        case    6 : KEY1_PUB_CONFIG_CHECK_ENTITY("Scope_Mode");                                                                                                           VALUE_u8;
        case    7 : KEY1_PUB_CONFIG_CHECK_ENTITY("Compile_Options_ATmega");                                                                                               VALUE_u8hex;
        case    8 : //return 0;
                  return 0;
        case    9 : KEY2_PUB_CONFIG_CHECK_ENTITY("Delay_Packet_Write");                                                                                                   VALUE_u16_LE;
        case   10 : return 0;
        case   11 : KEY2_PUB_CONFIG_CHECK_ENTITY("Delay_Packet_Write_Timeout");                                                                                           VALUE_u16_LE;
        case   12 : KEY1_PUB_CONFIG_CHECK_ENTITY("Reboot_MCUSR");                                                                                                         VALUE_u8hex;
#ifdef F_SERIES
        case   13 : HACONFIG; KEY1_PUB_CONFIG_CHECK_ENTITY("Model_Suggestion");                                                                                           VALUE_u8;
#endif /* F_SERIES */
        default   : return 0;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : HAENTITYCATEGORY_DIAGNOSTIC;
                    KEY1_PUB_CONFIG_CHECK_ENTITY("ESP_Major_Version");                                                                                                    VALUE_u8;
        case    1 : HAENTITYCATEGORY_DIAGNOSTIC;
                    KEY1_PUB_CONFIG_CHECK_ENTITY("ESP_Minor_Version");                                                                                                    VALUE_u8;
        case    2 : HAENTITYCATEGORY_DIAGNOSTIC;
                    KEY1_PUB_CONFIG_CHECK_ENTITY("ESP_Patch_Version");                                                                                                    VALUE_u8;
        case    3 : HAENTITYCATEGORY_DIAGNOSTIC;
                    KEY1_PUB_CONFIG_CHECK_ENTITY("ESP_Reboot_Count");                                                                                                     VALUE_u8;
        case    4 : HAENTITYCATEGORY_DIAGNOSTIC;
                    KEY1_PUB_CONFIG_CHECK_ENTITY("ESP_Reboot_Reason");                                                                                                    VALUE_u8hex;
        case    8 : HAENTITYCATEGORY_DIAGNOSTIC;
                    KEY4_PUB_CONFIG_CHECK_ENTITY("ESP_Output_Mode");                                                                                                      VALUE_u32hex_LE;
        case    9 : HAENTITYCATEGORY_DIAGNOSTIC;
                    KEY1_PUB_CONFIG_CHECK_ENTITY("Output_Filter");                                                                                                        VALUE_u8;
        case   10 : HAENTITYCATEGORY_DIAGNOSTIC;
                    KEY1_PUB_CONFIG_CHECK_ENTITY("P1P2_ESP_Interface_hwID_ESP");                                                                                          VALUE_u8hex;
        case   11 : // implied in createButtonSwitches() HACONFIG;
                    CHECK(1); // PUB_CONFIG
                    if (pubHa) {
                      createButtonsSwitches();
                      registerSeenByte(); // do not publish entity, as only config is needed
                    }
#ifdef E_SERIES
        case   12 : return 0;
        case   13 : HACONFIG;
                    HATEMP1;
                    KEY("RToffset_Room");
                    CHECK(2);
                    QOS_NUMBER;
                    if (pubHa) {
                      HADEVICE_NUMBER;
                      HADEVICE_NUMBER_RANGE(-3.00, 3.00, 0.1);
                      HADEVICE_NUMBER_MODE("box");
                      HADEVICE_NUMBER_COMMAND_TEMPLATE("{{'P" xstr(PARAM_RT_OFFSET) " %f'|format(value|float)}}");
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_s16div100_LE;
        case   14 : HACONFIG;
                    HATEMP2;
                    KEY("R1Toffset_Mid");
                    CHECK(1);
                    QOS_NUMBER;
                    if (pubHa) {
                      HADEVICE_NUMBER;
                      HADEVICE_NUMBER_RANGE(-1.00, 1.00, 0.01);
                      HADEVICE_NUMBER_MODE("box");
                      HADEVICE_NUMBER_COMMAND_TEMPLATE("{{'P" xstr(PARAM_R1T_OFFSET) " %f'|format(value|float)}}");
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_s8div100;
        case   15 : HACONFIG;
                    HATEMP2;
                    KEY("R2Toffset_LWT");
                    CHECK(1);
                    QOS_NUMBER;
                    if (pubHa) {
                      HADEVICE_NUMBER;
                      HADEVICE_NUMBER_RANGE(-1.00, 1.00, 0.01);
                      HADEVICE_NUMBER_MODE("box");
                      HADEVICE_NUMBER_COMMAND_TEMPLATE("{{'P" xstr(PARAM_R2T_OFFSET) " %f'|format(value|float)}}");
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_s8div100;
        case   16 : HACONFIG;
                    HATEMP2;
                    KEY("R4Toffset_RWT");
                    CHECK(1);
                    QOS_NUMBER;
                    if (pubHa) {
                      HADEVICE_NUMBER;
                      HADEVICE_NUMBER_RANGE(-1.00, 1.00, 0.01);
                      HADEVICE_NUMBER_MODE("box");
                      HADEVICE_NUMBER_COMMAND_TEMPLATE("{{'P" xstr(PARAM_R4T_OFFSET) " %f'|format(value|float)}}");
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_s8div100;
#endif /* E_SERIES */
        default   : return 0;
      }
      default   : return 0;
    }
    case 0x0F :                                                            CAT_PSEUDO;
                SUBDEVICE("_Bridge");
                haConfig = 0;
                switch (packetSrc) {
      case 0x00 : maxOutputFilter = 1;
                  switch (payloadIndex) {
        case    0 : HAENTITYCATEGORY_DIAGNOSTIC;
                    KEY1_PUB_CONFIG_CHECK_ENTITY("Error_Read_Count");                               HACONFIG;                                                             VALUE_u8;
        case    1 : HAENTITYCATEGORY_DIAGNOSTIC;
                    KEY1_PUB_CONFIG_CHECK_ENTITY("Error_Read_Type");                                                                                                      VALUE_u8;
#if defined MHI_SERIES || defined TH_SERIES
        case    2 : KEY1_PUB_CONFIG_CHECK_ENTITY("Error_Mask");                                                                                                           VALUE_u8;
        case    3 : KEY1_PUB_CONFIG_CHECK_ENTITY("Intra_Byte_Pause_Max");                                                                                                 VALUE_u8;
#endif /* MHI_SERIES || TH_SERIES */
#ifdef EF_SERIES
        case    2 : HACONFIG;
                    CAT_SETTING;
                    CHECK(1);
                    KEY("Write_Budget");
                    QOS_NUMBER;
                    if (pubHa) {
                      HADEVICE_NUMBER;
                      HADEVICE_NUMBER_RANGE(0, 255, 1);
                      HADEVICE_NUMBER_MODE("box");
                      HADEVICE_NUMBER_COMMAND_TEMPLATE("{{'N%i'|format(value|int)}}");
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case    3 : HACONFIG;
                    CAT_SETTING;
                    CHECK(1);
                    KEY("Write_Budget_Period");
                    QOS_NUMBER;
                    if (pubHa) {
                      HADEVICE_NUMBER;
                      HADEVICE_NUMBER_RANGE(15, 255, 1);
                      HADEVICE_NUMBER_MODE("box");
                      HADEVICE_NUMBER_COMMAND_TEMPLATE("{{'I%i'|format(value|int)}}");
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case    4 : HACONFIG; KEY1_PUB_CONFIG_CHECK_ENTITY("Writes_Refused_Busy");                                                                                        VALUE_u8;
        case    5 : HACONFIG; KEY1_PUB_CONFIG_CHECK_ENTITY("Writes_Refused_Budget");                                                                                      VALUE_u8;

        case    6 : HACONFIG; KEY1_PUB_CONFIG_CHECK_ENTITY("Error_Budget");                                                                                               VALUE_u8;
        case    7 : return 0;
        case    8 :           KEY2_PUB_CONFIG_CHECK_ENTITY("P1P2_Monitor_Counter_Parameter_Writes");                                                                      VALUE_u16_LE;
        case    9 :           KEY1_PUB_CONFIG_CHECK_ENTITY("Control_Level_If_Active");                                                                                    VALUE_u8;
        case   10 :           KEY1_PUB_CONFIG_CHECK_ENTITY("Control_ID");                                                                                                 VALUE_u8hex;
        case   11 : HACONFIG;
                    maxOutputFilter = 9; // overrule = 1
                    CHECK(1);
                    KEY("Control_Function");
                    QOS_AVAILABILITY;
                    if (pubHa) {
                      HADEVICE_SWITCH;
                      HADEVICE_SWITCH_PAYLOADS("L0", "L1");
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;                                                                                                                                         VALUE_u8;
#endif /* EF_SERIES */
#ifdef E_SERIES
        case   12 : HACONFIG;
                    maxOutputFilter = 9; // overrule = 1
                    CHECK(1);
                    KEY("Counter_Request_Function");
                    QOS_AVAILABILITY;
                    if (pubHa) {
                      HADEVICE_SWITCH;
                      HADEVICE_SWITCH_PAYLOADS("C0", "C2");
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_u8;
        case   13 : KEY1_PUB_CONFIG_CHECK_ENTITY("Auxiliary_Slots_Cnt");                                                                                                  VALUE_u8;
#endif /* E_SERIES */
        case   14 : return 0;
        case   15 : return 0;
        case   16 : return 0;
        case   17 : KEY4_PUB_CONFIG_CHECK_ENTITY("ATmega_Uptime");                                             HASECONDS;         maxOutputFilter = 2;                    VALUE_u32_LE;
        case   18 : HACONFIG; HAVOLTAGE; KEY1_PUB_CONFIG_CHECK_ENTITY("V_Interface");      HYST_U8(1);                            maxOutputFilter = 2;                    VALUE_u8div10;
        case   19 : KEY1_PUB_CONFIG_CHECK_ENTITY("V_3V3_ATmega");                                                                 maxOutputFilter = 2;                    VALUE_u8div10;
        default   : return 0;
      }
      case 0x40 : switch (payloadIndex) {
        case    3 : KEY4_PUB_CONFIG_CHECK_ENTITY("ESP_Uptime");                                                HASECONDS;         maxOutputFilter = 2;                    VALUE_u32_LE;
        case    5 : KEY2_PUB_CONFIG_CHECK_ENTITY("MQTT_Disconnected_Time_Total");                              HASECONDS;                                                 VALUE_u16_LE;
        case    7 : KEY2_PUB_CONFIG_CHECK_ENTITY("MQTT_Messages_Skipped_Not_Connected");                       HAEVENTS;                                                  VALUE_u16_LE;
        case    8 : KEY1_PUB_CONFIG_CHECK_ENTITY("ESP_Ethernet_Connected");                                                                                               VALUE_u8hex;
        case    9 : KEY1_PUB_CONFIG_CHECK_ENTITY("ESP_Telnet_Connected");                                                                                                 VALUE_u8;
        case   10 : switch (bitNr) {
          case  8 : bcnt = 0; BITBASIS;
          case  0 : HACONFIG; CAT_SETTING; KEYBIT_PUB_CONFIG_PUB_ENTITY("ESP_EEPROM_Saved");
          case  1 : HACONFIG; CAT_SETTING; KEYBIT_PUB_CONFIG_PUB_ENTITY("ESP_Factory_Reset_Scheduled");
          case  5 : HACONFIG; CAT_SETTING; QOS_AVAILABILITY; KEYBIT_PUB_CONFIG_PUB_ENTITY("ESP_Throttling");
#ifdef E_SERIES
          case  2 : HACONFIG; CAT_SETTING; KEYBIT_PUB_CONFIG_PUB_ENTITY("ESP_Waiting_For_Counters_D13");
          case  3 : HACONFIG;
                    // CHECK(1) already done by BITBASIS
                    CHECKBIT;
                    SUBDEVICE("_Setup");
                    KEY("HA_Setup"); // was Climate_LWT
                    QOS_SWITCH;
                    if (pubHaBit) {
                      HADEVICE_SWITCH;
                      HADEVICE_SWITCH_PAYLOADS("P" xstr(PARAM_HA_SETUP) " 0", "P" xstr(PARAM_HA_SETUP) " 1");
                      PUB_CONFIG;
                    }
                    CHECK_ENTITY;
                    VALUE_flag8;
          case  4 : SUBDEVICE("_Mode"); HACONFIG; CAT_SETTING; QOS_AVAILABILITY; KEYBIT_PUB_CONFIG_PUB_ENTITY("Heating_Only");
#endif /* E_SERIES */
          default : return 0;
        }
        case   13 : KEY2_PUB_CONFIG_CHECK_ENTITY("ESP_Mem_Free");                                                                                                         VALUE_u16_LE;
        case   14 : KEY1_PUB_CONFIG_CHECK_ENTITY("ESP_Serial_Input_Errors_Data_Short");;                                                                                  VALUE_u8;
        case   15 : KEY1_PUB_CONFIG_CHECK_ENTITY("ESP_Serial_Input_Errors_CRC");                                                                                          VALUE_u8;
        case   16 : HACONFIG; HYST_S8(3); KEY1_PUB_CONFIG_CHECK_ENTITY("WiFi_RSSI");                                                                                      VALUE_s8;
        case   17 : KEY1_PUB_CONFIG_CHECK_ENTITY("WiFi_Status");                                                                                                          VALUE_u8;
        case   19 : KEY2_PUB_CONFIG_CHECK_ENTITY("MQTT_Messages_Skipped_Low_Mem");                             HAEVENTS;                                                  VALUE_u16_LE;
        default   : return 0;
      }
      default   : return 0;
    }
