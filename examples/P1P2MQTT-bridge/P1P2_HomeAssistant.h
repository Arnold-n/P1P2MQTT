/* P1P2_HomeAssistant.h
 *
 * Copyright (c) 2023-2024 Arnold Niessen, arnold.niessen-at-gmail-dot-com - licensed under CC BY-NC-ND 4.0 with exceptions (see LICENSE.md)
 *
 * Version history
 * 20240605 v0.9.53 V_Interface entity is voltage
 * 20240519 v0.9.49 fix haConfigMsg max length
 * 20240515 v0.9.46 separated HA configuration from P1P2_Config.h
 * ..
 */

#ifndef P1P2_HomeAssistant
#define P1P2_HomeAssistant

#define DEVICE_UNIQ_ID_LEN ( DEVICE_NAME_LEN + BRIDGE_NAME_LEN)
#define ENTITY_UNIQ_ID_LEN MQTT_TOPIC_LEN // should be more than enough, given MQTT_PREFIX use in mqttTopic and not in entityUniqId

#define HACONFIG_PREFIX "homeassistant"                      // prefix for all MQTT topics
#define HACONFIG_PREFIX_LEN (16+1)

#define MQTT_TOPIC_LEN 200

#define DEVICE_SUBNAME_LEN (18+1)

char entityUniqId[ ENTITY_UNIQ_ID_LEN ];
char entityName[ ENTITY_UNIQ_ID_LEN ];
#define DEVICE_NAME_HA_LEN DEVICE_UNIQ_ID_LEN
char deviceNameHA[ DEVICE_UNIQ_ID_LEN ]; // defined once in setup(), depends on use*NameInNameHA, deviceName, bridgeName, deviceshortNameHA
char deviceUniqId[ DEVICE_NAME_HA_LEN ]; // defined once in setup(), depends on use*NameInDeviceId, deviceName, bridgeName, deviceshortNameHA

#define INIT_DEVICE_SHORT_NAME_HA "HC"
#define DEVICE_SHORT_NAME_HA_LEN (8+1)            // used if none of deviceName/bridgeName are used in device name for HA

#ifdef W_SERIES
#define INIT_METER_URL "http://192.168.4.81/api/v1/data" // can be changed with P command
#endif /* W_SERIES */

#ifdef W_SERIES
#define USE_BRIDGE_NAME_IN_TOPIC 0
#else /* W_SERIES */
#define USE_BRIDGE_NAME_IN_TOPIC 1
#endif /* W_SERIES */
#define USE_BRIDGE_NAME_IN_DEVICE_IDENTITY 0
#define USE_BRIDGE_NAME_IN_DEVICE_NAME_HA  0
#define USE_BRIDGE_NAME_IN_ENTITY_NAME 0
// USE_DEVICE_NAME_IN_DEVICE_IDENTITY implicitly 1
#define USE_DEVICE_NAME_IN_TOPIC 1
#define USE_DEVICE_NAME_IN_DEVICE_IDENTITY 0
#define USE_DEVICE_NAME_IN_DEVICE_NAME_HA 0
#define USE_DEVICE_NAME_IN_ENTITY_NAME 0

#define HA_DEVICE_MODEL "P1P2MQTT_bridge" // shows up as Device Info in HA
#define HA_MF "Home-CliCK"
#define HA_KEY_LEN 200

#ifdef F_SERIES
#define HA_VALUE_LEN 2000
#else
#define HA_VALUE_LEN 1500 // max seen 1359
#endif

// home assistant MQTT integration prefix

typedef enum {
  HA_NONE,
  HA_SENSOR,
  HA_BINSENSOR,
  HA_NUMBER,
  HA_SWITCH,
  HA_BUTTON,
  HA_SELECT,
  HA_TEXT,
  HA_CLIMATE
} hadevice;

#define HADEVICE_NONE      { haDevice = HA_NONE;      }
#define HADEVICE_SENSOR    { haDevice = HA_SENSOR;    }
#define HADEVICE_BINSENSOR { haDevice = HA_BINSENSOR; }
#define HADEVICE_NUMBER    { haDevice = HA_NUMBER;    }
#define HADEVICE_SWITCH    { haDevice = HA_SWITCH;    }
#define HADEVICE_BUTTON    { haDevice = HA_BUTTON;    }
#define HADEVICE_SELECT    { haDevice = HA_SELECT;    }
#define HADEVICE_TEXT      { haDevice = HA_TEXT;      }
#define HADEVICE_CLIMATE   { haDevice = HA_CLIMATE;   }

#define QOS_DATA_AVAILABILITY 1
#define QOS_DATA_CLIMATE 1
#define QOS_DATA_SELECT 1
#define QOS_DATA_SWITCH 1
#define QOS_DATA_NUMBER 1
#define QOS_DATA_TEXT 1

byte haQos = 0;
#define QOS_AVAILABILITY   { haQos = QOS_DATA_AVAILABILITY; }
#define QOS_CLIMATE        { haQos = QOS_DATA_CLIMATE;      }
#define QOS_SELECT         { haQos = QOS_DATA_SELECT;       }
#define QOS_SWITCH         { haQos = QOS_DATA_SWITCH;       }
#define QOS_NUMBER         { haQos = QOS_DATA_NUMBER;       }
#define QOS_TEXT           { haQos = QOS_DATA_TEXT;         }

const char* haPrefixString[] = {
    "undef",
    "sensor",
    "binary_sensor",
    "number",
    "switch",
    "button",
    "select",
    "text",
    "climate"
};

// home assistant entities

typedef enum {
  ENTITY_NONE,
  ENTITY_TEMPERATURE,
  ENTITY_POWER,
  ENTITY_FLOW,
  ENTITY_KWH,
  ENTITY_HOURS,
  ENTITY_MINUTES,
  ENTITY_SECONDS,
  ENTITY_MILLISECONDS,
  ENTITY_BYTES,
  ENTITY_EVENTS,
  ENTITY_CURRENT,
  ENTITY_FREQUENCY,
  ENTITY_PERCENTAGE,
  ENTITY_PRESSURE,
  ENTITY_COST,
  ENTITY_COP,
  ENTITY_VOLTAGE,
} haentity;

const char* haIconString[] = {
  "mdi:circle-outline",
  "mdi:coolant-temperature",
  "mdi:transmission-tower",
  "mdi:water-boiler",
  "mdi:transmission-tower",
  "mdi:clock-outline",
  "mdi:clock-outline",
  "mdi:clock-outline",
  "mdi:clock-outline",
  "mdi:memory",
  "mdi:counter",
  "mdi:heat-pump",
  "mdi:heat-pump",
  "mdi:valve",
  "mdi:water",
  "mdi:currency-eur",
  "mdi:poll",
  "mdi:volt", // TODO
};

const char* haUomString[] = {
  "",
  "°C",
  "W",
  "L/min",
  "kWh",
  "h",
  "min",
  "s",
  "ms",
  "byte",
  "events",
  "A",
  "Hz",
  "%",
  "bar",
  "€/kWh",
  " ",  // graph for unit-less COP
  "V",
};

typedef enum {
  HASTATECLASS_NONE,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_TOTAL,
  HASTATECLASS_TOTAL_INCREASING
} hastateclass;

const         byte haStateClass[] = {
  HASTATECLASS_NONE,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_TOTAL_INCREASING,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_TOTAL_INCREASING,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_TOTAL,
  HASTATECLASS_MEASUREMENT,
  HASTATECLASS_MEASUREMENT,
};

const char* haStateClassString[] = {
  "",
  "measurement",
  "total",
  "total_increasing"
};

// device_class for sensor, binsensor, number
const char* haDeviceClassString[] = {
  "",
  "temperature",
  "power",
  "",         // flow
  "energy",
  "",         // do not use "duration" for hours, to prevent hh:mm:ss format
  "duration",
  "duration",
  "duration",
  "data_size",
  "",          // packets/events
  "current",
  "frequency",
  "enum",
  "pressure",
  "",          // Euro/kWh cost
  "",
  "voltage",
// other options:
//  "monetary",
//  "problem",
//  "signal_strength",
//  "udpate",
//  "vibration",
//  "voltage",
//  "volume",
};

// device_class for button
typedef enum {
  BUTTONDEVICECLASS_NONE,
  BUTTONDEVICECLASS_IDENTIFY,
  BUTTONDEVICECLASS_RESTART,
  BUTTONDEVICECLASS_UPDATE,
} habuttondeviceclass;

#define HABUTTONDEVICECLASS_NONE     { haButtonDeviceClass = BUTTONDEVICECLASS_NONE; }
#define HABUTTONDEVICECLASS_IDENTIFY { haButtonDeviceClass = BUTTONDEVICECLASS_IDENTIFY; }
#define HABUTTONDEVICECLASS_RESTART  { haButtonDeviceClass = BUTTONDEVICECLASS_RESTART; }
#define HABUTTONDEVICECLASS_UPDATE   { haButtonDeviceClass = BUTTONDEVICECLASS_UPDATE; }

const char* haButtonDeviceClassString[] = {
  "identify",
  "restart",
  "update"
};

// entity category
typedef enum {
  ENTITYCATEGORY_NONE,
  ENTITYCATEGORY_DIAGNOSTIC
} haentitycategory;

#define HAENTITYCATEGORY_NONE       { haEntityCategory = ENTITYCATEGORY_NONE; }
#define HAENTITYCATEGORY_DIAGNOSTIC { /* no need to use diagnostic haEntityCategory = ENTITYCATEGORY_DIAGNOSTIC; */ }

const char* haEntityCategoryString[] {
  "",
  "diagnostic"
};

// number mode

typedef enum {
  NUMBERMODE_NONE,
  NUMBERMODE_AUTO,
  NUMBERMODE_BOX,
  NUMBERMODE_SLIDER
} hanumbermode;

#define HANUMBERMODE_NONE       { haNumberMode = NUMBERMODE_NONE; }
#define HANUMBERMODE_AUTO       { haNumberMode = NUMBERMODE_AUTO; }
#define HANUMBERMODE_BOX        { haNumberMode = NUMBERMODE_BOX; }
#define HANUMBERMODE_SLIDER     { haNumberMode = NUMBERMODE_SLIDER; }

const char* numberModeString[] = {
  "",
  "auto",
  "box",
  "slider"
};

#define EXTRA_AVAILABILITY_STRING_LEN 300
char extraAvailabilityString[EXTRA_AVAILABILITY_STRING_LEN];
uint16_t extraAvailabilityStringLength = 0;

#define HARESET   { haQos = MQTT_QOS_DATA; haConfig = 0; HADEVICE_SENSOR; HAENTITYCATEGORY_NONE; haEntity = ENTITY_NONE; HABUTTONDEVICECLASS_NONE; HAENTITYCATEGORY_NONE; HANUMBERMODE_NONE; haPrecision = 4; haConfigMessage[0] = '{'; haConfigMessage[1] = '\0'; haConfigMessageLength = 1; /* extraString[0] = '\0'; extraStringLength = 0; */ useSrc = 0; extraAvailabilityStringLength = 0; extraAvailabilityString[0] = '\0';}
#define HARESET2  { haQos = MQTT_QOS_DATA; haConfigMessage[0] = '{'; haConfigMessage[1] = '\0'; haConfigMessageLength = 1; extraAvailabilityStringLength = 0; extraAvailabilityString[0] = '\0';}
#define HACONFIG  { haConfig = 1; }

#define HANONE         { haEntity = ENTITY_NONE;          PRECISION(1); }
#define HATEMP0        { haEntity = ENTITY_TEMPERATURE;   PRECISION(0); }
#define HATEMP1        { haEntity = ENTITY_TEMPERATURE;   PRECISION(1); }
#define HATEMP2        { haEntity = ENTITY_TEMPERATURE;   PRECISION(2); }
#define HAPOWER        { haEntity = ENTITY_POWER;         PRECISION(0); }
#define HAFLOW         { haEntity = ENTITY_FLOW;          PRECISION(1); }
#define HAKWH          { haEntity = ENTITY_KWH;           PRECISION(0); }
#define HAHOURS        { haEntity = ENTITY_HOURS;         PRECISION(0); }
#define HAMINUTES      { haEntity = ENTITY_MINUTES;       PRECISION(0); }
#define HASECONDS      { haEntity = ENTITY_SECONDS;       PRECISION(0); }
#define HAMS           { haEntity = ENTITY_MILLISECONDS;  PRECISION(0); }
#define HABYTES        { haEntity = ENTITY_BYTES;         PRECISION(0); }
#define HAEVENTS       { haEntity = ENTITY_EVENTS;        PRECISION(0); }
#define HACURRENT      { haEntity = ENTITY_CURRENT;       PRECISION(1); }
#define HAFREQ         { haEntity = ENTITY_FREQUENCY;     PRECISION(0); }
#define HAPERCENT      { haEntity = ENTITY_PERCENTAGE;    PRECISION(0); }
#define HAPRESSURE     { haEntity = ENTITY_PRESSURE;      PRECISION(1); }
#define HACOST         { haEntity = ENTITY_COST;          PRECISION(2); }
#define HACOP          { haEntity = ENTITY_COP;           PRECISION(3); }
#define HAVOLTAGE      { haEntity = ENTITY_VOLTAGE;       PRECISION(1); }

#define PRECISION(x)   { haPrecision = (x); }

char deviceSubName[DEVICE_SUBNAME_LEN];
bool haBinarySensor = false;
byte haPrecision = 0;
byte useSrc = 0;
byte haConfig = 0;

hadevice haDevice = HA_NONE;
haentity haEntity = ENTITY_NONE;
habuttondeviceclass haButtonDeviceClass = BUTTONDEVICECLASS_NONE;
haentitycategory haEntityCategory = ENTITYCATEGORY_NONE;
hanumbermode haNumberMode = NUMBERMODE_NONE;

// delete topics (D12/D14):
#define DEL0  3 // # of categories in haDeleteCat
#define DEL1  8 // # of homeassistant subtopics in haPrefixString (without undef)
#define DEL2 12 // # of topics below

const char haDeleteCat[] = { 'M', 'L', 'Z' };

const char* haDeleteString[] = {
/*
    "homeassistant/sensor",
    "homeassistant/binary_sensor",
    "homeassistant/number",
    "homeassistant/switch",
    "homeassistant/button",
    "homeassistant/select",
    "homeassistant/text",
    "homeassistant/climate",
*/
    "C/9/#",
    "C/1/#",
    "C/9/#",
    "S/0/#",
    "S/1/#",
    "S/8/#",
    "S/9/#",
    "A/#",
    "M/#",
    "T/#",
    "F/2/#",
    "#",
};

#endif /* P1P2_HomeAssistant */
