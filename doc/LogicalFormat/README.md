# Protocol descriptions and reverse engineering

### Warning

This directory contains protocol information based on reverse engineering and assumptions, so there may be mistakes and misunderstandings.

### Documentation per brand:

[Daikin](README-Daikin.md)

Other brands: tbd

### How to contribute and reverse engineer?

For models that are not yet supported, it is helpful if you correlate changes in data to manual changes or observations such as temperature/state changes. You can look for any changes in the data output that match such changes. Using the 'J' command various output options can be enabled and disabled in the bridge. The command 'J10A' will output a lot of data in byte or bit form in MQTT topics P1P2/P/# for which we expect that the information could be useful. An example MQTT output topic for an unknown bit is `P1P2/P/P1P2MQTT/bridge0/U/0/PacketSrc_0x00_Type_0x13_Byte_2_Bit_0 0`

A very useful script donated by a P1P2MQTT user is [mqtt_subscriber.sh](mqtt_subscriber.sh) which outputs any parameter when it changes value over MQTT, together with its previous value:

~~~
---------------------------------------------------------------------------------------
                                                                  | Previous |   New  |
                    Topic                                         |   Value  |  Value |
---------------------------------------------------------------------------------------
P1P2/P/P1P2MQTT/bridge0/U/2/ParamSrc_0x00_Type_0x35_Nr_0x00C9     | 0x80     | 0x83   |
P1P2/P/P1P2MQTT/bridge0/T/1/Temperature_R3T_Refrigerant           | 29.797   | 29.828 |
P1P2/P/P1P2MQTT/bridge0/U/2/ParamSrc_0x00_Type_0x35_Nr_0x00C9     | 0x83     | 0x85   |
P1P2/P/P1P2MQTT/bridge0/U/0/PacketSrc_0x00_Type_0x13_Byte_2_Bit_0 | 0        | 1      |
~~~
