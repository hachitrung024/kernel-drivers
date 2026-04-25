# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

Out-of-tree Linux kernel modules written from scratch as a learning project. Currently one driver: [iio/dht20/dht20.c](iio/dht20/dht20.c) — an I²C driver for the ASAIR DHT20 humidity/temperature sensor.

The `iio/` path name is aspirational — the driver is currently a plain i2c_driver and does not yet use the IIO framework. Its sysfs surface is hand-rolled via `DEVICE_ATTR_RO`.

## Build / load workflow

**The workstation is not the build host.** There is no kernel tree or headers here. The driver is edited locally and built/loaded on a remote Raspberry Pi over SSH. Do not try to run `make` locally — it will fail looking for `/lib/modules/$(uname -r)/build`.

```bash
# Sync source to the Pi, then build in-place
scp iio/dht20/dht20.c iio/dht20/Makefile hachi@10.42.0.252:~/dht20/
ssh hachi@10.42.0.252 "cd ~/dht20 && make"

# Load / unload / inspect
ssh hachi@10.42.0.252 "sudo insmod ~/dht20/dht20.ko"
ssh hachi@10.42.0.252 "sudo rmmod dht20"
ssh hachi@10.42.0.252 "dmesg | tail -20"

# Verify sensor is on the bus
ssh hachi@10.42.0.252 "i2cdetect -y 1"   # expect 0x38
```

The Pi hostname `10.42.0.252` and user `hachi` are the current target — if they change, update commands accordingly.

## Hard rules for driver code

- **Read [Documents/dht20_datasheet.md](Documents/dht20_datasheet.md) before touching protocol code.** It has the byte layout, CRC polynomial, and required delays. The driver's constants (`0xAC 0x33 0x00` trigger, 80 ms wait, 20-bit fields, shared nibble in byte 3) all trace back to it.
- **Use `devm_*` allocators only.** No manual `kfree`. No manual `mutex_destroy` — the private struct is `devm_kzalloc`'d and cleaned up with the device.
- **No floating point in the kernel.** Scale outputs as integers ×100 (e.g., 2534 means 25.34 °C). The datasheet's float formulas must be converted: `T_c100 = (S_T * 20000) >> 20 - 5000`, `RH_c100 = (S_RH * 10000) >> 20`.
- **CRC failure → return `-EIO`.** Never surface a bad reading to userspace. Byte 6 is the CRC over bytes 0–5, init `0xFF`, poly `1 + x^4 + x^5 + x^8`.
- **`msleep()` for the 80 ms measurement wait, not `udelay()`.** Busy-waiting 80 ms in kernel is unacceptable.
- **Check every `i2c_master_send` / `i2c_master_recv` return value.** The current code in `dht20_read_sensor` ignores both — this is one of the known bugs to fix.
- **Serialize the full trigger → sleep → read sequence under `data->lock`.** A second caller mid-sequence will corrupt both readings.
