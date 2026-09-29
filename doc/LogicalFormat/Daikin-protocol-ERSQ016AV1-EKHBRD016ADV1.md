# Daikin ERSQ016AV1 + EKHBRD016ADV1 compatibility notes

This note documents the initial, conservative compatibility profile for the ERSQ016AV1 outdoor unit combined with the EKHBRD016ADV1 indoor/controller platform.

The profile intentionally does not try to fully decode this platform yet. It promotes only the bits that are strongly confirmed and suppresses a few temperature entities that are clearly misleading on this hardware family.

## How to enable the profile

The current implementation uses a manual bridge profile so existing E-series behaviour stays unchanged by default.

- Set bridge parameter `P53` to `1` to enable the `EKHBRD ADV1 profile`
- Set bridge parameter `P53` back to `0` to return to generic E-series decoding
- Use `D5` to save the setting to EEPROM if you want it to survive reboot

## Trusted mappings

For this profile, the repeating `0x10` request/response family is treated as the trusted source for:

- `Heating_Enabled`
- `DHW_Enabled`

Confirmed request matrix:

- `000010010101...` = heat on, DHW on
- `000010010100...` = heat on, DHW off
- `000010000101...` = heat off, DHW on
- `000010000100...` = heat off, DHW off

Confirmed response matrix:

- `4000100100110130...` = heat on, DHW on
- `4000100100110030...` = heat on, DHW off
- `4000100000110130...` = heat off, DHW on
- `4000100000110030...` = heat off, DHW off

Observed bit positions used by the profile:

- request `000010...`: payload byte 0 bit 0 = heating enabled
- request `000010...`: payload byte 2 bit 0 = DHW enabled
- response `400010...`: payload byte 0 bit 0 = heating enabled
- response `400010...`: payload byte 3 bit 0 = DHW enabled

The implementation publishes the named booleans from both the request and response family so either side can refresh the same MQTT topic.

## Sample frames

Steady heat on, DHW on:

- `00001001010130000A0020000000000200004001B1`
- `4000100100110130000A00200018000000000002000939`

Steady heat on, DHW off:

- `00001001010030000A002000000000020000400131`
- `4000100100110030000A002000180000000000020009E0`

Steady heat off, DHW on:

- request family prefix: `000010000101...`
- response family prefix: `4000100000110130...`

Steady heat off, DHW off:

- request family prefix: `000010000100...`
- response family prefix: `4000100000110030...`

Observed one-shot transition or acknowledge frame:

- `80001001010030000A0020000000000200000001A4`

The current profile documents this frame but does not rely on it for steady-state decoding.

## Intentionally suppressed entities

The generic E-series mapping currently produces clearly implausible values for some water-side temperatures on this platform. To avoid presenting bogus values as trusted sensors, the profile suppresses these named entities:

- `Temperature_R2T_Leaving_Water`
- `Temperature_R4T_Return_Water`
- `Temperature_R5T_DHW_Tank`
- `Temperature_R1T_Leaving_Water`
- `Temperature_R1T_HP2Gas_Water`

For the same reason, Home Assistant climate entities created by the bridge do not advertise those suppressed temperatures as current-temperature topics when this profile is active.

## What remains uncertain

- The `400011...` family appears to contain live sensor/process data, but the current generic mapping is not reliable enough yet for this model
- The exact meaning of several water-side temperatures on this platform is still unresolved
- The `0x80` transition family needs more captures before it should drive state
- Automatic model detection is not implemented yet for this profile

## Suggested reverse-engineering next steps

- Collect longer logs around heating-only, DHW-only, and compressor-off states
- Correlate `400011...` bytes with independently measured leaving/return water temperatures
- Capture restart traffic including `A1` and `B1` model-identification frames to evaluate safe auto-detection
