# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

### Changed

- The repository moved to kokeb-labs-drivers; URLs follow.
- Hardware page: the pinout, the block diagram and the application circuit are now figures drawn from
  `docs/hardware.yml`, which also generates the pin table (replacing the hand-written one) and the parts table.
  Every pin, block and part cites the datasheet section and page; values the datasheet does not give are marked as
  not confirmed. `kokeb-docs hardware --kicad-netlist` exports the circuit.
- Synced with the driver template: `tools/preflight.sh` runs clang-tidy over every file CI lints, as CI does, and
  the hardware-lessons review adds the switch-off, channel-in-doubt and fuzz-skip lessons.

## [0.1.0] - 2026-11-09

### Added

- **Driver**: the first version of the Infineon TLE92466ED driver (datasheet Rev. 1.2): start-up with the RESN
  pulse or t_POR, INIT_DONE, the ICVID identity and the start-up faults reported and cleared; Config and Mission
  Mode; per-channel mode, current setpoint, integrating current controller, PWM frequency controller, dither, slew
  rate and diagnosis; parallel pairs; channel enables written from a fresh read of CH_CTRL; feedback under the
  freeze of section 4.10.5; supply and temperature readings; every diagnosis register, a fault latch and the clears;
  the OFF-state overcurrent check; the self-test; the SPI watchdog; the external clock; raw register reads.
- **Protocol**: 32-bit frames with CRC-8 SAE J1850 and a two-frame access with an anchor read that checks every
  reply (CRC, width, R/W echo, status, stuck and looped-back lines, repeated words), bounded retries of reads,
  resynchronisation after a failure, reset detection.
- **Safe switch-off**: a disable writes even when the CH_CTRL read fails, `EnterConfigMode()` and a refused
  `EnterMissionMode()` drive EN low whatever happens on the bus, and `GetDroppedChannels()` reports the channels the
  chip switched off on its own, from validated reads only. The CH_CTRL copy never reports a channel on that is off,
  whether a write's reply carries the register before or after the write (section 5.2.1); the host tests run under
  both readings. A write whose reply failed may have taken effect: every copy the driver decides on without I/O
  then holds the more dangerous value or is unknown, and a guard on an unknown value returns `WrongState` with
  nothing sent (`docs/contract.md`, "Ambiguous writes").
- **Release gate**: bench step 1 of `docs/limitations.md` (how the chip clears its flags) must pass before a
  final release; until then the driver is released only as a release candidate.
- **Port**: `SpiInterface` and the `SpiPort` concept with exact-signature hooks; optional `GpioSet()`, `GpioRead()`
  and `Log()`; the bus requirements as `constexpr`.
- **Tests and examples**: a chip model written from the datasheet, host tests of every operation and failure path,
  compile-fail tests, documentation snippets, a fuzz target with its corpus, the host example and the ESP32
  example.
