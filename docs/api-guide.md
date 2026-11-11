# API guide

The driver's API by task, with code that compiles: start, operation states, channels, feedback, faults, the
watchdog, errors and shut-down. Section numbers refer to the datasheet, Rev. 1.2.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Infineon IFX-ntz1491827137291 1.2:

    - Reset State, Config Mode, Mission Mode and Critical Fault: 4.3.1 TDS_IC Operation states; Figure 4, [p. 19](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=19)
    - Channel modes and the measurement period: 4.4 Channel modes, [p. 21](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=21)
    - Parallel channel pairs: 4.5.2 TDS_Parallel channel operation, [p. 22](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=22)
    - Integrating current controller, Autolimit and the PWM frequency controller: 4.6.2 Integrating Current Controller (ICC); Equations 4, 5, [p. 24](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=24)
    - Setpoint range and accuracy: 4.6.3 Electrical characteristics current control, Table 14, [p. 28](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=28)
    - Dither waveform and its update: 4.7.1 TDS_Dither configuration; 4.7.2 TDS_Dither parameter update; Equations 6 to 9, [p. 29](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=29)
    - ON-state and OFF-state diagnosis: 4.9.1 TDS_Overview; Figures 14, 15, [p. 32](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=32)
    - Open load or short to ground, thresholds and warning: 4.9.2 TDS_Open Load/Short to Ground (OLSG); 4.9.3 TDS_OLSG Warning, [p. 34](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=34)
    - Overcurrent and the OFF-state overcurrent check: 4.9.6 TDS_Overcurrent (OC); Equation 17, [p. 38](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=38)
    - Register and OTP ECC, the self-test: 4.9.7 TDS_Register/OTP ECC; 4.9.8 TDS_Built in Self Test (BIST), [p. 39](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=39)
    - Average current, duty cycle and VBAT feedback: 4.10.2 TDS_Average feedback values; Equations 18 to 22, [p. 41](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=41)
    - Feedback freeze and update: 4.10.5 TDS_Update/Freeze Mechanism; Figure 22, [p. 43](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=43)
    - Overtemperature and the temperature feedback: 4.11.1 TDS_Overtemperature protection; Equation 32, [p. 45](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=45)
    - SPI watchdog: 5.2.2 TDS_SPI watchdog; Equation 33, [p. 50](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=50)
    - FB_STAT, the status summary: 5.3.2.21 Feedback Status Register, [p. 77](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=77)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The shape of the API

| Group | Calls |
| --- | --- |
| Lifecycle | `EnsureInitialized()`, `IsInitialized()`, `Deinitialize()`, `HardwareReset()` |
| Configuration | `ApplyConfig()`, `GetConfig()`, `ConfigureChannel()`, `SetParallel()` |
| Operation state | `EnterMissionMode()`, `EnterConfigMode()`, `IsMissionMode()`, `ReadChannelControl()` |
| Channels | `SetCurrent()`, `GetCurrentSetpoint()`, `EnableChannel()`, `DisableChannel()`, `SetEnabledChannels()`, `GetEnabledChannels()`, `SetDither()`, `StartOvercurrentCheck()`, `SetDrivePin()` |
| Feedback | `ReadChannelFeedback()`, `ReadSupply()` |
| Diagnosis | `ReadStatus()`, `ReadFaults()`, `ClearFaults()`, `ClearChannelFaults()`, `GetLatchedFaults()`, `ClearLatchedFaults()`, `GetStartupFaults()`, `ReadFaultPin()`, `ReadPinStatus()`, `RunBist()` |
| Watchdog | `ReloadWatchdog()` |
| Identification | `GetIcVersion()`, `ReadChipId()`, `tle92466ed::GetDriverVersion()` |
| Raw access | `ReadRegister()`, `GetLastReply()`, `GetLastCriticalFault()`, `GetBusDiagnostics()`, `IsResetWatchArmed()` |

Every call that touches the chip returns `Result<T>`: check `ok()` (or the `bool` conversion) and read `value`, or
read `error`. `Result` is `[[nodiscard]]`, so an unchecked call is a compiler warning. Setters take a trailing
`Verify verify = Verify::None`: `Verify::ReadBack` reads the written register back (two more frames each) and
returns `WriteMismatch` when the chip did not take the value.

## Start and enter Mission Mode

```cpp
--8<-- "tests/docs/api_tasks.cpp:lifecycle"
```

The constructor stores the port and the configuration (`Config{}` unless you pass one) and does no I/O.
`EnsureInitialized()` is the one way to start the chip ([Contract](contract.md#promises) lists its steps); it ends
in Config Mode with every channel off. Once it has succeeded it returns `Ok` at once with no I/O, so a caller can
call it before every use. After a failure the driver stays uninitialised and the next call starts again.

The chip has four operation states (section 4.3.1, Figure 4):

| State | What works | The driver |
| --- | --- | --- |
| Reset | Nothing; registers return to their reset values | RESN low in `EnsureInitialized()` and `HardwareReset()` |
| Config Mode | Registers; every channel off; MODE, GLOBAL_CONFIG, CLK_DIV, CH_PAR and SFF_BIST writable only here | `EnsureInitialized()` ends here; `EnterConfigMode()`; `ApplyConfig()`, `ConfigureChannel()`, `SetParallel()`, `RunBist()` |
| Mission Mode | Channels switch; their diagnosis runs; EN_CH settable only here | `EnterMissionMode()`; `EnableChannel()`, `StartOvercurrentCheck()` |
| Critical Fault | Every reply is a critical fault frame; FAULTN low | A critical fault frame confirmed by a second one is `CriticalFault`: EN low, the driver uninitialised; `EnsureInitialized()` (with RESN) or a power cycle leaves it |

`EnterMissionMode()` checks the entry conditions of Figure 4 first and returns `MissionBlocked`, writing nothing,
while INIT_DONE is 0 or one fails: FB_STAT (SPI watchdog, clock, central temperature, VDD, VIO, VBAT or an internal
supply), the DIAG_ERR_CHGR and DIAG_WARN_CHGR groups FB_STAT flags (a channel OTE or OTW) and GLOBAL_DIAG2
(REG_ECC_ERR). VPRE_OV blocks entry too, a fail-safe choice beyond Figure 4. Then it drives EN high, sets OP_MODE
and reads CH_CTRL back, always: a chip that refused is `MissionBlocked`, and `IsMissionMode()` says what the chip
did. A supply, clock or watchdog fault later drops the chip back to Config Mode on its own and clears every EN_CH
(sections 4.1.2, 4.2.2, 5.2.2); a channel fault clears that channel's EN_CH (4.9.2 to 4.9.6). The driver never
writes CH_CTRL from a copy the chip may have changed: every enable change reads CH_CTRL first.

## Drive a channel

```cpp
--8<-- "tests/docs/api_tasks.cpp:drive"
```

`SetCurrent()` sets the average current of the integrating current controller (sections 4.6.1, 4.6.2). The value
is checked against the operating range of Table 14 and refused, never clamped: up to 1.5 A, up to 1.8 A with the
configured dither amplitude, and above the dither amplitude when it is not 0 (section 4.7.1). The master of a
parallel pair takes up to 2.43 A with dither (`kMaxParallelSetpointUa`: Table 14's 2.7 A includes the overshoot,
and the driver keeps the single channel's 90 % headroom), and its TARGET step doubles to 122 µA (section 4.5.2).
`SetParallel()` sets the master's setpoint to 0 when it joins a pair. The call
returns the current actually programmed, rounded to the TARGET step; `GetCurrentSetpoint()` returns it with no
I/O. Setpoints may be written in either state: the channel switches once it is enabled in Mission Mode.

`EnableChannel()`, `DisableChannel()` and `SetEnabledChannels()` read CH_CTRL, then write it whole with only the
EN_CH bits they name changed: enabling one channel never switches another off, a channel a fault switched off is
never switched back on, and OP_MODE is never set (only `EnterMissionMode()` sets it). An enable that finds the chip
back in Config Mode is `MissionBlocked`. Switching off never depends on that read: when it fails, a disable (or
any mask that only clears) still writes CH_CTRL from the copy with the channels cleared and returns the read's
error, and `EnterConfigMode()` drives EN low whatever the SPI result. Section 5.2.1 does not say whether a write's reply
carries CH_CTRL before or after the write, so the copy holds an EN_CH or OP_MODE bit only when both the value
written and the reply have it: `GetEnabledChannels()` can under-report a channel that is on until the next read,
never report one that is off. Channels the driver wrote on that a validated read of CH_CTRL (the read before every
channel call, or `Verify::ReadBack`) finds off are logged and kept in `GetDroppedChannels()` until
`ClearDroppedChannels()` or the next start; a refused `EnterMissionMode()` drives EN low again. A
channel must be declared `loaded` in its `ChannelConfig` and must not be a slave. The chip keeps a channel
switched off by a fault off until its flags are cleared and EN_CH is set again (sections 4.9.2 to 4.9.6);
`Verify::ReadBack` finds such a channel as `WriteMismatch`, and the driver's copy then shows it off.

`SetDither()` changes a channel's dither while it runs: DITHER_CLK_DIV takes effect at once, DITHER_STEP and
DITHER_CTRL at the next dither period after the DITHER_CTRL write (section 4.7.2). The dither period is also the
feedback measurement period in current control and direct drive (section 4.4.2).

## Read the feedback

```cpp
--8<-- "tests/docs/api_tasks.cpp:feedback"
```

`ReadChannelFeedback()` reads one measurement period under the freeze of section 4.10.5: average current (signed,
the pair's sum on a master), duty cycle, VBAT, the measurement period, the minimum and maximum current and PWM
period, and the fast-feedback counters. With no new values since the last read it returns `NotReady` after two
frames; with no measurement period at all (TP_MANT = 0, as with a dither clock of 0) it returns `NotReady` too,
never a current of 0. The conversions use f_SYS of the configuration. `ReadSupply()` reads VIO, VDD, VBAT and the
central temperature (±20 °C uncalibrated, Table 16).

## Faults

```cpp
--8<-- "tests/docs/api_tasks.cpp:faults"
```

| Call | I/O | What it gives |
| --- | --- | --- |
| `ReadStatus()` | 2 frames | FB_STAT: INIT_DONE, the global faults, and a bit per channel group with an error or warning |
| `ReadFaults()` | 18 frames | GLOBAL_DIAG0..2, DIAG_ERR_CHGR0..2, DIAG_WARN_CHGR0..2 as `Faults`; `Has()` and `Of()` decode them |
| `ClearFaults()` | 18 frames, 2 more per register with a flag (4 with OC or OTP_ECC_ERR) | Every register read and latched, then exactly the flags read cleared: rwh flags by a 1, rw flags by a 0, OC and OTP_ECC_ERR twice; OTP_VIRGIN never |
| `ClearChannelFaults(ch)` | 4 frames, 2 more per write | The group read and latched (both channels), then this channel's flags that read 1 cleared |
| `GetLatchedFaults()` | none | Every flag any read of a diagnosis register saw since `ClearLatchedFaults()` |
| `GetStartupFaults()` | none | The diagnosis registers as the start-up found them, before it cleared them |
| `ReadFaultPin()` | `GpioRead()` | FAULTN: low on Critical Fault, RESN low or an unmasked fault |
| `ReadPinStatus()` | 2 frames | PIN_STAT: DRV0, DRV1, EN, the internal fault state and the FAULTN level |

The diagnosis runs only in Mission Mode (section 4.9.1). In ON state the shunt current shows an open load or a
short to ground only together, as OLSG, below the open-load threshold; in OFF state (EN_CH set with setpoint 0, or
a setpoint with EN_CH cleared) the current sources of Figure 14 tell OL from SG. Use both. OLSG_WARN_CHK_NOK is
cleared by reading DIAG_WARN_CHGR (section 4.9.3): the latch keeps it. `StartOvercurrentCheck()` runs the
OFF-state overcurrent check of section 4.9.6 once, on a channel that is off with setpoint 0 and not asked on since
the last read of CH_CTRL (after a refused enable, call `ReadChannelControl()` first); read the result with
`ReadFaults()` after t_OCon. The channel then stays enabled with setpoint 0, the OFF-state diagnosis condition.
`RunBist()` runs the safety flip-flop self-test in Config Mode (section 4.9.8) and clears the REG_ECC_ERR it sets
on purpose.

## The SPI watchdog

```cpp
--8<-- "tests/docs/api_tasks.cpp:watchdog"
```

With `Config::spi_watchdog`, the chip counts WD_TIME down at f_SYS / 2^14 (0.585 ms per count) and, at 0, sets
SPI_WD_ERR, ends Mission Mode and switches every channel off (section 5.2.2). `EnsureInitialized()` writes
`Config::watchdog_reload` to WD_RELOAD before it enables the watchdog, since the reset value of 1 would expire at
once. `ReloadWatchdog()` writes it again; call it faster than the timeout. WD_TIME is 11 bits: at most 2047 counts,
1.198 s. After an expiry, a reload clears SPI_WD_ERR, `ClearFaults()` the latched flag, and `EnterMissionMode()`
works again.

## Handle errors

```cpp
--8<-- "tests/docs/api_tasks.cpp:errors"
```

| Signal | Meaning | What to do |
| --- | --- | --- |
| `Error::Bus` | The port failed a transfer, a pin write or the bus bring-up | Check the port and the wiring; retry |
| `Error::NoDevice` | MISO stuck low or high, or MOSI looped back | VIO, SO, CSN and the chip's supplies |
| `Error::Crc`, `ReplyMismatch`, `FrameRejected` | A frame was corrupted, lost or ignored | SPI mode 1, the clock, CSN high time; a shared bus lock |
| `Error::Timeout` | INIT_DONE never read 1, or the self-test did not finish | VDD, RESN, the clock source |
| `Error::DeviceIdMismatch` | ICVID is not a TLE92466ED | The part, the chip select |
| `Error::DeviceFault` | OTP not programmed, or an internal bus fault | Replace the part |
| `Error::CriticalFault` | Internal supply or clock fault, confirmed by a second frame; EN is low | `GetLastCriticalFault()`, then `EnsureInitialized()` (RESN) |
| `Error::MissionBlocked` | A fault holds the chip in Config Mode, or it left Mission Mode on its own | `ReadFaults()`, fix the cause, `ClearFaults()`, `EnterMissionMode()` |
| `Error::WrongState` | The call needs the other state, the channel is a slave, or an earlier write failed and the chip may hold either value (see the contract, "Ambiguous writes") | Change state first; after a failed write, repeat it or `ReadChannelControl()` |
| `Error::WriteMismatch` | The chip did not take a write | A latched fault, the state, or a lost frame: read the faults |
| `Error::NotReady` | No feedback to report | Wait one measurement period; configure the dither clock |
| `Error::UnexpectedReset` | The chip reset behind the driver's back; EN is low | `EnsureInitialized()` configures it again |
| `Error::Unsupported` | The port lacks the hook | Add `GpioSet()` or `GpioRead()` |

Every value is on the [Contract](contract.md#errors) page. `tle92466ed::ToString()` names an error for a log.

## Raw access and counters

For bring-up and fault finding (handbook KB-CPP-129):

| Call | I/O | What it gives |
| --- | --- | --- |
| `ReadRegister(address)` | 2 frames, no retry | The 16 or 22 data bits of any register of Tables 19 and 24; a diagnosis register read here feeds the latch, and DIAG_WARN_CHGR clears OLSG_WARN_CHK_NOK on the chip |
| `GetLastReply()` | none | The last 32-bit word clocked in as a reply, by any call |
| `GetLastCriticalFault()` | none | The flags byte of the last critical fault frame |
| `GetBusDiagnostics()` | none | Frames sent, reads retried, replies rejected, resyncs |
| `GetDroppedChannels()` | none | Channels the chip switched off on its own, bit x for channel x; `ClearDroppedChannels()` empties it |
| `IsResetWatchArmed()` | none | Whether every FB_STAT anchor checks for a chip reset |

## Shut down

```cpp
--8<-- "tests/docs/api_tasks.cpp:deinit"
```

After `Deinitialize()` every call returns `NotInitialized` until the next `EnsureInitialized()`, which starts the
chip again from the beginning: `Deinitialize()` then `EnsureInitialized()` is also the recovery after a fault.

Next: [Configuration](configuration.md).
