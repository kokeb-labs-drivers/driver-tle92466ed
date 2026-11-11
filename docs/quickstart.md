# Quickstart

Run the driver on your PC first, with no hardware; then regulate a coil current from your MCU: wire the chip, write
a small port class, call four driver functions.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Infineon IFX-ntz1491827137291 1.2:

    - Pins, RESN, EN, DRV0/DRV1 and FAULTN: 2 Pin configuration; 4.2.3 TDS_I/O Pins, [p. 7](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=7)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Try it on your PC

[`examples/host/`](../examples/host/README.md) runs the real driver against a simulated TLE92466ED, through a host
port. From the repository root, with CMake and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/tle92466ed_host_example
```

```text
TLE92466ED driver 0.1.0 on the host
tle92466ed: initialised, Config Mode
ICVID version 0xFE, reset watch armed
CH0 setpoint 399976 uA
CH0 397714 uA, duty 40.00 %, VBAT 13981 mV
```

The calls in [`main.cpp`](../examples/host/main.cpp) are the ones below; only the port differs on an MCU. CI builds
and runs this example on every pull request.

## 1. Wire it

| MCU | TLE92466ED | Notes |
| --- | --- | --- |
| SPI SCK | SCK (6) | Mode 1, up to 8 MHz; 2 MHz for bring-up on jumper wires |
| SPI MOSI | SI (7) | |
| SPI MISO | SO (9) | Driven from VIO; high impedance while CSN is high |
| GPIO (chip select) | CSN (5) | Low for one 32-bit frame, then high for at least 600 ns |
| MCU supply | VIO (8) | 3.3 V or 5 V: set `Config::vio` to match |
| 5 V | VDD (29) | 4.5 V to 5.5 V, 100 nF to GND |
| Load supply | VBAT (22) | 6 V to 18 V; the valve between VBAT and LOAD0, a freewheeling diode to FWD0 |
| GND | GND, GNDP0..5, tab | All grounds together |

For this example tie RESN (26) and EN (27) to VIO through 10 kΩ, DRV0 and DRV1 to GND, and leave FAULTN open: a
port without `GpioSet()` then starts the chip by waiting t_POR. The [Hardware](hardware.md) page has every pin,
the timing and the application circuit.

## 2. Add the driver

Add the repository as a submodule and link the `tle92466ed::tle92466ed` CMake target. [Installation](installation.md)
covers CMake on any MCU, the installed package and ESP-IDF.

```bash
git submodule add https://github.com/kokeb-labs-drivers/driver-tle92466ed.git external/driver-tle92466ed
```

## 3. Write the port

The port is the only platform code: it brings the bus up and moves 32-bit frames. Replace the `platform_*` calls
with your SDK's.

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

[Porting](porting.md) explains each method, the optional hooks and the transfer rules.

## 4. Drive a valve

```cpp title="main.cpp"
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

What each step does:

1. `EnsureInitialized()` brings the port up, resets or waits out the chip, waits for INIT_DONE, checks ICVID,
   reports and clears the faults found, and writes every register. The chip is in Config Mode, every channel off.
2. `EnterMissionMode()` checks that no supply, clock or watchdog fault holds the chip in Config Mode, then sets
   OP_MODE. Only in Mission Mode do the channels switch and their diagnosis run.
3. `SetCurrent()` programs the average current; the value is checked against 1.5 A, rounded to the 61 µA TARGET
   step, and the programmed current is returned.
4. `EnableChannel()` sets EN_CH0: the integrating current controller now holds 400 mA through the valve.

This example is built by the host CI, so it compiles against the current headers.

## If it does not work

| Symptom | First thing to check |
| --- | --- |
| `EnsureInitialized()` returns `Bus` | Your port's `EnsureInitialized()` or `Transfer()` returned false |
| `EnsureInitialized()` returns `NoDevice` | VIO, SO and CSN: MISO reads all zeros or all ones ([Troubleshooting](troubleshooting.md)) |
| `EnsureInitialized()` returns `Crc` or `ReplyMismatch` | The SPI mode (it must be 1) and the clock |
| `EnsureInitialized()` returns `Timeout` | VDD and RESN: INIT_DONE never read 1 |
| `EnterMissionMode()` returns `MissionBlocked` | `Config::vio` against the real VIO, then `ReadFaults()` |
| `ReadChannelFeedback()` returns `NotReady` | The channel's dither clock: no measurement period without one |

## Next

- [API guide](api-guide.md): states, channels, feedback, faults, the watchdog, errors.
- [Configuration](configuration.md): the current-control preset, every `Config` field, Kconfig.
- [Examples](examples.md): the host example and the ESP32 example.
- [Status and limitations](limitations.md): what still needs a bench.
