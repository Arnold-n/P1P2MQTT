**P1P2Monitor topic organization**

- P1P2/W :          Channel to configure the ESP and P1P2Monitor, and to send commands to P1P2Monitor to write raw packets to the P1/P2 bus, or to switch its limited control functionality
- P1P2/R :          packet data read by P1P2Monitor
- P1P2/S :          other output from P1P2Monitor, and some additional output from the ESP
- P1P2/P/\*/KEY :   topic folder for decoded parameter values from the P1P2 bus, published as separate topics in subfolders (P for parameter settings, M for measurements (except for temp/flow), F for external controller (type 35) communication, T for temperature and flow measurements, or U for unknown parameters); and example is P1P2/P/T/TempLWT
- P1P2/J :          json channel for json formatted strings containing decoded parameter values from the P1P2 bus

