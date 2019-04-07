**EGSQH10S18A9W protocol data format**

The information below is based on observed data from this ground source heat pump.

Packets transmitted by the thermostat are always the same:

| Byte | Example/value observed |                                 |
| ---- | ---------------------- | ------------------------------- |
| 1    | 03                     | source address, 03 = thermostat |
| 2    | 40                     | target address, 40 = heat pump  |
| 3    | 61                     | packet type requested?          |
| 4    | 5B                     | CRC checksum                    |



The packets transmitted by the heat pump are:

| Byte  | Example/value observed |                                                                    |
| ----- | ---------------------- | ------------------------------------------------------------------ |
| 1     | 40                     | source address, 40 = heat pump                                     |
| 2     | 61                     | packet type?                                                       |
| 3     | 12                     | packet type?                                                       |
| 4     | 80                     | Operation mode?                                                    |
| 5     | 00                     | ?                                                                  |
| 6-7   | 33 01                  | Temp in 0.1 degrees (after backup heater?) (0x0133 = 307 -> 30.7C) |
| 8-9   | 3A 01                  | LWT in 0.1 degrees (0x013A = 314 -> 31.4C)                         |
| 10-11 | D6 00                  | Liquid refrigerant in 0.1 degrees (0x00D6 = 214 -> 21.4C)          |
| 12-13 | 1D 01                  | RWT in 0.1 degrees (0x011D = 285 -> 28.5C)                         |
| 14-15 | DD 01                  | DHW boiler in 0.1 degrees (0x01DD = 477 -> 47.7C)                  |
| 16-17 | E1 00                  | Temp? (0x00E1 = 241 -> 24.1C)                                      |
| 18-19 | 00 00                  | ?                                                                  |
| 20    | XX                     | CRC checksum                                                       |

