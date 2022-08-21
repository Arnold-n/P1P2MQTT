<<<<<<< Updated upstream
# Serial protocol description

## Serial input

The serial interface is used to give commands to P1P2Monitor. P1P2Monitor's command description can be found in P1P2Monitor-commands.md, but you may be more interested in P1P2MQTT's [MQTT usage description](https://github.com/Arnold-n/P1P2Serial/blob/main/P1P2MQTT.md).

To suppress serial input not intended for P1P2Monitor (such as output (debug/boot) messages from ESP8266), each serial line must start with SERIAL_MAGICSTRING (default "1P2P") if defined. Further, first line received after a reboot will be ignored (P1P2MQTT aims to flush this). Lines which are too long (> approximately 90 characters) will also be ignored.

## Serial output

The serial interface further provides hex-formatted raw P1/P2 packet data (and pseudopacket data). The serial output may be forwarded by P1P2-bridge-esp8266 over WiFi via telnet and via MQTT topics P1P2/R/# (raw hex data), P1P2/X/# (binary data), and P1P2/P/# (parameter data), and P1P2/S/# (reporting and status data).  

Verbosity of the serial output can be set to 0..4 with the 'V' command. Level 3 is default and recommended.

Verbosity level 0 is minimal: limited reporting, and only hex data, not prefixed.

In all other verbosity levels, each serial output line starts with
- "R " for any error-free raw hex data read directly from the P1/P2 bus, or 
- "C " for P1/P2 bus timing information of messages with read errors,
- "c " for P1/P2 bus timing information of error-free messages, or
- "\* " for any other (human-readable) output (including raw data with errors).

Hex packet data is formatted as 2 characters per byte. The CRC byte is included. In verbosity level 1, the CRC byte is separated by " CRC=".

In verbosity level 1 or 3, a fixed-length relative timing info segment is added before the hex packet data, starting with a 'T' for a relative time stamp for P1/P2 bus data, or starting with a 'P' with an empty time stamp for pseudopacket data. 

In verbosity levels 1 or 4 provide more detail than other levels for debugging purposes. 

In verbosity level 4, *no* raw hex data is transmitted, unless it contains errors.

#### Example serial output data

Verbosity level 0: 
```
0000100001010000000014000000000800000F00003D0029
```

Verbosity level 1:
```
R T  0.036: 0000100001010000000014000000000800000F00003D00 CRC=29
```

Verbosity level 2: 
```
R 0000100001010000000014000000000800000F00003D0029
```

Verbosity level 3: 
```
R T  0.036: 0000100001010000000014000000000800000F00003D0029
```

Verbosity level 3, boot procedure output:
```
* P1P2Monitor-v0.9.14
* Compiled Jul 24 2022 14:31:11 +control
* 
* checking EEPROM
* EEPROM sig match1
* Control_ID=0xF0
* Verbosity=3
* Counterrepeatingrequest=1
* Reset cause: MCUSR=2 (external-reset)
* CPU_SPEED=8000000
* SERIALSPEED=250000
* CONTROL_ID_DEFAULT=0x0
* F030DELAY=100
* F03XDELAY=30
* First line ignored: 1P2P\* Dummy line 1.
R T  0.105: 0000100001010000000014000000000800000F00003D0029
R T  0.024: 400010000081013D000F0014001A000000000000000000E0
P P         00000D0000000000000000000000000000000000000000C4
R P         40000D0D00000001C63D0000000000000000000000000019
```
