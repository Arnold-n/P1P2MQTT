# Known issues, warnings, wish list

### Software related

 - P1P2-bridge-esp8266 is end-of-life and will be replaced by P1P2MQTT, which is more robust and more memory-efficient, and more user friendly
 - 'C1' and 'C2' command violate P1/P2 bus protocol, check your status log P1P2/S/# and raw data logs P1P2/R/# for bus collissions and other problems when using this, use with care!

### Software related, fixed

 - (solved in 0.9.27) Power\_\* was not visible in HA
 - (solved in 0.9.26) MQTT username/password length was limited to 19/39 characters in WiFiManager. Max length now 80.
 - (solved in 0.9.23) MQTT reconnect may fail, especially if the WIFI signal is weak (~ -90 dBm), resulting in an ESP8266 reboot

### Hardware related

 - Arduino/XL1192 or MM1192 based adapters should preferably not be connected to the P1/P2 bus if the adapter is not powered by the Arduino

### Todo list

 - COP calculation
 - HA integration
 - smart grid / dynamic power limitation
 - web server
 - setting field settings
 - EKHBRD support
 - documentation
 - Hitachi support
 - Toshiba support
 - Panasonic support
