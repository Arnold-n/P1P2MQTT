# Set up Home Assistant with the P1P2MQTT bridge

Text in *italics* applies only to Daikin Altherma/E-series models.

##### Set up MQTT server

An MQTT server is required. If you do not have one yet, you can set it up in HA under `Settings`/`Add-ons`/`ADD-ON STORE`. Find `Mosquitto broker`, `INSTALL` it, enable the Watchdog, and click `START`.

Under `Settings`/`People`, click blue button (lower right corner) `ADD PERSON`, enter user name `P1P2`, switch `Allow person to login` on, click `CREATE`, set password to `P1P2` (or preferably something better), and click `CREATE`.

You can find the IPv4 address of the MQTT server in HA under `Settings`/`System`/`Network`/`Configure network interfaces`/`3-dots`/`IP-information`.

##### Power off Daikin system

For safety, remove power from your system (likely from the outdoor unit) to remove power from the P1/P2 interface. The display of your room thermostat (if you have any) should be off.

##### Connect P1P2MQTT bridge to P1/P2 wires

The P1P2MQTT bridge can be connected in parallel to your room thermostat, either near your thermostat or near your indoor unit. *If you have a monoblock, the P1/P2 connection may be outside. It is recommended to use a P1/P2 cable such that the P1P2MQTT bridge can be in-house.*

##### Power on Daikin system

A WiFi Access Point with SSID `P1P2` should become available.

##### Provide WiFi and MQTT server credentials

Connect to the AP `P1P2` (default password `P1P2P3P4`) and enter WiFi credentials, and do not forget to also provide MQTT server credentials at the same time.

##### Add MQTT integration

Under `Settings`/`Devices & Services`, click (lower right) blue button `ADD INTEGRATION`, search for MQTT, select `MQTT`, enter MQTT server details, user and password, and `SUBMIT`.

##### Wait for auto-discovery

HA will start to recognize several controls and entities under various devices called `HC_*`, visible under `Settings`/`Devices & Services`/`MQTT`/`devices`.

Some entities become visible only when the bridge is operating in auxiliary control mode *and in counter request mode*. These modes can be switched on with the `Control_Function` switch *and the `Counter_Request_Function`* switch in the `HC_Bridge` device. After switching on, wait at least 10 minutes for all entities to be recognized in HA, then proceed to add the following subdevices to your HA dashboards.

| Device (E-series)|#  | Function                                                   | # |Controls
|:-----------------|--:|:----------------------------------------------------------|---:|:---------------|
| HC_Bridge        | 4 | Settings and status of P1P2MQTT bridge                     | 5  | `Control_Function`<br>`Counter_Request_Function`<br>`Restart_*`<br>`EEPROM_*``Factory_Reset_*`<br>`MQTT_*`
| HC_Sensors       |16 | Temperature, flow, water pressure (*)                      | n/a|
| HC_Room          |12 | Room temperature setpoints                                 |  1  | `Room_Heating`<br>`Room_Cooling` (*)
| HC_LWT           |15 | Leaving Water Temperature setpoints main zone              |  2  | `LWT_ABS_Heating`<br>`LWT_ABS_Cooling`<br>`LWT_Deviation_Heating`<br>`LWT_Deviation_Cooling` (*)
| HC_LWT2          |17 | Leaving Water Temperature setpoints add zone               |  3  | `LWT_ABS_Heating`<br>`LWT_ABS_Cooling`<br>`LWT_Deviation_Heating`<br>`LWT_Deviation_Cooling` (*)
| HC_Prices        |11 | Electricity/gas price                                      | 10 |  `Gas price`
| HC_Power         |13 | Power consumption (requires external meter) and production |n/a |
| HC_Mode          |14 | Operating modes                                            |  6 |  `Altherma_ON`<br>`Daikin_Defrost_Request`<br>`Daikin_Restart_Careful`<br>`Heating/Cooling/Auto`<br>`Abs/WD/Abs+prog/WD+prog`
| HC_FieldSettings |n/a| Field settings; changing these requires a system restart   |  7 |  `Nr_Of_Zones`<br>`Overshoot`<br>`RT/LWT`<br>`RT_Modulation`<br>`RT_Modulation_Max`
| HC_DHW           |(D)| DHW settings and info                                      |(D) |  `DHC_Setpoint` <br>`DHW_heat/off`<br>`DHW_Boost`
| HC_Quiet         |n/a| Quiet/silent mode setting                                  |  8 |  `Quiet_Level` <br>`Quiet_Level_When_On` <br>`Auto/On/Always-off`
| HC_COP           | 9 | COP calculations
| HC_Meters        |(S)| Hours/kWh counters (`C2` mode recommended to update counters)
| HC_Unknown       |(S)| Some unknown entities
| HC_Setup         |(S)| Single control entity for HA setup                         | (C)| `HA_Setup`

 # 1-17 is a suggestions to order devices on a single dashboard in a 3-column configuration for PC use; for HA app another order may be more appropriate <br>
(D) Suggest to use separate DHW dashboard <br>
(S) Suggest to use separate dashboard <br>
(C) Can be controlled from MQTT integration, no need to add to dashboard <br>
(*) Depending on model and operation mode, more or fewer controls may be visible. Some systems do not report flow or water pressure.


| Device (F-series)| Function                                                   | Controls
|:-----------------|:-----------------------------------------------------------|:---------------|
| HC_Bridge        | Settings and status of P1P2MQTT bridge                     |`Control_Function`<br>`Restart_*`<br>`EEPROM_*``Factory_reset_*`<br>`MQTT_*`
| HC_Sensors       | Temperature                                                |
| HC_Thermistors   | Temperature                                                |
| HC_Setpoints     | Temperature setpoints, fan speed                           | `Setpoint_Cooling`<br>`Setpoint_Heating`<br>(more tbd)
| HC_Mode          | Operating modes                                            | `Power` (on/off)
| HC_Unknown       | Some unknown entities                                      |
| HC_Filter        | Filter status                                              |

(*) Depending on model and operation mode, more or fewer controls may be visible.

##### Set up HA dashboards

After waiting for the entities to be recognized, the `HC_*` devices can now be added to your HA dashboards. You can skip adding `HC_Setup`.

##### *Finish set-up*

*Some controls may not be useful for your system (additional zone, cooling), these can now be deleted by editing your dashboards*

*Once all dashboards are set up, it is recommended to switch `HA_setup` (in `HC_Setup`) off to improve the HA control interface, and to make this change permanent with the `EEPROM_ESP_Save_Changes` button in `HC_Bridge`.*

##### Ready!

You should now be ready to monitor your system, and, if you switch(ed) `Control_Function` on, to control your system.

To limit the number of write instructions to your Daikin, a `Write_Budget` is available; if run-out, you can increase it under `HC_Bridge`. The budget is increased by 1 every `Write_Budget_Period` minutes.


*The controls for room temperature are only visible if the system operates in `RT` mode. If the system is switched to `RT` mode later, this may not become visible on the dashboard until you add the newly discovered controls.*

*Changes made under `HC_Fieldsettings` will require a Daikin system restart. The `Daikin_Restart_Careful` button is enabled only if the system is not heating or cooling.*

##### Trouble-shooting

If the red LED flashes in the P1P2MQTT bridge, this signals read errors or bus collission errors. You can telnet to the P1P2MQTT bridge to find out why.

If the P1P2MQTT bridge fails to connect to WiFi, it will recreate the AP, with a SSID/password (default `P1P2`/`P1P2P3P4`, unless it was reconfigured by you), and you can re-enter all credentials.

If the P1P2MQTT bridge connects to WiFi but not to the MQTT server, you can double-reset the bridge to recreate the original AP (always with default SSID/password `P1P2`/`P1P2P3P4`) and you can re-enter WiFi and MQTT credentials.

If the P1P2MQTT bridge cannot be put in `Control_Function` (if the switch bounces back), you can telnet to the P1P2MQTT bridge to find out if there are any informational or error messages.

If some controls or entities appear to be missing, you can *switch `HA_Setup` on again (no need to save this) and* press `MQTT_Rebuild` (under `HC_Bridge`) to repeat the MQTT configuration topics for Home Assistant, and re-add devices to your dashboard.

If you want to delete old MQTT topics (after an update from an earlier firmware, or if you change the topic structure, device name or bridge name yourself), you can press `MQTT_Delete_Rebuild` (under `HC_Bridge`) to delete all MQTT topics from all bridges and recreate new ones.
