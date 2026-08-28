# P1P2MQTT-ESP32

> Modern ESP32 + Ethernet bridge for the P1P2MQTT ecosystem.

**Status: Early Production**

Core functionality is operational and has been validated on real hardware with
a live ATmega328P/P1P2Monitor and Daikin system. Ethernet, MQTT, bidirectional
UART communication, MQTT write commands, Home Assistant discovery and native
openHAB MQTT integration are working end-to-end.

Production hardening and some advanced compatibility items are still in progress.

## Product Goal

The primary goal of P1P2MQTT-ESP32 is to replace the legacy ESP8266 Wi-Fi
network bridge with a modern, reliable **Ethernet + PoE** platform.

For a permanently installed heating-system gateway, wired Ethernet provides a
more stable and predictable network connection than Wi-Fi, while IEEE 802.3af
PoE allows the gateway to receive both network connectivity and power through
a single Ethernet cable.

The ESP32 therefore modernizes the network and platform layer without changing
the proven ATmega328P/P1P2Monitor architecture or the actual Daikin P1/P2
communication.

In short:

```text
Legacy:
ATmega + ESP8266 Wi-Fi
        |
        v
      MQTT

New:
ATmega + ESP32 Ethernet + PoE
        |
        v
      MQTT
```

The goal is not to replace the proven P1/P2 controller, but to provide a more
reliable, maintainable and production-oriented network gateway for 24/7
operation.

## Why ESP32 instead of ESP8266?

ESP32 is a better fit for a permanently installed gateway:

- **Native Ethernet** instead of Wi-Fi for a more stable network connection.
- **PoE support** for power and network through a single Ethernet cable.
- **More CPU, RAM and Flash** for MQTT, web services, diagnostics and OTA.
- **More peripherals and interfaces** for future extensions.

The ESP32 replaces the legacy network/bridge layer; the proven ATmega328P/P1P2Monitor
and the Daikin P1/P2 communication remain unchanged.

## Overview

P1P2MQTT-ESP32 replaces the original ESP8266 network/bridge controller with an
ESP32 Ethernet platform while keeping the ATmega328P/P1P2Monitor as the sole
P1/P2 bus controller.

The project reuses Arnold's P1P2MQTT protocol implementation wherever possible.

**The ESP32 does not communicate directly with the P1/P2 bus.**

Architecture:

```text
                 MQTT / Web UI / openHAB / HA
                              |
                              v
                       +-------------+
                       |    ESP32    |
                       | Ethernet    |
                       | MQTT / Web  |
                       +------+------+
                              |
                    UART2 / 250000 8N1
                              |
                       +------+------+
                       |   ATmega    |
                       | P1P2Monitor |
                       +------+------+
                              |
                            P1/P2
                              |
                           Daikin
```

## Reference hardware

**M5Stack PoESP32 Unit (U138)**

- ESP32
- Native 100 Mbps Ethernet
- IEEE 802.3af PoE
- 16 MB hardware flash
- UART2 used for the ATmega link

Current UART connection:

```text
ATmega TX  -> ESP32 UART2 RX  GPIO17
ATmega RX  <- ESP32 UART2 TX  GPIO16

250000 baud / 8N1
```

The ATmega328P firmware and P1/P2 implementation remain unchanged.

## Current functionality

### ESP32 platform

- Ethernet with DHCP
- MQTT with automatic connect/reconnect
- Bidirectional ATmega UART
- Web UI
- Web System Log
- UART diagnostics / Web Terminal
- Web firmware upload
- ArduinoOTA
- System Health diagnostics
- ESP32 restart from web UI and MQTT
- Persistent boot counter and reset reason
- Free heap / minimum heap diagnostics

### MQTT

The ESP32 provides the network/MQTT side of the original bridge.

Working functionality includes:

- automatic MQTT connection after boot/restart
- automatic reconnect after network recovery
- MQTT state publishing
- MQTT write/command subscription
- ESP32-local commands
- ATmega command forwarding
- publish diagnostics
- Arnold-compatible `1P2P` command framing

The current development topic tree is based on Arnold's:

```text
P1P2/L/...
P1P2/M/...
P1P2/P/...
P1P2/W/...
```

The ESP32 port also retains its development-specific MQTT diagnostics and
topic handling where required.

## ATmega / P1P2 compatibility

The ATmega328P running P1P2Monitor remains responsible for:

- P1/P2 bus timing
- P1/P2 transmission and reception
- protocol-level operations
- execution of commands received from the ESP32

ESP32 is responsible for:

- transport
- networking
- MQTT
- web services
- integrations
- diagnostics

The compatibility layer adapts the ESP32 runtime to Arnold's existing
implementation.

The main upstream protocol implementation remains:

```text
P1P2_ParameterConversion.h
```

It should not be modified merely to satisfy ESP32-specific requirements.

## MQTT -> ESP32 -> ATmega

Write commands use the same command semantics as the original bridge:

```text
MQTT
  |
  v
ESP32 MQTT command handler
  |
  v
1P2P + command + CR/LF
  |
  v
UART2 TX
  |
  v
ATmega328P
  |
  v
P1/P2
```

Verified on real hardware:

- DHW ON/OFF
- DHW Boost
- Altherma ON/OFF
- Room Heating setpoint
- DHW setpoint
- ATmega software reset
- ESP32 restart

For example:

```text
E3500401       DHW ON
E3500400       DHW OFF
```

The ATmega's own bus acknowledgement and read-back were used to verify the
write path.

## openHAB

Native openHAB MQTT integration is **working end-to-end**.

The current integration uses openHAB's generic MQTT binding directly rather
than depending on Home Assistant discovery.

Implemented and verified:

- sensors
- binary states
- switches
- buttons
- DHW control
- Altherma ON/OFF
- DHW Boost
- Room Heating setpoint
- DHW setpoint
- ESP32 restart
- ATmega restart

The openHAB configuration is based directly on the P1P2 MQTT topic tree.

This allows openHAB to remain independent of Home Assistant.

## Home Assistant

Home Assistant discovery is also implemented and has been verified for:

- sensors
- binary sensors
- numbers
- selects
- switches
- buttons
- ESP32 restart

The ESP32 can therefore operate with the existing Arnold-style Home Assistant
discovery mechanism, although the current preferred integration for this
installation is native openHAB MQTT.

## Web UI

The web interface provides:

- MQTT configuration
- firmware update
- restart
- UART diagnostics
- system log
- live system status

The main page includes:

```text
SYSTEM HEALTH

Ethernet
MQTT
P1P2
MQTT publish
MQTT command
Free RAM
Uptime
```

and:

```text
ESP32 DIAGNOSTICS

Free heap
Minimum free heap
Boot count
Reset reason
```

The page supports an **Auto-refresh ON/OFF** switch.

## Arnold compatibility

Upstream project:

**P1P2MQTT by Arnold Niessen**

https://github.com/Arnold-n/P1P2MQTT

Arnold's project remains the source of truth for:

- Daikin P1/P2 protocol
- parameter/register definitions
- command semantics
- model support
- CRC/protocol details

The ESP32 project aims to remain compatible rather than becoming an independent
protocol implementation.

## ESP8266 decommissioning

The original ESP8266 bridge can be replaced by the ESP32.

A separate minimal companion firmware was created for the old ESP8266:

https://github.com/JackyKNX/P1P2MQTT-ESP8266-PARKED

Its purpose is to keep the old ESP8266 safely parked while the ESP32 takes over
the network/bridge role.

It does not communicate with the ATmega UART. Its only P1P2-related function
is maintaining the required `ATMEGA_SERIAL_ENABLE` state so that the ATmega
continues operating.

## Current MQTT topic model

The main branches used by the integration are:

```text
P1P2/
├── L/P1P2MQTT/bridge0/     availability
├── M/P1P2MQTT/bridge0/     raw / diagnostic data
├── P/P1P2MQTT/bridge0/     processed parameters
└── W/P1P2MQTT/bridge0/     write / command path
```

Arnold's original implementation also exposes additional branches such as
`Z`, `S` and raw `M` variants. Some details of complete topic-tree parity
remain a compatibility item rather than a blocker for the current integration.

## Production status

The project is now at **Early Production** stage.

### Working and validated

- ESP32 Ethernet runtime
- real M5Stack U138 hardware
- ATmega328P/P1P2Monitor communication
- full-duplex UART2
- MQTT automatic reconnect
- MQTT state publishing
- MQTT write path
- real E-Series write verification
- Home Assistant discovery
- native openHAB MQTT integration
- ESP32 restart
- ATmega software reset
- web diagnostics
- system logging
- OTA components

### Still to harden

- authentication/security of every exposed management endpoint
- formal interrupted-OTA recovery testing
- hardware watchdog testing
- dedicated long-duration stress testing
- complete Arnold MQTT topic-tree parity
- complete validation of every available write command
- additional Daikin model/series validation

The remaining items are primarily production hardening and compatibility
coverage; the core ESP32 bridge functionality is operational.

## Design principle

The project intentionally separates protocol intelligence from platform
services:

```text
Arnold P1P2 implementation
        |
        v
ESP32 compatibility layer
        |
        v
ESP32 platform services
        |
        +--> Ethernet
        +--> MQTT
        +--> Web UI
        +--> OTA
        +--> Diagnostics
        +--> openHAB
        +--> Home Assistant
```

This makes it possible to benefit from future upstream protocol improvements
without duplicating the Daikin protocol implementation.

## Repository

```text
P1P2MQTT-ESP32/

├── src/
├── include/
├── docs/
├── platformio.ini
└── README.md
```

The detailed development history and low-level porting notes are intentionally
kept out of this README. They belong in the `docs/` directory and project
history.

## Acknowledgements

Special thanks to **Arnold Niessen** for the P1P2MQTT project and the extensive
reverse engineering of the Daikin P1/P2 protocol.

Without that work, this ESP32 port would not exist.

## License

This project follows the licensing terms applicable to the reused components
of the original P1P2MQTT project.

ESP32-specific additions are licensed according to the license specified in
this repository.
