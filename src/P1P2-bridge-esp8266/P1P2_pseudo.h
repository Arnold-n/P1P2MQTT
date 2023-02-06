#ifdef PSEUDO_PACKETS
// ATmega/ESP pseudopackets
    case 0x08 :
    case 0x0C : return 0;
    case 0x09 :                                                            CAT_PSEUDO2;
    case 0x0D :                                                            CAT_PSEUDO;
                switch (packetSrc) {
//#define ADC_AVG_SHIFT 4     // P1P2Serial sums 16 = 2^ADC_AVG_SHIFT samples before doing min/max check
//#define ADC_CNT_SHIFT 4     // P1P2Serial sums 16384 = 2^(16-ADC_CNT_SHIFT) samples to V0avg/V1avg
      case 0x00 : maxOutputFilter = 1;
                  switch (payloadIndex) {
        case   16 : KEY("ATmega_controlLevel");                                                          maxOutputFilter = 9;                    VALUE_u8;
        case   17 : KEY("ATmega_controlLevel_bin");                                                      maxOutputFilter = 9;                    VALUE_u8;
        case    1 : if (hwID) { KEY("V_bus_ATmega_ADC_min"); VALUE_F_L(FN_u16_LE(&payload[payloadIndex]) * (20.9  / 1023 / (1 << ADC_AVG_SHIFT)), 2);   }; // based on 180k/10k resistor divider, 1.1V range
        case    3 : if (hwID) { KEY("V_bus_ATmega_ADC_max"); VALUE_F_L(FN_u16_LE(&payload[payloadIndex]) * (20.9  / 1023 / (1 << ADC_AVG_SHIFT)), 2);   };
        case    7 : if (hwID) { KEY("V_bus_ATmega_ADC_avg"); VALUE_F_L(FN_u32_LE(&payload[payloadIndex]) * (20.9  / 1023 / (1 << (16 - ADC_CNT_SHIFT))), 4); };
        case    9 : if (hwID) { KEY("V_3V3_ATmega_ADC_min"); VALUE_F_L(FN_u16_LE(&payload[payloadIndex]) * (3.773 / 1023 / (1 << ADC_AVG_SHIFT)), 2);   }; // based on 34k3/10k resistor divider, 1.1V range
        case   11 : if (hwID) { KEY("V_3V3_ATmega_ADC_max"); VALUE_F_L(FN_u16_LE(&payload[payloadIndex]) * (3.773 / 1023 / (1 << ADC_AVG_SHIFT)), 2);   };
        case   15 : if (hwID) { KEY("V_3V3_ATmega_ADC_avg"); VALUE_F_L(FN_u32_LE(&payload[payloadIndex]) * (3.773 / 1023 / (1 << (16 - ADC_CNT_SHIFT))), 4); };
        default   : return 0;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : KEY("ESP_Compile_Options");                                                                                                  VALUE_u8hex;
        case    2 : KEY("MQTT_messages_skipped_not_connected");            HACONFIG; HAEVENTS;                                                   VALUE_u16_LE;
        case    6 : KEY("MQTT_messages_published");                    /*  HACONFIG; HAEVENTS;    */                                             VALUE_u32_LE;
        case    8 : KEY("MQTT_disconnected_time");                         HACONFIG; HASECONDS;                                                  VALUE_u16_LE;
        case    9 : KEY("WiFi_RSSI");                                      HACONFIG;                                                             VALUE_s8_ratelimited;
        case   10 : KEY("WiFi_status");                                    HACONFIG;                                                             VALUE_u8;
        case   11 : KEY("ESP_reboot_reason");                              HACONFIG;                                                             VALUE_u8hex;
        case   15 : KEY("ESP_Output_Mode");                                HACONFIG;                                                             VALUE_u32hex_LE;
        default   : return 0;
      }
      default   : return 0;
    }
    case 0x0A :                                                            CAT_PSEUDO2;
    case 0x0E :                                                            CAT_PSEUDO;
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    3 : KEY("ATmega_Uptime");                                  HACONFIG; HASECONDS;          maxOutputFilter = 2;                    VALUE_u32_LE_uptime;
        case    7 : KEY("CPU_Frequency");                                                                                                        VALUE_u32_LE;
        case    8 : KEY("Writes_Refused");                                 HACONFIG;                                                             VALUE_u8;
        case    9 : KEY("Counter_Request_Repeat");                         HACONFIG;                                                             VALUE_u8;
        case   10 : KEY("Counter_Request_Counter");                                                      maxOutputFilter = 2;                    VALUE_u8;
        case   11 : KEY("Error_Oversized_Packet_Count");                   HACONFIG;                                                             VALUE_u8;
        case   12 : KEY("Parameter_Write_Request");                                                                                              VALUE_u8;
        case   13 : KEY("Parameter_Write_Packet_Type");                                                                                          VALUE_u8hex;
        case   15 : KEY("Parameter_Write_Nr");                                                                                                   VALUE_u16hex_LE;
        case   19 : KEY("Parameter_Write_Value");                                                                                                VALUE_u32_LE;
        default   : return 0;
      }
      case 0x40 : switch (payloadIndex) {
        case    3 : KEY("ESP_Uptime");                                     HACONFIG; HASECONDS;          maxOutputFilter = 2;                    VALUE_u32_LE_uptime;
        case    7 : KEY("MQTT_Acknowledged");                                                                                                    VALUE_u32_LE;
        case   11 : KEY("MQTT_Gaps");                                                                                                            VALUE_u32_LE;
        case   13 : KEY("MQTT_Min_Memory_Size");                           HACONFIG; HABYTES;                                                    VALUE_u16_LE;
        case   14 : KEY("P1P2_ESP_Interface_hwID_ESP");                                                                                          VALUE_u8hex;
        case   15 : KEY("ESP_ethernet_connected");                                                                                               VALUE_u8hex;
        case   19 : KEY("ESP_Max_Loop_Time");                              HACONFIG; HAMILLISECONDS                                              VALUE_u32_LE;
        default   : return 0;
      }
      default   : return 0;
    }
    case 0x0B :                                                            CAT_PSEUDO2;
    case 0x0F :                                                            CAT_PSEUDO;
                switch (packetSrc) {
      case 0x00 : switch (payloadIndex) {
        case    0 : KEY("Compile_Options_ATmega");                                                                                               VALUE_u8hex;
        case    1 : KEY("Verbose");                                        HACONFIG;                                                             VALUE_u8;
        case    2 : KEY("Reboot_MCUSR");                                   HACONFIG;                                                             VALUE_u8hex;
        case    3 : KEY("Write_Budget");                                   HACONFIG;                                                             VALUE_u8;
        case    4 : KEY("Error_Budget");                                   HACONFIG;                                                             VALUE_u8;
        case    6 : KEY("Counter_Parameter_Writes");             parameterWritesDone = FN_u16_LE(&payload[payloadIndex]);
                                                                           HACONFIG;                                                             VALUE_u16_LE;
        case    8 : KEY("Delay_Packet_Write");                                                                                                   VALUE_u16_LE;
        case   10 : KEY("Delay_Packet_Write_Timeout");                                                                                           VALUE_u16_LE;
        case   11 : KEY("P1P2_ESP_Interface_hwID_ATmega");                                                                                       VALUE_u8hex;
        case   12 : KEY("Control_ID_EEPROM");                                                                                                    VALUE_u8hex;
        case   13 : KEY("Verbose_EEPROM");                                                                                                       VALUE_u8;
        case   14 : KEY("Counter_Request_Repeat_EEPROM");                                                                                        VALUE_u8;
        case   15 : KEY("EEPROM_Signature_Match");                                                                                               VALUE_u8;
        case   17 : KEY("Error_Read_Count");                               HACONFIG;                                                             VALUE_u8;
        case   18 : KEY("Error_Read_Type");                                HACONFIG;                                                             VALUE_u8;
        case   19 : KEY("Control_ID");                                     HACONFIG;                                                             VALUE_u8hex;
        default   : return 0;
      }
      case 0x40 : switch (payloadIndex) {
        case    0 : KEY("ESP_telnet_connected");                           HACONFIG;                                                             VALUE_u8;
        case    1 : KEY("ESP_Output_Filter");                                                                                                    VALUE_u8;
        case    3 : KEY("ESP_Mem_free");                                   HACONFIG; HABYTES;                                                    VALUE_u16_LE;
        case    5 : KEY("MQTT_disconnected_time_total");                   HACONFIG; HASECONDS;                                                  VALUE_u16_LE;
        case    7 : KEY("Sprint_buffer_overflow");                                                                                               VALUE_u16_LE;
        case    8 : KEY("ESP_serial_input_Errors_Data_Short");;            HACONFIG;                                                             VALUE_u8;
        case    9 : KEY("ESP_serial_input_Errors_CRC");                    HACONFIG;                                                             VALUE_u8;
        case   13 : KEY("MQTT_Wait_Counter");                              HACONFIG; HAEVENTS;                                                   VALUE_u32_LE;
        case   15 : KEY("MQTT_disconnects");                               HACONFIG; HAEVENTS;                                                   VALUE_u16_LE;
        case   17 : KEY("MQTT_disconnected_skipped_packets");              HACONFIG; HAEVENTS;                                                   VALUE_u16_LE;
        case   19 : KEY("MQTT_messages_skipped_low_mem");                  HACONFIG; HAEVENTS;                                                   VALUE_u16_LE;
        default   : return 0;
      }
      default   : return 0;
    }
#else
    case 0x08 ... 0x0F : return 0;
#endif /* PSEUDO_PACKETS */
