# Troubleshooting

Symptoms, causes and fixes, cheapest check first. Section numbers refer to the datasheet, Rev. 1.2.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Infineon IFX-ntz1491827137291 1.2:

    - Supplies, VIO_SEL and the power-on reset: 4.1.1 TDS_Power Supply, [p. 14](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=14)
    - Voltage monitoring and the VBAT thresholds: 4.1.2 TDS_Voltage monitoring; Equation 1, [p. 14](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=14)
    - Reset State, Config Mode, Mission Mode and Critical Fault: 4.3.1 TDS_IC Operation states; Figure 4, [p. 19](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=19)
    - Open load or short to ground, thresholds and warning: 4.9.2 TDS_Open Load/Short to Ground (OLSG); 4.9.3 TDS_OLSG Warning, [p. 34](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=34)
    - Register and OTP ECC, the self-test: 4.9.7 TDS_Register/OTP ECC; 4.9.8 TDS_Built in Self Test (BIST), [p. 39](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=39)
    - Average current, duty cycle and VBAT feedback: 4.10.2 TDS_Average feedback values; Equations 18 to 22, [p. 41](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=41)
    - The reply pipeline, frames and reply status: 5.2.1 TDS_Data flow; 5.2.3 SPI frame definition, [p. 50](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=50)
    - Identity, ICVID and CHIPID: 5.3.2.19 Version Register; 5.3.2.24 Unique Chip Identification Number Register 0, [p. 75](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=75)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

`GetBusDiagnostics()` counts frames, retries, rejected replies and resyncs, and `GetLastReply()` holds the last
32-bit word read: log both while you look for a bus problem.

## EnsureInitialized returns Bus

The port reported a failure: its `EnsureInitialized()`, a `Transfer()` or a `GpioSet()` returned `false`.

1. Check that your port's `EnsureInitialized()` returns `true` on its own (log it).
2. Check the bus and control pins in your port against the board.
3. With a logic analyser, check that the first frames appear on the bus: an ICVID read (0x..C00200), then FB_STAT
   reads.

## EnsureInitialized returns NoDevice

MISO read all zeros, all ones, or exactly the frame just sent. Nothing drives SO, or MOSI reaches MISO.

| `GetLastReply()` | Likely cause | Fix |
| --- | --- | --- |
| `0x00000000` | VIO missing (SO is powered from VIO), the chip held in reset, SO not connected | Check VIO, RESN high after the start, SO continuity |
| `0xFFFFFFFF` | SO floating with a pull-up, or the chip unpowered | Check VDD and VIO, CSN reaching pin 5 |
| The frame sent | MOSI and MISO shorted, or SO not soldered with MOSI coupled in | Check the board |

## EnsureInitialized returns Crc or ReplyMismatch

Frames arrive, but not intact or not in step. The chip shifts SO on the rising SCK edge and samples SI on the
falling edge: **SPI mode 1** (section 5.1.1). Mode 0 typically gives `Crc`; a shared bus that interleaves another
transfer gives `ReplyMismatch`. Check CSN high time between frames (600 ns minimum, Table 18), lower the clock, and
make sure no other code sends frames to this chip during a driver call.

## EnsureInitialized returns Timeout

INIT_DONE in FB_STAT did not read 1 within `Config::init_timeout_us` (section 4.2.1): the clock or the logic is not
ready. Check VDD (4.5 V to 5.5 V), RESN released, and with an external clock the CLK signal and
`Config::external_clock_khz`.

## EnsureInitialized returns DeviceIdMismatch or DeviceFault

`DeviceIdMismatch`: ICVID did not read manufacturer 0xC1 with version 0xFC (A11) or 0xFE (A13) (section
5.3.2.19): another part, or another design step this driver does not know. `DeviceFault`: OTP_VIRGIN is set, the
chip's trim was never programmed (section 4.9.7: "this bit must always be 0"); `GetStartupFaults()` shows it.
Replace the part.

## EnsureInitialized returns WriteMismatch

GLOBAL_CONFIG did not read back what was written: the chip did not take a Config Mode write. Check that nothing
else drives the chip and that MOSI reaches SI (reads alone work without it).

## EnterMissionMode returns MissionBlocked

A fault of Figure 4 holds the chip in Config Mode. `ReadStatus()` says which kind, `ReadFaults()` which flag:

| Flag | Likely cause | Fix |
| --- | --- | --- |
| `VioUndervoltage` or `VioOvervoltage` | `Config::vio` does not match the board (reset value 5 V) | Set `Config::vio` to `V3V3` on a 3.3 V MCU |
| `VddUndervoltage` | VDD below 4.5 V | Check the 5 V rail |
| `VbatUndervoltage` | No load supply, or `vbat_uv_code` too high | Check VBAT; the threshold is 162 mV per count |
| `SpiWatchdog` | The SPI watchdog expired | `ReloadWatchdog()` often enough; then `ClearFaults()` |
| `ClockFault` | The clock watchdog saw f_SYS out of range | The CLK pin and the dividers |
| `CentralOtWarning`, `CentralOtError` | The die is hot | Cooling, load current |

Then `ClearFaults()` and `EnterMissionMode()` again. A flag that sets again at once means the cause is still there.

## Calls return CriticalFault

Every reply is a critical fault frame: the chip is in the Critical Fault state (section 4.3.1), after an internal
supply fault or a clock that runs too fast. `GetLastCriticalFault()` holds the flags (section 5.2.3.5): bits 7, 6
and 5 are 1 when the 1V5, 2V5 and the ADC bandgap are fine, bits 4 to 0 are clock faults (too slow, too fast,
digital clock too slow or too fast, watchdog reference missing). A flags byte of 0x00 almost always means VDD is
missing. The driver accepts the state only from two critical fault frames in a row (one alone can be noise),
then drives EN low and is uninitialised. Only RESN (`EnsureInitialized()` with `GpioSet()`) or a power cycle leaves
the state.

## A channel does not switch on

`EnableChannel(..., Verify::ReadBack)` returns `WriteMismatch`, or the current stays at 0:

- **A latched fault keeps it off.** After an OLSG, OL, SG, OC or OTE the chip keeps the channel disabled until the
  flag is cleared and EN_CH is set again (sections 4.9.2 to 4.9.6). An OFF-state diagnosis can also latch OL or SG
  on a channel that was disabled with a setpoint still set. `ReadFaults()`, fix the load, `ClearChannelFaults()`,
  enable again.
- **Not in Mission Mode**, or the chip left it (a supply or watchdog fault): `EnableChannel()` returns
  `MissionBlocked`; `ReadChannelControl()` shows it.
- **Setpoint 0**: in ON state the setpoint must not be 0 (section 4.9.1).
- **EN low**: `ReadPinStatus().value.enable`.
- **A slave**: drive the master of the pair.

## The current regulates badly or stalls

- `PWM_REG_WARN` or `I_REG_WARN` in `ReadFaults()`: the integrator hit its limit or its threshold went negative
  (sections 4.6.2.4, 4.6.2.5). Set `icc.min_int_thresh` above 1 (the preset uses 2) and keep `icc.auto_limit`
  above `min_int_thresh` + 3.
- The PWM frequency drifts: set a PWM target (`icc.period_mant`) within 110 Hz to 4 kHz and a controller gain.
- A setpoint step overshoots: leave Autolimit on (`icc.auto_limit_disable = false`).

## ReadChannelFeedback returns NotReady

- **Every time**: the measurement period is the dither period (section 4.4.2) and DITHER_CLK_DIV resets to 0, so
  with `dither.mant` = 0 the feedback never updates. Give the channel a dither reference clock
  (`CurrentControlChannel()` sets 1 ms), even with no dither amplitude.
- **Sometimes**: no new period since the last read; read at most once per measurement period.
- The channel must be enabled with a setpoint above 0 in Mission Mode, with EN high.

## Calls return UnexpectedReset

POR_event or RES_event read 1 in an FB_STAT anchor after the start-up had cleared them: the chip reset since the
driver started it, usually a brown-out on VDD or a RESN glitch. The driver is uninitialised again;
`EnsureInitialized()` configures it. If it repeats, check the supply, its decoupling and RESN.

## Reads return Crc now and then

Single corrupted replies point at signal integrity: lower the bus clock, shorten the wires, add a ground next to
the clock line. The driver retries a read up to `Config::max_retries` times (`GetBusDiagnostics().retries`); a
write is never retried, so its caller sees the error.
