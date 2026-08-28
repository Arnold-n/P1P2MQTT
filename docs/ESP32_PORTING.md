\# P1P2MQTT ESP32 Porting



\## 1. Goal



Replace the original ESP8266-based P1P2MQTT bridge with an ESP32-based

network/bridge controller while preserving the existing ATmega328P

P1P2Monitor controller.



The ESP32 is NOT replacing the ATmega328P as the P1/P2 protocol controller.



The architecture is therefore:



&#x20;   Network / MQTT / Home Assistant

&#x20;                |

&#x20;                v

&#x20;           +---------+

&#x20;           |  ESP32  |

&#x20;           | Bridge  |

&#x20;           +----+----+

&#x20;                |

&#x20;                | UART 250000 baud

&#x20;                |

&#x20;           +----v-----+

&#x20;           | ATmega328P|

&#x20;           | P1P2Monitor|

&#x20;           +----+-----+

&#x20;                |

&#x20;                |

&#x20;             P1/P2 bus

&#x20;                |

&#x20;                v

&#x20;            Daikin HVAC





\## 2. Design principle



The original P1P2MQTT architecture consists of two logical parts:



1\. ATmega328P

&#x20;  - P1/P2 bus communication

&#x20;  - P1P2Monitor

&#x20;  - P1/P2 protocol handling

&#x20;  - generation and processing of P1/P2 packets



2\. ESP8266

&#x20;  - WiFi / Ethernet connectivity

&#x20;  - MQTT

&#x20;  - Home Assistant integration

&#x20;  - Telnet / network interface

&#x20;  - configuration

&#x20;  - forwarding commands to/from ATmega

&#x20;  - interpretation of ATmega output where required



The ESP32 port replaces the ESP8266 bridge functionality.



The ATmega328P firmware and P1P2Monitor remain unchanged unless a

specific compatibility issue requires otherwise.





\## 3. Hardware architecture



\### 3.1 Original board UART mapping



The physical UART connections on the existing board have been verified.



UART0:



&#x20;   ESP8266 -> ATmega

&#x20;   RX = GPIO3

&#x20;   TX = GPIO1



UART2:



&#x20;   ATmega -> ESP8266

&#x20;   RX = GPIO17

&#x20;   TX = GPIO16



Serial speed:



&#x20;   250000 baud



Definitions used by the original bridge:



&#x20;   #define ENABLE\_UART0\_SNIFFER 1



&#x20;   #define UART0\_RX\_PIN 3

&#x20;   #define UART0\_TX\_PIN 1



&#x20;   #define UART2\_RX\_PIN 17

&#x20;   #define UART2\_TX\_PIN 16



&#x20;   #define UART\_BAUD 250000





\### 3.2 ESP32 target mapping



The ESP32 implementation must preserve the physical signal directions:



&#x20;   ESP32 TX -> ATmega RX

&#x20;   ESP32 RX <- ATmega TX



Target ESP32 UART:



&#x20;   RX = GPIO17

&#x20;   TX = GPIO16

&#x20;   baud = 250000



Therefore:



&#x20;   ESP32 GPIO16 (TX) -> ATmega serial input

&#x20;   ESP32 GPIO17 (RX) <- ATmega serial output



The GPIO16/GPIO17 UART connection is the primary

ESP32 <-> ATmega communication channel.





\## 4. Serial protocol



The ESP32 must preserve compatibility with the existing bridge serial

protocol.



The original bridge uses SERIAL\_MAGICSTRING when forwarding commands

to the ATmega.



Conceptually:



&#x20;   ESP32/network command

&#x20;            |

&#x20;            v

&#x20;      handleCommand()

&#x20;            |

&#x20;            v

&#x20;      SERIAL\_MAGICSTRING

&#x20;            +

&#x20;         command

&#x20;            |

&#x20;            v

&#x20;      UART 250000

&#x20;            |

&#x20;            v

&#x20;         ATmega





For a normal forwarded command the original bridge performs:



&#x20;   Serial.print(SERIAL\_MAGICSTRING);

&#x20;   Serial.println(cmdString);





This behaviour must be preserved by the ESP32 port.



The exact value of SERIAL\_MAGICSTRING must be taken from the existing

project source and must not be independently redefined.





\## 5. ATmega commands



The original bridge explicitly distinguishes ESP commands from ATmega

commands.



ATmega commands include:



&#x20;   A  reset ATmega

&#x20;   M  show brand/model/version

&#x20;   L  set control mode (Daikin)

&#x20;   C  set counter request mode (Daikin E-series)

&#x20;   E/F parameter write command (Daikin E/F-series)

&#x20;   W  raw packet write command

&#x20;   T  write delay

&#x20;   O  write timeout



Additional platform-specific commands may exist:



&#x20;   Z  Hitachi command

&#x20;   X  maximum pause between package bytes

&#x20;   E  error mask

&#x20;   M  MHI 3-byte/1-byte format conversion



The exact command set is conditional on the compiled series

configuration and must be preserved from the original project.





\## 6. ESP commands



The following commands are handled directly by the ESP bridge:



&#x20;   P  P1P2MQTT / ESP settings

&#x20;   J  outputMode / J-mask

&#x20;   V  system information

&#x20;   D  restart / reconnect / EEPROM / factory reset

&#x20;   H/? command help



The ESP implementation must not blindly forward commands that are

handled locally by the bridge.





\## 7. Command forwarding rule



The original bridge uses a switch-based command dispatcher.



Commands which are handled locally are processed by the ESP.



Commands which are not handled locally are forwarded to the ATmega:



&#x20;   SERIAL\_MAGICSTRING + cmdString + CR/LF





The special case:



&#x20;   V



is handled by both the bridge and P1P2Monitor.



Therefore the ESP32 implementation must preserve this behaviour.





\## 8. ATmega reset



ATmega reset is a hardware operation performed by the ESP bridge.



The original implementation:



1\. drives RESET\_PIN LOW

2\. waits briefly

3\. changes the pin state

4\. waits for ATmega startup

5\. sends serial synchronization/dummy data



The ESP32 implementation must retain the same logical sequence.



The actual ESP32 GPIO used for ATmega RESET must be determined from the

existing hardware definition before implementation.





\## 9. ATmega serial enable



The original hardware contains an ATMEGA\_SERIAL\_ENABLE control.



The bridge disables ATmega serial output during selected operations,

for example while reconnecting MQTT.



The original reconnect sequence is:



&#x20;   disable ATmega serial output

&#x20;   disconnect MQTT

&#x20;   wait until disconnected

&#x20;   re-enable ATmega serial output

&#x20;   reset serial parser state





The ESP32 port must preserve this behaviour.



The physical GPIO assigned to ATMEGA\_SERIAL\_ENABLE must be taken from

the existing hardware definitions.





\## 10. Serial input parser



The ESP32 must implement the existing ATmega -> ESP serial parser.



The original bridge contains state used for:



&#x20;   serial\_rb

&#x20;   ESP\_serial\_input\_Errors\_Data\_Short

&#x20;   ESP\_serial\_input\_Errors\_CS

&#x20;   ESP\_serial\_input\_Errors\_XOR

&#x20;   ESP\_serial\_input\_Errors\_CRC

&#x20;   ignoreRemainder

&#x20;   ATmega\_uptime\_prev



These variables indicate that the serial input is not simply a

line-oriented text stream.



The parser must therefore be ported as a dedicated component rather

than replaced with a generic Serial.readString() implementation.



Target component:



&#x20;   AtmegaSerial.cpp

&#x20;   AtmegaSerial.h





Responsibilities:



&#x20;   - receive bytes from ATmega

&#x20;   - maintain receive buffer

&#x20;   - detect complete messages

&#x20;   - validate checksums / CRC

&#x20;   - detect malformed messages

&#x20;   - maintain parser state

&#x20;   - forward valid P1P2Monitor output to the bridge layer

&#x20;   - preserve error counters

&#x20;   - handle synchronization after reset/reconnect





\## 11. Serial buffering



The ESP32 implementation should use a dedicated receive buffer.



Do NOT use blocking operations such as:



&#x20;   readString()

&#x20;   readStringUntil()

&#x20;   while (!Serial.available()) {}



The serial interface operates at:



&#x20;   250000 baud



and must not block MQTT, WiFi, OTA or web-server processing.



The preferred architecture is:



&#x20;   loop()

&#x20;      |

&#x20;      +-- processAtmegaSerial()

&#x20;      |

&#x20;      +-- processMQTT()

&#x20;      |

&#x20;      +-- processNetwork()

&#x20;      |

&#x20;      +-- processOTA()

&#x20;      |

&#x20;      +-- other non-blocking tasks





\## 12. UART implementation



Use an ESP32 HardwareSerial instance for the ATmega connection.



Conceptually:



&#x20;   HardwareSerial AtmegaSerial(2);



&#x20;   AtmegaSerial.begin(

&#x20;       UART\_BAUD,

&#x20;       SERIAL\_8N1,

&#x20;       UART2\_RX\_PIN,

&#x20;       UART2\_TX\_PIN

&#x20;   );



with:



&#x20;   UART2\_RX\_PIN = 17

&#x20;   UART2\_TX\_PIN = 16

&#x20;   UART\_BAUD = 250000





The exact UART instance and initialization should be isolated inside

AtmegaSerial.cpp.





\## 13. MQTT compatibility



The ESP32 port should preserve the existing MQTT interface.



The following should remain compatible wherever possible:



&#x20;   MQTT prefix

&#x20;   topic structure

&#x20;   retained messages

&#x20;   availability / online / offline state

&#x20;   Home Assistant discovery

&#x20;   command topics

&#x20;   parameter topics

&#x20;   raw P1P2 topics





The ESP32 port must not unnecessarily modify MQTT topic names merely

because the underlying MCU has changed.





\## 14. Home Assistant compatibility



Existing Home Assistant MQTT structures should remain compatible.



The ESP32 port should preserve:



&#x20;   entity names

&#x20;   MQTT topic structure

&#x20;   discovery data

&#x20;   state topics

&#x20;   command topics

&#x20;   availability handling





Any intentional breaking change must be documented separately.





\## 15. OutputMode



The original bridge exposes OutputMode as a bit mask.



Important bits include:



&#x20;   0x0001  raw packet data over MQTT

&#x20;   0x0002  individual MQTT parameter data

&#x20;   0x0004  pseudo packet data

&#x20;   0x0008  unknown parameters

&#x20;   0x0010  raw data over Telnet

&#x20;   0x0020  individual parameter data over Telnet

&#x20;   0x0040  timing information over Telnet

&#x20;   0x0100  non-HACONFIG parameters in P1P2/P/#

&#x20;   0x0800  restart HA-data communication after MQTT reconnect

&#x20;   0x1000  timing data over P1P2/R/xxx

&#x20;   0x2000  error data over P1P2/R/xxx

&#x20;   0x8000  ignore serial input from ATmega



The ESP32 implementation must preserve the semantics of these flags.





\## 16. OutputFilter



OutputFilter values:



&#x20;   0  all parameters

&#x20;   1  only changed data

&#x20;   2  changed data excluding temperature/flow

&#x20;   3  changed data excluding temperature/flow/time

&#x20;   4  changed data excluding temperature/flow/time/electricity/heat



These semantics must remain unchanged.





\## 17. Separation from upstream P1P2MQTT



The original Arnold Niessen P1P2MQTT library should remain as close

as possible to upstream.



Do not modify the upstream P1P2MQTT library merely to accommodate

ESP32-specific bridge functionality.



ESP32-specific functionality belongs in the bridge/application layer.



Current project structure uses:



&#x20;   lib/P1P2MQTT -> ../../P1P2MQTT



This keeps the upstream library outside the ESP32 project while making

it available to PlatformIO.





\## 18. Proposed ESP32 project structure



&#x20;   P1P2MQTT-ESP32/

&#x20;   |

&#x20;   +-- src/

&#x20;   |   |

&#x20;   |   +-- main.cpp

&#x20;   |   |

&#x20;   |   +-- AtmegaSerial.cpp

&#x20;   |   +-- AtmegaSerial.h

&#x20;   |   |

&#x20;   |   +-- P1P2Bridge.cpp

&#x20;   |   +-- P1P2Bridge.h

&#x20;   |   |

&#x20;   |   +-- Mqtt.cpp

&#x20;   |   +-- Mqtt.h

&#x20;   |   |

&#x20;   |   +-- Network.cpp

&#x20;   |   +-- Network.h

&#x20;   |   |

&#x20;   |   +-- Web.cpp

&#x20;   |   +-- Web.h

&#x20;   |

&#x20;   +-- lib/

&#x20;   |   |

&#x20;   |   +-- P1P2MQTT -> ../../P1P2MQTT

&#x20;   |

&#x20;   +-- docs/

&#x20;   |   |

&#x20;   |   +-- ESP32\_PORTING.md

&#x20;   |   +-- ATMEGA\_SERIAL\_PROTOCOL.md

&#x20;   |   +-- ARCHITECTURE.md

&#x20;   |

&#x20;   +-- platformio.ini





\## 19. Recommended implementation order



\### Phase 1 - Serial transport (DONE / operational)



Implement:



&#x20;   AtmegaSerial.h

&#x20;   AtmegaSerial.cpp



Requirements:



&#x20;   - UART2

&#x20;   - GPIO17 RX

&#x20;   - GPIO16 TX

&#x20;   - 250000 baud

&#x20;   - non-blocking receive

&#x20;   - transmit command

&#x20;   - receive raw bytes

&#x20;   - buffering

&#x20;   - reset/synchronization handling





\### Phase 2 - Command forwarding (prepared / testable)



Implement:



&#x20;   sendAtmegaCommand()



It must reproduce:



&#x20;   SERIAL\_MAGICSTRING + command + CR/LF





Test:



&#x20;   A

&#x20;   M

&#x20;   V

&#x20;   L

&#x20;   C

&#x20;   W

&#x20;   T

&#x20;   O





Do not yet implement MQTT.





\### Phase 3 - ATmega input parser (current receive path operational)



Port the existing parser.



Test:



&#x20;   normal messages

&#x20;   malformed messages

&#x20;   checksum errors

&#x20;   CRC errors

&#x20;   short packets

&#x20;   ATmega restart

&#x20;   serial resynchronization





\### Phase 4 - Arnold processing integration

This is the current development stage.

The ESP32 must reconstruct the Arnold bridge context required by `P1P2_ParameterConversion.h` rather than rewriting that implementation.

Target sequence:

- recreate the required Arnold globals, macros and helper functions
- compile `P1P2_ParameterConversion.h`
- validate `bytes2keyvalue()` and `bits2keyvalue()`
- reproduce `process_for_mqtt()`
- validate real E-Series traffic first, then preserve support for all Arnold-configured series

The existing UART transport/parser must not be regressed while doing this.

The ATmega328P/P1P2Monitor remains unchanged and remains responsible for the physical P1/P2 bus.

### Phase 5 - MQTT



Port:



&#x20;   MQTT connection

&#x20;   MQTT reconnect

&#x20;   online/offline status

&#x20;   subscriptions

&#x20;   command forwarding

&#x20;   P1P2 output publishing





\### Phase 6 - Home Assistant



Port:



&#x20;   discovery

&#x20;   entities

&#x20;   state publishing

&#x20;   command topics

&#x20;   availability





\### Phase 7 - Web / configuration



Port only the functionality actually required.



Do not copy the entire ESP8266 bridge blindly.





\### Phase 8 - OTA / production hardening



Add OTA after the basic bridge is stable.



OTA must not interfere with the 250000 baud ATmega serial processing.





\## 20. Testing strategy



Testing must be performed in layers.



\### Test 1 - ESP32 UART TX



Send:



&#x20;   A



and verify that ATmega receives the expected serial frame.





\### Test 2 - ESP32 UART RX



Capture ATmega output and verify that the ESP32 receives bytes without

loss at 250000 baud.





\### Test 3 - Full command path



&#x20;   MQTT

&#x20;     |

&#x20;     v

&#x20;   ESP32

&#x20;     |

&#x20;     v

&#x20;   UART

&#x20;     |

&#x20;     v

&#x20;   ATmega

&#x20;     |

&#x20;     v

&#x20;   P1/P2





\### Test 4 - Full response path



&#x20;   P1/P2

&#x20;     |

&#x20;     v

&#x20;   ATmega

&#x20;     |

&#x20;     v

&#x20;   UART

&#x20;     |

&#x20;     v

&#x20;   ESP32

&#x20;     |

&#x20;     v

&#x20;   MQTT

&#x20;     |

&#x20;     v

&#x20;   Home Assistant





\### Test 5 - Stress



Verify simultaneous:



&#x20;   P1/P2 traffic

&#x20;   MQTT traffic

&#x20;   WiFi traffic

&#x20;   Home Assistant updates

&#x20;   serial traffic





No blocking operation should cause serial data loss.





\## 21. Compatibility rule



The primary goal is:



&#x20;   ESP32 should behave like the original ESP8266 bridge

&#x20;   from the perspective of the ATmega and MQTT/Home Assistant.



The MCU change should be transparent to:



&#x20;   ATmega328P

&#x20;   P1P2Monitor

&#x20;   P1/P2 bus

&#x20;   MQTT consumers

&#x20;   Home Assistant





\## 22. Known facts



The following are confirmed and must be treated as fixed design

parameters:



&#x20;   ATmega controller: ATmega328P

&#x20;   P1P2 controller software: P1P2Monitor

&#x20;   Original network MCU: ESP8266

&#x20;   Target network MCU: ESP32



&#x20;   UART baud rate: 250000



&#x20;   Original board UART0:

&#x20;       RX GPIO3

&#x20;       TX GPIO1



&#x20;   External ATmega UART:

&#x20;       RX GPIO17

&#x20;       TX GPIO16



&#x20;   ESP32 ATmega UART:

&#x20;       RX GPIO17

&#x20;       TX GPIO16



&#x20;   ATmega command forwarding uses:

&#x20;       SERIAL\_MAGICSTRING



&#x20;   Normal forwarded command format:

&#x20;       SERIAL\_MAGICSTRING + command + CR/LF





\## 23. Important non-goals



This project does NOT initially attempt to:



&#x20;   - rewrite P1P2Monitor

&#x20;   - replace the ATmega with ESP32 P1/P2 processing

&#x20;   - redesign the P1/P2 protocol

&#x20;   - redesign MQTT topics

&#x20;   - redesign Home Assistant entities

&#x20;   - rewrite the upstream P1P2MQTT library

&#x20;   - introduce a new proprietary serial protocol





The first objective is a faithful ESP8266 -> ESP32 bridge replacement.





\## 24. Current status

**Status as of 2026-08-14**

### Build milestone — DONE

- [x] ESP32 target platform
- [x] M5Stack PoESP32 Unit (U138) selected
- [x] ATmega328P remains P1/P2 controller
- [x] ATmega328P firmware remains unchanged
- [x] UART speed = 250000
- [x] UART2 RX = GPIO17
- [x] UART2 TX = GPIO16
- [x] UART 8N1
- [x] `SERIAL_MAGICSTRING` mechanism identified
- [x] `AtmegaSerial` transport implemented
- [x] `AtmegaProtocol` transport/command layer implemented
- [x] Ethernet startup implemented
- [x] HTTP server implemented
- [x] Web Serial implemented
- [x] Web firmware update endpoint implemented
- [x] ArduinoOTA support implemented
- [x] PlatformIO build succeeds
- [x] Build memory checked: approximately 20.9% RAM and 81.3% flash
- [x] Web Serial buffers reduced to 4 KB per UART

### Current physical test configuration

Receive-side validation is intentionally isolated from ATmega command transmission.

```text
ATmega TX  ---------------->  ESP32 UART2 RX / GPIO17
          |
          +---------------->  ESP32 UART0 RX / GPIO3
                             temporary receive-only sniffer

ESP32 UART2 TX / GPIO16  -X->  ATmega RX
                             disconnected
```

UART0 is a temporary sniffer only and is not part of the final hardware design.

### Runtime validation — CURRENT

- [ ] Boot current firmware on the real U138
- [ ] Confirm Ethernet link
- [ ] Confirm DHCP/IP/hostname
- [ ] Confirm Web UI
- [ ] Confirm `/serial`
- [ ] Confirm UART2 receives ATmega traffic
- [ ] Compare UART0 sniffer and UART2 receive streams
- [ ] Verify sustained reception at 250000 baud
- [ ] Verify receive-buffer overflow behaviour
- [ ] Verify parser synchronization
- [ ] Verify checksum/CRC/error handling
- [ ] Verify ATmega restart detection

### Command transmission — NOT YET VALIDATED

- [ ] Reconnect UART2 TX -> ATmega RX
- [ ] Verify ESP32 -> ATmega `1P2P` framing
- [ ] Test non-destructive/status commands
- [ ] Verify ATmega response path
- [ ] Test control/write commands only after the read path is proven

### Arnold processing integration — IN PROGRESS

- [ ] Recreate required Arnold globals/macros/helpers
- [ ] Validate `bytes2keyvalue()`
- [ ] Validate `bits2keyvalue()`
- [ ] Reproduce `process_for_mqtt()`
- [ ] Validate real E-Series traffic
- [ ] Preserve other configured series

### MQTT / Home Assistant — NOT COMPLETE

- [ ] MQTT connection/reconnect
- [ ] Online/offline state
- [ ] Subscriptions
- [ ] Command forwarding
- [ ] P1P2 data publishing
- [ ] Home Assistant discovery
- [ ] Home Assistant state/command compatibility

### Production hardening — NOT COMPLETE

- [ ] Remove/disable UART0 sniffer
- [ ] Protect or disable unauthenticated web firmware upload
- [ ] Verify OTA partition layout
- [ ] Verify OTA recovery/rollback behaviour
- [ ] Verify watchdog/restart behaviour
- [ ] Long-duration 250000-baud serial stress test
- [ ] Network/MQTT stress test
- [ ] Document recovery procedure

### OTA policy during development

The project supports both ArduinoOTA and a web firmware upload endpoint (`/update`). The web update endpoint currently has no authentication and must only be exposed on a trusted network.

A remote OTA upload of the current build is **not low-risk** because the physical U138 is inaccessible and the current build has not yet been runtime-validated on that exact device. A failed remote OTA can leave the device unreachable until physical recovery is possible.

Preferred sequence:

1. keep the known-good firmware binary;
2. verify the target device is reachable;
3. perform the smallest controlled OTA test possible;
4. immediately verify Ethernet, Web UI and UART2 reception;
5. do not connect ESP32 TX to ATmega RX until receive-side validation is complete.

### What the next tests prove

The first remote test is valuable even without UART TX. It will validate:

```text
U138 boot
  |
  +--> ESP32 application starts
  +--> Ethernet + DHCP
  +--> HTTP server
  +--> Web Serial
  +--> UART2 RX @ 250000
  +--> ATmega traffic reaches ESP32
  +--> UART0 sniffer matches UART2 receive data
  +--> receive buffering does not lose data
```

These tests do **not** prove ESP32 -> ATmega transmission, command forwarding, write/control operations, MQTT end-to-end processing, Home Assistant operation, or complete production OTA recovery.

## 25. Next immediate task

The next milestone is **runtime receive-side validation on the real U138**. Do not start with ESP32 -> ATmega writes. First verify boot, Ethernet, Web UI, UART2 RX, UART0/UART2 comparison, sustained serial reception, and parser stability.

Only after these are confirmed should UART2 TX be connected to the ATmega RX path and command forwarding be tested.

The ATmega328P/P1P2Monitor remains unchanged throughout this phase.

## 26. Current release position

This repository is **development firmware, not a production release**. The successful PlatformIO build is an important porting milestone, but the absence of physical access to the target U138 means runtime verification remains outstanding.

The next evidence required for a production-quality milestone is a successful remote boot followed by verified Ethernet and ATmega UART2 receive operation at 250000 baud.


---

## 27. Updated project status — 2026-08-19

The project has moved beyond the initial remote/runtime validation stage.

### Confirmed operational

- [x] ESP32 firmware builds successfully with PlatformIO
- [x] M5Stack PoESP32 Unit (U138) is running the ESP32 firmware
- [x] Ethernet link is operational
- [x] Web UI / Web Serial are operational
- [x] ATmega traffic is received by ESP32 UART2 at 250000 baud
- [x] MQTT connection is operational
- [x] MQTT configuration is stored in ESP32 NVS
- [x] MQTT IP/port changes through the Web UI take effect without an ESP32 restart
- [x] Restart + NVS persistence has been tested successfully
- [x] MQTT state publishing is operational
- [x] The ESP32 can report MQTT connection state and publish statistics
- [x] The MQTT server was changed during runtime testing and the ESP32 reconnected to the new server

The current firmware therefore has a functioning network/MQTT bridge foundation.

### Important observed compatibility issue

A controlled DHW ON/OFF test exposed a significant difference between the
Arnold bridge and the ESP32 port.

Arnold exposes the DHW state as the expected DHW object/topic/value,
while the ESP32 currently maps the corresponding change incorrectly
(e.g. into a BUH1_Q... object).

The raw P1/P2 traffic confirms that the physical ON/OFF command is present
and that the relevant E3500 parameter changes between:

```text
1P2PE3500401    -> ON
1P2PE3500400    -> OFF
```

Therefore the current problem is considered a **compatibility-layer
mapping/context problem**, not evidence that the underlying Arnold
`P1P2_ParameterConversion.h` implementation should be rewritten.

`P1P2_ParameterConversion.h` remains the upstream protocol conversion
source of truth and must not be modified merely to fix this ESP32 port.

---

## 28. Arnold state lifecycle compatibility

A detailed review of the Arnold implementation established the following
important rule:

**`resetDataStructures()` must not be called unconditionally on every
ESP32 startup.**

The ESP32 compatibility layer must reproduce the Arnold lifecycle:

```text
ESP32 startup
     |
     v
restore/load persisted state
     |
     +-----------------------------+
     |                             |
  state valid                 state missing /
     |                       invalid / incompatible
     v                             |
normal operation                   v
                            resetDataStructures()
                                    +
                              initDataRTC()
```

The reset path is also used for an explicit reset request.

In particular, Arnold's `D11` reset path uses:

```cpp
initDataRTC();
resetDataStructures();
```

Factory-reset handling can enter the same reset path.

The compatibility layer must therefore distinguish:

- normal boot with valid/restorable state;
- first initialization;
- invalid/incompatible state recovery;
- explicit reset;
- normal runtime operation.

It must **not** execute:

```cpp
resetDataStructures();
```

as an unconditional part of normal startup.

This lifecycle is now a hard compatibility requirement.

---

## 29. Compatibility-layer scope

The compatibility layer is responsible for reproducing the Arnold runtime
context needed by the upstream conversion/processing code.

The target architecture is:

```text
ATmega / P1P2Monitor
        |
        v
ESP32 UART2 RX
        |
        v
AtmegaSerial / parser
        |
        v
P1P2Processor
        |
        v
P1P2_Compat
        |
        v
Arnold conversion / processing
        |
        +--> MQTT / HA / Web
```

The compatibility layer must reproduce Arnold semantics for:

- required global state;
- parameter context;
- data structures;
- initialization;
- state restoration;
- reset handling;
- parameter conversion;
- MQTT publication mapping;
- command/write context.

The goal is not to create a new ESP32-specific interpretation of the
P1/P2 protocol.

---

## 30. Read path — current priority

The immediate read-side task is to make the ESP32 output semantically
match Arnold.

The validation must compare the two bridges using identical live P1/P2
traffic:

```text
Arnold
  10.192.160.17
       |
       +--> MQTT topics/data

ESP32
  10.192.160.9
       |
       +--> MQTT topics/data
```

The comparison must cover:

- topic hierarchy;
- topic names;
- parameter identifiers;
- decoded values;
- ON/OFF states;
- data types;
- retained state;
- timestamps/time fields where applicable;
- output filtering;
- HA/non-HA parameter placement.

The DHW ON/OFF case is the first known regression and should be used as a
reference test while correcting the compatibility context.

The correct sequence is:

```text
raw E-series frame
      |
      v
same parameter/address interpretation
      |
      v
same Arnold conversion
      |
      v
same MQTT topic/value
```

Do not fix the symptom by renaming a wrong MQTT topic after conversion.
The parameter/context mapping must be corrected at the compatibility layer.

---

## 31. Write path — now part of the required scope

The project must support correct writing in both directions.

### Direct ESP32 -> ATmega

```text
ESP32 local command
        |
        v
command dispatcher
        |
        v
Arnold-compatible command semantics
        |
        v
SERIAL_MAGICSTRING + command + CR/LF
        |
        v
UART2 TX / GPIO16
        |
        v
ATmega RX
```

### MQTT -> ESP32 -> ATmega

```text
MQTT command
        |
        v
ESP32 MQTT subscription
        |
        v
validation / translation
        |
        v
same Arnold-compatible command path
        |
        v
SERIAL_MAGICSTRING + command + CR/LF
        |
        v
ATmega
```

Both write paths must converge on the same command implementation.

MQTT must not introduce a second proprietary command encoding.

### Write validation order

1. Use a non-destructive/status command.
2. Verify the exact serial bytes sent to ATmega.
3. Verify ATmega response.
4. Verify the response is parsed correctly by ESP32.
5. Verify the MQTT-originated command uses the same path.
6. Only after this is proven, test controlled parameter writes.
7. Use DHW ON/OFF as a controlled end-to-end write test.
8. Verify resulting MQTT state against Arnold.

Destructive or operationally significant writes must not be the first TX
test.

---

## 32. Updated implementation phases

### Phase 1 — Serial transport

**DONE / operational**

- UART2
- GPIO17 RX
- GPIO16 TX
- 250000 baud
- non-blocking receive
- receive buffering
- ATmega serial processing

### Phase 2 — Command forwarding

**IMPLEMENTED / requires final runtime validation**

The forwarding mechanism preserves:

```text
SERIAL_MAGICSTRING + command + CR/LF
```

Final runtime TX validation remains required.

### Phase 3 — ATmega input/parser

**OPERATIONAL**

The ESP32 receives and processes live ATmega traffic.

### Phase 4 — Arnold processing integration

**CURRENT DEVELOPMENT PHASE**

Priority:

1. correct Arnold lifecycle/state handling;
2. correct compatibility context;
3. correct E-series parameter mapping;
4. verify DHW and other representative parameters;
5. compare ESP32 MQTT output against Arnold.

Do not modify `P1P2_ParameterConversion.h`.

### Phase 5 — MQTT

**OPERATIONAL FOUNDATION / COMPATIBILITY VALIDATION IN PROGRESS**

Confirmed:

- connection;
- reconnect;
- configuration;
- NVS persistence;
- runtime server change;
- publishing.

Remaining:

- complete topic parity with Arnold;
- complete value/mapping parity;
- command subscriptions;
- end-to-end command/write validation.

### Phase 6 — Home Assistant

**PARTIALLY OPERATIONAL / COMPATIBILITY VALIDATION IN PROGRESS**

The current MQTT output is visible, but the Arnold-vs-ESP32 topic/value
difference must be eliminated before declaring full HA compatibility.

### Phase 7 — Web/configuration

**OPERATIONAL**

MQTT configuration includes persistent:

- enabled state;
- server;
- port;
- user;
- password;
- client configuration as applicable.

Leaving the password field empty must preserve the stored password rather
than erase it; this behaviour should remain covered by regression testing.

### Phase 8 — OTA / production hardening

**NOT COMPLETE**

Remaining:

- OTA recovery validation;
- watchdog/restart validation;
- long-duration serial stress;
- MQTT/network stress;
- final security review;
- final production recovery procedure.

---

## 33. Updated testing strategy

The next testing sequence is:

```text
1. Arnold vs ESP32 topic-tree comparison
              |
              v
2. Representative E-series parameter comparison
              |
              v
3. Fix compatibility-layer context/mapping
              |
              v
4. Repeat DHW ON/OFF read test
              |
              v
5. Direct ESP32 -> ATmega non-destructive TX
              |
              v
6. MQTT -> ESP32 -> ATmega TX
              |
              v
7. Controlled DHW write test
              |
              v
8. Compare resulting MQTT state with Arnold
              |
              v
9. Stress / reconnect / restart / NVS tests
```

The comparison should be performed with both bridges operating against the
same physical P1/P2 behaviour wherever practical.

---

## 34. Compatibility rules — updated

The following rules are now mandatory:

1. **Arnold remains the protocol source of truth.**
2. Do not modify `P1P2_ParameterConversion.h` to solve ESP32-specific
   mapping issues.
3. Do not call `resetDataStructures()` on every normal ESP32 boot.
4. Preserve Arnold's first/invalid/incompatible-state reset semantics.
5. Preserve explicit D11/factory-reset behaviour.
6. Preserve `SERIAL_MAGICSTRING` for ATmega command forwarding.
7. Direct ESP32 writes and MQTT-originated writes must use the same command
   path.
8. Do not solve wrong parameter mapping by post-processing MQTT topic names.
9. Validate raw frame -> parameter -> value -> topic as one compatibility
   chain.
10. Do not introduce blocking operations into the 250000-baud serial path.
11. Do not declare full compatibility based only on successful compilation.
12. Every compatibility correction must be verified against Arnold.

---

## 35. Current milestone

**Milestone: ESP32 bridge foundation operational; Arnold compatibility
correction in progress.**

The project has successfully crossed the basic hardware/network/MQTT
integration milestone.

The main remaining technical risk is no longer basic ESP32 connectivity.
It is **faithful reproduction of Arnold's processing context and mapping**.

The first concrete regression is DHW ON/OFF mapping. The next development
step is therefore to inspect and correct `P1P2_Compat.*` and its interaction
with Arnold processing/state, then repeat the Arnold-vs-ESP32 comparison.

After the read path is correct, write support must be validated in both
directions:

```text
ESP32 -> ATmega
MQTT -> ESP32 -> ATmega
```

Only then should the project move to broad series coverage and final
production hardening.

---

## 36. Observed Arnold MQTT topic tree — 2026-08-19

A live comparison of the working Arnold bridge identified an important
additional part of the MQTT compatibility contract.

Arnold exposes these parallel top-level branches:

```text
P1P2/
├── Z/P1P2MQTT/bridge0 = 10.192.160.56
├── L/P1P2MQTT/bridge0 = online
├── M/P1P2MQTT/bridge0/
│   ├── A ... L = long raw hexadecimal payloads
├── P/P1P2MQTT/bridge0/
│   ├── S
│   ├── A
│   ├── C
│   ├── M
│   ├── T
│   └── F
├── S/P1P2MQTT/bridge0 = * [ESP] <timestamp> Uptime <seconds>
└── W/P1P2MQTT/bridge0 = <write/command payload>
```

Observed branch roles:

| Branch | Observed content | Working interpretation |
|---|---|---|
| `Z` | IP address | bridge/network identification |
| `L` | `online` | availability/status |
| `M` | raw hexadecimal `A`–`L` data | raw P1P2 representation |
| `P` | parsed parameter groups | processed P1P2 data |
| `S` | ESP diagnostic/uptime text | bridge status/diagnostics |
| `W` | write payload | command/write path |

The current ESP32 tree is primarily:

```text
ESP32P1P2/
└── P/P1P2MQTT/bridge0/
    ├── S
    ├── M
    ├── T
    └── C
```

Thus the ESP32 port currently does not reproduce Arnold's parallel
`Z`, `L`, `M`, `S` and `W` branches at the same root level.

### Required investigation before implementation

Inspect Arnold's actual source and runtime behaviour to establish:

1. publication code for `Z`, `L`, `M`, `P`, `S` and `W`;
2. retain/QoS behaviour;
3. availability/LWT handling;
4. exact payload formatting;
5. publication triggers/timing;
6. generation of raw `M/A`–`M/L` data;
7. parsing/handling of `W`;
8. interaction with `outputMode`;
9. whether the `P1P2` root should eventually replace the current
   development prefix `ESP32P1P2`.

This section records an observed compatibility requirement only.
**Do not change the code solely because of this documentation update.**

The existing `P` processing path must remain stable while the additional
MQTT contract is reverse-engineered.

### Updated investigation order

```text
Arnold MQTT tree
      |
      v
Z / L / M / P / S / W semantics
      |
      v
Arnold vs ESP32 comparison
      |
      v
compatibility-layer correction
      |
      v
E-Series read validation
      |
      v
direct ESP32 -> ATmega TX
      |
      v
MQTT -> ESP32 -> ATmega TX
```

No MQTT topic-tree implementation change is implied by this checkpoint.

---

## 37. MQTT lifecycle, HA Discovery and parser fixes — 2026-08-20 (continued)

This section documents the root causes found and fixed for the three
issues tracked as open in sections 27 and 36, plus two items discovered
during live testing that were not previously tracked (the MQTT reconnect
behaviour and a parser-level bug).

### 37.1 MQTT never auto-connected after boot/restart

**Symptom:** after every ESP32 boot or restart, `status.mqtt.connected`
stayed `false` (state `-1`, `DISCONNECTED`) despite `connect_attempts`
incrementing every ~5s, even though Ethernet already had a valid IP well
before the first attempt. The only way to connect was to open `/mqtt` and
click "Save & Reconnect".

**Root cause:** `Esp32Mqtt::connectMqtt()` (called from the main loop's
retry timer) only called a bare `mqtt.connect()`. `Esp32Mqtt::reconnect()`
(the code path behind "Save & Reconnect") additionally called
`mqtt.disconnect()` and fully reconfigured the client
(`setServer`/`setClientId`/`setKeepAlive`/`setWill`/`setCredentials`)
*before* calling `connect()`. Empirically, `AsyncMQTT_ESP32`'s client
needs this teardown+reconfigure sequence to succeed reliably after a fresh
boot; a bare `connect()` on an unconfigured/stale client object does not
recover on its own.

**Fix:** extracted the reconfigure block into `configureConnection()`,
called from both `begin()` (once, at startup) and `connectMqtt()` (every
attempt, preceded by `mqtt.disconnect()`). Reconfiguring on every attempt
is cheap (the setters do no network I/O; only `connect()` does).

**Also added:** `onMqttDisconnect()` now decodes and logs the actual
`AsyncMqttClientDisconnectReason` (`TCP_DISCONNECTED`,
`MQTT_NOT_AUTHORIZED`, etc.) instead of discarding it with `(void)reason`,
so a future connection failure can be diagnosed from the log instead of
guessed at.

### 37.2 HA Discovery — buttons and several switches never created

Three independent root causes stacked on top of each other; all three
had to be fixed for buttons to appear.

**(a) `writePseudoPacket()` was a no-op.** The port's implementation only
printed a trace line under `P1P2_COMPAT_TRACE`; it never fed the pseudo
packet back into the decoder. Upstream's real implementation computes a
CRC over the pseudo packet's bytes, appends it, optionally publishes raw
hex, and then calls `process_for_mqtt(WB, rh)` — the *same* function used
for real bus packets. Since `createButtonsSwitches1()`/`2()` (all ~17
buttons) and 3 switches (`Altherma_On`, `DHW_Boost`, `HA_Setup`) in
`P1P2_Pseudo.h` only run as a side effect of that traversal reaching
specific `payloadIndex` cases inside pseudo packet types `0x0B`–`0x0F`,
a no-op `writePseudoPacket()` meant none of them ever ran.

Fix: extracted the real-packet decode loop (previously inline in
`P1P2Compat_process()`) into a shared, forward-declared
`static void processBusPacket(const uint8_t* rb, uint8_t n)`. Rewrote
`writePseudoPacket()` to compute/append the CRC (same E/F/W-series
CRC_GEN/CRC_FEED algorithm as `P1P2Parser::crcEseries()`), optionally
publish raw hex under `outputMode & 0x0004`, and call
`processBusPacket(WB, rh + 1)` when `outputMode & 0x0022` is set (and
`!mqttDeleting`, a placeholder always `false` today since "delete all HA
configs" isn't ported).

**(b) `pseudo0E` didn't exist.** Upstream increments `pseudo0B`,
`pseudo0C`, `pseudo0D`, `pseudo0E` and `pseudo0F` once per second in its
main loop, and once each exceeds 5, resets to 0 and emits the
corresponding packet. `pseudo0E` builds packet type `0x0E`, `packetSrc
0x40` (ESP) inline (there is no separate `writePseudoSystemPacket0E()`
function upstream — it's inlined in the main `.ino` loop). This port only
had `pseudo0B/0C/0D/0F`; `pseudo0E` — and therefore payload byte 11 of
that packet, which Arnold's own comment labels *"dummy for switches and
buttons"* and which is the actual trigger for `createButtonsSwitches1()`/
`2()` — never existed.

Fix: added `byte pseudo0E` (declared in `P1P2_Compat.h`, defined in
`P1P2_Compat.cpp`), its periodic (~1s, gated on `espUptime` changing)
increment/threshold-reset in `P1P2Compat_tick()`, and the packet-0x0E
construction (`SW_*_VERSION`/reboot-diagnostics fields zeroed —
cosmetic only, don't affect entity creation — `EE.outputMode`,
`EE.outputFilter`, `EE.ESPhwID`, and the E-Series `R*Toffset` fields
copied verbatim from `EE`), followed by `writePseudoPacket(readHex, 20)`
(E-Series) / `(..., 15)` (otherwise).

**(c) `P1P2Parser::parseRLine()` only understood one of two line
formats.** This is the deepest and most consequential of the three: it
affected not just ESP32-generated pseudo packets, but *real ATmega
output*. P1P2Monitor emits two line shapes on its serial output:

```text
R T xxx.xxx: <hex...>     -- real bus traffic, has a relative timestamp
R P         <hex...>      -- pseudo-packet / internal state, no timestamp,
                             just padding so the hex lines up at the same
                             column as the "R T" case
```

`parseRLine()` located the hex payload via `strchr(line, ':')`; for `R P`
lines there is no colon at all, so `strchr` returned `NULL` and the
function returned `false` — the entire line was discarded as an
unparseable/error line. This silently dropped **every** `R P` line,
including ATmega's own periodic `0x0E`/`0x0F` status packets (the ones
carrying `Control_Function`, `Counter_Request_Function`, ATmega version,
`Control_ID`, etc. — confirmed present and repeating in the raw UART0
capture, so the data was arriving; it was being thrown away downstream of
the UART, inside the line parser).

Fix: `parseRLine()` now branches on whether a colon is present. If yes,
unchanged colon-based extraction (`R T` case). If no, skip `'R'`, any
whitespace, the type letter (`'P'`), then whitespace again, to reach the
same hex payload start (`R P` case). Both branches converge on the
existing hex/CRC parsing code below, unchanged.

**Net effect:** with (a)+(b)+(c) fixed, HA now discovers the full button
set, the pseudo-packet-driven switches, and — via the parser fix — the
switches/sensors that live in the ATmega's own `0x0E`/`0x0F` src-`0x00`
status packets (`Control_Function`, `Counter_Request_Function`, and
associated diagnostics), which no ESP-side pseudo-packet fix could ever
have surfaced on its own.

### 37.3 ESP32 restart wired end-to-end

`P1P2Compat_restartEsp()` (new): publishes `offline` on the availability
topic (mirroring Arnold's `D0` handling — HA doesn't have to wait out the
LWT/keepalive timeout), then `ESP.restart()`. Called from:

- `Mqtt.cpp`'s `onMqttMessage()`, when the write-topic payload is exactly
  `"D0"` — intercepted *before* the generic write-topic forward to
  `AtmegaProtocol::sendCommand()`, since `D0` (like all `D#`/`L#` codes) is
  an ESP-local command upstream handles directly on the ESP8266 and never
  sends to the ATmega.
- `WebManager.cpp`'s new `handleRestart()`, bound to `POST /api/restart`,
  with a "Restart ESP32" button (JS `confirm()` guard) added to the main
  page.

Other `D#`/`L#` codes are not yet implemented; they currently fall through
to `AtmegaProtocol::sendCommand()` unchanged (harmless — P1P2Monitor
ignores commands it doesn't recognize) and are tracked as open work.
ATmega hard-reset (`A`/`a`, a `RESET_PIN` GPIO toggle upstream) is not
implemented; this hardware has no such pin broken out.

### 37.4 MQTT write path verified

`onMqttMessage()` forwards any write-topic payload that isn't an
ESP-local command to `AtmegaProtocol::sendCommand()`, which prefixes
`SERIAL_MAGICSTRING` (`1P2P`) and writes to UART2 TX via
`AtmegaSerial::sendCommand()`. Verified on Web Serial's UART2 trace:
publishing `E3500401` / `E3500400` to the write topic produced
`[TX] 1P2PE3500401` / `[TX] 1P2PE3500400` — correctly framed. Physical
UART2 TX → ATmega RX is not yet connected (see open items below), so the
ATmega has not yet acted on a command from this path.

### 37.5 System log channel added

A new, independent web-visible log (`logPrintf()` in `WebSerial.cpp/h`,
backed by an 8 KB ring buffer, same shape as the UART0/UART2 channels but
storing pre-formatted, timestamped text lines instead of raw bus bytes)
was added and wired into `main.cpp`'s boot sequence and the MQTT/restart
log lines. Exposed as a third panel ("System Log") on `/serial`, polled the
same way as the UART0/UART2 panels, via an extended `/api/serial/data`
response (`totalLog`/`overflowLog`/`dataLog`). This removes the need for a
USB serial console for routine diagnostics.

### 37.6 Firmware versioning

`FW_VERSION` bumped to `1.0.1`; `FW_AUTHOR` (`JackyKNX`) added and
surfaced on the main page, the firmware-update page, the `/api/status`
JSON, and the boot log line.

### 37.7 Open items after this round

- Full `D#`/`L#` ESP-local command table — only `D0` implemented.
- ATmega hard-reset (`A`/`a`) — needs a `RESET_PIN` GPIO not present on
  this hardware.
- Physical UART2 TX (GPIO16) → ATmega RX wiring — not yet connected. The
  ESP8266 soldered to the same board must be reflashed with a
  passive/inert firmware first (two active transmitters must not drive
  the ATmega RX line at the same time).
- Full write-command coverage beyond the one verified DHW on/off test.
- Full Arnold MQTT topic-tree parity (`Z`/`L`/`M`/`S`/`W` root branches,
  section 36) — this port's `M` is a custom full-state-snapshot dump, not
  Arnold's per-register raw-fallback mechanism; both now work
  independently (the pseudo-packet loopback fix in 37.2(a) applies to
  either), but they are not the same mechanism. Revisit if per-register
  `M/0/xx`-style raw topics are specifically required.

---

## 38. Live hardware validation — 2026-08-20/21

Section 37's fixes have now been exercised on the real, physically-wired
board (M5Stack U138 + live ATmega + live heat pump), not just reviewed in
code. This section records what changed physically and what that proved.

### 38.1 ESP8266 decommissioning and the `ATMEGA_SERIAL_ENABLE` pitfall

The ESP8266 (previously running Arnold's `P1P2-bridge-esp8266`) was
reflashed with a new, minimal, standalone "parked" firmware
(`P1P2-ESP8266-PARKED`, own repo/project) before physically connecting
ESP32's UART2 TX (GPIO16) to the ATmega's RX pin -- two live UART
transmitters on one line is not safe.

The parked firmware:
- Never calls `Serial.begin()`; GPIO1(TX)/GPIO3(RX) are left completely
  untouched, so they can never drive against ESP32's lines.
- Provides WiFi (hardcoded SSID/password), a small status page, a
  password-protected `/update` (`ESP8266HTTPUpdateServer`), ArduinoOTA,
  and its own web-visible log (same ring-buffer pattern as this project's
  `WebSerial.cpp`, independently reimplemented since it's a separate
  Arduino-core target).

**Pitfall found during bring-up:** parking the ESP8266 this way silenced
the ATmega entirely (zero bytes on both UART0 and UART2 RX). Root cause,
found in `P1P2MQTT-bridge.ino`:

```cpp
#define ATMEGA_SERIAL_ENABLE 15 // required for v1.2
...
digitalWrite(ATMEGA_SERIAL_ENABLE, HIGH);
pinMode(ATMEGA_SERIAL_ENABLE, OUTPUT);
// comment in source: "Allow ATmega to enable serial input/output"
```

This is a *separate* control line (ESP8266 GPIO15 -> ATmega PD4), not
part of the UART TX/RX data path, that Arnold's original firmware asserts
HIGH so P1P2Monitor will enable its own serial I/O at all. Fix: the parked
firmware now also drives GPIO15 HIGH at boot -- the only P1P2-related
thing it does. With that added, the ATmega resumed transmitting normally.

Also relevant, found in the same source file:

```cpp
#define RESET_PIN 5 // GPIO_5 on ESP-12F pin 20, connected to ATmega328P's reset line
```

This is the line behind Arnold's `'A'` hard-reset command (ESP toggles
this GPIO, which is wired to the ATmega's physical RESET pin). The ESP32
has no equivalent wiring on this board -- see 38.4 for how this was
worked around instead of replicated.

### 38.2 Physical UART re-wiring: UART2 becomes the live full-duplex link

Board re-wired:
- ATmega TX -> ESP32 GPIO17 (UART2 RX) -- moved from UART0 (GPIO3), which
  is now physically disconnected.
- ESP32 GPIO16 (UART2 TX) -> ATmega RX -- newly connected.

GPIO16/17 as ESP32 UART2 pins for this exact module were independently
confirmed against M5Stack's own official forum response for the U138:
*"you are correct, the GPIOs for Serial2 ... should be 16/17"* --
consistent with the schematic-derived pin assignment already in
`Config.h` since section 26.

Software changes to match:
- `main.cpp`: the `AtmegaProtocol::available()` decode block (disabled in
  section 37 to avoid double-processing during a temporary UART0/UART2
  fan-out) was re-enabled, since that fan-out no longer exists physically
  -- UART2 RX is now the sole real data source. `processUart0()` is left
  in place (harmless; UART0 RX is disconnected, so it never fires) with a
  comment explaining the current wiring state and what to undo if UART0
  is ever reconnected.
- `WebManager.cpp` / `AtmegaSerial.cpp`: rather than add a new ring
  buffer, the now-unused UART0 web panel/buffer (`webSerialWriteUART0`)
  was repurposed to show a dedicated **UART2 TX-only** view -- easier to
  spot outgoing commands than scrolling the combined RX+TX stream in the
  UART2 panel. Panel relabelled `"UART2 (TX only - commands sent to
  ATmega)"`.
- `AtmegaProtocol.cpp`: removed leftover `[PROTO/LF]` / `[PROTO/AVAILABLE]`
  debug prints that had been silently inert while UART2 RX carried no
  real data; once it did, they duplicated every line in the combined
  UART2 panel. Remaining diagnostics (line-too-long) routed through
  `logPrintf()` instead of raw `Serial`.

### 38.3 Write path: physically verified, not just framed correctly

Previously verified only as far as "`[TX] 1P2P...` appears correctly
framed on the wire" (section 37.4). With UART2 TX physically connected,
sending `E3500401` / `E3500400` (DHW on/off) via MQTT `W` now produces
the ATmega's own acknowledgement in the live bus stream:

```
* wr 0x35 0x40 to 0x01   (after E3500401)
* wr 0x35 0x40 to 0x00   (after E3500400)
```

...with the corresponding byte visibly changing in real `40F035...` bus
reply packets between the two tests. This is P1P2Monitor confirming the
write itself, not an artifact of our own logging.

### 38.4 ATmega reset from HA/MQTT: implemented and confirmed, hardware reset declined

The existing HA button "Restart_P1P2Monitor_ATmega" sends Arnold's
ESP8266 hardware-reset command `"A"` (see 38.1's `RESET_PIN` finding --
this board has no such GPIO wired to the ESP32, so forwarding `"A"`
verbatim to the ATmega over serial would be a meaningless string it
doesn't recognize, silently ignored).

Fix, in `Mqtt.cpp`'s `onMqttMessage()`: `"A"` is now remapped to `"k"` --
P1P2Monitor's own serial-only software self-reset:

```cpp
case 'k': // soft-reset ESP
case 'K': Serial_println(F("* Resetting ATmega ...."));
          resetFunc();
          break;
```

No new wiring required; this travels over the already-working UART2 TX.
The source's own switch-statement comment confirms `'k'`/`'K'` are valid,
ATmega-recognized commands (*"in use by ATmega: cefgiklmnoqtuvwx"*).
Lowercase `'k'` was chosen as the final implementation (functionally
identical to `'K'` per the case fall-through above) after both were
tested live.

**Confirmed with a real reset cycle:** `[TX] 1P2Pk` was immediately
followed in the live log by `* Resetting ATmega ....`, then
P1P2Monitor's full boot banner, with `Reset cause: MCUSR=0` -- distinct
from `MCUSR=2 (ext-reset)` seen on an actual power-cycle/hardware reset.
This confirms the reset was genuinely software-triggered, not a
coincidental power blip. One isolated retry produced no visible
confirmation line; given P1P2Monitor's serial handling runs alongside
timing-critical P1/P2 bus bit-banging, this reads as an occasional single
dropped byte on a short command rather than a systemic issue -- acceptable
for a manual, rarely-invoked recovery action.

A true hardware reset (replicating `RESET_PIN`/GPIO5 by wiring a new
ESP32 GPIO directly to the ATmega's physical RESET pin) was considered
and explicitly **not** pursued: the software reset already covers the
normal case, and hardware reset only matters if the ATmega is hung badly
enough to not process serial input at all -- not encountered so far.

### 38.5 Alternative hardware evaluated: Olimex ESP32-PoE(-WROVER)

Compared against the current M5Stack U138 in response to a specific
product query. Findings:
- **WROVER variant**: PSRAM occupies GPIO16/17 (this project's UART2
  pins) -- not a drop-in swap; would require remapping
  `ATMEGA_UART_RX_PIN`/`TX_PIN` to different free GPIOs.
- **Ethernet PHY differs**: LAN8720 (Olimex) vs IP101G (M5Stack U138) --
  different `eth_phy_type_t`, different clock direction/pin
  (`ETH_CLOCK_GPIOxx_OUT` vs this project's `ETH_CLOCK_GPIO0_IN`), and a
  different `ETH_POWER_PIN`. MDC/MDIO (GPIO23/18) happen to match.
- **Non-WROVER Olimex variants** ("ESP32-PoE"/"ESP32-PoE-ISO") avoid the
  GPIO16/17 conflict, add a real GPIO header (would allow finally wiring
  a genuine ATmega hardware reset line, unlike the U138) and optionally
  galvanic isolation from PoE power (relevant given the P1/P2 bus already
  floats at ~15V).

Decision: no hardware change. Staying on the M5Stack U138. Documented here
in case a future hardware revision revisits this trade-off.

### 38.6 Telnet: closed as unnecessary, not deferred

Evaluated against Arnold's telnet feature set (issue commands, view
decoded output, basic-auth-gated access) and found fully superseded by
the existing web UI: the Send Command box + System Log + UART0(now
UART2-TX)/UART2 panels on `/serial`, and password-protected `/update`,
cover the same ground with a smaller attack surface (one HTTP port
instead of an additional unauthenticated-by-default telnet port). Marked
closed rather than "not started" -- a deliberate decision, not a gap.

### 38.7 OpenHAB integration path

Investigated as a forward-looking question, not yet implemented/tested
live. Finding: openHAB ships a dedicated `mqtt.homeassistant` binding that
subscribes to the *same* `homeassistant/<component>/<node>/<object>/config`
discovery convention this project already emits for Home Assistant --
meaning openHAB integration should work with **zero additional firmware
changes**, purely as an openHAB-side configuration exercise (install MQTT
+ Home Assistant bindings, point at the same broker, scan). Documented
caveats from the openHAB community to check for on first real test:
occasional discovery flakiness on broker/openHAB restart (may need a
manual re-scan), and a historical switch-state mapping quirk (`true`/
`false` vs `ON`/`OFF`) that shouldn't affect this project since its
switches already use the correct `stat_off`/`stat_on` numeric convention.
Still open: an actual live test against a running openHAB instance.

