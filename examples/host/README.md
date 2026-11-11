# TLE92466ED host example

The driver on your PC, with no hardware: the host port in
[`host_tle92466ed_port.hpp`](host_tle92466ed_port.hpp) connects the real driver to a simulated TLE92466ED (the register model
the host tests use), and [`main.cpp`](main.cpp) starts it, drives channel 0 at 400 mA in Mission Mode and reads its
current feedback three times.

## Build and run

From the repository root, with CMake 3.16+ and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/tle92466ed_host_example
```

## Expected output

```text
TLE92466ED driver 0.1.0 on the host
tle92466ed: initialised, Config Mode
ICVID version 0xFE, reset watch armed
CH0 setpoint 399976 uA
CH0 397714 uA, duty 40.00 %, VBAT 13981 mV
CH0 397714 uA, duty 40.00 %, VBAT 13981 mV
CH0 397714 uA, duty 40.00 %, VBAT 13981 mV
VIO 5000 mV, VDD 5000 mV, 24873 mdegC
```

The host tests build this folder too and run the example (CTest `tle92466ed_host_example`), so CI fails if it stops
compiling or a call fails.

## Files

| File | What it is |
| --- | --- |
| [`main.cpp`](main.cpp) | The example: start, Mission Mode, set 400 mA, enable, read feedback |
| [`host_tle92466ed_port.hpp`](host_tle92466ed_port.hpp) | The host port: bus hooks over the simulated chip, `DelayUs()`, `GpioSet()` and `Log()` |
| [`CMakeLists.txt`](CMakeLists.txt) | Standalone project; adds the driver with `add_subdirectory(../..)` |
