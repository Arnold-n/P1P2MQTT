**Known issues and warnings**

v0.9.4

 - The ESP8266 program seems to hang shortly now and then. Not sure why. If this happens, a json message may have duplicate entries, and parameters may end up in the wrong location and reported with the wrong value. 
 - The ESP8266 does both paramter-over-mqtt of json-over-mqtt, doing one of both may be enough
 - MQTT_MAX_PACKET_SIZE must be increased in PubSubClient.h; not sure why this cannot be done before including the header file
