# Security considerations

Security of the P1P2MQTT bridge is limited by various factors:
- firmware update via webserver: no protection (yet), protection thus relies on your local network
- firmware update via OTA mechanism: password-protected (password configurable via `P18`)
- AP when MQTT or WiFi fails: password-protected, SSID/password can be configured with parameter (configurable via `P00`/`P01`)
- AP when reset button is tapped twice quickly: default SSID/password
- telnet: can be protected with login-code via parameter (conifigurable via`P16`)
- MQTT: user/password protected, but anyone with user/password to the MQTT server can give instructions to the bridge and can monitor traffic for passwords being configured

This practically means that anyone with access to your local network, or with physical access to the P1P2MQTT bridge reset button, has full access.
