**EGSQH10S18A9W protocol data format**

The information below is based on observed data from the EGSQH10S18A9W ground source heat pump.

# Packet type 61

### Packet type 61:request

Packets transmitted by the thermostat are always the same. The prefix source address used by the main controller is 03, while we usually see 00 in other systems. The packet type header convention is also different.

Header: 034061

Empty payload

### Packet type 61:request

The header format deviates from header format on other systems. This may indicate packet type 61, and perhaps 12 is part of the payload)

Header: 406112 

The packets transmitted by the heat pump are:

| Byte  | Example/value observed | Description                                                        | Data type
| ----- |:---------------------- |:------------------------------------------------------------------ |-
|  0    | 80                     | Operation mode?                                                    |
|  1    | 00                     | ?                                                                  |
|  2-3  | 33 01                  | Temperature (after backup heater?)                                 | u16div10
|  4-5  | 3A 01                  | LWT                                                                | u16div10 
|  6-7  | D6 00                  | Liquid refrigerant                                                 | u16div10
|  8-9  | 1D 01                  | RWT                                                                | u16div10
| 10-11 | DD 01                  | DHW boiler                                                         | u16div10
| 12-13 | E1 00                  | Temperature? (0x00E1 = 241 -> 24.1C)                               | u16div10
| 14-15 | 00 00                  | ?                                                                  |
