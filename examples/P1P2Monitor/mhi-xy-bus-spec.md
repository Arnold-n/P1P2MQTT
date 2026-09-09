# MHI X/Y Bus Protocol Specification

**Source:** Reverse engineering (exablue GmbH report #2018-08-13-001 + live capture analysis)  
**Status:** Draft — incomplete datablock (bytes 7–15) not fully decoded  
**Tested hardware:** FDUM71VF, RC-E5, RCN-TC-24W-ER  

---

## 1. Physical Layer

| Parameter | Value |
|---|---|
| Bus type | 2-wire, polarity-independent |
| Terminals | X and Y |
| Idle voltage | 13–18 V (measured: 16.9 V) |
| Available current | ≥ 80 mA (parasitic power supply supported) |
| Max bus length | 600 m |
| Short circuit protection | Yes (triggers error code E1) |

---

## 2. Frame Structure

Each frame is **16 bytes** (48 symbols) and takes **59.956 ms** to transmit.

### Inter-frame timing

| Condition | Silence duration |
|---|---|
| Normal operation | ~300 ms (minimum 250 ms) |
| After command (fast propagation) | ~196–250 ms |
| System power-up | Up to 30 seconds before first frame |

### Frame layout

| Byte(s) | Field | Description |
|---|---|---|
| 1–2 | Receiver address | 16-bit address of the target device |
| 3 | Mode / power / swing | Operating mode, on/off state, and vane swing flag |
| 4 | Fan speed / vane position | Fan level and fixed vane position |
| 5 | Setpoint temperature | Target temperature |
| 6 | Unknown temperature | Reported by AC unit; `0xFF` in RC frames |
| 7–15 | Data block | Status/operational data (partially unknown) |
| 16 | Checksum | `sum(byte1 .. byte15) % 255` |

---

## 3. Device Addresses

| Address | Device | Role |
|---|---|---|
| `0x8001` | RCN-TC-24W-ER | Master |
| `0x0007` | FDUM71VF indoor unit | Slave |
| `0x1FF7` | Master poll address for RC-E5 | — |
| `0x9FF7` | RC-E5 wired remote controller | Slave (reply address) |

### Address convention

The RC-E5 uses two addresses:
- `0x1FF7` — the address the master uses to poll it
- `0x9FF7` — the address the RC-E5 uses in its reply frames

The high nibble of byte 1 flips between request and reply (`1F` → `9F`), while byte 2 (`F7`) remains identical. The checksum difference between a request and its reply is always exactly `0x80` due to this single-byte change.

---

## 4. Bus Session / Communication Flow

### Normal cycle (no RC-E5)

```
[Master 0x8001] → 0007  poll FDUM           (gap: ~3.133 s)
[Slave  0x0007] → 8001  FDUM reply          (gap: ~0.250 s)
[Master 0x8001] → 0X07  scan probe          (gap: ~2.829 s)
[Master 0x8001] → 1FF7  poll RC-E5          (gap: ~3.133 s)
                         [silence — no RC-E5]
```

### Normal cycle (with RC-E5)

```
[Master 0x8001] → 0007  poll FDUM           (gap: ~3.133 s)
[Slave  0x0007] → 8001  FDUM reply          (gap: ~0.250 s)
[Master 0x8001] → 0X07  scan probe          (gap: ~2.829 s)
[Master 0x8001] → 1FF7  poll RC-E5          (gap: ~3.133 s)
[Slave  0x9FF7] → reply RC-E5 reply         (gap: ~0.250 s)
```

### Command cycle (RC-E5 sends a command)

When RC-E5 changes state (power, temperature), the master immediately rebroadcasts to the FDUM without waiting for the normal 2.8 s cadence:

```
[Master 0x8001] → 1FF7  poll RC-E5 (with new state)
[Slave  0x9FF7] → reply RC-E5 echoes new state    (gap: ~0.250 s)
[Master 0x8001] → 0007  immediate FDUM update      (gap: ~0.196–0.245 s)
[Slave  0x0007] → 8001  FDUM acknowledges          (gap: ~0.250 s)
```

### RC-E5 presence detection

The `9FF7` reply frame is a reliable indicator of RC-E5 bus presence:
- `9FF7` present → RC-E5 connected and responding
- `9FF7` absent → RC-E5 disconnected or offline

---

## 5. Bus Scanning

The master continuously scans for additional slave devices by probing candidate addresses. A device is discovered when it replies within ~0.250 s of being polled.

### Scan pattern

Starting address: `0x0107`

1. Inner loop: increment by `0x1000` four times → `0x0107`, `0x1107`, `0x2107`, `0x3107`
2. Outer loop: add `0x0100`, repeat inner loop → continues to `0x3F07`
3. Then: `0x0017`..`0x0F17`, `0x0027`..`0x0F27`, ..., `0x0F07`..`0x0FF7`
4. Then: `0x1FF7`, `0x2FF7`, `0x3FF7` (with different data block)
5. Cycle repeats

### Scan timing

| Parameter | Value |
|---|---|
| Time per address probe | ~10 seconds |
| Full cycle duration | ~29 minutes |
| Scan rate after power loss | Faster |

---

## 6. Field Definitions

### Byte 3 — Mode / Power / Swing

| Bit | Field | Values |
|---|---|---|
| 1 (LSB) | On/Off | `0` = off, `1` = on |
| 2–7 | Mode | See table below |
| 6 | Vane swing | `0` = fixed position, `1` = swing |
| 8 (MSB) | Unknown | `1` = default, `0` = not observed |

Note: bit 6 overlaps with the mode field. Swing is only meaningful when the mode bits allow vane control.

#### Mode bit patterns (bits 2–7)

| Bits 2–7 | Mode |
|---|---|
| `010101` | Cooling only |
| `010111` | Fan only |
| `111001` | Heating only |
| `010001` | Auto |
| `010011` | Dehumidify |

#### Observed byte 3 values

| Value | Binary | Mode | Power | Swing |
|---|---|---|---|---|
| `0xAA` | `10101010` | Cooling | OFF | No |
| `0xAB` | `10101011` | Cooling | ON | No |
| `0xA2` | `10100010` | Auto | OFF | No |
| `0xA3` | `10100011` | Auto | ON | No |
| `0xAF` | `10101111` | Fan only | ON | No |
| `0xEF` | `11101111` | Fan only | ON | Yes |

### Byte 4 — Fan Speed / Vane Position

Byte 4 encodes both fan speed (bits 3 and 1–0) and vane fixed position (bits 5–4). Bit 3 is always `1`.

#### Bit layout

| Bits | Field |
|---|---|
| 7–6 | Fan speed group (see below) |
| 5–4 | Vane position (when swing = off) |
| 3 | Always `1` |
| 2–0 | Fan speed detail |

#### Vane position (bits 5–4) — only active when byte 3 bit 6 = `0`

| Bits 5–4 | Vane position | Byte 4 (fan level 1) |
|---|---|---|
| `00` | Top | `0x88` |
| `01` | Mid-top | `0x98` |
| `10` | Mid-bottom | `0xA8` |
| `11` | Bottom | `0xB8` |

Note: when swing is active (byte 3 bit 6 = `1`), byte 4 vane bits are ignored. Byte 4 = `0x98` is used as the swing default.

#### Fan speed values

| Value | Fan speed |
|---|---|
| `0x88` / `0x98` / `0xA8` / `0xB8` | Level 1 (varies by vane position) |
| `0x99` | Level 2 |
| `0x9A` | Level 3 |
| `0xAA` | Off |

#### Complete vane / swing mapping

| Position | Byte 3 | Byte 4 | Swing active |
|---|---|---|---|
| Swing | `0xEF` | `0x98` | Yes |
| Top | `0xAF` | `0x88` | No |
| Mid-top | `0xAF` | `0x98` | No |
| Mid-bottom | `0xAF` | `0xA8` | No |
| Bottom | `0xAF` | `0xB8` | No |

### Byte 5 — Setpoint Temperature

Conversion formula: `(value - 0x80) / 2.0 = °C`

| Value | Temperature |
|---|---|
| `0xAC` | 22.0 °C |
| `0xAD` | 22.5 °C |
| `0xAE` | 23.0 °C |

Temperature is retained in memory after power off.

### Byte 6 — Unknown Temperature

Reported by the AC unit in reply frames. Set to `0xFF` in all RC/master frames.

Conversion formula (same as byte 5): `(value - 0x80) / 2.0 = °C`

Observed values suggest return air or coil temperature. Slowly drifts upward when unit is idle (e.g. 9.5 °C → 11.0 °C over ~5 minutes with compressor off).

### Bytes 7–15 — Data Block

Partially unknown. Normally `0x0000800000FFFFFFFF` in idle/off state. Transitions to live operational data when unit starts up:

| State | Example value |
|---|---|
| Idle / off | `0000800000FFFFFFFF` |
| Unit starting / running | `0000 0800 FF 35 10 08 01` |

### Byte 16 — Checksum

```
checksum = sum(byte1 + byte2 + ... + byte15) % 255
```

Note: modulo **255** (not 256). This is not a bitwise AND with `0xFF`.

#### Verified examples

| Frame | Sum (decimal) | Sum % 255 | Checksum |
|---|---|---|---|
| `1FF7AA98ADFF0000800000FFFFFFFF` | 2176 | 128 | `0x80` ✓ |
| `9FF7AA98ADFF0000800000FFFFFFFF` | 2304 | 0 | `0x00` ✓ |
| `0007AA98ADFF0000800000FFFFFFFF` | 1945 | 113 | `0x71` ✓ |
| `1FF7AB98ADFF0000800000FFFFFFFF` | 2177 | 129 | `0x81` ✓ |

The checksum difference between a master poll (`1FF7`) and its RC-E5 reply (`9FF7`) is always exactly `0x80`, because only byte 1 changes (`0x1F` → `0x9F`, difference = `0x80`).

---

## 7. Request / Reply Mapping (RC-E5 Example)

The RC-E5 is a pure echo slave — it echoes all payload bytes unchanged, only substituting its own reply address in byte 1.

| Byte | Master request (`1FF7`) | RC-E5 reply (`9FF7`) | Notes |
|---|---|---|---|
| 1 | `1F` | `9F` | Address high byte flips |
| 2 | `F7` | `F7` | Unchanged |
| 3 | command | echoed | Mode/power |
| 4 | command | echoed | Fan speed |
| 5 | command | echoed | Setpoint |
| 6 | `FF` | `FF` | No sensor on RC-E5 |
| 7–15 | data block | echoed | Unchanged |
| 16 | checksum | checksum | Recalculated (+0x80) |

### Command examples

**Unit off, 22.5 °C (baseline):**
```
Request: 1F F7 AA 98 AD FF 00 00 80 00 00 FF FF FF FF 80
Reply:   9F F7 AA 98 AD FF 00 00 80 00 00 FF FF FF FF 00
```

**Power ON:**
```
Request: 1F F7 AB 98 AD FF 00 00 80 00 00 FF FF FF FF 81
Reply:   9F F7 AB 98 AD FF 00 00 80 00 00 FF FF FF FF 01
                ↑ AA→AB (bit 0 set)
```

**Temperature change to 23.0 °C (unit on):**
```
Request: 1F F7 AB 98 AE FF 00 00 80 00 00 FF FF FF FF 82
Reply:   9F F7 AB 98 AE FF 00 00 80 00 00 FF FF FF FF 02
                       ↑ AD→AE (+0.5 °C)
```

**Power OFF (temperature retained):**
```
Request: 1F F7 AA 98 AE FF 00 00 80 00 00 FF FF FF FF 81
Reply:   9F F7 AA 98 AE FF 00 00 80 00 00 FF FF FF FF 01
                ↑ AB→AA (bit 0 cleared)
```

**Vane swing ON (fan only mode, on):**
```
Request: 1F F7 EF 98 AC FF 00 00 80 00 00 FF FF FF FF C4
Reply:   9F F7 EF 98 AC FF 00 00 80 00 00 FF FF FF FF 44
                ↑ AF→EF (bit 6 set)
```

**Vane fixed top:**
```
Request: 1F F7 AF 88 AC FF 00 00 80 00 00 FF FF FF FF 74
Reply:   9F F7 AF 88 AC FF 00 00 80 00 00 FF FF FF FF F4
                ↑ EF→AF (bit 6 cleared)    ↑ 98→88 (bits 5–4 = 00)
```

**Vane fixed mid-top:**
```
Request: 1F F7 AF 98 AC FF 00 00 80 00 00 FF FF FF FF 84
Reply:   9F F7 AF 98 AC FF 00 00 80 00 00 FF FF FF FF 04
                            ↑ bits 5–4 = 01
```

**Vane fixed mid-bottom:**
```
Request: 1F F7 AF A8 AC FF 00 00 80 00 00 FF FF FF FF 94
Reply:   9F F7 AF A8 AC FF 00 00 80 00 00 FF FF FF FF 14
                            ↑ bits 5–4 = 10
```

**Vane fixed bottom:**
```
Request: 1F F7 AF B8 AC FF 00 00 80 00 00 FF FF FF FF A4
Reply:   9F F7 AF B8 AC FF 00 00 80 00 00 FF FF FF FF 24
                            ↑ bits 5–4 = 11
```

---

## 8. Known Unknowns

| Item | Status |
|---|---|
| Bytes 7–15 data block full decode | Not complete |
| Byte 6 exact temperature source (coil / return air / other) | Uncertain |
| `0x1FF7` / `0x2FF7` / `0x3FF7` different data block content | Not captured |
| Master's own address in frame headers | Not observed transmitting as slave |
| Multiple simultaneous slave handling | Not tested |
| Full address space map | Scan cycle ~29 min, not fully observed |
