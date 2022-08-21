**Known issues, warnings, wish list **

 - Commands 'E'/'n' broken in P1P2Monitor until v0.9.17, please upgrde P1P2Monitor to v0.9.17-fix1
 - P1P2Serial (<v0.9.17) may incorrectly report read errors; please upgrade P1P2Monitor to v0.9.17-fix1
 - Arduino/XL1192 or MM1192 based adapters should preferably not be connected to the P1/P2 bus if the adapter is not powered by the Arduino
 - P1P2-bridge-esp8266 is end-of-life and will be replaced by P1P2MQTT. It has some reliability issues (CPU overload crashes the ESP8266)
