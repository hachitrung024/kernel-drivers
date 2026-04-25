## Tasks
 
- [OK] Task 1: verify hardware — i2cdetect -y 1 shows 0x38
- [OK] Task 2: minimal skeleton — probe/remove + module_i2c_driver
- [OK] Task 3: private data — struct dht20_data, devm_kzalloc, mutex
- [OK] Task 4: i2c communication — trigger, msleep, read 7 bytes, mutex wrap toàn bộ sequence
- [OK] Task 5: parsing — CRC check, raw to scaled integers, no float
- [OK] Task 6: sysfs — DEVICE_ATTR_RO temperature + humidity
- [ ] Task 7: probe init — msleep(100) power-on delay, read status via 0x71, return -ENODEV if (status & 0x18) != 0x18, msleep(10) before returning from probe
- [ ] Task 8: busy polling — after msleep(80), read Byte0 and check bit[7]; if still busy, retry up to 3 times with msleep(10) each; return -ETIMEDOUT if all retries exhausted
- [ ] Task 9: measurement cache — add last_update (unsigned long jiffies) and cached t_c100/rh_c100 (s32) to struct dht20_data; in dht20_measure, skip trigger and return cached values if time_before(jiffies, data->last_update + msecs_to_jiffies(2000))

## Task Notes

### Task 7 — probe init sequence

Goal: implement the power-on init flow from the datasheet before the driver
declares probe success.

What to add:
- `dht20_check_status(struct i2c_client *client)` — sends command `0x71`,
  reads 1 byte status, returns the byte or negative errno on I2C failure.
- In `dht20_probe`, before `i2c_set_clientdata`:
  1. `msleep(100)` — power-on stabilization per datasheet.
  2. Call `dht20_check_status`. If I2C fails, propagate error.
  3. If `(status & 0x18) != 0x18`, log `dev_err` and return `-ENODEV`.
     Do not attempt to write init registers (values not documented in
     datasheet; treating uncalibrated sensor as unsupported is correct here).
  4. `msleep(10)` — datasheet mandates 10ms between status OK and first
     trigger command.

Constants to add:
- `DHT20_STATUS_CMD`   0x71
- `DHT20_STATUS_MASK`  0x18

Design constraints:
- `dht20_check_status` must not take the mutex — probe runs before the
  struct is registered, no concurrent access is possible yet.
- Return `-ENODEV` (not `-EIO`) when calibration check fails — this signals
  to the kernel that the device is genuinely unsupported, not a transient
  error, so udev does not retry endlessly.

Verification:
- `dmesg` on successful bind must show no new error lines.
- To test the failure path: temporarily change `DHT20_STATUS_MASK` to
  `0xFF` so the check always fails; confirm `dmesg` shows `dev_err` and
  probe returns without exposing sysfs files.

### Task 8 — busy bit polling

Goal: replace the fixed `msleep(80)` assumption with an actual busy-bit
check so the driver handles a sensor that is not ready after 80ms.

What to change inside `dht20_read_sensor`:
- Keep the initial `msleep(80)` — this covers the typical case with no
  extra I2C traffic.
- After the sleep, read 1 byte from the sensor (same `i2c_master_recv`
  call, length 1 this time) and inspect `buf[0] bit[7]`.
- If busy, retry: `msleep(10)` then re-read, up to `DHT20_BUSY_RETRIES`
  (3) additional attempts.
- If still busy after all retries, return `-ETIMEDOUT`.
- Only when not busy, issue the full 7-byte read.

Constants to add:
- `DHT20_BUSY_BIT`      BIT(7)
- `DHT20_BUSY_RETRIES`  3
- `DHT20_BUSY_WAIT_MS`  10

Design constraints:
- All of this runs inside the existing mutex scope — do not release and
  re-acquire.
- The 1-byte busy read and the 7-byte data read are two separate
  `i2c_master_recv` calls. The sensor holds data until it is fully read;
  a partial read of 1 byte does not consume the remaining 6.
- Check return value of every `i2c_master_recv` call.

Verification:
- Normal path: `dmesg` timing should still show ~80-82ms per measurement.
- Timeout path: not easily triggered on real hardware; verify by temporarily
  setting `DHT20_TRIGGER_DELAY_MS` to 1ms so the first busy check always
  fires, then confirm the retry loop runs and eventually succeeds (or times
  out if retries are set to 0).

### Task 9 — measurement cache

Goal: enforce the datasheet's recommended 2s minimum interval between
measurements. Return cached values when called too soon instead of
triggering a new measurement.

What to add to `struct dht20_data`:
- `unsigned long last_update` — jiffies timestamp of last successful
  measurement. Initialize to 0 in probe (zero means "never measured").
- `s32 cached_t_c100` — last temperature result.
- `s32 cached_rh_c100` — last humidity result.

What to change in `dht20_measure`:
- Before calling `dht20_read_sensor`, check:
  ```c
  if (data->last_update != 0 &&
      time_before(jiffies, data->last_update + msecs_to_jiffies(2000)))
  ```
  If true, copy cached values to the output pointers and return 0.
- After a successful `dht20_read_sensor` + `dht20_parse`, update
  `last_update = jiffies` and store results in `cached_*`.
- This entire block runs inside the existing mutex in `dht20_read_sensor`.
  Move the cache check into `dht20_read_sensor` so the lock covers the
  timestamp read too, avoiding a TOCTOU race between two concurrent
  `cat` calls.

Design constraints:
- `time_before(a, b)` handles jiffies wraparound correctly — do not use
  plain `<` comparison on jiffies values.
- `last_update == 0` guard is needed because jiffies is non-zero at boot;
  without it, a freshly loaded driver could incorrectly serve a "cached"
  value of 0.
- Do not cache across probe/remove cycles — `devm_kzalloc` zeroes the
  struct, so `last_update` resets automatically on each probe.

Verification:
- `time cat temperature` twice in quick succession: second call should
  return same value as first and complete in <5ms (no I2C traffic).
- `sleep 2 && cat temperature`: should trigger a fresh measurement (~84ms).
- `dmesg` should show no extra log lines on cache hits.
