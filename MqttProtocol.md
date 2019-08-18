**P1P2Monitor topic organization**

- P1P2/W :          Channel to configure the ESP and P1P2Monitor, and to instruct P1P2Monitor to write raw packets to the P1/P2 bus.
- P1P2/R :          is a read channel with the serial output from P1P2Monitor and additional output from the ESP
- P1P2/P/KEY :      is a topic folder where decoded parameter values from the P1P2 bus are published as separate topics, for example P1P2/P/TempLWT
- P1P2/j :          is a MQTT channel where ESP published json formatted strings with decoded parameter values from the P1P2 bus

