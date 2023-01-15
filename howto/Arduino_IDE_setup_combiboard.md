# Arduino IDE setup for P1P2Monitor and P1P2-bridge-esp8266 on Arduino Uno or on Uno/ESP8266 combi-board

## Install Arduino IDE 1.8.19 and libraries

- add board support package via File/Preferences in text box "Additional Board Manager URLs", this is a comma-separated strings of URLs, add at the end
",http://arduino.esp8266.com/stable/package_esp8266com_index.json", optionally click "Use external editor"  if you prefer to use vim/emacs/your-own to edit outside the IDE (recommended, see below) and click OK,
- add libraries via Tools/Manage-libraries, filter on "wifi" and install WiFiManager by tablatronix version 0.16.0, ESP Telnet by Lennart Hennigs version 2.0.0,
- find library folder (I think this is in libraries within the folder mentioned as Sketchblook location in File/Preferences)
- close Arduino IDE
- add two more libraries manually by either "cd ~/Arduino/libraries ; git clone https://github.com/marvinroger/async-mqtt-client; git clone https://github.com/philbowles/ESPAsyncTCP" (or by downloading ZIP files from Github and unzipping these)
- add P1P2Serial code with examples by "cd ~/Arduino/libraries ; git clone https://github.com/Arnold-n/P1P2Serial" (or by downloading ZIP files from Github and unzipping these, but git clone is recommended as it enables git use to track changes)
- open Arduino IDE

The combi-board has 2 CPUs (ATmega328P and ESP8266) each of which requires its own firmware image (each CPU counts as a separate "board" in the IDE).

## Build and flash firmware image for P1P2Monitor on combi-board or Arduino Uno

- select Tools/Board/Arduino AVR Boards/Arduino Uno
- open P1P2Monitor code go to File/Examples/ and under "Examples from Custom Libraries" select P1P2Serial/P1P2-bridge-esp8266
- if you edit code outside IDE (recommended!), you are modifying within the library examples, and you can use git tooling
- if you edit within IDE, IDE asks you to save the modified example code to a different location
- edit P1P2Config.h based on your system and preferences
- attach Arduino Uno via USB, for combi-board: set DIP switches 3-4 only, and compile and upload via "Upload"  button (right arrow symbol)

Warning:
- if you make a folder copy, check which version you are editing, and which you are compiling/flashing. The Arduino IDE can be confusing as it only uses the folder name under File/Open Recent. 

## For combi-board or separate ESP8266: build and flash firmware image for P1P2-bridge-esp8266 on ESP8266

- select Tools/Board/ESP8266 Boards (3.0.2)/ESP8266 Generic
- select Tools/Flash Size/1MB (FS:no OTA: ~502kB) (NOTE: 4MB should be OK, may be needed in future, but ESP01 is only 1MB, and 4MB sometimes gave me issues)
- under Tools/Erase/Flash, select either default "only Sketch" or "All flash contents"; for OTA only sketch can be flashed, but using the ESP01-compatible programmer the WiFi settings can be erased if they cause any issue
- open P1P2-bridge-esp8266 code: go to File/Examples/ and under  "Examples from Custom Libraries" select P1P2Serial/P1P2-bridge-esp8266-if you edit code outside IDE (recommended!), you are modifying within the library examples, and you can use git tooling
- select programming port: select a network port for OTA named P1P2MQTT_xxx if available, or a serial port like "/dev/ttyUSB0" (and for USB programming combi-board set only DIP switches 5,6, and 7)
- edit P1P2_Config.h based on your system and preferences
- flash code using "Upload" button (right arrow symbol)
- set DIP switches 1-2 only
- you may have to re-enter WiFi and MQTT parameters using WifiManager
