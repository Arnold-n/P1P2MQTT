# Set up Home Assistant with the P1P2MQTT bridge

This page describes how to configure your P1P2MQTT bridge and Home Assistant (HA).

Text in *italics* applies only to Daikin Altherma/E-series models.

##### Firmware update from earlier version

As a new user you may skip this section.

You can delete old MQTT topics and rebuild the new ones with the MQTT_Delete_Own_Rebuild button in HC_Bridge; this button may be unavailable until the bridge is running for 2 minutes. Deleting/rebuilding may take ~5 minutes. If upgrading from v0.9.44 or older, use MQTT_All_Delete_Rebuild because of the different topic structure, but be aware that this also deletes all homeassistant-config messages from other devices (if any) on the same MQTT server.

The EEPROM data in the ESP includes MQTT server credentials and should survive a firmware update, but if that fails (especially for rc candidates shared by mail), you may encounter a bridge which connects to WiFi but not to MQTT. In that case you can either
-telnet to the bridge, and use the commands `P7 IPv4-MQTT-server`, and if necessary also `P8 MQTT-port-nr`, `P9 MQTT-username` and `P10 MQTT-password`, or
-double-tap the reset-button until the blue LED lights up, connect to the AP  with SSID `P1P2MQTT-2x` and password `P1P2P3P4`, and enter WiFi and MQTT server credentials.

##### Set up MQTT server

An MQTT server is required. If you do not have one yet, you can set it up in HA under `Settings`/`Add-ons`/`ADD-ON STORE`. Find `Mosquitto broker`, `INSTALL` it, enable the Watchdog, and click `START`.

Under `Settings`/`People`, click blue button (lower right corner) `ADD PERSON`, enter user name `P1P2`, switch `Allow person to login` on, click `CREATE`, set password to `P1P2` (or preferably something better), and click `CREATE`.

##### Power off Daikin system

For safety, remove power from your system (likely from the outdoor unit) to remove power from the P1/P2 interface. The display of your room thermostat (if you have any) should be off.

##### Connect P1P2MQTT bridge to P1/P2 wires

The P1P2MQTT bridge can be connected in parallel to your room thermostat, either near your thermostat or near your indoor unit. *If you have a monoblock, the P1/P2 connection may be outside. It is recommended to use a P1/P2 cable such that the P1P2MQTT bridge can be in-house.*

Polarity of the P1/P2 cables is irrelevant, you can connect the 2 P1/P2 wires without looking which one is P1 or P2.

##### Power on Daikin system

A WiFi Access Point with SSID `P1P2MQTT` (or `P1P2MQTT-2x` after a double-reset) should become available.

##### Provide WiFi and MQTT server credentials

Connect to the AP `P1P2MQTT` or `P1P2MQTT-2x`. Default password for `P1P2MQTT` is `P1P2P3P4` but you can change it later. Password for `P1P2MTT-2x` is `P1P2P3P4` but cannot be changed. After connecting, enter WiFi credentials, and do not forget to also provide MQTT server credentials at the same time.

If you use the MQTT server from HA and do not know its IPv4 address, it can be found under `Settings`/`System`/`Network`/`Configure network interfaces`/`3-dots`/`IP-information`. Default port is 1883, MQTT user is `P1P2` and MQTT password is what you entered yourself.

##### Add MQTT integration

In HA, under `Settings`/`Devices & Services`, click (lower right) blue button `ADD INTEGRATION`, search for MQTT, select `MQTT`, enter MQTT server details, user and password, and `SUBMIT`.

##### Wait for auto-discovery

HA will start to recognize several controls and entities under various devices called `HC_*`, visible under `Settings`/`Devices & Services`/`MQTT`/`devices`. Give it some time to find all controls and entities.

Some entities become visible only when the bridge is operating in auxiliary control mode *and in counter request mode*. These modes can be switched on with the `Control_Function` switch *and the `Counter_Request_Function`* switch in the `HC_Bridge` device. After switching on, wait at least 10 minutes for all entities to be recognized in HA, then proceed to add the following subdevices to your HA dashboards.

| Device (E-series)|#  | Function                                                   | # |Controls
|:-----------------|--:|:----------------------------------------------------------|---:|:---------------|
| HC_Bridge        | 4 | Settings and status of P1P2MQTT bridge                     | 5  | `Control_Function`<br>`Counter_Request_Function`<br>`Restart_*`<br>`EEPROM_*` `MQTT_Rebuild`
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
| HC_Setup         |(S)| Single control entity for HA setup                         | (C)| `HA_Setup` <br> `Factory_Reset_*` <br> `MQTT_Delete_`

(Numbering # 1-17 is a suggestion how to order devices on a single dashboard in a 3-column configuration for PC use; for HA app another order may be more appropriate) <br>

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

If the P1P2MQTT bridge fails to connect to WiFi, it will recreate the AP, with a SSID/password (default `P1P2MQTT`/`P1P2P3P4`, unless the password was reconfigured by you), and you can re-enter all credentials.

If the P1P2MQTT bridge connects to WiFi but not to the MQTT server, you can double-reset the bridge to recreate the original AP (always with default SSID/password `P1P2MQTT-2x`/`P1P2P3P4`) and you can re-enter WiFi and MQTT credentials.

If the P1P2MQTT bridge cannot be put in `Control_Function` (if the switch bounces back), you can telnet to the P1P2MQTT bridge to find out if there are any informational or error messages.

If some controls or entities appear to be missing, you can *switch `HA_Setup` on again (no need to save this) and* press `MQTT_Rebuild` (under `HC_Bridge`) to repeat the MQTT configuration topics for Home Assistant, and re-add devices to your dashboard.

If you want to delete old MQTT topics (after an update from an earlier firmware, or if you change the topic structure, device name or bridge name yourself), you can press `MQTT_Delete_Rebuild` (under `HC_Bridge`) to delete all MQTT topics from all bridges and recreate new ones.

#### Control and entity overview (Daikin Altherma / E-series)

##### HC_Setup

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| HA_Setup                                 | Enables proper initial set-up of HA dashboards (or otherwise controls may be incomplete), enables button for locking production/consumption counters for COP_Before_Bridge/COP_After_Bridge calculations, and enables factory_reset button. After setup, strongly recommended to switch to off. No need to add to any dashboard.

##### HC_Bridge

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Control_Function                         | Determines whether P1P2MQTT bridge operates in auxiliary control mode. Not needed for monitoring, but required for controlling your system
| Counter_Request_Function                 | Determines whether P1P2MQTT bridge will request kWh/hour/start counters once every minute. Violates P1/P2 timing protocol, but works on most systems
| EEPROM_ESP_Save_Changes                  | Saves changes made (offsets, Cons_Prod_Counters) to ESP EEPROM
| EEPROM_ESP_Set_Cons_Prod_Counters        | If not already done automatically, pressing this button will store the current consumption/production counters for calculation of COP_Before_Bridge and COP_After_Bridge enabling you to monitor improvements of your system's average COP; to save changes, also press EEPROM_ESP_Save_Changes. Button is disabled if HA_Setup is switched off.
| EEPROM_ESP_Undo_Changes                  | Undo changes made to EEPROM (not possible after saving)
| Factory_Reset_ESP_After_Restart          | Use with care! This will schedule a factory reset on the next ESP restart. It will then reset all ESP-stored values to their original value, including device name, bridge name, AP, and more, but (as of v0.9.52:) excluding MQTT server credentials. Can be cancelled with Factory_Reset_ESP_Cancel but cannot be undone after ESP restart.  Button is disabled if HA_Setup is switched off.
| Factory_Reset_ESP_Cancel                 | Unschedule any scheduled factory reset
|MQTT_Rebuild                              | Retransmits all Home Assistant configuration messages; can be used if HA missed one or more of these messages.
|MQTT_Delete_Rebuild                       | Idem, but first deletes all Home Assistant configuration messages from (any of) your P1P2MQTT bridge(s), followed by a rebuild of this bridge (but not any others if you have multiple). Useful after a firmware upgrade to delete old entities. Button is disabled for ca 2 minutes after ESP restart of after a MQTT reconnect.
| R1Toffset_Mid                            | Correction value for temperature sensor R1T (after heat exchanger, before gas boiler or backup heater). If your system is not heating or cooling, but water flow is high, R1T and R2T and R4T should give the same temperature reading. Many users report temperature differences, leading to wrong heat production and wrong COP calculations. Tune these values such that Power_Heatpump and Power_Gasboiler (/BUH) are minimal when flow is high (during the start-up phase of your system)
| R2Toffset_LWT                            | Correction value for temperature sensor R2T (leaving water temperature)
| R4Toffset_RWT                            | Correction value for temperature sensor R4T (return water temperature)
| Restart_P1P2Monitor_ATmega               | Restarts ATmega. Should not be needed, but can be used to quickly increase write budget
| Restart_P1P2MQTT_ESP                     | Restarts ESP. Does factory_reset if scheduled. Should not be needed, but can be attempted in case of communication or other problems.
| RToffset_Room                            | Can be used to offset reported room temperature settings, has no other effects
| Write_Budget                             | Write budget limits the amount of control operations to reduce memory wear; can be increased manually if needed
| Write_Budget_Period                      | Write budget is regularly increased, by default once every hour. Period is in minutes. For Daikin E-systems, it is likely that flash wear levling is implemented to it is likely safe to reduce the period
| Error_Budget                             | Indicates how many read errors are accepted before switching Control_Function and Counter_Request_Function off. Should be 20, or a bit lower after a system restart. Errors should not be present, except for a few messages when the Daikin system restarts.
| ESP_EEPROM_Saved                         | Indicates whether all changes made have been saved to EEPROM
| ESP_Ectory_Reset_Scheduled               | Indicates if a factory reset is scheduled for the next ESP reset
| ESP_Throttling                           | Indicates that the ESP is throttling data - messages can be missing or delayed. Happens after an ESP restart or after a MQTT disconnect/reconnect action
| ESP_Waiting_For_Counters_D13             | Indicates that the ESP is waiting for consumption/production counters to store these counters for COP_Before_Bridge/COP_After_Bridge calculation. Counters will be read when (1) Counter_Request_Function is switched on, (2) Daikin system is restarted, (3) room thermostat requests counters, or (4) EEPROM_ESP_Set_Cons_Prod_Counters is pressed.
| V_Interfce                               | Reports voltage on P1/P2 interface for bus-powered devices. For bridges powered from a DC power supply, reports DC voltage.
| Writes_Refused_Budget                    | Indicates how many writes have been refused due to missing write budget; should be 0
| Writes_Refused_Busy                      | Indicates how many writes have been refused due to lack of buffering; should be 0

##### HC_Mode

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Altherma_On                              | Switches Daikin on/off
| Daikin_Defrost_Request                   | Requests Daikin to defrost; Daikin may schedule a defrost depending on environmental conditions
| Heating_Cooling_Auto                     | Switches system between Heating, Cooling, and Auto
| Program_WD_Abs                           | Switches system between Abs (absolute LWT), WD (weather-dependent LWT), Abs+prog (+clock program), or WD+prog
| BUH_2                                    | Shows status of 2nd backup heater 
| Circulation_Pump                         | Shows status of circulation pump for LWT
| Climate_Cooling                          | Shows whether climate for cooling is active
| Climate_HC_Auto                          | Shows whether climate will change automatically between heating and cooling
| Climate_Heating                          | Shows whether climate for heating is active
| Compressor                               | Shows whether compressor is on
| Date_Time_Daikin                         | Reports Daikin internal time
| Defrost_Active                           | Reports whether outdoor unit is defrosting
| ErrorCode1/2/Subcode                     | Shows Daikin error report. Encoding largely unknown. If you see any error, please share codes and the error code on your room thermostat
| Gasboiler_Active                         | Shows whether gas boiler (on all-electric: backup heater) is active
| Heating_Only                             | Indicates that system has no cooling option
| Preferential_Mode                        | Indicates that preferential mode on hybrid system is actived (switch on X5M-3,4)
| Valve_Zone_Add                           | ? Indicates status of valve for additional zone
| Valve_Zone_Main                          | ? Indicates status of valve for main zone

##### HC_Room

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Room_Heating                             | In RT mode, controls room temperature setpoint, on/off, and reports room temperature
| Room_Cooling                             | In RT mode, controls room temperature setpoint, on/off, and reports room temperature
| Room_Heating_Setpoint                    | Setpoint for room temperature if in heating mode
| Room_Cooling_Setpoint                    | Setpoint for room temperature if in cooling mode
| Room_Setpoint                            | Currently active setpoint for room temperature

##### HC_LWT

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Abs_Heating_Add or Deviation_Heating_Add | In LWT/Abs mode, control for on/off and for absolute setpoint of leaving water temperature. In LWT/WD mode, controls deviation from WD setpoint. Also reports actual LWT temperature.
| Abs_Cooling_Add or Deviation_Cooling_Add | Idem for cooling
| Abs_Heating                              | Absolute LWT setpoint heating mode
| Abs_Cooling                              | Absolute LWT setpoint cooling mode
| Deviation_Heating                        | Deviation from WD curve if in heating mode
| Deviation_Cooling                        | Deviation from WD curve if in cooling mode
| LWT_Setpoint                             | Currently active LWT setpoint

##### HC_LWT2

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Same as for HC_LWT, with _Add added      | Idem as for HC_LWT, but for additional zone

##### HC_Quiet

On (some?) newer Altherma 3 R models, Quiet\_Level monitors the current quiet status but controls do not work.

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Quiet_Level                              | Switches unit to quiet level 0 (off), 1, 2, or 3 (most silent). Can be used to limit power consumption
| Quiet_Level_When_On                      | Quiet level when selected on room thermostat
| Quiet_Mode                               | Switches quiet mode on or off

##### HC_Prices

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Gas                                      | Can be used to modify gas price
| Electricity                              | Shows current electricity price (based on clock programs, predetermined low/mid/high prices can only be modified via field settings)

##### HC_FieldSettings

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Daikin_Restart_Careful                   | Restarts Daikin. Disabled if Daikin is on. Also restarts ESP and ATmega (unless separately powered). Required to make field setting changes effective.
| Number_of_Zones                          | Indicates whether additional zone is used. Corresponds to menu A.2.1.8 or field setting [7-02].
| Overshoot                                | Maximum allowed LWT overshoot. Unfortunately cannot be changed on hybrid systems (even though max is reported to be 4, it refuses to change this value). Default value of 1 leads to cycling. Recommended to set to 4 on all-electric systems. Corresponds to field setting [9-04].
| RT_LWT                                   | Switches between RT (room temperature) and LWT (leaving water temperature) mode. In RT mode, LWT control is not possible via room thermostat, but is still possible via P1P2MQTT bridge (in selected Abs or WD mode).
| various entities                         | overview of all current field settings with description and menu entry (based on hybrid model), for example:
| 0_0D_A47_DHW_WD_Curve_High_Amb           |
| 0_0E_A47_DHW_WD_Curve_Low_Amb            |
| 1_0A_A64_Averaging_Time                  |
| C_03_Bivalent_Activation_Temperature     |

##### HC_Power

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Consumption_BUH                          | (not supported yet) gas consumption gas boiler or electricity consumption backup heater
| Consumption_Heatpump                     | External electricity meter input as reported via MQTT P1P2/P/meter/U/9/Electricity_Power in Watt
| Production_Gasboiler                     | Gas boiler or backup heater heat production (flow * delta-T, with offset correction on temperature sensors)
| Production_Heatpump                      | Heat pump (without backup heater) heat production (flow * delta-T, with offset correction on temperature sensors)

Note: Consumption_Heatpump is based on P1P2/P/meter/U/9/Electricity_Power which is a MQTT topic to be provided by an external electricity meter for the outside unit's compressor (so without BUH). It is required for the calculation of COP_Realtime.

##### HC_COP

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| COP_Lifetime                             | Average COP over lifetime of Daikin, based on Daikin reported kWh counters
| COP_Before_Bridge                        | Average COP over lifetime of Daikin until P1P2MQTT bridge locked consumtion/production counters (EEPROM_ESP_Set_Cons_Prod_Counters)
| COP_Before_Bridge                        | Average COP over lifetime of Daikin afterP1P2MQTT bridge locked consumtion/production counters
| COP_Realtime                             | Real-time COP based on Production_Heatpump/Consumption_Heatpump

Note: COP_Realtime is only calculated if Consumption_Heatpump is (regularly) provided by an external electricity meter via MQTT.

##### HC_Meters

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Various                                  | kWh/hours/start counters, only updated every minute if Counter_Request_Function is on, otherwise only if room thermostat asks for these counters, for example:
| Electricity_Consumed_Backup_DHW          |
| Electricity_Consumed_Backup_Heating      |
| Electricity_Consumed_Compressor_Heating  |
| Electricity_Consumed_Compressor_Cooling  |
| Energy_Produced_Compressor_Cooling       |
| Hours_Backup1_DHW                        |
| Hours_Circulation_Pump                   |
| Hours_Compressor_Heating                 |
| Starts_Compressor                        |
| Starts_Gasboiler                         |

##### HC_Sensors

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Flow                                     | Water circulation flow (some systems report 0.0, making production calculation impossible)
| Temperature_Outside                      | Outside temperature for WD calculation, based on external temperature sensor or outside unit
| Temperature_Outside_Unit                 | Outside unit temperature
| Temperature_R1T_HP2Gas_Water             | Water temperature after heat exchanger and before backup heater or gas boiler
| Temperature_R2T_Leaving_Water            | Leaving water temperature (after backup heater or gas boiler)
| Temperature_R4T_Return_Waterr            | Return water temperature
| Temperature_Refrigerant_2                | Refrigerant temperature after water/water heat exchanger
| Temperature_Room                         | Temperature reported by room thermostat
| Water_Pressure                           | Circulation water pressure (only supported by a few systems)

##### HC_DHW

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| DHW_Setpoint                             | Controls on/off status and DHW setpoint
| DHW_Boost                                | Controls boost status
| DHW                                      | Current on/off status
| DHW_Demand                               | Domestic hot water flow sensor for hybrid gas boiler (not supported on tank systems?)
| DHW_Related_Q                            | ?
| DHW_Setpoint                             | Hot water setpoint

##### HC_UI

Simulates functions of the auxiliary controller

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| LCD_Off                                  | Switches (simulated) LCD backlight of bridge off
| LCD_On                                   | Switches (simulated) LCD backlight of bridge on (LCD backlights of main and auxiliary controller cannot be both on)
| Mode_0_Normal_User                       | Puts P1P2MQTT bridge into normal user mode
| Mode_1_Advanced_User                     | Puts P1P2MQTT bridge into advanced user mode
| Mode_2_Installer                         | Puts P1P2MQTT bridge into installer mode (if/when main controller is not in installer mode)

#### Control and entity overview (Daikin AC / F-series)

This section is still very much raw format and likely to contains errors; too many entities are currently reported in HA, also for further reverse engineering. Some simplification of the HA interface and further reverse engineering is required. Feedback is welcome.

##### HC_Setup

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| HA_Setup                                 | Enables proper initial set-up of HA dashboards (or otherwise controls may be incomplete), and enables factory_reset button. After setup, strongly recommended to switch to off. No need to add to any dashboard.

##### HC_Bridge

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Control_Function                         | Determines whether P1P2MQTT bridge operates in auxiliary control mode. Not needed for monitoring, but required for controlling your system
| Model                                    | Allows protocol selection based on major version of Daikin unit; control works only correct configured
| Model_suggestion                         | Recommended protocol selection
| EEPROM_ESP_Save_Changes                  | Saves changes made to ESP EEPROM
| EEPROM_ESP_Undo_Changes                  | Undo changes made to EEPROM (not possible after saving)
| Factory_Reset_ESP_After_Restart          | Use with care! This will schedule a factory reset on the next ESP restart. It will then reset all ESP-stored values to their original value, including device name, bridge name, AP and MQTT server credentials, and more. Can be cancelled with Factory_Reset_ESP_Cancel but cannot be undone after ESP restart. Button is disabled if HA_Setup is switched off.
| Factory_Reset_ESP_Cancel                 | Unschedule any scheduled factory reset
|MQTT_Rebuild                              | Retransmits all Home Assistant configuration messages; can be used if HA missed one or more of these messages.
|MQTT_Delete_Rebuild                       | Idem, but first deletes all Home Assistant configuration messages from (any of) your P1P2MQTT bridge(s), followed by a rebuild of this bridge (but not any others if you have multiple). Useful after a firmware upgrade to delete old entities.
| Restart_P1P2Monitor_ATmega               | Restarts ATmega. Should not be needed, but can be used to quickly increase write budget
| Restart_P1P2MQTT_ESP                     | Restarts ESP. Does factory_reset if scheduled. Should not be needed, but can be attempted in case of communication or other problems.
| Write_Budget                             | Write budget limits the amount of control operations to reduce memory wear; can be increased manually if needed
| Write_Budget_Period                      | Write budget is regularly increased, by default once every hour. Period is in minutes. For Daikin E-systems, it is likely that flash wear levling is implemented to it is likely safe to reduce the period
| Error_Budget                             | Indicates how many read errors are accepted before switching Control_Function and Counter_Request_Function off. Should be 20, or a bit lower after a system restart. Errors should not be present, except for a few messages when the Daikin system restarts.
| ESP_EEPROM_Saved                         | Indicates whether all changes made have been saved to EEPROM
| ESP_Ectory_Reset_Scheduled               | Indicates if a factory reset is scheduled for the next ESP reset
| ESP_Throttling                           | Indicates that the ESP is throttling data - messages can be missing or delayed. Happens after an ESP restart or after a MQTT disconnect/reconnect action
| V_Interfce                               | Reports voltage on P1/P2 interface for bus-powered devices. For bridges powered from a DC power supply, reports DC voltage.
| Writes_Refused_Budget                    | Indicates how many writes have been refused due to missing write budget; should be 0
| Writes_Refused_Busy                      | Indicates how many writes have been refused due to lack of buffering; should be 0

##### HC_Mode

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Power_Request                            | Power request from room thermostat
| Power                                    | System on/off status
| Heatpump_On                              | Heat pump status
| Target_Operating_Mode                    | Target operating mode from room thermostat to aux controller
| Target_Operating_Mode_0                  | Target operating mode from room thermostat to indoor unit
| Target_Operating_Mode_1                  | Target operating mode by indoor unit
| Actual_Operating_Mode                    | Actual operating mode from room thermostat to aux controller
| Actual_Operating_Mode_0                  | Actual operating mode from room thermostat to indoor unit
| Actual_Operating_Mode_1                  | Actual operating mode by indoor unit
| System_Status                            | Status stand-by/compressor/fan/off
| Zone_Status                              | ?

##### HC_Setpoints

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Setpoint_Heating                         | Controls room temperature setpoint in heating mode
| Setpoint_Cooling                         | Controls room temperature setpoint in cooling mode
| Setpoint_Temperature                     | Current setpoint for room temperature
| Setpoint_Temperature_1                   | Setpoint for room temperature copied back by indoor unit
| Fan_Speed                                | Setpoint for fan speed
| Fan_Speed_1                              | Setpoint for fan speed copied back by indoor unit
| Fan_Speed_Cooling                        | Setpoint for fan speed cooling from room thermostat to aux controller
| Fan_Speed_Heating                        | Setpoint for fan speed heating from room thermostat to aux controller

##### HC_Sensors

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Temperature_Inside_Air_Intake            | Air intake temperature
| Temperature_Heat_Exchanger               | 

##### HC_Thermistors

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Th1                                      | Temperature sensor, function?
| Th2                                      | Temperature sensor, function?
| Th3                                      | Temperature sensor, function?
| Th4                                      | Temperature sensor, function?
| Th5                                      | Temperature sensor, function?
| Th6                                      | Temperature sensor, function?

##### HC_Filter

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Filter_Related                           | ? 000011-3
| Filter_Alarm_Reset_Q                     | ?

##### HC_Unknown

| Function                                 | Description
|:-----------------------------------------|:----------------------------------------------------------|
| Unknown_xxxxxx_xx                        | Many individual protocol bytes, function unknown, for reverse engineering purposes
| Error_Setting_1_Q                        | Function unknown, for reverse engineering purposes
