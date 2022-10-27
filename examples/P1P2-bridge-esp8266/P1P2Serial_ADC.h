/* P1P2Serial_ADC.h: header file for ADC support in P1P2Serial
 *
 * Copyright (c) 2022 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 * 20221029 v0.9.23 ADC code
 *
 */

// file included by P1P2Serial (P1P2Serial.h) and by P1P2-bridge-esp8266/P1P2MQTT in different locations, so keep header files in sync

#define ADC_AVG_SHIFT 4     // sum 16 = 2^ADC_AVG_SHIFT samples before doing min/max check
#define ADC_CNT_SHIFT 4     // sum 16384 = 2^(16-ADC_CNT_SHIFT) samples to V0avg/V1avg
