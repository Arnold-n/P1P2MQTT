# P1P2Monitor serial raw data format and serial commands

This document describes the serial interface to the ATmega328P or Arduino Uno. It is only relevant in the unlikely case that you interface directly to P1P2Monitor. This information is not useful if you use Home Assistant and the P1P2MQTT bridge. For interfacing via MQTT to the P1P2MQTT bridge, please read the [MQTT protocol information](https://github.com/Arnold-n/P1P2MQTT/blob/main/P1P2MQTT.md) instead.

## Serial output from P1P2Monitor

Each P1P2Monitor serial output line starts with
- `R` for error-free raw hex data read directly from the P1/P2 bus,
- `E` for not error-free raw hex data
- `C` for P1/P2 bus timing information of messages with read errors,
- `c` for P1/P2 bus timing information of error-free messages, or
- `*` for any other (human-readable) output.
The serial output is forwarded by P1P2MQTT via various MQTTT topics.

P1P2Monitor outputs further information in so-called pseudo packets, which look like P1/P2 packet data, but are internally generated.

Hex packet data is formatted as one line per packet, and 2 ASCII characters per byte. Each line of hex data originating from the bus is prefixed by a `T` and a relative time stamp against the previous packet. Pseudo packets are prefixed by `P` and have no relative time stamp:

~~~
R T  0.105: 0000100001010000000014000000000800000F00003D0029
R T  0.024: 400010000081013D000F0014001A000000000000000000E0
P P         00000D0000000000000000000000000000000000000000C4
R P         40000D0D00000001C63D0000000000000000000000000019
~~~

```
R T  0.105: 0000100001010000000014000000000800000F00003D0029
R T  0.024: 400010000081013D000F0014001A000000000000000000E0
P P         00000D0000000000000000000000000000000000000000C4
R P         40000D0D00000001C63D0000000000000000000000000019
```

## Serial input

Commands to P1P2Monitor can be given via the serial input. Commands are case-insensitive.

Warnings:
 - If SERIAL\_MAGICSTRING is defined, each line must start with this string.
 - The first command line received over serial by P1P2Monitor after its reboot will - on purpose - be ignored for robustness reasons. If you communicate directly over serial (not via P1P2MQTT), a good habit is to send a dummy line `*` as first command. If you use P1P2MQTT, the code on the ESP will take care of this.

### Summary of most useful commands

The most useful commands:
 - `L1` to start P1P2Monitor acting as an auxiliary controller
 - `L0` to stop P1P2Monitor acting as an auxiliary controller
 - `C2` (Daikin E only) to start requesting counters every minute
 - `C0` (Daikin E only) to stop requesting counters every minute
 - `E` (Daikin E only) or `F` for parameter writing, more information in [Commands-Eseries.md](https://github.com/Arnold-n/P1P2MQTT/blob/main/doc/Commands-Eseries.md) or [Commands-Fseries.md](https://github.com/Arnold-n/P1P2MQTT/blob/main/doc/Commands-Fseries.md)
 - `F` (Daikin F only) for parameter writing, more information in [Commands-Eseries.md](https://github.com/Arnold-n/P1P2MQTT/blob/main/doc/Commands-Eseries.md) or [Commands-Fseries.md](https://github.com/Arnold-n/P1P2MQTT/blob/main/doc/Commands-Fseries.md)
 - `F` (Daikin E only) to set fake room temperature sensor (`F1 205` sets reported room temperature to 20.5C, `F1` switches fake sensor on, `F0` switches fake sensor off)

### Monitor commands:

- `V`  Show P1P2Monitor version and status information
- `U`  Shows scope mode (default 0 off, 1 on),
- `Ux` Sets scope mode (default 0 off, 1 on); adds timing info for the start of some of the packets read via serial output and R topic, and
- `K` instructs ATmega328P to reset itself.
- `E` (not for Daikin E) to set error mask; mask is default 0x3B on Hitachi to ignore PE/UC reports which are expected; mask is default 0x7F (all) for other brands)

## Auxiliary controller commands:

- `L1` sets auxiliary controller mode on (controller address (0xF0 or 0xF1) is auto-detected after check whether another auxiliary controller is present or not) (can also be set in P1P2Config.h) (controller ID is saved in EEPROM, so this command remains effective after a restart),
- `L0` sets auxiliary controller mode off (remains effective after restart),
- `L2` (and L3) switch auxiliary controller mode off (and on) but do not save this change to EEPROM,
- `L5` (F-series only) switches auxiliary controller mode partially on: only 00F030 messages are responded to. This enable monitoring which 00F03x packets will be requested. Not saved to EEPROM,
- `L80` (E-series only) reports (simulated) LCD backlight of bridge to be off
- `L81` (E-series only) reports (simulated) LCD backlight of bridge to be on (if/when main controller backlight is off; this will block LCD light on main controller)
- `L90` (E-series only) puts bridge in normal user mode
- `L91` (E-series only) puts bridge in advanced user mode
- `L92` (E-series only) puts bridge in installer mode (if/when main controller is not in installer mode)
- `L99` (E-series only) restarts Daikin system, use with care!
- `L`  displays current controller\_id (0x00 = off; 0xF0/0xF1 is first/secondary auxiliary controller),

## Counter-request commands (Daikin E only):

- `C1` triggers single cycle of 6 B8 packets to request (energy/operation/start) counters from heat pump; counters are requested in time slot intended for 40F030 reply
- `C2` like C1, but keeps repeating every new minute,
- `C0` stop requesting counters, and
- `C`  show counter-repeating-request status.
- `Q9` (Daikin E only) to cycle-steal the pause after 400012 message for doing a counter-request; this works on certain systems but could give bus collissions on others, use with care. Upon bus collission detection Q9 and auxiliary control mode will be switched off
- `Q0` (Daikin E only) to stop cycle-stealing

## Raw data commands:

Commands for raw data writing to the bus (only for reverse engineering purposes), avoid these commands unless you know what you do:
- `W<hex data>` Write raw packet (max 32 bytes (as defined by WB\_SIZE)) (no 0x prefix should be used for the hex bytes; hex bytes may be concatenated or separated by white space), works only in `L0` mode,
- `T`  display current delay value (a packet will be written after exactly \<delay\> ms after the latest start bit, or if the bus has been silent for \<delaytimeout\> ms),
- `Tx` sets new delay value in ms, to be used for future packets (default 50 (older versions: 0)),
- `O`  display current delaytimeout value, and
- `Ox` sets new delay timeout value in ms, to be used immediately (default 2500).
