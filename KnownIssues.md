**Known issues, warnings, wish list **

v0.9.7

 - Still work in progress
 - Hang/hickup bug in v0.9.6 seems solved (serial input handling changes, and timer2 instead of timer0 used in P1P2Serial library)
 - Confirmation for operation on other models would be appreciated
 - MQTT_MAX_PACKET_SIZE must be increased in PubSubClient.h; not sure why this cannot be done before including the header file
