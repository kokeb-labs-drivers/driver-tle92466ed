# TLE92466ED ESP32 examples

ESP-IDF applications that run the TLE92466ED driver on an ESP32-C6, built by CI on every pull request.

## Wiring (ESP32-C6 DevKitC-1)

| ESP32-C6 | TLE92466ED | Notes |
| --- | --- | --- |
| GPIO6 | SCK (6) | SPI2, mode 1, 2 MHz in the example (up to 8 MHz) |
| GPIO7 | SI (7) | MOSI |
| GPIO2 | SO (9) | MISO, driven from VIO |
| GPIO10 | CSN (5) | Driven by spi_master, one 32-bit transfer per frame |
| GPIO11 | RESN (26) | Set `resn = -1` in `Esp32Tle92466edPins` when pulled up on the board |
| GPIO15 | EN (27) | Set `en = -1` when tied high |
| GPIO3 | FAULTN (11) | Input with the internal pull-up (add 10 kΩ to VIO for long wires); `faultn = -1` when not read |
| GND | DRV0 (25), DRV1 (28), CLK (24) | Tied low: no direct drive, internal oscillator |
| 3V3 | VIO (8) | `CONFIG_TLE92466ED_VIO_3V3=y` in `sdkconfig.defaults` |
| 5 V | VDD (29) | 100 nF to GND at the pin |
| 12 V supply | VBAT (22) | Through a reverse-protection diode; 100 µF and 100 nF to GND |
| Valve | LOAD0 (1) to the 12 V supply | Freewheeling diode from FWD0 (2) to the supply |
| GND | GND, GNDP0..5, tab | All grounds together |

## Build and flash

```bash
git submodule update --init scripts      # the ESP-IDF build scripts
. $IDF_PATH/export.sh                     # ESP-IDF v5.5
./scripts/build_app.sh list
./scripts/build_app.sh tle92466ed_basic_example Debug
./scripts/flash_app.sh flash_monitor tle92466ed_basic_example Debug
```

`idf.py menuconfig` shows "TLE92466ED solenoid driver" under Component config: VIO, the CRC check, the SPI watchdog,
the clock and the driver's timing that `tle92466ed::KconfigDefaults()` returns.

## Expected output

```text
I (312) tle92466ed_example: TLE92466ED driver 0.1.0
tle92466ed: initialised, Config Mode
I (330) tle92466ed_example: initialised, design step 0xFE
I (331) tle92466ed_example: CH0 on, setpoint 299982 uA
I (832) tle92466ed_example: CH0 299104 uA, duty 2150/10000, VBAT 12034 mV
```

An `EnsureInitialized failed: NoDevice` line means MISO reads all zeros or all ones: check VIO, SO and CSN first. A
`Crc` or `ReplyMismatch` points at the SPI mode (it must be 1) or the wiring. An `EnterMissionMode failed:
MissionBlocked` usually means the VIO setting does not match the board. The output above is what the example
prints on a working board; it is **not verified on hardware** yet.

## Files

| File | What it is |
| --- | --- |
| [`main/tle92466ed_basic_example.cpp`](main/tle92466ed_basic_example.cpp) | The example app |
| [`main/esp32_tle92466ed_port.hpp`](main/esp32_tle92466ed_port.hpp) | The ESP-IDF port: bus, RESN, EN, DRV0/1, FAULTN, delay, log |
| [`main/tle92466ed_cxx20_check.cpp`](main/tle92466ed_cxx20_check.cpp) | The public headers compiled as ISO C++20 inside the ESP-IDF build |
| [`CMakeLists.txt`](CMakeLists.txt) | The ESP-IDF project; `EXTRA_COMPONENT_DIRS` points at the repository root, which is the component |
| [`app_config.yml`](app_config.yml) | The app matrix for the scripts, CI and the docs |
