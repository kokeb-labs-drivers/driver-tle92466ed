# Contract

What the TLE92466ED driver promises, what it needs, and the rules a caller follows.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Infineon IFX-ntz1491827137291 1.2:

    - The reply pipeline, frames and reply status: 5.2.1 TDS_Data flow; 5.2.3 SPI frame definition, [p. 50](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=50)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

The driver inherits the [Kokeb Labs driver contract](https://github.com/kokeb-labs/.github/blob/main/standards/driver.md);
this page states only what is specific to it. A change to anything here is a breaking change, listed under
**Breaking** in the [changelog](changelog.md).

## Promises

- The constructor is `noexcept`, stores the configuration, does no I/O and cannot fail.
- `EnsureInitialized()` is the only way to start the driver. Its first call checks the stored configuration
  (`InvalidArgument`, nothing sent), brings the bus up, resets the chip (RESN when the port has `GpioSet()`,
  otherwise it waits t_POR), waits for INIT_DONE, checks ICVID, puts the chip in Config Mode with every channel off,
  reports and clears the faults it finds, and only then writes the configuration. On any failure the driver stays
  uninitialised and the next call starts again from the beginning; once it has succeeded, later calls return `Ok`
  with no I/O.
- Every call that touches the chip returns `Error::NotInitialized` before `EnsureInitialized()` succeeds. No call
  initialises or resets the chip as a side effect. `ApplyConfig()` and `ConfigureChannel()` before the start only
  store the configuration.
- Every port failure propagates as `Error::Bus`; a failed call returns `T{}`, never a stale or zero value.
- A reply is used only after every check of [Architecture](architecture.md#the-spi-pipeline) passed. A read is
  repeated at most `Config::max_retries` times; a write never.
- A call sends only the frames the datasheet needs; a read-back costs two frames per register and is asked for per
  call with `Verify::ReadBack`. The start-up checks (INIT_DONE, ICVID, GLOBAL_CONFIG read back, the reset watch)
  are unconditional.
- A setter checks its arguments and the operation state before any I/O (`InvalidArgument`, `WrongState`: nothing
  sent), reads CH_CTRL before it writes it, writes it whole with only what it names changed, never sets an EN_CH
  bit it was not asked for nor OP_MODE (only `EnterMissionMode()` does). The copy can only under-report: after a
  write it holds an EN_CH or OP_MODE bit only when the value written and the reply both have it (after a failed
  write, the value and the previous copy), whichever contents the reply carries (section 5.2.1).
- A confirmed critical fault and a detected reset drive EN low and leave the driver uninitialised.
- Switching off never depends on a read: a clear-only enable change writes even when the CH_CTRL read failed,
  `EnterConfigMode()` drives EN low whatever the SPI result, and a refused `EnterMissionMode()` drives EN low again.
- A write whose reply failed may have taken effect (see "Ambiguous writes" below): every value the driver decides
  on without I/O then holds the more dangerous of the two, or is unknown, and a guard on an unknown value returns
  `WrongState` with nothing sent. Switching off and a setpoint of 0 are never refused: `SetEnabledChannels()` keeps
  an unknown channel that is already on as the chip has it and still switches the others off.
- Diagnosis flags are read and latched before they are cleared, and only the flags read are cleared.
- Every wait is bounded: INIT_DONE by `Config::init_timeout_us`, the self-test by `kBistTimeoutUs`, each poll by
  `kPollIntervalUs`.
- `Config{}` equals the datasheet reset values; a channel the configuration does not declare `loaded` cannot be
  enabled, runs no OFF-state diagnosis and never reaches FAULTN.
- A missing `DelayUs()` is a compile error; a missing `GpioSet()` gives `Error::Unsupported` from
  `HardwareReset()` and `SetDrivePin()`; a missing `GpioRead()` from `ReadFaultPin()`; a missing `Log()` logs
  nothing. A misdeclared hook is a compile error.
- `Deinitialize()` leaves the chip in Config Mode with every channel off and EN low.

## Depends on

| Dependency | Version | Why |
| --- | --- | --- |
| A C++20 compiler | GCC 10, Clang 13 or later | Concepts constrain the port |
| CMake | 3.16 or later | Generates `tle92466ed_version.h` |
| Your port | | 32-bit frames within the bus requirements; the delay; optionally RESN, EN, DRV0/1 at their idle levels before the first transfer, FAULTN, and logging ([Porting](porting.md)) |

Nothing else: no RTOS, no SDK, no other repository of the org.

## Threading

The driver is not thread-safe and takes no lock. Use each driver object from one task at a time, or guard every
call on it with one mutex; the layer that owns the driver owns that lock. One call sends several frames that
belong together (the reply to each comes in the next), so no other code may send frames to the same chip while a
call runs; other devices on the bus may, between the frames, since the chip ignores the bus while its CSN is high.
Nothing may run in an ISR: every call can block for a bus transfer or a delay. Separate objects share no state.

## Errors

`tle92466ed::Error` inside `tle92466ed::Result<T>`. Values 0 to 9 are the shared base set; this driver returns all
but `Busy`, plus nine chip-specific values.

| `Error` | Value | Returned when |
| --- | --- | --- |
| `Ok` | 0 | The call succeeded |
| `NotInitialized` | 1 | `EnsureInitialized()` has not succeeded, `Deinitialize()` ran since, or the chip reset |
| `InvalidArgument` | 2 | A `Config` field or an argument is out of range (`Validate()`), or a channel is not declared loaded; nothing was sent |
| `Bus` | 3 | A port hook returned `false` |
| `Timeout` | 4 | INIT_DONE or the self-test did not finish in time |
| `DeviceIdMismatch` | 5 | ICVID is not manufacturer 0xC1 with version 0xFC or 0xFE |
| `Crc` | 6 | A reply's CRC is wrong, or the chip found a wrong CRC in the driver's frame |
| `Busy` | 7 | Not returned: the chip is never busy to SPI |
| `DeviceFault` | 8 | OTP_VIRGIN at start-up, or an internal bus fault in a reply's status |
| `Unsupported` | 9 | The optional hook the call needs is missing |
| `NoDevice` | 16 | MISO stuck low or high, or MOSI looped back |
| `CriticalFault` | 17 | Two critical fault frames in a row: the chip is in the Critical Fault state; EN low, uninitialised |
| `ReplyMismatch` | 18 | A reply does not answer the request (width, echo, repeated word, anchor) |
| `FrameRejected` | 19 | The chip reported an SPI frame error or a read-only write |
| `WrongState` | 20 | The call needs the other operation state, the channel is a slave, or it depends on a register a failed write may have changed |
| `MissionBlocked` | 21 | A fault of Figure 4 keeps the chip in Config Mode, it refused OP_MODE, or an enable found it back in Config Mode |
| `WriteMismatch` | 22 | A read-back differs from what was written |
| `NotReady` | 23 | No new feedback, or no measurement period |
| `UnexpectedReset` | 24 | POR_event or RES_event set: the chip reset; EN low, the driver is uninitialised |

The [API guide](api-guide.md#handle-errors) says what to do about each.

## Memory

No heap, no exceptions, no RTTI, no `std::function`, no `std::string`, no `printf` and no static mutable state. A
driver object holds a reference to the port, a `Config` (about 350 bytes), the fault latch and start-up faults, the
CH_CTRL and setpoint copies, the two 4-byte frame buffers and the protocol counters: under 512 bytes. No buffer
lives on the stack for the bus.

## Platform

Platform-free: the same code runs on any target with a C++20 compiler, including the host tests. The ESP-IDF port
in [`examples/esp32`](examples.md) is an example, not part of the driver.

## Deviations from the standard

None.

## Ambiguous writes

Writes are never retried, and a failed reply does not say whether the chip took the frame. Each copy the driver
decides on without I/O is handled so a failed write never makes a guard pass that the chip's state would fail:

| Copy | Decides | After a write whose reply failed | Known again after |
| --- | --- | --- | --- |
| Setpoint per channel (`SetCurrent()`) | The OFF-state overcurrent check needs 0; the dither limit; `GetCurrentSetpoint()` | The larger of the old and the new TARGET | A `SetCurrent()` that succeeds |
| EN_CH (CH_CTRL copy and the channels asked on) | Which channels the overcurrent check takes as off; the switch-off fallback | Copy: only bits both old and new have; asked on: old OR new | A validated read of CH_CTRL; a successful write |
| OP_MODE | Config Mode only calls: `ApplyConfig()`, `ConfigureChannel()`, `SetParallel()`, `RunBist()` | Mission Mode possible: those calls return `WrongState` | A validated read of CH_CTRL; a successful CH_CTRL write |
| CH_PAR (parallel pairs) | TARGET scaling, the current limit, master and slave | The pair is unknown: current above 0, dither, enables and the overcurrent check on both channels return `WrongState` | A read of CH_CTRL that matches the configuration; a `SetParallel()` or `ApplyConfig()` that succeeds |
| Channel registers (`ConfigureChannel()`, a split slave) | `loaded`, MODE, the controller, the range, TON for t_OCon | The channel is unknown: current above 0, dither, enables and the overcurrent check return `WrongState` | A `ConfigureChannel()` or `ApplyConfig()` that succeeds |
| Dither registers (`SetDither()`) | The dither limit with the setpoint; t_OCon (DITHER_CTRL.EXP) | Dither unknown: current above 0, enables and the overcurrent check return `WrongState` | A `SetDither()`, `ConfigureChannel()` or `ApplyConfig()` that succeeds |
| Global registers (`ApplyConfig()`: GLOBAL_CONFIG, VBAT_TH, FAULT_MASK0..2, WD_RELOAD) | f_SYS for t_OCon; the watchdogs and masks in Mission Mode | Unknown: `EnterMissionMode()`, and every channel as above, return `WrongState` | An `ApplyConfig()` that succeeds |
| Fault latch, start-up faults, identity, reset watch | Reporting, `EnterMissionMode()` checks | Taken from reads only; a clear never removes a latched flag | (not set by writes) |

`EnsureInitialized()` and `HardwareReset()` write every register and start with nothing unknown.

