# Known issues, warnings, wish list

### Software related

 - MQTT username/password length is limited to 19/39 characters. HA's MQTT server may generate 64-char passwords. Max length will be increased in a next release.
 - MQTT reconnect may fail, especially if the WIFI signal is weak (~ -90 dBm), resulting in an ESP8266 reboot
 - repeated MQTT reconnects due to poor WiFi may reduce free memory (repeated subscription related)?
 - P1P2-bridge-esp8266 is end-of-life and will be replaced by P1P2MQTT, which is more robust and more memory-efficient, and more user friendly

### Hardware related

 - Arduino/XL1192 or MM1192 based adapters should preferably not be connected to the P1/P2 bus if the adapter is not powered by the Arduino

### Older versions

 - Commands 'E'/'n' broken in P1P2Monitor until v0.9.17, please upgrde P1P2Monitor to v0.9.17-fix1
 - P1P2Serial (<v0.9.17) may incorrectly report read errors; please upgrade P1P2Monitor to v0.9.17-fix1
