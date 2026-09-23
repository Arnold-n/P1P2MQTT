#include "P1P2_Compat.h"
#include "Mqtt.h"
#include "P1P2Parser.h"
#include "P1P2_CompatAPI.h"
#include "WebSerial.h"
#include <Preferences.h>
#include <ETH.h>

// Arnold's original parameter conversion layer.
// Keep this file untouched; it is intentionally isolated here.
#include "P1P2_ParameterConversion.h"

// Forward declaration: defined near P1P2Compat_process() further down,
// but writePseudoPacket() (defined earlier in this file) needs to call
// it too -- both real bus packets and pseudo packets go through the
// exact same Arnold decode/publish traversal.
static void processBusPacket(const uint8_t* rb, uint8_t n);

// -----------------------------------------------------------------------------
// Arnold compatibility state
// -----------------------------------------------------------------------------

EEPROMSettings EE = {};

bool EE_dirty = false;

Preferences preferences;

static constexpr const char* NVS_NAMESPACE = "p1p2mqtt";

static uint32_t nextStateSave = 0;

const char* P1P2Compat_mqttServer()
{
  return EE.mqttServer;
}

void P1P2Compat_setMqttServer(const char* value)
{
  if (value)
  {
    strlcpy(EE.mqttServer,
            value,
            sizeof(EE.mqttServer));

    EE_dirty = true;
  }
}


void P1P2Compat_setMqttPort(uint16_t value)
{
  EE.mqttPort = value;
  EE_dirty = true;
}


void P1P2Compat_setMqttUser(const char* value)
{
  if (value)
  {
    strlcpy(EE.mqttUser,
            value,
            sizeof(EE.mqttUser));

    EE_dirty = true;
  }
}


void P1P2Compat_setMqttPassword(const char* value)
{
  if (value)
  {
    strlcpy(EE.mqttPassword,
            value,
            sizeof(EE.mqttPassword));

    EE_dirty = true;
  }
}


void P1P2Compat_setMqttClientName(const char* value)
{
  if (value)
  {
    strlcpy(EE.mqttClientName,
            value,
            sizeof(EE.mqttClientName));

    EE_dirty = true;
  }
}


void P1P2Compat_setMqttEnabled(bool value)
{
  EE.mqttEnabled = value;
  EE_dirty = true;
}


bool P1P2Compat_mqttEnabled()
{
  return EE.mqttEnabled;
}

uint16_t P1P2Compat_mqttPort()
{
  return (uint16_t)EE.mqttPort;
}

const char* P1P2Compat_mqttUser()
{
  return EE.mqttUser;
}

const char* P1P2Compat_mqttPassword()
{
  return EE.mqttPassword;
}

const char* P1P2Compat_mqttClientName()
{
  return EE.mqttClientName;
}

void P1P2Compat_saveSettings()
{
  preferences.begin(NVS_NAMESPACE, false);

  preferences.putString("mqttServer", EE.mqttServer);
  preferences.putUInt("mqttPort", (uint16_t)EE.mqttPort);
  preferences.putString("mqttUser", EE.mqttUser);
  preferences.putString("mqttPassword", EE.mqttPassword);
  preferences.putString("mqttClient", EE.mqttClientName);
  preferences.putBool("mqttEnabled", EE.mqttEnabled);

  preferences.end();

  EE_dirty = false;

  Serial.println("[NVS] MQTT settings saved");
}

uint32_t espUptime = 0;

byte pseudo0B = 0;
byte pseudo0C = 0;
byte pseudo0D = 0;
byte pseudo0E = 0;
byte pseudo0F = 9;

// Placeholder to mirror Arnold's "!mqttDeleting" guard in writePseudoPacket().
// We don't yet implement a "delete all HA configs" action, so this stays
// false and never blocks pseudo-packet processing; add real logic here if
// that feature gets ported later.
static bool mqttDeleting = false;
byte throttle = 1;
byte throttleValue = 1;

static bool compatMqttConnected = false;

uint32_t throttleStepTime = 0;

char mqtt_value[MQTT_VALUE_LEN] = "\0";
char mqttTopic[MQTT_TOPIC_LEN] = "";

byte mqttTopicChar = 0;
byte mqttSlashChar = 0;
byte mqttTopicCatChar = 0;
byte mqttTopicSrcChar = 0;
byte mqttTopicPrefixLength = 0;

byte readHex[HB] = {};

char haConfigTopic[HA_KEY_LEN] = "";
uint16_t haConfigTopicLength = 0;
uint16_t haConfigTopicLengthMax = 0;

char haConfigMessage[HA_VALUE_LEN] = "";
uint16_t haConfigMessageLength = 0;
uint16_t haConfigMessageLengthMax = 0;

// -----------------------------------------------------------------------------
// Arnold energy state
// -----------------------------------------------------------------------------

uint16_t ePower = 0;
uint16_t bPower = 0;

uint32_t eTotal = 0;
uint32_t bTotal = 0;

uint32_t ePowerTime = 0;
uint32_t eTotalTime = 0;
uint32_t bPowerTime = 0;
uint32_t bTotalTime = 0;

byte ePowerAvailable = 0;
bool eTotalAvailable = false;
byte bPowerAvailable = 0;
bool bTotalAvailable = false;

// -----------------------------------------------------------------------------
// Transport compatibility
// -----------------------------------------------------------------------------

static void buildMqttGenericTopic(
    char key,
    char* out,
    size_t outSize)
{
  size_t used = 0;

  used += snprintf(
      out + used,
      outSize - used,
      "%s/%c",
      EE.mqttPrefix,
      key
  );

  if (EE.useDeviceNameInTopic && used < outSize)
  {
    used += snprintf(
        out + used,
        outSize - used,
        "/%s",
        EE.deviceName
    );
  }

  if (EE.useBridgeNameInTopic && used < outSize)
  {
    used += snprintf(
        out + used,
        outSize - used,
        "/%s",
        EE.bridgeName
    );
  }

  if (used >= outSize)
    out[outSize - 1] = '\0';
}

void compatPrintfTopicS(const char* formatstring, ...)
{
  if (!formatstring)
    return;

  char message[MQTT_VALUE_LEN];
  va_list args;
  va_start(args, formatstring);
  vsnprintf(message, sizeof(message), formatstring, args);
  va_end(args);
  Serial.println(message);

  char topic[MQTT_TOPIC_LEN];
  buildMqttGenericTopic('S', topic, sizeof(topic));
  clientPublishMqtt(topic, MQTT_QOS_SIGNAL, MQTT_RETAIN_SIGNAL, message);
}

const char* P1P2Compat_mqttAvailabilityTopic()
{
  static char topic[MQTT_TOPIC_LEN];

  P1P2Compat_init();

  buildMqttGenericTopic(
      'L',
      topic,
      sizeof(topic)
  );

  return topic;
}

const char* P1P2Compat_mqttIpTopic()
{
    static char topic[MQTT_TOPIC_LEN];

    P1P2Compat_init();

    buildMqttGenericTopic(
        'Z',
        topic,
        sizeof(topic)
    );

    return topic;
}

void P1P2Compat_onMqttConnected()
{
    P1P2Compat_init();

    compatMqttConnected = true;

    // 1. Z — IP
    P1P2Compat_publishIp();

    // 2. L — availability
    const char* availability =
        P1P2Compat_mqttAvailabilityTopic();

    clientPublishMqtt(
        availability,
        MQTT_QOS_WILL,
        MQTT_RETAIN_WILL,
        "online"
    );

    // 3. Arnold reset semantics
    if (EE.outputMode & 0x0800)
    {
        P1P2Compat_resetData();

        throttleStepTime =
            espUptime + THROTTLE_STEP_S;

        throttleValue =
            THROTTLE_VALUE;

        pseudo0F = 9;
    }

    // 4. diagnostics
    compatPrintfTopicS("MQTT connected");

    // 5. M state snapshot
    P1P2Compat_publishStateSnapshot();
}


void P1P2Compat_publishIp()
{
    P1P2Compat_init();

    if (!ETH.localIP())
    {
        Serial.println("[P1P2/MQTT] Ethernet has no IP yet");
        return;
    }

    static char ip[20];

    IPAddress address = ETH.localIP();

    snprintf(
        ip,
        sizeof(ip),
        "%u.%u.%u.%u",
        address[0],
        address[1],
        address[2],
        address[3]
    );

    const char* topic =
        P1P2Compat_mqttIpTopic();

    clientPublishMqtt(
        topic,
        MQTT_QOS_CONFIG,
        MQTT_RETAIN_CONFIG,
        ip
    );

    Serial.printf(
        "[P1P2/MQTT] IP: %s = %s\n",
        topic,
        ip
    );
}

void P1P2Compat_restartEsp()
{
    P1P2Compat_init();

    logPrintf("[P1P2-COMPAT] Restart requested (D0) -- restarting ESP32...");

    // Same as Arnold's D0 handling: publish "offline" ourselves so Home
    // Assistant doesn't have to wait for the MQTT keep-alive/LWT timeout
    // to notice we're gone.
    clientPublishMqtt(
        P1P2Compat_mqttAvailabilityTopic(),
        MQTT_QOS_WILL,
        MQTT_RETAIN_WILL,
        "offline"
    );

    delay(200);

    ESP.restart();
}


void P1P2Compat_onHomeAssistantOnline()
{
    P1P2Compat_init();

    Serial.println(
        "[P1P2/MQTT] Home Assistant online"
    );

    // Re-announce bridge
    P1P2Compat_publishIp();

    const char* availability =
        P1P2Compat_mqttAvailabilityTopic();

    clientPublishMqtt(
        availability,
        MQTT_QOS_WILL,
        MQTT_RETAIN_WILL,
        "online"
    );

    // Arnold: mqttConnected == 3
    // => reset communication data
    P1P2Compat_resetData();

    throttleStepTime =
        espUptime + THROTTLE_STEP_S;

    throttleValue =
        THROTTLE_VALUE;

    pseudo0F = 9;

    // Re-publish persistent state
    P1P2Compat_publishStateSnapshot();
}


static void publishRawPacket(
    const P1P2Parser::Packet& packet)
{
  if (!(EE.outputMode & 0x0001))
    return;

  if (!packet.rawLine[0])
    return;

  char topic[MQTT_TOPIC_LEN];

  buildMqttGenericTopic(
      'R',
      topic,
      sizeof(topic)
  );

  clientPublishMqtt(
      topic,
      MQTT_QOS_HEX,
      MQTT_RETAIN_HEX,
      packet.rawLine
  );

#if P1P2_COMPAT_TRACE
  Serial.printf(
      "[P1P2/RAW] %s = %s\n",
      topic,
      packet.rawLine
  );
#endif
}



bool clientPublishMqtt(const char* key,
                       uint8_t qos,
                       bool retain,
                       const char* value)
{
    bool result = Esp32Mqtt::publish(
        key,
        qos,
        retain,
        value
    );

#if P1P2_COMPAT_TRACE
    if (value)
        Serial.printf(
            "[P1P2/MQTT] %s = %s\n",
            key ? key : "",
            value
        );
    else
        Serial.printf(
            "[P1P2/MQTT] %s = <null>\n",
            key ? key : ""
        );
#endif

    return result;
}

bool clientPublishMqttChar(char key, uint8_t qos, bool retain,
                           const char* value)
{
  if (mqttTopicChar < MQTT_TOPIC_LEN - 2)
    topicCharSpecific(key);

  return clientPublishMqtt(mqttTopic, qos, retain, value);
}

bool P1P2Compat_publishStateSnapshot()
{
  P1P2Compat_init();
  constexpr size_t kBytesPerBlock = 512;
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&M);
  size_t remaining = sizeof(M);
  size_t offset = 0;
  char segment = 'A';
  bool complete = true;

  while (remaining)
  {
    if (segment == '[')
      segment = 'a';
    if (segment > 'z')
    {
      compatPrintfTopicS("M snapshot too large for topic segments");
      return false;
    }

    const size_t count = remaining > kBytesPerBlock ? kBytesPerBlock : remaining;
    char topic[MQTT_TOPIC_LEN];
    char payload[kBytesPerBlock * 2 + 1];
    buildMqttGenericTopic('M', topic, sizeof(topic));
    const size_t topicLength = strlen(topic);
    if (topicLength + 2 >= sizeof(topic))
      return false;
    topic[topicLength] = '/';
    topic[topicLength + 1] = segment;
    topic[topicLength + 2] = '\0';

    for (size_t i = 0; i < count; ++i)
      snprintf(payload + i * 2, 3, "%02X", bytes[offset + i]);
    if (!clientPublishMqtt(topic, MQTT_QOS_HEX, true, payload))
      complete = false;

    offset += count;
    remaining -= count;
    ++segment;
  }
  return complete;
}

void clientPublishTelnet(bool includeTopic, const char* value, bool /*addDate*/)
{
#if P1P2_COMPAT_TRACE
  if (includeTopic)
    Serial.printf("[P1P2/TELNET] %s %s\n", mqttTopic, value ? value : "");
  else
    Serial.printf("[P1P2/TELNET] %s\n", value ? value : "");
#else
  (void)includeTopic;
  (void)value;
#endif
}

void clientPublishTelnetChar(char key, const char* value)
{
  if (key && mqttTopicChar < MQTT_TOPIC_LEN - 2)
    topicCharSpecific(key);

  clientPublishTelnet(true, value);
}

void writePseudoPacket(byte* WB, byte rh)
{
  if (!WB || rh > MAXRH)
  {
    printfTopicS("[P1P2/PSEUDO] invalid packet rh=%u", rh);
    return;
  }

  // Compute the CRC exactly like a real E/F/W-series bus packet and
  // append it as the last byte, mirroring Arnold's writePseudoPacket().
  // CRC_GEN/CRC_FEED come from P1P2_Config.h (0xD9/0x00 for EF_SERIES).
  uint8_t crc = CRC_FEED;

  for (uint8_t i = 0; i < rh; i++)
  {
    uint8_t c = WB[i];

    if (CRC_GEN != 0)
    {
      for (uint8_t bit = 0; bit < 8; bit++)
      {
        crc = ((crc ^ c) & 0x01) ? ((crc >> 1) ^ CRC_GEN) : (crc >> 1);
        c >>= 1;
      }
    }
  }

  WB[rh] = crc;

#if P1P2_COMPAT_TRACE
  Serial.print("[P1P2/PSEUDO] R");
  for (uint8_t i = 0; i <= rh; ++i) {
    if (WB[i] < 0x10)
      Serial.print('0');
    Serial.print(WB[i], HEX);
  }
  Serial.println();
#endif

  if (EE.outputMode & 0x0004)
  {
    // Raw hex ('R') publish of the pseudo packet, same shape as a real
    // bus frame on the R topic.
    char hex[MAXRH * 2 + 3];
    hex[0] = '\0';

    for (uint8_t i = 0; i <= rh; ++i)
    {
      snprintf(hex + strlen(hex), sizeof(hex) - strlen(hex), "%02X", WB[i]);
    }

    clientPublishMqttChar('R', MQTT_QOS_HEX, MQTT_RETAIN_HEX, hex);
  }

  // This is the actual fix: feed the pseudo packet through the exact same
  // decode/publish traversal as real bus packets. Without this call,
  // pseudo-packet-only entities (button creation via createButtonsSwitches1/2,
  // some switches like HA_Setup/Main_LCD_Light/Main_Installer, energy
  // counters via writePseudoSystemPacket0C, ...) never run at all.
  if ((EE.outputMode & 0x0022) && !mqttDeleting)
  {
    processBusPacket(WB, rh + 1);
  }
}

// -----------------------------------------------------------------------------
// Home Assistant compatibility
// -----------------------------------------------------------------------------

bool publishHomeAssistantConfig(
    const char* deviceSubName,
    const hadevice haDevice,
    const haentity haEntity,
    const haentitycategory haEntityCategory,
    byte haPrecision,
    const habuttondeviceclass haButtonDeviceClass,
    bool useSrc,
    bool useCommonName,
    const char* commonNameString)
{

  #if P1P2_DISABLE_HA_DISCOVERY
    (void)deviceSubName;
    (void)haDevice;
    (void)haEntity;
    (void)haEntityCategory;
    (void)haPrecision;
    (void)haButtonDeviceClass;
    (void)useSrc;
    (void)useCommonName;
    (void)commonNameString;
    return true;
  #endif

  if (useCommonName && commonNameString) {
    snprintf_P(entityUniqId, ENTITY_UNIQ_ID_LEN,
               PSTR("%s_%s_%s"),
               EE.deviceName, EE.bridgeName, commonNameString);
  } else {
    snprintf_P(entityUniqId, ENTITY_UNIQ_ID_LEN,
               PSTR("%s_%s_%c%c_%s_%c"),
               EE.deviceName, EE.bridgeName,
               mqttTopic[mqttTopicCatChar],
               mqttTopic[mqttTopicSrcChar],
               mqttTopic + mqttTopicPrefixLength,
               mqttTopic[mqttTopicSrcChar]);
  }

  if (EE.useDeviceNameInEntityName) {
    snprintf_P(entityName, ENTITY_UNIQ_ID_LEN, PSTR("%s_"), EE.deviceName);
  } else {
    entityName[0] = '\0';
  }

  if (EE.useBridgeNameInEntityName && EE.bridgeName[0]) {
    snprintf_P(entityName + strlen(entityName),
               ENTITY_UNIQ_ID_LEN - strlen(entityName),
               PSTR("%s_"), EE.bridgeName);
  }

  snprintf_P(entityName + strlen(entityName),
             ENTITY_UNIQ_ID_LEN - strlen(entityName),
             PSTR("%s"), mqttTopic + mqttTopicPrefixLength);

  if (useSrc) {
    snprintf_P(entityName + strlen(entityName),
               ENTITY_UNIQ_ID_LEN - strlen(entityName),
               PSTR("_%c"), mqttTopic[mqttTopicSrcChar]);
  }

  HACONFIGTOPIC("%s/%s/%s/%s/config",
                EE.haConfigPrefix,
                haPrefixString[haDevice],
                EE.bridgeName,
                entityUniqId);

  topicCharSpecific('L');

  HACONFIGMESSAGE_ADD(
      "\"name\":\"%s\",\"uniq_id\":\"%s\","
      "\"avty\":[{\"topic\":\"%s\",\"pl_avail\":\"online\","
      "\"pl_not_avail\":\"offline\"}%s],\"avty_mode\":\"all\","
      "\"dev\":{\"name\":\"%s%s\",\"ids\":[\"%s%s\"],"
      "\"mf\":\"%s\",\"mdl\":\"%s\",\"sw\":\"%s\"}",
      entityName,
      entityUniqId,
      mqttTopic,
      extraAvailabilityString,
      deviceNameHA, deviceSubName,
      deviceUniqId, deviceSubName,
      HA_MF,
      HA_DEVICE_MODEL,
      HA_SW);

  if (haEntityCategory)
    HACONFIGMESSAGE_ADD(",\"ent_cat\":\"%s\"",
                        haEntityCategoryString[haEntityCategory]);

  if (haEntity) {
    HACONFIGMESSAGE_ADD(",\"ic\":\"%s\"", haIconString[haEntity]);

    switch (haDevice) {
      case HA_SENSOR:
        HACONFIGMESSAGE_ADD(",\"sug_dsp_prc\":%d", haPrecision);
        HACONFIGMESSAGE_ADD(",\"stat_cla\":\"%s\"",
                            haStateClassString[haStateClass[haEntity]]);
        [[fallthrough]];

      case HA_NUMBER:
        HACONFIGMESSAGE_ADD(",\"unit_of_meas\":\"%s\"",
                            haUomString[haEntity]);
        [[fallthrough]];

      case HA_BINSENSOR:
      case HA_ENUM:
        if (haDeviceClassString[haEntity][0])
          HACONFIGMESSAGE_ADD(",\"dev_cla\":\"%s\"",
                              haDeviceClassString[haEntity]);
        break;

      default:
        break;
    }
  }

  if ((haDevice == HA_BUTTON) && haButtonDeviceClass) {
    HACONFIGMESSAGE_ADD(",\"dev_cla\":%s",
                        haButtonDeviceClassString[haButtonDeviceClass]);
  }

  topicCharSpecificSlash('P');

  switch (haDevice) {
    case HA_BINSENSOR:
      HACONFIGMESSAGE_ADD(",\"pl_off\":0,\"pl_on\":1");
      [[fallthrough]];

    case HA_SELECT:
    case HA_TEXT:
    case HA_SENSOR:
    case HA_ENUM:
    case HA_NUMBER:
    case HA_SWITCH:
      HACONFIGMESSAGE_ADD(",\"stat_t\":\"%s\"", mqttTopic);
      break;

    default:
      break;
  }

  if (haQos)
    HACONFIGMESSAGE_ADD(",\"qos\":1");

  HACONFIGMESSAGE_ADD("}");

// Arnold compatibility: give the asynchronous MQTT client
// time between Home Assistant discovery messages.

  delay(50);

  return clientPublishMqtt(haConfigTopic,
                           MQTT_QOS_CONFIG,
                           MQTT_RETAIN_CONFIG,
                           haConfigMessage);
}

bool deleteHomeAssistantConfig(
    const char* deviceSubName,
    const hadevice haDevice,
    const haentity haEntity,
    const haentitycategory haEntityCategory,
    byte haPrecision,
    const habuttondeviceclass haButtonDeviceClass,
    bool useSrc,
    bool useCommonName,
    const char* commonNameString)
{
  (void)deviceSubName;
  (void)haEntityCategory;
  (void)haPrecision;
  (void)haButtonDeviceClass;
  (void)useSrc;

  if (useCommonName && commonNameString) {
    snprintf_P(entityUniqId, ENTITY_UNIQ_ID_LEN,
               PSTR("%s_%s_%s"),
               EE.deviceName, EE.bridgeName, commonNameString);
  } else {
    snprintf_P(entityUniqId, ENTITY_UNIQ_ID_LEN,
               PSTR("%s_%s_%c%c_%s_%c"),
               EE.deviceName, EE.bridgeName,
               mqttTopic[mqttTopicCatChar],
               mqttTopic[mqttTopicSrcChar],
               mqttTopic + mqttTopicPrefixLength,
               mqttTopic[mqttTopicSrcChar]);
  }

  HACONFIGTOPIC("%s/%s/%s/%s/config",
                EE.haConfigPrefix,
                haPrefixString[haDevice],
                EE.bridgeName,
                entityUniqId);

  return clientPublishMqtt(haConfigTopic,
                           MQTT_QOS_CONFIG,
                           MQTT_RETAIN_CONFIG,
                           nullptr);
}


static void P1P2Compat_loadSettings()
{
  preferences.begin(NVS_NAMESPACE, true);

  String server =
      preferences.getString("mqttServer", "");

  uint16_t port =
      (uint16_t)preferences.getUInt("mqttPort", 1883);

  String user =
      preferences.getString("mqttUser", "");

  String password =
      preferences.getString("mqttPassword", "");

  String client =
      preferences.getString("mqttClient", "");

  bool enabled =
      preferences.getBool("mqttEnabled", true);

  preferences.end();

  if (server.length() > 0)
    strlcpy(EE.mqttServer,
            server.c_str(),
            sizeof(EE.mqttServer));

  EE.mqttPort = port;

  if (user.length() > 0)
    strlcpy(EE.mqttUser,
            user.c_str(),
            sizeof(EE.mqttUser));

  if (password.length() > 0)
    strlcpy(EE.mqttPassword,
            password.c_str(),
            sizeof(EE.mqttPassword));

  if (client.length() > 0)
    strlcpy(EE.mqttClientName,
            client.c_str(),
            sizeof(EE.mqttClientName));

  EE.mqttEnabled = enabled;

  Serial.println("[NVS] MQTT settings loaded");
}

// -----------------------------------------------------------------------------
// Runtime context
// -----------------------------------------------------------------------------


static bool dataStructuresInitialized = false;

void P1P2Compat_init()
{
  static bool initialized = false;
  if (initialized)
    return;

  initialized = true;

  memset(&EE, 0, sizeof(EE));

  // ---------------------------------------------------------------------------
  // MQTT defaults
  // ---------------------------------------------------------------------------

  strlcpy(
      EE.mqttServer,
      MQTT_SERVER,
      sizeof(EE.mqttServer)
  );

  EE.mqttPort = atoi(MQTT_PORT);

  strlcpy(
      EE.mqttUser,
      MQTT_USER,
      sizeof(EE.mqttUser)
  );

  strlcpy(
      EE.mqttPassword,
      MQTT_PASSWORD,
      sizeof(EE.mqttPassword)
  );

  EE.mqttEnabled = true;


  strlcpy(EE.deviceName, DEVICE_NAME, sizeof(EE.deviceName));
  strlcpy(EE.bridgeName, BRIDGE_NAME, sizeof(EE.bridgeName));
  strlcpy(EE.haConfigPrefix, HACONFIG_PREFIX, sizeof(EE.haConfigPrefix));
  strlcpy(EE.mqttPrefix, MQTT_PREFIX, sizeof(EE.mqttPrefix));
  strlcpy(EE.mqttClientName, MQTT_CLIENTNAME, sizeof(EE.mqttClientName));
  strlcpy(EE.deviceShortNameHA,
          INIT_DEVICE_SHORT_NAME_HA,
          sizeof(EE.deviceShortNameHA));

  EE.useDeviceNameInTopic = USE_DEVICE_NAME_IN_TOPIC;
  EE.useDeviceNameInEntityName = USE_DEVICE_NAME_IN_ENTITY_NAME;
  EE.useBridgeNameInTopic = USE_BRIDGE_NAME_IN_TOPIC;
  EE.useBridgeNameInEntityName = USE_BRIDGE_NAME_IN_ENTITY_NAME;
  EE.useBridgeNameInDeviceIdentity = USE_BRIDGE_NAME_IN_DEVICE_IDENTITY;
  EE.useBridgeNameInDeviceNameHA = USE_BRIDGE_NAME_IN_DEVICE_NAME_HA;
  EE.useDeviceNameInDeviceIdentity = USE_DEVICE_NAME_IN_DEVICE_IDENTITY;
  EE.useDeviceNameInDeviceNameHA = USE_DEVICE_NAME_IN_DEVICE_NAME_HA;

P1P2Compat_loadSettings();

// ---------------------------------------------------------------------------
// Arnold runtime data structures
//
// This is equivalent to Arnold's initial data initialization.
// It must NOT be executed for every packet.
// -----------------------------------------------------------------------------
if (!dataStructuresInitialized)
{
    Serial.println("[P1P2-COMPAT] Initializing Arnold data structures");

    resetDataStructures();
    initDataRTC();

    dataStructuresInitialized = true;

    Serial.println("[P1P2-COMPAT] Arnold data structures initialized");
}

// ---------------------------------------------------------------------------
// Arnold MQTT topic context
// ---------------------------------------------------------------------------


  strlcpy(
      mqttTopic,
      EE.mqttPrefix,
      MQTT_TOPIC_LEN
  );

  strlcpy(
      mqttTopic + strlen(mqttTopic),
      "/X",
      3
  );

  mqttTopicChar = strlen(mqttTopic) - 1;

  if (EE.useDeviceNameInTopic)
  {
    strlcpy(
        mqttTopic + strlen(mqttTopic),
        "/",
        2
    );

    strlcpy(
        mqttTopic + strlen(mqttTopic),
        EE.deviceName,
        DEVICE_NAME_LEN
    );
  }

  if (EE.useBridgeNameInTopic)
  {
    strlcpy(
        mqttTopic + strlen(mqttTopic),
        "/",
        2
    );

    strlcpy(
        mqttTopic + strlen(mqttTopic),
        EE.bridgeName,
        BRIDGE_NAME_LEN
    );
  }

  strlcpy(
      mqttTopic + strlen(mqttTopic),
      "/M/0/",
      6
  );

  mqttTopicCatChar = strlen(mqttTopic) - 4;
  mqttTopicSrcChar = strlen(mqttTopic) - 2;
  mqttSlashChar = strlen(mqttTopic) - 5;
  mqttTopicPrefixLength = strlen(mqttTopic);

  // 0x0002 = publish MQTT for named/mapped parameters (P/... topics, drives HA).
  // 0x0008 = outputUnknown: also publish raw, unmapped registers under the
  //          default topic category, i.e. Arnold's "M" output (.../M/0/xx).
  //          Without this bit, unmapped bytes are silently dropped and never
  //          reach M at all -- only P1P2Compat_publishStateSnapshot()'s
  //          separate full-memory dump would show up under M/A, M/B, ...
  EE.outputMode = 0x000A;
  EE.outputFilter = 1;

#ifdef E_SERIES
  EE.useTotal = INIT_USE_TOTAL;
  EE.R1Toffset = INIT_R1T_OFFSET;
  EE.R2Toffset = INIT_R2T_OFFSET;
  EE.R4Toffset = INIT_R4T_OFFSET;
  EE.RToffset = INIT_RT_OFFSET;
  EE.voltage = 230;
  EE.nrPhases = 1;
  EE.powerBUH1 = 0;
  EE.powerBUH2 = 0;
  EE.useR1T = 0;
  EE.haSetup = true;
#endif

    haConfigTopic[0] = '\0';
    haConfigMessage[0] = '\0';
    mqtt_value[0] = '\0';


deviceNameHA[0] = '\0';
deviceUniqId[0] = '\0';

if (EE.deviceName[0] && EE.useDeviceNameInDeviceNameHA)
{
    if (EE.bridgeName[0] && EE.useBridgeNameInDeviceNameHA)
    {
        snprintf(
            deviceNameHA,
            DEVICE_NAME_HA_LEN,
            "%s_%s",
            EE.deviceName,
            EE.bridgeName
        );
    }
    else
    {
        snprintf(
            deviceNameHA,
            DEVICE_NAME_HA_LEN,
            "%s",
            EE.deviceName
        );
    }
}
else
{
    if (EE.bridgeName[0] && EE.useBridgeNameInDeviceNameHA)
    {
        snprintf(
            deviceNameHA,
            DEVICE_NAME_HA_LEN,
            "%s",
            EE.bridgeName
        );
    }
    else
    {
        strlcpy(
            deviceNameHA,
            EE.deviceShortNameHA,
            DEVICE_NAME_HA_LEN
        );
    }
}

if (EE.deviceName[0] && EE.useDeviceNameInDeviceIdentity)
{
    if (EE.bridgeName[0] && EE.useBridgeNameInDeviceIdentity)
    {
        snprintf(
            deviceUniqId,
            DEVICE_UNIQ_ID_LEN,
            "%s_%s",
            EE.deviceName,
            EE.bridgeName
        );
    }
    else
    {
        snprintf(
            deviceUniqId,
            DEVICE_UNIQ_ID_LEN,
            "%s",
            EE.deviceName
        );
    }
}
else
{
    if (EE.bridgeName[0] && EE.useBridgeNameInDeviceIdentity)
    {
        snprintf(
            deviceUniqId,
            DEVICE_UNIQ_ID_LEN,
            "%s",
            EE.bridgeName
        );
    }
    else
    {
        strlcpy(
            deviceUniqId,
            EE.deviceShortNameHA,
            DEVICE_NAME_HA_LEN
        );
    }
}


  deviceSubName[0] = '\0';
  extraAvailabilityString[0] = '\0';
  extraAvailabilityStringLength = 0;

  EE_dirty = false;
  espUptime = millis() / 1000U;
  pseudo0B = 0;
  pseudo0D = 0;
  pseudo0F = 9;
  throttle = 1;
  throttleValue = 1;
}


void P1P2Compat_resetData()
{
    Serial.println("[P1P2-COMPAT] Resetting Arnold data structures");

    resetDataStructures();
    initDataRTC();

    dataStructuresInitialized = true;

    Serial.println("[P1P2-COMPAT] Arnold data structures reset");
}

void P1P2Compat_tick()
{
    espUptime = millis() / 1000U;

    if (throttleValue &&
        espUptime > throttleStepTime)
    {
        throttleValue -= THROTTLE_STEP_P;
        throttleStepTime += THROTTLE_STEP_S;

        if (!throttleValue)
        {
            pseudo0F = 9;
        }
    }

    if (compatMqttConnected &&
        espUptime >= nextStateSave)
    {
        nextStateSave = espUptime + 60;

        P1P2Compat_publishStateSnapshot();
    }

    // Arnold's periodic pseudo-packet trigger (upstream: pseudo0B/0C/0D++
    // once per second, writePseudoSystemPacket0X() once each counter
    // passes 5). This is what progressively creates HA button/switch
    // entities and publishes energy counters -- without it, nothing in
    // P1P2_Pseudo.h ever runs.
    if (compatMqttConnected)
    {
        static uint32_t lastPseudoTickUptime = 0;

        if (espUptime != lastPseudoTickUptime)
        {
            lastPseudoTickUptime = espUptime;

            pseudo0B++;
            pseudo0C++;
            pseudo0D++;
            pseudo0E++;

            if (pseudo0B > 5)
            {
                pseudo0B = 0;
                writePseudoSystemPacket0B();
            }

            if (pseudo0C > 5)
            {
                pseudo0C = 0;
                writePseudoSystemPacket0C();
            }

            if (pseudo0D > 5)
            {
                pseudo0D = 0;
                writePseudoSystemPacket0D();
            }

            if (pseudo0E > 5)
            {
                pseudo0E = 0;

                // Packet type 0x0E, src 0x40 (ESP). This is what upstream
                // (P1P2MQTT-bridge.ino) builds in its main loop -- there is
                // no separate writePseudoSystemPacket0E() function, it's
                // inlined there. Payload byte at index 11 ("dummy for
                // switches and buttons" per Arnold's own comment) is the
                // one that actually triggers createButtonsSwitches1()/2()
                // in P1P2_Pseudo.h -- without this packet ever being sent,
                // NONE of the ~17 HA button entities are ever created,
                // regardless of pseudo0B/C/D/F working fine.
                readHex[0] = 0x40;
                readHex[1] = 0x00;
                readHex[2] = 0x0E;

                // Version/reboot-diagnostics fields: upstream fills these
                // from ESP8266-specific double-reset-detector RTC memory
                // and firmware version macros we don't have a port of yet.
                // Zeros here only affect the informational ESP_* sensors,
                // not entity creation.
                readHex[3] = 0;  // SW_MAJOR_VERSION
                readHex[4] = 0;  // SW_MINOR_VERSION
                readHex[5] = 0;  // SW_PATCH_VERSION
                readHex[6] = 0;  // ESP restart count
                readHex[7] = 0;  // reboot reason

                readHex[8]  = (EE.outputMode >> 24) & 0xFF;
                readHex[9]  = (EE.outputMode >> 16) & 0xFF;
                readHex[10] = (EE.outputMode >> 8) & 0xFF;
                readHex[11] = EE.outputMode & 0xFF;
                readHex[12] = EE.outputFilter;
                readHex[13] = EE.ESPhwID;
                readHex[14] = 0; // dummy for switches and buttons

#ifdef E_SERIES
                readHex[15] = (EE.RToffset >> 8) & 0xFF;
                readHex[16] = EE.RToffset & 0xFF;
                readHex[17] = EE.R1Toffset;
                readHex[18] = EE.R2Toffset;
                readHex[19] = EE.R4Toffset;
                writePseudoPacket(readHex, 20);
#else
                writePseudoPacket(readHex, 15);
#endif
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Arnold packet processing bridge
// -----------------------------------------------------------------------------
//
// This is intentionally kept inside the compatibility layer.
// P1P2Processor.cpp must not know anything about Arnold's
// bytes2keyvalue()/bits2keyvalue() implementation.
//

// -----------------------------------------------------------------------------
// Core Arnold process_for_mqtt() traversal, extracted so it can be fed both
// by real bus packets (P1P2Compat_process, below) AND by pseudo packets
// (writePseudoPacket) -- exactly like upstream's process_for_mqtt(WB, rh)
// is called from both places. This is what actually triggers button/switch
// creation (createButtonsSwitches1/2 in P1P2_ParameterConversion.h) and any
// other pseudo-packet-only entity, since those live behind packet types
// 0x0B/0x0C/0x0D/0x0F which only ever arrive via pseudo packets.
// -----------------------------------------------------------------------------

static void processBusPacket(const uint8_t* rb, uint8_t n)
{
  if (!rb || n < 3)
    return;

#ifdef EF_SERIES

  // Exact Arnold special case for EF-series packets without payload.
  if (n == 3)
  {
    bytes2keyvalue(
        rb[0],
        rb[1],
        rb[2],
        EMPTY_PAYLOAD,
        const_cast<uint8_t*>(rb + 3)
    );
  }

#endif

#ifdef MHI_SERIES

  // Exact Arnold MHI packet layout.
  for (uint8_t i = 1; i < n; i++)
  {
    uint8_t doBits =
        bytes2keyvalue(
            rb[0],
            0,
            0,
            i - 1,
            const_cast<uint8_t*>(rb + 1)
        );

    for (uint8_t j = 0; j < 8; j++)
    {
      if (doBits & (1 << j))
      {
        bits2keyvalue(
            rb[0],
            0,
            0,
            i - 1,
            const_cast<uint8_t*>(rb + 1),
            j
        );
      }
    }
  }

#else

  // Exact Arnold process_for_mqtt() traversal for normal series.
  // bytes 0..2 = source/destination/type; payload starts at byte 3.
  for (uint8_t i = 3; i < n; i++)
  {
    if (!--throttle)
      throttle = THROTTLE_VALUE;

    bool processByte = (throttle >= throttleValue);

#ifdef E_SERIES

    // Arnold's explicit anti-throttling exceptions.
    if ((rb[0] == 0x00) &&
        ((rb[2] == 0x31) || (rb[2] == 0x12)))
      processByte = true;

    if ((rb[0] == 0x40) &&
        (rb[2] == 0xB8) && ((rb[3] & 0xFE) == 0x00))
      processByte = true;

    if ((rb[0] == 0x40) && (rb[2] == 0x0F))
      processByte = true;

    if ((rb[0] == 0x40) &&
        (rb[2] >= 0x60) && (rb[2] <= 0x8F))
      processByte = true;

#endif

    if (!processByte)
      continue;

    uint8_t doBits =
        bytes2keyvalue(
            rb[0],
            rb[1],
            rb[2],
            i - 3,
            const_cast<uint8_t*>(rb + 3)
        );

#if P1P2_COMPAT_TRACE
    Serial.printf(
        "[P1P2/PROCESS] %02X %02X %02X payload[%u]=%02X doBits=%02X\\n",
        rb[0],
        rb[1],
        rb[2],
        (unsigned)(i - 3),
        rb[i],
        doBits
    );
#endif

    for (uint8_t j = 0; j < 8; j++)
    {
      if (doBits & (1 << j))
      {
        bits2keyvalue(
            rb[0],
            rb[1],
            rb[2],
            i - 3,
            const_cast<uint8_t*>(rb + 3),
            j
        );
      }
    }
  }

#endif
}

void P1P2Compat_process(const P1P2Parser::Packet& packet)
{
  P1P2Compat_init();
  P1P2Compat_tick();

  if (!compatMqttConnected)
      return;

  publishRawPacket(packet);

  processBusPacket(packet.data, packet.length);
}

uint32_t compatGetOutputMode()
{
    return EE.outputMode;
}

uint8_t compatGetOutputFilter()
{
    return EE.outputFilter;
}

bool compatGetHaSetup()
{
    return EE.haSetup;
}

void P1P2Compat_onMqttDisconnected()
{
    compatMqttConnected = false;
}


const char* P1P2Compat_mqttWriteTopic()
{
    static char topic[MQTT_TOPIC_LEN];

    P1P2Compat_init();

    buildMqttGenericTopic(
        'W',
        topic,
        sizeof(topic)
    );

    return topic;
}

void P1P2Compat_mqttWriteBroadcastTopic(
    char* topic,
    size_t topicSize)
{
    P1P2Compat_init();

    buildMqttGenericTopic(
        'W',
        topic,
        topicSize
    );
}
