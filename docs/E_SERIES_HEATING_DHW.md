# E-Series Heating / DHW ON-OFF

Tested with the Daikin E-Series installation using P1P2MQTT.

Before sending E-series control commands, enable auxiliary control mode:

Topic:
P1P2/W/P1P2MQTT/bridge0

Payload:
L1

## Heating ON/OFF

Heating control uses parameter `0x2F`.

### Heating ON

Topic:
P1P2/W/P1P2MQTT/bridge0

Payload:
E35002F01

### Heating OFF

Topic:
P1P2/W/P1P2MQTT/bridge0

Payload:
E35002F00

These commands were tested successfully on the installation.

---

## DHW ON/OFF

DHW control uses parameter `0x40`.

### DHW ON

Topic:
P1P2/W/P1P2MQTT/bridge0

Payload:
E3500401

### DHW OFF

Topic:
P1P2/W/P1P2MQTT/bridge0

Payload:
E3500400

These commands were tested successfully on the installation.

---

## Summary

| Function | ON | OFF |
|---|---|---|
| Heating | `E35002F01` | `E35002F00` |
| DHW | `E3500401` | `E3500400` |

MQTT topic for all commands:

`P1P2/W/P1P2MQTT/bridge0`

Auxiliary control mode:

`L1`

The commands above have been verified against the actual installation.