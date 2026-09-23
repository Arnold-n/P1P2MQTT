#pragma once

//
// Arnold P1P2 series configuration
//
// Select exactly one series.
//

#define E_SERIES
// #define F_SERIES
// #define H_SERIES
// #define M_SERIES
// #define MHI_SERIES
// #define S_SERIES
// #define T_SERIES
// #define W_SERIES

#if defined(E_SERIES) || defined(F_SERIES) || defined(W_SERIES)
#define EF_SERIES
#endif

#if defined(T_SERIES) || defined(H_SERIES)
#define TH_SERIES
#endif

// Avoid MQTT/CPU overload while the decoder repopulates its state after a
// restart or Home Assistant reconnect.  These are Arnold's defaults for
// E-series (the generic branch in upstream P1P2_Config.h).
#if defined(S_SERIES) || defined(MHI_SERIES)
#define THROTTLE_STEP_S 2
#define THROTTLE_STEP_P 1
#define THROTTLE_VALUE  0
#elif defined(H_SERIES)
#define THROTTLE_STEP_S 1
#define THROTTLE_STEP_P 10
#define THROTTLE_VALUE  100
#elif defined(F1F2_SERIES)
#define THROTTLE_STEP_S 1
#define THROTTLE_STEP_P 50
#define THROTTLE_VALUE  100
#else
#define THROTTLE_STEP_S 1
#define THROTTLE_STEP_P 1
#define THROTTLE_VALUE  100
#endif

//
// P1P2 serial protocol
//

#ifndef SERIAL_MAGICSTRING
#define SERIAL_MAGICSTRING "1P2P"
#endif

#ifndef SERIALSPEED
#define SERIALSPEED 250000
#endif

//
// P1P2 packet / parser sizes
//

#ifndef RB
#define SPRINT_VALUE_LEN 1000
#define MQTT_KEY_LEN 100
#define MQTT_VALUE_LEN 1000

#define RB 1000
#endif

#ifndef HB
#define HB 24
#endif

//
// CRC
//

#if defined(M_SERIES)
#define CRC_GEN 0x00
#elif defined(TH_SERIES)
#define CRC_GEN 0x00
#elif defined(EF_SERIES)
#define CRC_GEN 0xD9
#elif defined(F1F2_SERIES)
#define CRC_GEN 0x00
#endif

#ifndef CRC_FEED
#define CRC_FEED 0x00

// EEPROM signature
#define EEPROM_SIGNATURE_LEN (9+1)
#define EEPROM_SIGNATURE_COMMON "P1P2"
#define EEPROM_SIGNATURE_OLD4 "P1P2sij"

#ifdef E_SERIES
#define EEPROM_SIGNATURE_NEW "P1P2sEz"
#else
#define EEPROM_SIGNATURE_NEW "P1P2sEz"
#endif

#endif

// -----------------------------------------------------------------------------
// Arnold P1P2MQTT MQTT compatibility
// -----------------------------------------------------------------------------

#define MQTT_QOS_HEX       0
#define MQTT_QOS_DATA      0
#define MQTT_QOS_SIGNAL    0
#define MQTT_QOS_WILL      1
#define MQTT_QOS_CONFIG    0
#define MQTT_QOS_DELETE    MQTT_QOS_CONFIG
#define MQTT_QOS_CONTROL   1
#define MQTT_QOS_EMETER    0
#define MQTT_QOS_HEX_IN    0

#define MQTT_RETAIN_DATA    true
#define MQTT_RETAIN_WILL    true
#define MQTT_RETAIN_CONFIG  true
#define MQTT_RETAIN_DELETE  true
#define MQTT_RETAIN_SIGNAL  false
#define MQTT_RETAIN_HEX     false


//
// Firmware compatibility
//

#define SAVEPARAMS
#define SAVEPACKETS
