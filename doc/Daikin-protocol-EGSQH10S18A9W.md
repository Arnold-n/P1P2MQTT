**EGSQH10S18A9W protocol data format**

The information below is based on observed data from this ground source heat pump.

Packets transmitted by the thermostat are always the same:

| Byte | Example/value observed |                                 |
| ---- | ---------------------- | ------------------------------- |
| 0    | 03                     | source address, 03 = thermostat |
| 1    | 40                     | target address, 40 = heat pump  |
| 2    | 61                     | packet type requested?          |
| 3    | 5B                     | CRC checksum                    |



The packets transmitted by the heat pump are:

| Byte  | Example/value observed |                                                                    |
| ----- | ---------------------- | ------------------------------------------------------------------ |
| 0     | 40                     | source address, 40 = heat pump                                     |
| 1     | 61                     | packet type?                                                       |
| 2     | 12                     | packet type?                                                       |
| 3     | 80                     | Operation mode?                                                    |
| 4     | 00                     | ?                                                                  |
| 5-6   | 33 01                  | Temp in 0.1 degrees (after backup heater?) (0x0133 = 307 -> 30.7C) |
| 7-8   | 3A 01                  | LWT in 0.1 degrees (0x013A = 314 -> 31.4C)                         |
| 9-10  | D6 00                  | Liquid refrigerant in 0.1 degrees (0x00D6 = 214 -> 21.4C)          |
| 11-12 | 1D 01                  | RWT in 0.1 degrees (0x011D = 285 -> 28.5C)                         |
| 13-14 | DD 01                  | DHW boiler in 0.1 degrees (0x01DD = 477 -> 47.7C)                  |
| 15-16 | E1 00                  | Temp? (0x00E1 = 241 -> 24.1C)                                      |
| 17-18 | 00 00                  | ?                                                                  |
| 19    | XX                     | CRC checksum                                                       |

