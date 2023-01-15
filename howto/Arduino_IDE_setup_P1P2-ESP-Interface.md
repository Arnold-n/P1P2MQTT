# Build and flash P1P2Monitor on ATmega328P and P1P2-bridge-esp8266 on ESP8266 for P1P2-ESP-Interface

## Install Arduino IDE 1.8.19 and libraries

- add board support package via File/Preferences in text box "Additional Board Manager URLs", this is a comma-separated strings of URLs, add at the end
",http://arduino.esp8266.com/stable/package_esp8266com_index.json,
https://mcudude.github.io/MiniCore/package_MCUdude_MiniCore_index.json", optionally click "Use external editor" if you prefer to use vim/emacs/your-own to edit outside the IDE (recommended, see below) and click OK,
- add libraries via Tools/Manage-libraries, filter on "wifi" and install WiFiManager by tablatronix version 0.16.0, filter on "telnet" and install ESP Telnet by Lennart Hennigs version 2.0.0,
- find library folder (I think this is in libraries within the folder mentioned as Sketchblook location in File/Preferences)
- close Arduino IDE
- add two more libraries manually by either "cd ~/Arduino/libraries ; git clone https://github.com/marvinroger/async-mqtt-client; git clone https://github.com/philbowles/ESPAsyncTCP" (or by downloading ZIP files from Github and unzipping these)
- add P1P2Serial code with examples by "cd ~/Arduino/libraries ; git clone https://github.com/Arnold-n/P1P2Serial" (or by downloading ZIP files from Github and unzipping these, but git clone enables git use to track changes)
- open Arduino IDE

The P1P2-ESP-Interface has 2 CPUs (ATmega328P and ESP8266) each of which requires its own firmware image (each CPU counts as a separate "board" in the IDE).

## Build and flash P1P2-bridge-esp8266 firmware image for ESP8266 on Arduino IDE

- edit P1P2Config.h based on your system and preferences
- select Tools/Board/ESP8266 Boards (3.0.2)/ESP8266 Generic
- select Tools/Flash Size/1MB (FS:no OTA: ~502kB) (NOTE: 4MB should be OK, may be needed in future, but ESP01 is only 1MB, and 4MB sometimes gave me issues)
- open P1P2-bridge-esp8266 code: go to File/Examples/ and under "Examples from Custom Libraries" select P1P2Serial/P1P2-bridge-esp8266
- if you edit code outside IDE (recommended!), you can modify the library examples, and you can use git tooling
- under Tools/Erase/Flash, select either default "only Sketch" or "All flash contents"; via OTA only the sketch can be flashed, but using the ESP01-compatible programmer you can also erase the WiFi settings if necessary
- select programming port: select a network port for OTA named P1P2MQTT_xxx, or "/dev/ttyUSB0" or something similar for the ESP01-programmer
- compile and flash image using "Upload" button (right arrow symbol)
- you may have to re-enter WiFi and MQTT settings using WifiManager
- if you want to keep the firmware image for reference, copy P1P2-bridge-esp8266.ino.bin from the most recent /tmp/arduino_build_xxx folder, and preferably rename it to include date/time/version/function in its name; you can compile without flasing using the "Verify" button (verify symbol)

Warning:
- if you make a folder copy of the source code, check which version you are editing, and which you are compiling/flashing. The Arduino IDE can be confusing as it only uses the folder name under File/Open Recent.

## Build P1P2-bridge-esp8266 firmware image for ESP8266 using Arduino-cli

- go to examples/P1P2-bridge-esp8266
- edit P1P2_Config.h based on your system and preferences
- Run "arduino-cli compile --fqbn esp8266:esp8266:generic:xtal=80,vt=flash,exception=disabled,stacksmash=disabled,ssl=all,mmu=3232,non32xfer=fast,ResetMethod=nodemcu,CrystalFreq=26,FlashFreq=40,FlashMode=dout,eesz=4M,led=2,sdk=nonosdk_190703,ip=lm2f,dbg=Disabled,lvl=None____,wipe=none,baud=115200 P1P2-bridge-esp8266.ino"
- Firmware image can be found in build/esp8266.esp8266.generic

## Build P1P2-bridge-esp8266 image for ESP8266 using platformio

- Not supported yet

## Flash pre-built P1P2-bridge-esp8266 firmware image on ESP8266 (OTA)

- Run "~/.arduino15/packages/esp8266/tools/python3/3.7.2-post1/python3 -I ~/.arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/espota.py -i 192.168.4.XXX -p 8266 --auth=P1P2MQTT -f P1P2-bridge-esp8266.ino.bin"

## Flash pre-built P1P2-bridge-esp8266 image on ESP8266 (USB, sketch only)

- Run "~/.arduino15/packages/esp8266/tools/python3/3.7.2-post1/python3 -I ~/.arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/upload.py --chip esp8266 --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset write_flash 0x0 P1P2-bridge-esp8266.ino.bin"

## Flash pre-built P1P2-bridge-esp8266 image on ESP8266 (USB, erase all flash including WiFi settings)

- Run "~/.arduino15/packages/esp8266/tools/python3/3.7.2-post1/python3 -I ~/.arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/upload.py --chip esp8266 --port /dev/ttyUSB0 --baud 115200 erase_flash --before default_reset --after hard_reset write_flash 0x0 P1P2-bridge-esp8266.ino.bin"

## Build and flash P1P2Monitor for ATmega328P on Arduino IDE

- select Tools/Board/MiniCore/ATmega328P
- select Tools/Clock/External 8MHz (this is essential otherwise ATmega will not work and will start crashing ESP8266, if so, remove jumper, reflash ATmega, add jumper) (this may have to be done every time you open Arduino IDE unless ATmega328P was default; to make "external 8MHz" the default, change order of menu items in ~/.arduino15/packages/MiniCore/hardware/avr/2.1.3/boards.txt: from line 987 onwards, the 7-line entry for "external 8MHz" should be listed first,
- other options should be OK as default: BOD2.7V, retain EEPROM, variant 328P, bootloader UART0
- open P1P2Monitor code go to File/Examples/ and under "Examples from Custom Libraries" select P1P2Serial/P1P2Monitor
- compile using "Verify" button (verify symbol)
- if you edit code outside IDE (recommended!), you can modify the library examples, and you can use git tooling
- if you edit within IDE, IDE asks you to save the modified example code to a different location
- I don't know yet how Arduino IDE can download code via ESP to ATmega, so do the following: while Arduino IDE is still open, find the most recent /tmp/arduino_build_xxx folder and copy firmware image /tmp/arduino_build_XX/P1P2Monitor.ino.hex to a permanent location, rename to include date/time/version/function in its name
- continue to "Flash P1P2Monitor image on ATmega328P (OTA)" below

## Flash P1P2Monitor image on ATmega328P (OTA)

- to flash P1P2Monitor it is necessary that P1P2-bridge-esp8266 is running and connected to WiFi
- flash image to ATmega using "avrdude -c avrisp -p atmega328p -P net:192.168.XXX.XXX:328 -e -Uflash:w:P1P2Monitor.ino.hex:i" (you may have to install avrdude or locate it in the ~/.arduino package tooling, for example in ~/.arduino15/packages/arduino/tools/avrdude/6.3.0-arduino18)

## For ATmega328PB

If avrdude does not recognize the ATmega328PB signature, add the option "-F" to overrule the signature.

## Issues?

After programming, WiFi and MQTT settings may have to be re-entered via the AP (SSID: P1P2MQTT).

If you encounter a non-working ATmega328P, possible causes are:
- forgot to re-install jumper
- wrong CPU frequency setting

If you encounter a non-working ESP, possible causes are:
- 512kB memory selected
- wrong serial speed of ATmega
- wrong clock frequency selection of P1P2Monitor
- (if you modify code) wrong printf argument list or other memory/stack violations
- network overload
- serial input overload
