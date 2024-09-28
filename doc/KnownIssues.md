# Known issues, warnings, wish list

### Software related

 - Home Assistant sometimes transmits an outdated setpoint, giving the impression the bridge is unresponsive. This seems an issue with HA, not with the bridge
 - Some controls and entities in Home Assistant appear with a delay; this seems partly an HA issue, partly because the P1P2MQTT bridge has to discover the Daikin system capabilities first; it is recommended not to build the dashboards immediately but first wait for all controls and entities to become discovered
 - the `C1`, `C2`, and `Q` commands intentionally violate the timing of the P1/P2 bus protocol, check your status log `P1P2/S/#` and raw data logs `P1P2/R/#` for bus collisions and other problems when using this, or the counter value of Error\_Budget in HA under HC\_Bridge (should be and remain at least 9, should not decrease), use with care! The bridge should detect any issues and switch these functions off in case of bus collissions

### Software related, fixed

 - (solved in 0.9.45) in some cases counter requests may stop after Daikin system restart
 - (solved in 0.9.27) Power\_\* was not visible in HA
 - (solved in 0.9.26) MQTT username/password length was limited to 19/39 characters in WiFiManager. Max length now 80.
 - (solved in 0.9.23) MQTT reconnect may fail, especially if the WIFI signal is weak (~ -90 dBm), resulting in an ESP8266 reboot

### Hardware related

 - Arduino/XL1192 or MM1192 based adapters should preferably not be connected to the P1/P2 bus if the adapter is not powered by the Arduino

### Todo list

 - COP calculation
 - smart grid / dynamic power limitation
 - web server ([security](Security.md), functionality)
 - logging/setting field settings
 - EKHBRD support
 - documentation
 - Hitachi support
 - Mitsubishi support
 - MHI (Mitsubishi Heavy Industries) support
 - Toshiba support
 - Panasonic support
 - Sanyo support
 - ...
