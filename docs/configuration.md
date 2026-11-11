# Configuration

Every field of `tle92466ed::Config` and `ChannelConfig`, the current-control preset, how a configuration is checked,
and the optional Kconfig layer. Section numbers refer to the datasheet, Rev. 1.2.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Infineon IFX-ntz1491827137291 1.2:

    - Functional range of VDD, VBAT, VIO, f_SYS and f_PWM: 3.2.1 Functional range, Table 6, [p. 11](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=11)
    - Supplies, VIO_SEL and the power-on reset: 4.1.1 TDS_Power Supply, [p. 14](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=14)
    - Voltage monitoring and the VBAT thresholds: 4.1.2 TDS_Voltage monitoring; Equation 1, [p. 14](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=14)
    - Clock source, PLL dividers and INIT_DONE: 4.2.1 TDS_Clock; Equations 2, 3; Table 10, [p. 16](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=16)
    - Clock watchdog: 4.2.2 TDS_Clock Watchdog, [p. 17](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=17)
    - Channel modes and the measurement period: 4.4 Channel modes, [p. 21](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=21)
    - Parallel channel pairs: 4.5.2 TDS_Parallel channel operation, [p. 22](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=22)
    - Slew rate: 4.5.3 Electrical characteristics power stages, Table 13, [p. 23](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=23)
    - Integrating current controller, Autolimit and the PWM frequency controller: 4.6.2 Integrating Current Controller (ICC); Equations 4, 5, [p. 24](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=24)
    - Dither waveform and its update: 4.7.1 TDS_Dither configuration; 4.7.2 TDS_Dither parameter update; Equations 6 to 9, [p. 29](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=29)
    - Direct drive via SPI on-time and DRV pins: 4.8.1 TDS_Direct Drive; Equation 10, [p. 32](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=32)
    - ON-state and OFF-state diagnosis: 4.9.1 TDS_Overview; Figures 14, 15, [p. 32](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=32)
    - Fast feedback and the sequence counters: 4.10.3 TDS_Fast measurement feedback; 4.10.4 TDS_Sequence counter, [p. 42](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=42)
    - SPI watchdog: 5.2.2 TDS_SPI watchdog; Equation 33, [p. 50](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=50)
    - Central registers, reset values and bit types: 5.3.1 Overview of Register Types; 5.3.2 Central registers, [p. 54](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=54)
    - Channel registers and their bank addresses: 5.3.3 Channel registers, Tables 24 and 25, [p. 85](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=85)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Global fields

`Config{}` holds the chip's reset values (section 5.3), so it changes nothing on a freshly reset chip, and declares
no channel loaded. Fields with a unit carry it in the name.

| Field | Default | Range | Register |
| --- | --- | --- | --- |
| `vio` | `VioLevel::V5V0` | `V3V3`, `V5V0`: **set it to your board's VIO** | GLOBAL_CONFIG.VIO_SEL |
| `crc_check` | `true` | | GLOBAL_CONFIG.CRC_EN |
| `clock_watchdog` | `true` | | GLOBAL_CONFIG.CLK_WD_EN |
| `spi_watchdog` | `false` | | GLOBAL_CONFIG.SPI_WD_EN |
| `watchdog_reload` | 1 | 1 to 2047 counts of 0.585 ms (`WatchdogReloadCode()`) | WD_RELOAD.WD_TIME |
| `clock` | `ClockSource::Internal` | `Internal`, `External` | CLK_DIV.EXT_CLK |
| `external_clock_khz` | 0 | 1000 to 8000 in 500 kHz steps (Table 10) | CLK_DIV dividers |
| `vbat_uv_code`, `vbat_ov_code` | 0x19 (4.05 V), 0xFF (41.3 V) | UV below OV; 162.08 mV per count (`VbatThresholdCode()`) | VBAT_TH |
| `fault_pin` | every source, EN off | `FaultPinMask` | FAULT_MASK0..2 |
| `parallel` | all `false` | indexed by `ParallelPair` | CH_CTRL.CH_PAR_x_y |
| `channels` | `ChannelConfig{}` each | indexed by `Channel` | the channel banks |
| `init_timeout_us` | 10000 | 100 to 100000 | driver only: INIT_DONE wait |
| `max_retries` | 2 | 0 to 3 | driver only: read retries |

The driver always sends a correct CRC and checks every reply's CRC; `crc_check` only decides whether the chip
checks the driver's frames. With an external clock, `PllRefDiv()` and `PllFbDiv()` give the dividers of Table 10
and `SystemClockHz()` the resulting f_SYS, which every time conversion uses.

## Channel fields

| Field | Default | Range | Register |
| --- | --- | --- | --- |
| `loaded` | `false` | | driver only: may be enabled; load-dependent diagnosis on |
| `mode` | `ChannelMode::Off` | `Off`, `CurrentControl`, `DirectDriveSpi`, `DirectDriveDrv0`, `DirectDriveDrv1`, `Measurement` | MODE.CH_MODE (Config Mode only) |
| `slew` | `SlewRate::VPerUs10` | 1, 2.5, 5, 10 V/µs | CH_CONFIG.SLEWR |
| `on_time_mant` | 0 | 0 to 1023 | TON.TON_MANT: direct-drive on-time, OFF-state OC on-time |
| `icc.min_int_thresh` | 0 | -128 to 127; above 1 recommended (4.6.2.4) | CTRL.MIN_INT_THRESH |
| `icc.skip_falling_dither` | `false` | | CTRL.PWM_PERIOD_CALC_MODE |
| `icc.period_mant`, `period_exp`, `low_freq_range` | 0, 0, `false` | 110 Hz to 4 kHz, or mantissa 0 (controller off) | PERIOD |
| `icc.ki` | 0 | 0 to 15 | PERIOD.PWM_CTRL_PARAM |
| `icc.limit`, `icc.auto_limit` | 1023, 16 | 0 to 1023; 0 to 31 and above `min_int_thresh` + 3 | INTEGRATOR_LIMIT |
| `icc.auto_limit_disable` | `false` | | SETPOINT.AUTO_LIMIT_DIS |
| `icc.int_thresh` | 3 | 0 to 511 | CTRL_INT_THRESH |
| `dither.mant`, `dither.exp` | 0, 0 | 0 to 1023, 0 to 15 | DITHER_CLK_DIV: t_ref_clk = MANT * 2^EXP / f_SYS |
| `dither.pwm_sync`, `setpoint_sync` | `false` | | DITHER_CLK_DIV (4.7.3, 4.7.5) |
| `dither.steps`, `dither.flat` | 0, 0 | 0 to 255 | DITHER_STEP |
| `dither.step_size`, `deep`, `fast` | 0, `false`, `DitherPeriod` | 0 to 4095; fast feedback needs FLAT above 2 | DITHER_CTRL |
| `diag.current` | `DiagCurrent::Ua80` | 80, 190, 720, 1250 µA | CH_CONFIG.I_DIAG |
| `diag.ol_relative_eighths` | 0 | 0 (off) to 7; current control only | CH_CONFIG.OL_TH |
| `diag.ol_fixed` | 0 | 0 to 63, 7.8 mA per count (15.6 mA in a pair) | CH_CONFIG.OL_TH_FIXED |
| `diag.off_state` | `OffStateDiag::Enabled` | `Enabled`, `LowSideSource`, `HighSideSource`, `Disabled` | CH_CONFIG.OFF_DIAG_CH |
| `diag.olsg_warning`, `olsg_warn_window` | `true`, 3 | 0 to 31 | CTRL.OLSG_WARN_EN, OLSG_WARN_WINDOW |
| `diag.olsg_timeout` | 0 | 0 to 63 | TON.OLSG_TIMEOUT |

On a channel that is not `loaded` the driver writes the OFF-state diagnosis off and both open-load thresholds 0,
and leaves it out of FAULT_MASK0 and FAULT_MASK1, whatever its fields say (handbook KB-CPP-131): an unconnected
output would otherwise report an open load forever.

!!! warning "No dither clock, no feedback"
    In current control and direct drive the dither period is the feedback measurement period (section 4.4.2).
    DITHER_CLK_DIV resets to 0, so with `dither.mant` = 0 the averaged feedback never updates and
    `ReadChannelFeedback()` returns `NotReady`. `CurrentControlChannel()` sets a 1 ms reference clock.

## The current-control preset

`CurrentControlChannel()` returns a loaded channel in current control with a 1 kHz PWM target (PERIOD_MANT 219,
PERIOD_EXP 7: 998 Hz), MIN_INT_THRESH 2 so the integrator threshold cannot go negative (section 4.6.2.4), a 1 ms
dither reference clock with no dither amplitude so the averaged feedback updates every millisecond, and the reset
values elsewhere. Tune the PWM target, the controller gain, the dither and the open-load threshold to the load:

```cpp
--8<-- "tests/docs/configuration.cpp:custom"
```

## Parallel pairs

CH0 and CH3, CH1 and CH2, CH4 and CH5 can drive one load together (section 4.5.2). Only the master is configured
and driven; the slave's MODE, SETPOINT and EN_CH are ignored by the chip, and `Validate()` refuses a slave declared
`loaded`. One TARGET count is then twice the current, the fixed open-load threshold doubles, and FB_I_AVG reports
the sum of both shunts.

```cpp
--8<-- "tests/docs/configuration.cpp:parallel"
```

## Validation

`tle92466ed::Validate(config)` checks a configuration without touching the chip, and `EnsureInitialized()`,
`ApplyConfig()`, `ConfigureChannel()`, `SetParallel()` and `SetDither()` call it before any I/O. It returns
`Error::InvalidArgument` when an enumeration holds a value that is not an enumerator, a field does not fit its
register, WD_TIME is 0, VBAT_UV_TH is not below VBAT_OV_TH, an external clock is not a Table 10 step or gives an
f_SYS outside 27.5 to 28.5 MHz, a PWM target is outside 110 Hz to 4 kHz (Table 6), AUTO_LIM_VALUE_ABS is not above
MIN_INT_THRESH + 3 (note in 4.6.2.3), fast feedback has FLAT of 2 or less (4.10.3), a relative open-load threshold
is set outside current control (4.9.2.1), or the slave of an enabled pair is declared loaded.
`CheckedConfig{config}` does the same at build time: an invalid configuration does not compile.

## Kconfig

Under ESP-IDF, `idf.py menuconfig` shows **TLE92466ED solenoid driver** when the driver is a component of the
project ([Installation](installation.md)); the menu is [`_config/Kconfig`](../_config/Kconfig). The options reach
the code only through [`inc/tle92466ed_kconfig.hpp`](../inc/tle92466ed_kconfig.hpp), never through a driver
signature. Channels are not Kconfig options: declare them in code.

```cpp
--8<-- "tests/docs/configuration.cpp:kconfig"
```

| Option | Default | `Config` field |
| --- | --- | --- |
| `CONFIG_TLE92466ED_VIO_3V3` | n | `vio` |
| `CONFIG_TLE92466ED_CRC_CHECK_DISABLE` | n | `crc_check` |
| `CONFIG_TLE92466ED_SPI_WATCHDOG` | n | `spi_watchdog` |
| `CONFIG_TLE92466ED_WATCHDOG_TIMEOUT_MS` | 100 | `watchdog_reload`, through `WatchdogReloadCode()` |
| `CONFIG_TLE92466ED_EXTERNAL_CLOCK_KHZ` | 0 (internal) | `clock`, `external_clock_khz` |
| `CONFIG_TLE92466ED_INIT_TIMEOUT_US` | 10000 | `init_timeout_us` |
| `CONFIG_TLE92466ED_MAX_RETRIES` | 2 | `max_retries` |

Without Kconfig (plain CMake, other platforms) `KconfigDefaults()` returns `Config{}`. Both cases are host tests.

Next: [Porting](porting.md).
