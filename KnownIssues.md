**Known issues, warnings, wish list **

 - MQTT_MAX_PACKET_SIZE must be increased in PubSubClient.h; not sure why this cannot be done before including the header file
 - XL1192/MM1192 based adapter should not be connected to P1/P2 bus if the device is not powered
