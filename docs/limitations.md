# Status and limitations

What this driver has and has not been shown to do, so you can decide whether it is ready for your use. Section
numbers refer to the datasheet, Rev. 1.2.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Infineon IFX-ntz1491827137291 1.2:

    - Reset times t_RESN and t_POR: 4.1.3 Electrical characteristics power supply, Table 9, [p. 15](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=15)
    - Setpoint range and accuracy: 4.6.3 Electrical characteristics current control, Table 14, [p. 28](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=28)
    - ON-state and OFF-state diagnosis: 4.9.1 TDS_Overview; Figures 14, 15, [p. 32](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=32)
    - SPI watchdog: 5.2.2 TDS_SPI watchdog; Equation 33, [p. 50](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=50)
    - Central registers, reset values and bit types: 5.3.1 Overview of Register Types; 5.3.2 Central registers, [p. 54](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=54)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Status

| | |
| --- | --- |
| **Version** | 0.1.0, not released |
| **On hardware** | **Not verified on hardware.** No board has run this driver yet: run the bench steps below first |
| **Tested against** | A protocol model of the chip written from the datasheet ([Testing and CI](testing.md)), a fuzzer and the quality gates |
| **Assurance level** | `high` (Kokeb Labs handbook): not qualified for use in a safety function. The chip is an ASIL C safety element out of context; this driver is not part of any safety case |
| **Parts** | TLE92466ED, design steps A11 (ICVID version 0xFC) and A13 (0xFE). One chip per driver object |

## Interpretations of the datasheet

Where the datasheet leaves room, the driver takes the reading below and a bench step confirms it.

| Topic | The datasheet says | The driver | Bench step |
| --- | --- | --- | --- |
| Clearing flags | Diagnosis and voltage flags are "cleared on write" (4.1.2, 4.9.1) without a polarity; 5.3.1 defines rwh only as "modifiable by the IC". Counter-evidence: the SFF_BIST result bits clear by writing 0 (4.9.8) | Reads every register first and latches it; writes 1 only to the rwh flags that read 1, 0 to the rw ones (POR_EVENT, RES_EVENT, VDD2V5_UV/OV), and nothing to a register with no flag set. Under a write-0 or any-write reading the flags are still read and latched before they go | 1, **a release gate** |
| POR_EVENT | Set by any power-on reset; "since the bit was set to 0" (4.1.1) | Clears it at start-up, and arms the reset watch only if it then reads 0 | 2 |
| CHIPID0..2 | Drawn with a bits 21..16 row and a six-digit reset value: 22-bit replies (5.3.2.24) | Expects 22-bit replies | 3 |
| CH_CTRL, MODE, CH_CONFIG | rw (5.3.2.3, 5.3.3.10, 5.3.3.11) | Writes them whole from its copy and never relies on reading them; `Verify::ReadBack` reads them | 4 |
| SETPOINT | rwh (5.3.3.3) | `Verify::ReadBack` compares all 16 bits | 4 |
| Read frame bits 23..17 | Don't care (5.2.3.2) | Sent as 1100000B, so a looped-back frame cannot pass as a reply | 5 |
| Reply CRC | "Added to all SPI communication frames"; CRC_EN only turns the chip's check off (5.1.2) | Checks every reply's CRC, whatever CRC_EN | 5 |
| RESN pulse | No minimum low time | 1 ms (`kResetPulseUs`) | 6 |
| INIT_DONE, BIST | No duration | Polled every 100 µs; INIT_DONE up to `Config::init_timeout_us`, BIST up to 10 ms | 6, 9 |
| Parallel setpoint | 2.7 A including dither and overshoot (Table 14, feature list) | Refuses a setpoint plus dither amplitude above 2.43 A on a master: the single channel's 1.8 A of its 2.0 A range, the same 90 %, leaves room for the overshoot | 7 |
| Critical fault frame | Bits 31..24 don't care: no CRC (5.2.3.5) | Counts it only when the next frame is one too; then drives EN low and leaves the driver uninitialised | 5 |
| VPRE_OV and Mission Mode | Figure 4 lists the internal supplies but not VPRE_OV; VPRE_OV does not end Mission Mode (4.1.2) | `EnterMissionMode()` refuses while VPRE_OV is set (SUP_NOK_INT): a fail-safe choice | none |
| SPI watchdog at start | WD_TIME resets to 1 (0.585 ms) (5.3.2.11) | SPI_WD_EN is the last start-up write, after WD_RELOAD; `Validate()` refuses `spi_watchdog` with fewer than 9 counts (5.3 ms) | 10 |
| Write reply | "The actual contents of addressed register will be returned" during the next frame (5.2.1), without saying before or after the write | Safe under either reading: the CH_CTRL copy holds an EN_CH or OP_MODE bit only when both the value written and the reply have it, so it can under-report a channel that is on but never report one that is off; every enable change reads CH_CTRL first, the OFF-state overcurrent check counts a channel asked on as on until a read finds it off, and dropped channels are recorded only from a validated read. The host tests run in full under both readings (`tle92466ed_host_tests`, `tle92466ed_host_tests_reply_before_write`) | 4 |
| CH_CTRL after a fault | The chip clears EN_CH on a channel fault and leaves Mission Mode on a supply, clock or watchdog fault (4.1.2, 4.2.2, 4.9.2 to 4.9.6, 5.2.2) | Reads CH_CTRL before every enable change and after `EnterMissionMode()`; never sets an EN_CH bit or OP_MODE it was not asked for | 4 |
| A frame the chip ignores | "If the number of bits transferred is not 32, the data frame is ignored" (5.1.1) | Expects the previous reply to be sent again and refuses a word that repeats in both slots | 8 |

## Choices this driver does not make, and why

Each of these is a common shortcut with this chip; the datasheet says otherwise:

- **CRC_EN left on.** It is the reset value (5.3.2.4) and protects every configuration write. Turning it off does
  not help a noisy MISO: the replies carry a CRC either way (5.1.2), and the driver retries a bad read.
- **VIO_SEL at its reset value, 5 V.** Forcing 3.3 V on a 5 V board makes VIO read as overvoltage; the board's VIO
  belongs in `Config::vio`.
- **Setpoints up to 1.5 A, not 2 A.** TARGET reaches 2 A, but values above 0x6000 (1.5 A) are saturated (5.3.3.3)
  and Table 14 specifies 0 to 1500 mA.
- **The PWM target within 110 Hz to 4 kHz** (Table 6), in the low-frequency range too; a target below 110 Hz is
  refused.
- **WD_TIME is 11 bits** (5.3.2.11): at most 2047 counts, 1.198 s at 28 MHz.
- **CHIPID0..2 replies are 22 bits** (bench step 3).
- **MIN_INT_THRESH stays at its reset value 0 in `Config{}`**; the datasheet recommends above 1 (4.6.2.4), and the
  `CurrentControlChannel()` preset sets 2.

## Bench steps

Run these on a board with a TLE92466ED, VDD at 5 V, VBAT at 12 V to 14 V and a solenoid or a 10 Ω, 1 mH load on
LOAD0 with its freewheeling diode, using the ESP32 example or your own port. Record each result in the pull request.

1. **Flag clearing. A release gate: no final (non-rc) release before this step has passed.** Inject a VBAT
   undervoltage (lower VBAT below 4 V, then restore it). `ReadFaults()` shows `VbatUndervoltage`; `ClearFaults()`;
   `ReadFaults()` shows it clear. Set OLSG on CH0 (disconnect the load, enable at 100 mA) and on CH1;
   `ClearChannelFaults(Ch0)` clears CH0's flag and leaves CH1's set, so a write to one channel's byte does not clear
   the other's. With every flag clear, `ReadRegister()` of DIAG_ERR_CHGR0 after writing 0x1F1F to it (a debugger or a
   test build) still reads 0: writing 1 to a clear flag does not set it. Also check that POR_EVENT clears with a
   0, and that OC needs exactly two clears.
2. **POR_EVENT.** After `EnsureInitialized()`, `IsResetWatchArmed()` is true. Pulse VDD off for 100 ms: the next
   `ReloadWatchdog()` or `SetCurrent()` returns `UnexpectedReset`.
3. **CHIPID width.** `ReadChipId()` returns `Ok` and the same ID on every call.
4. **Read-back and the write reply.** Find which reading the chip uses: write CH_CTRL with CH_PAR_4_5 set in Config
   Mode, then again with it clear, and capture the second write's reply on a logic analyser (CH_PAR_4_5 set: the
   reply is the contents before the write; clear: after). The driver is safe under either; the answer only says how
   often the copy under-reports. With OL latched on CH0, `EnableChannel(Ch0)` without read-back leaves
   `GetEnabledChannels()` at 0 and `GetDroppedChannels()` at 0; the next `ReadChannelControl()` shows CH0 in
   `GetDroppedChannels()`. `EnterMissionMode()`, `EnableChannel(Ch0, Verify::ReadBack)`,
   `SetCurrent(Ch0, 300000, Verify::ReadBack)` and `ConfigureChannel(..., Verify::ReadBack)` in Config Mode all
   return `Ok`.
5. **Frames.** At 1 MHz and at 8 MHz, `ReadRegister(0x0200)` returns 0xC1FC or 0xC1FE; `GetBusDiagnostics()` shows
   no rejected reply over 10000 calls. Capture one access on a logic analyser: the request, the anchor, CSN high
   at least 600 ns between them.
6. **Reset and readiness.** Scope RESN and SO through `EnsureInitialized()`: RESN low for 1 ms, the first FB_STAT
   reads INIT_DONE = 1 within a few polls. Repeat with RESN tied high (a port without `GpioSet()`).
7. **Current.** In Mission Mode, set CH0 to 100 mA, 500 mA and 1000 mA: `ReadChannelFeedback()` reports within
   ±2.5 mA below 250 mA and ±1 % above (Table 14), and a clamp meter agrees. Join CH1 and CH2, load them with one
   coil, set 2 A: the feedback reports the sum.
8. **Ignored frame.** With a logic analyser that can inject, cut one request frame short (31 clocks): the call
   returns `ReplyMismatch` (a write) or succeeds after one retry (a read).
9. **Self-test.** `RunBist()` in Config Mode returns `passed`, both error classes tested, within 10 ms; afterwards
   `EnterMissionMode()` works.
10. **SPI watchdog.** With `spi_watchdog` and a 50 ms timeout, stop calling `ReloadWatchdog()`: every channel turns
    off within 50 ms, `ReadStatus()` shows `spi_watchdog_error`, and a reload plus `ClearFaults()` lets
    `EnterMissionMode()` work again.
11. **Diagnosis.** Open the load of CH0 while it regulates 300 mA with `ol_fixed` = 6: OLSG sets and the channel
    turns off; with the setpoint kept and EN_CH off, the OFF-state diagnosis reports OL. Short LOAD0 to ground:
    it reports SG. Short LOAD0 to VBAT and run `StartOvercurrentCheck(Ch0)` with `on_time_mant` = 100: OC sets.
12. **ESP32 port timing.** Scope CSN and SCK at 2 MHz: t_CSN_LEAD and t_CSN_LAG at least 250 ns.

## Not covered

- Supply-monitor and overtemperature test modes (GLOBAL_CONFIG.UV_OV_SWAP, OT_TEST, V1V5_UV/OV_TEST): the last two
  reset the chip on purpose. Use them only with a custom sequence.
- FB_I_AVG_s16 and FB_INT_THRESH have no decoded call; `ReadRegister()` reads them.
- Daisy chains and more than one chip per driver object.
