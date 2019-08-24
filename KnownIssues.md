**Known issues and warnings**

v0.9.6

 - The P1P2Monitor program serial output seems to hang shortly now and then. Not sure why. If this happens, P1P2-bridge-esp8266 may show a json message with duplicate entries, and data may be read partially or incomplete.
 - The ESP8266 does both paramter-over-mqtt of json-over-mqtt, doing one of both may be enough
 - MQTT_MAX_PACKET_SIZE must be increased in PubSubClient.h; not sure why this cannot be done before including the header file
