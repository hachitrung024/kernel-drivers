## Implementation Logs

### Task 6 — sysfs (2026-04-23)

**Files touched:** [iio/dht20/dht20.c](iio/dht20/dht20.c)

**What was added:**
- `dht20_measure(data, *t_c100, *rh_c100)` — thin wrapper around `dht20_read_sensor` + `dht20_parse`, shared by both show functions.
- `temperature_show` / `humidity_show` — each resolves `dht20_data` via `dev_get_drvdata`, calls `dht20_measure`, and emits `%d.%02d\n` with `abs()` on the remainder for sub-zero temps. Each read triggers a fresh measurement.
- `DEVICE_ATTR_RO(temperature)`, `DEVICE_ATTR_RO(humidity)`, `dht20_attrs[]`, and `ATTRIBUTE_GROUPS(dht20)` boilerplate.
- `.dev_groups = dht20_groups` on `struct i2c_driver.driver` — the driver core creates/removes the files around probe/unbind automatically, so `dht20_probe` and `dht20_remove` stayed untouched apart from dropping the debug read.
- Removed the probe-time `dht20_read_sensor` + `dht20_parse` + `dev_info("T=... C, RH=... %")` scaffolding from Task 5. Probe now only logs `dht20 probed (addr=0x38)`.

**Design notes:**
- `dev_get_drvdata` works directly because `i2c_set_clientdata` stores the same pointer — no `to_i2c_client` hop needed.
- No caching. Each `cat` triggers a full trigger → 80 ms sleep → read → parse, ~80 ms per attribute. A paired atomic read or IIO-style buffering is out of scope for this task; concurrent readers are still serialized by `data->lock` inside `dht20_read_sensor`.
- `sysfs_emit` is the modern replacement for `scnprintf` into the sysfs buffer — bounds-safe and the standard choice for new show functions.

**Verification on Pi (10.42.0.252):**
- Built cleanly against `6.12.47+rpt-rpi-v8` headers.
- After `new_device`, `ls /sys/bus/i2c/devices/1-0038/` contained both `temperature` and `humidity`.
- Back-to-back reads drifted: `29.42` → `29.50` °C and `54.35` → `54.40` %RH, confirming each `cat` triggers a fresh measurement (no cached value).
- `time cat temperature humidity` took 168 ms wall — ~84 ms per attribute, consistent with the 80 ms `msleep` + I²C overhead.
- `dmesg` showed only `dht20 probed (addr=0x38)` on bind and `dht20 removed` on unbind; no parse/CRC warnings.
- `delete_device` + `rmmod` tore down cleanly; the sysfs files disappeared with the device because they came from `.dev_groups`.

### Task 5 — parsing (2026-04-23)

**Files touched:** [iio/dht20/dht20.c](iio/dht20/dht20.c)

**What was added:**
- `DHT20_CRC_INIT` (0xFF) and `DHT20_CRC_POLY` (0x31) constants.
- `dht20_crc8(data, len)` — bit-by-bit MSB-first CRC-8. Skipped `<linux/crc8.h>` (256-byte table infra is overkill for a 6-byte input hit once per read).
- `dht20_parse(buf, *t_c100, *rh_c100)` — verifies `buf[6] == crc(buf[0..5])` and returns `-EIO` on mismatch (per CLAUDE.md: never surface bad readings). Extracts the 20-bit `S_RH` / `S_T` fields (byte 3 is shared) and converts to scaled integers ×100 with `u64` multiplies to avoid 32-bit overflow (`S_T * 20000` can reach ~2.1 × 10^10).
- Replaced the probe-time `raw: %*ph` dev_info with a parsed `T=%d.%02d C, RH=%d.%02d %%` line. `abs()` on the remainder so sub-zero values print correctly.

**Design notes:**
- Formulas from CLAUDE.md had to be re-parenthesized: `(S_T * 20000) >> 20 - 5000` as written would parse as `>> (20 - 5000)` because `>>` has lower precedence than `-` in C.
- Parse signature takes two `s32 *` outputs so Task 6's sysfs handlers can share it directly.

**Verification on Pi (10.42.0.252):**
- Built cleanly against `6.12.47+rpt-rpi-v8` headers.
- `dmesg` showed `dht20 1-0038: T=29.35 C, RH=53.17 %` — plausible for the room and consistent with the Task 4 reading (~28.4 °C, ~55.9 %RH) taken earlier in the day.
- No `parse failed` warning, so CRC verified against the real sensor byte. Hand-check on the Task 4 capture (`1c 8f 40 c6 45 9a f0`): `S_RH = 0x8F40C → 55.91 %`, `S_T = 0x6459A → 28.20 °C` — matched.
- `delete_device` + `rmmod` unbound cleanly; `dht20_remove` fired.

### Task 4 — i2c communication (2026-04-23)

**Files touched:** [iio/dht20/dht20.c](iio/dht20/dht20.c)

**What was added:**
- `#include <linux/delay.h>` for `msleep()`.
- Constants: `DHT20_TRIGGER_DELAY_MS` (80), `DHT20_RESP_LEN` (7), and `dht20_trigger_cmd[3] = { 0xAC, 0x33, 0x00 }`.
- `dht20_read_sensor(struct dht20_data *data, u8 *buf)` helper — takes the mutex, sends the 3-byte trigger, `msleep(80)`, reads 7 bytes, releases the mutex. Single `unlock:` label so the release path is unambiguous. Returns `0` or a negative errno.
- Both `i2c_master_send` and `i2c_master_recv` return values are checked: negative returns are propagated; short transfers return `-EIO`.
- Temporary debug call at the end of `dht20_probe` that dumps the raw 7 bytes via `%*ph` (to be removed when Task 6 wires up sysfs).

**Design notes:**
- Response buffer lives on the caller's stack (not in `struct dht20_data`) since it's transient.
- The entire trigger→sleep→read runs under `data->lock`, matching the CLAUDE.md rule about serializing the full sequence.
- Status-byte init dance (`0x71` + regs `0x1B/0x1C/0x1E`) was intentionally skipped — not in the task list, and the observed status byte (`0x1c`) already has the CAL-enable bit set, so the sensor is calibrated out of the box on this unit.

**Verification on Pi (10.42.0.252):**
- Built cleanly against `6.12.47+rpt-rpi-v8` headers.
- Instantiated with `echo dht20 0x38 > /sys/bus/i2c/devices/i2c-1/new_device`.
- `dmesg` showed `raw: 1c 8f 40 c6 45 9a f0`. Spot-check with datasheet formulas gave ~55.9 %RH and ~28.4 °C — plausible for the room.
- Elapsed time between "probed" and "raw:" log lines was ~82 ms, consistent with the 80 ms `msleep` + I²C transfer.
- `delete_device` + `rmmod` unbound cleanly; `dht20_remove` fired.
