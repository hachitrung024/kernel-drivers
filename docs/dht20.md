# DHT20 — Humidity & Temperature Sensor (ASAIR)
> Datasheet v1.0, May 2021 | I²C | Drop-in upgrade for DHT11

---

## Pinout

| Pin | Name | Description              |
|-----|------|--------------------------|
| 1   | VDD  | Power supply (2.2–5.5V)  |
| 2   | SDA  | Serial Data (bidirectional) |
| 3   | GND  | Ground                   |
| 4   | SCL  | Serial Clock (bidirectional) |

**Pull-up resistor:** 4.7kΩ on SDA and SCL (MCU drives low only)  
**Decoupling cap:** 100nF between VDD and GND, as close to sensor as possible

---

## Key Specs

### Humidity
| Parameter      | Value         | Unit    |
|----------------|---------------|---------|
| Accuracy       | ±3            | %RH     |
| Resolution     | 0.024         | %RH     |
| Range          | 0–100         | %RH     |
| Response time  | <8 (τ 63%)    | s       |
| Long-term drift| <0.5          | %RH/yr  |

### Temperature
| Parameter      | Value         | Unit    |
|----------------|---------------|---------|
| Accuracy       | ±0.5          | °C      |
| Resolution     | 0.01          | °C      |
| Range          | -40–80        | °C      |
| Response time  | 5–30 (τ 63%)  | s       |
| Long-term drift| <0.04         | °C/yr   |

### Electrical
| Parameter         | Min  | Typical | Max  | Unit |
|-------------------|------|---------|------|------|
| Supply voltage    | 2.2  | 3.3     | 5.5  | V    |
| Current (sleep)   | —    | 250     | —    | nA   |
| Current (measure) | —    | 980     | —    | µA   |

---

## I²C Interface

- **Device address:** `0x38` (7-bit, fixed)
- **Read bit:** `1`, Write bit: `0`
- **SCL frequency:** 0–100 kHz (Standard), 0–400 kHz (Fast mode)

### I²C Timing (Fast Mode)
| Symbol   | Parameter          | Min  | Max  | Unit |
|----------|--------------------|------|------|------|
| f(SCL)   | Clock frequency    | 0    | 400  | kHz  |
| tw(SCLL) | SCL low time       | 1.3  | —    | µs   |
| tw(SCLH) | SCL high time      | 0.6  | —    | µs   |
| tsu(SDA) | SDA setup time     | 100  | —    | ns   |
| th(SDA)  | SDA hold time      | 0.02 | 0.9  | µs   |

---

## Communication Protocol

### Status Register Bits (read with cmd `0x71`)
| Bit    | Name        | Description                        |
|--------|-------------|------------------------------------|
| Bit[7] | Busy        | `1` = measuring, `0` = idle        |
| Bit[3] | CAL Enable  | `1` = calibrated, `0` = not        |
| others | —           | Reserved                           |

---

## Sensor Reading Process (Step-by-Step)

```
Power ON
  └─> Wait ≥ 100ms (SCL high)
      └─> Send 0x71 → read 1 byte status
          ├─ If (status & 0x18) != 0x18:
          │    Initialize registers 0x1B, 0x1C, 0x1E  (see datasheet routine)
          └─ If OK:
               Wait 10ms
               └─> Send cmd 0xAC, 0x33, 0x00  (trigger measurement)
                   └─> Wait ≥ 80ms
                       └─> Read status byte
                           ├─ If Bit[7] == 1: still busy, wait more
                           └─ If Bit[7] == 0: ready
                               └─> Read 6 bytes (+ optional 1 CRC byte)
                                   └─> Calculate RH and T
```

### Trigger Measurement Command
```
Write to 0x38:  0xAC  0x33  0x00
```

### Read Response: 6 Bytes
```
Byte 0: Status
Byte 1: Humidity[19:12]
Byte 2: Humidity[11:4]
Byte 3: Humidity[3:0] | Temperature[19:16]   ← shared byte
Byte 4: Temperature[15:8]
Byte 5: Temperature[7:0]
Byte 6: CRC (optional)
```

> **Note:** Bytes 1–3 upper nibble = 20-bit humidity raw (S_RH)  
> Bytes 3 lower nibble + bytes 4–5 = 20-bit temperature raw (S_T)

---

## Signal Conversion Formulas

### Humidity
```
S_RH = (Byte1 << 12) | (Byte2 << 4) | (Byte3 >> 4)

RH [%] = (S_RH / 2^20) * 100
```

### Temperature
```
S_T = ((Byte3 & 0x0F) << 16) | (Byte4 << 8) | Byte5

T [°C] = (S_T / 2^20) * 200 - 50
```

---

## CRC Check (optional)
- Initial value: `0xFF`
- Polynomial: `CRC[7:0] = 1 + X^4 + X^5 + X^8`
- Covers bytes 0–5 (6 bytes)

---

## Timing Summary (important delays)

| Event                        | Delay      |
|------------------------------|------------|
| Power-on stabilization       | ≥ 100 ms   |
| After init → trigger cmd     | 10 ms      |
| After trigger cmd → read     | ≥ 80 ms    |
| Recommended measurement interval | ≥ 2 s  |
| Max active duty cycle        | ≤ 10%      |

---

## Typical Application Circuit Notes
1. MCU VDD must match sensor VDD
2. After power-on: set SCL/SDA high **after 5ms**
3. Place 100nF decoupling cap as close to sensor as possible
4. Do not use reflow/wave soldering — manual only, ≤ 5s at max 300°C
5. After soldering: store at >75%RH for 12h (or >40%RH for 2 days) to rehydrate

---

## Absolute Maximum Ratings
| Parameter           | Min   | Max       | Unit |
|---------------------|-------|-----------|------|
| VDD to GND          | -0.3  | 5.5       | V    |
| SDA/SCL to GND      | -0.3  | VDD + 0.3 | V    |
| Input current/pin   | -10   | 10        | mA   |

---

## Source
ASAIR DHT20 Datasheet v1.0 — www.aosong.com
