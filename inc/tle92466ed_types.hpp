/**
 * @file tle92466ed_types.hpp
 * @brief Public types of the TLE92466ED driver: errors, `Result<T>`, configuration, decoded
 * status and feedback, and the unit conversions.
 * @copyright Copyright (c) 2026 The driver-tle92466ed authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Everything here is plain data with no platform dependency, so handlers, tests and tools can
 * include it without the driver template. The configuration defaults are the chip's reset values
 * (TLE92466ED datasheet Rev. 1.2, section 5.3), so `Config{}` describes a chip that was just
 * reset. Section numbers below refer to that datasheet.
 */
#ifndef TLE92466ED_TYPES_HPP
#define TLE92466ED_TYPES_HPP

#include <array>
#include <cstdint>

#include "tle92466ed_detail.hpp"
#include "tle92466ed_version.h"

/** @brief The TLE92466ED driver: everything it declares lives here; internals in `detail`. */
namespace tle92466ed {

/**
 * @defgroup tle92466ed_types Driver types
 * @brief Errors, results, configuration and decoded values.
 * @{
 */

/**
 * @brief Why a call failed. Every call of the driver returns one of these in its `Result`.
 *
 * Values 0 to 9 are the shared base set; chip-specific values start at 16. `[[nodiscard]]` on
 * the type: a function that returns a bare `Error` cannot have it ignored either (KB-CPP20-037).
 */
enum class [[nodiscard]] Error : uint8_t {
  // Base set: the same names and values in every driver of the org (standards/driver.md).
  Ok = 0,               ///< The call succeeded.
  NotInitialized = 1,   ///< `EnsureInitialized()` has not succeeded, `Deinitialize()` ran since,
                        ///< or the chip was found reset (`UnexpectedReset`).
  InvalidArgument = 2,  ///< An argument or a `Config` field is out of range; nothing was sent.
  Bus = 3,              ///< A port hook (`Transfer()`, `EnsureInitialized()`, `GpioSet()`,
                        ///< `GpioRead()`) returned `false`.
  Timeout = 4,          ///< INIT_DONE did not read 1 within `Config::init_timeout_us` (section
                        ///< 4.2.1), or the self-test did not finish (section 4.9.8).
  DeviceIdMismatch = 5, ///< ICVID is not manufacturer 0xC1 with a known version (5.3.2.19).
  Crc = 6,              ///< A reply's CRC-8 was wrong, or the chip reported a wrong CRC in the
                        ///< driver's frame (status 00010B, sections 5.1.2 and 5.2.3.3).
  Busy = 7,             ///< Not returned by this driver: the chip is never busy to SPI.
  DeviceFault = 8,      ///< The chip reports damage: OTP_VIRGIN set at start-up (4.9.7), or an
                        ///< internal bus fault in a reply's status (5.2.3.3).
  Unsupported = 9,      ///< The port lacks the optional hook this call needs (`GpioSet()`,
                        ///< `GpioRead()`).
  // Chip-specific values start at 16; each one is returned by some call (tests/test_errors.cpp).
  NoDevice = 16,        ///< MISO read all zeros, all ones or the frame just sent: nothing drives
                        ///< SO (no chip, no VIO, a loose wire) or MOSI is looped back.
  CriticalFault = 17,   ///< The chip answered with a critical fault frame (5.2.3.5), twice in a
                        ///< row: it is in the Critical Fault state; `GetLastCriticalFault()` has
                        ///< the flags. EN is driven low and the driver is uninitialised: only a
                        ///< power-on reset or RESN leaves that state (Figure 4).
  ReplyMismatch = 18,   ///< A reply with a valid CRC that does not answer the request: wrong
                        ///< reply mode or width, wrong R/W echo, or the anchor read did not
                        ///< answer as expected (a frame lost or added on the bus).
  FrameRejected = 19,   ///< The chip reported an SPI frame error or a write to a read-only
                        ///< register in its reply status (5.2.3.3); the frame was ignored.
  WrongState = 20,      ///< The call needs the other operation state (Config Mode or Mission
                        ///< Mode, section 4.3.1), or the channel is the slave of a parallel
                        ///< pair, or it depends on a register an earlier failed write may have
                        ///< changed (docs/contract.md, "Ambiguous writes"). Nothing was sent.
  MissionBlocked = 21,  ///< `EnterMissionMode()` found a fault that keeps the chip in Config
                        ///< Mode (Figure 4) or INIT_DONE = 0, or the chip refused OP_MODE; or an
                        ///< enable found the chip back in Config Mode on its own.
  WriteMismatch = 22,   ///< A register read back differs from what was written: at start-up,
                        ///< or with `Verify::ReadBack`. The chip did not take the write.
  NotReady = 23,        ///< No feedback to report: no new values (FB_UPD = 0) or no
                        ///< measurement period (TP_MANT = 0, section 4.10.2).
  UnexpectedReset = 24, ///< The chip reset since the start (POR_event or RES_event in FB_STAT,
                        ///< section 4.1.1). EN is driven low and the driver is uninitialised.
};

/**
 * @brief The name of an error, for logs.
 * @param error The error.
 * @return A static string such as `"Timeout"`; `"Unknown"` for a value that is not an enumerator.
 */
constexpr const char* ToString(Error error) noexcept {
  switch (error) {
  case Error::Ok:
    return "Ok";
  case Error::NotInitialized:
    return "NotInitialized";
  case Error::InvalidArgument:
    return "InvalidArgument";
  case Error::Bus:
    return "Bus";
  case Error::Timeout:
    return "Timeout";
  case Error::DeviceIdMismatch:
    return "DeviceIdMismatch";
  case Error::Crc:
    return "Crc";
  case Error::Busy:
    return "Busy";
  case Error::DeviceFault:
    return "DeviceFault";
  case Error::Unsupported:
    return "Unsupported";
  case Error::NoDevice:
    return "NoDevice";
  case Error::CriticalFault:
    return "CriticalFault";
  case Error::ReplyMismatch:
    return "ReplyMismatch";
  case Error::FrameRejected:
    return "FrameRejected";
  case Error::WrongState:
    return "WrongState";
  case Error::MissionBlocked:
    return "MissionBlocked";
  case Error::WriteMismatch:
    return "WriteMismatch";
  case Error::NotReady:
    return "NotReady";
  case Error::UnexpectedReset:
    return "UnexpectedReset";
  default:
    break;
  }
  return "Unknown";
}

/**
 * @brief The value of a call, or why there is none.
 *
 * Check `ok()` (or the `bool` conversion) before reading `value`; on failure `value` is `T{}`.
 *
 * @tparam T The value type. `Result<void>` carries only the error.
 */
template <typename T>
struct [[nodiscard]] Result {
  T value{};              ///< The value; `T{}` when the call failed.
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /**
   * @brief A successful result.
   * @param v The value.
   * @return `{v, Error::Ok}`.
   */
  static constexpr Result Success(T v) noexcept {
    return {v, Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{T{}, e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {T{}, e};
  }
};

/** @brief The result of a call that returns no value. */
template <>
struct [[nodiscard]] Result<void> {
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /** @return A successful result. */
  static constexpr Result Success() noexcept {
    return {Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {e};
  }
};

/** @brief Severity passed to the optional `Log()` hook. */
enum class LogLevel : uint8_t {
  Error = 0, ///< A call failed.
  Warning,   ///< Something unexpected that the driver recovered from.
  Info,      ///< Lifecycle events: initialised, reset.
  Debug,     ///< Detail for bring-up.
};

/**
 * @brief Extra traffic a setter may spend to confirm its write (handbook KB-ARCH-041).
 */
enum class Verify : uint8_t {
  None = 0,     ///< No extra frame and no compare. The default.
  ReadBack = 1, ///< Read the written register back (two more frames) and compare it.
};

/** @brief One of the six output channels. */
enum class Channel : uint8_t {
  Ch0 = 0, ///< LOAD0. Master of the CH0/CH3 pair.
  Ch1 = 1, ///< LOAD1. Master of the CH1/CH2 pair.
  Ch2 = 2, ///< LOAD2. Slave of the CH1/CH2 pair.
  Ch3 = 3, ///< LOAD3. Slave of the CH0/CH3 pair.
  Ch4 = 4, ///< LOAD4. Master of the CH4/CH5 pair.
  Ch5 = 5, ///< LOAD5. Slave of the CH4/CH5 pair.
};
/// Number of output channels (section 4.5.1).
inline constexpr uint32_t kChannelCount = 6U;

/** @brief The channel pairs that can drive one load together (section 4.5.2). */
enum class ParallelPair : uint8_t {
  Ch0Ch3 = 0, ///< CH0 master, CH3 slave (CH_CTRL.CH_PAR_0_3).
  Ch1Ch2 = 1, ///< CH1 master, CH2 slave (CH_CTRL.CH_PAR_1_2).
  Ch4Ch5 = 2, ///< CH4 master, CH5 slave (CH_CTRL.CH_PAR_4_5).
};
/// Number of parallel pairs.
inline constexpr uint32_t kPairCount = 3U;

/** @brief How a channel drives its output (MODE.CH_MODE, section 5.3.3.11). */
enum class ChannelMode : uint8_t {
  Off = 0,             ///< Output off, no measurement. Reset value.
  CurrentControl = 1,  ///< ICC: the integrating current controller holds the setpoint (4.6.2).
  DirectDriveSpi = 2,  ///< On-time `on_time_mant` per dither period, from SPI (4.8.1).
  DirectDriveDrv0 = 3, ///< The output follows the DRV0 pin (4.8.1).
  DirectDriveDrv1 = 4, ///< The output follows the DRV1 pin (4.8.1).
  Measurement = 5,     ///< Free-running measurement, switch off (4.4.3); CH_MODE = CH.
};

/** @brief Output current slew rate (CH_CONFIG.SLEWR, Table 13). */
enum class SlewRate : uint8_t {
  VPerUs1 = 0,   ///< 1 V/us typical.
  VPerUs2p5 = 1, ///< 2.5 V/us typical.
  VPerUs5 = 2,   ///< 5 V/us typical.
  VPerUs10 = 3,  ///< 10 V/us typical. Reset value.
};

/** @brief OFF-state diagnosis current I_HS, I_LS (CH_CONFIG.I_DIAG, Table 15). */
enum class DiagCurrent : uint8_t {
  Ua80 = 0,   ///< 80 uA typical. Reset value.
  Ua190 = 1,  ///< 190 uA typical.
  Ua720 = 2,  ///< 720 uA typical.
  Ua1250 = 3, ///< 1250 uA typical.
};

/** @brief OFF-state diagnosis current sources (CH_CONFIG.OFF_DIAG_CH, section 4.9.1). */
enum class OffStateDiag : uint8_t {
  Enabled = 0,        ///< The OL/SG sequence of Figure 15 runs. Reset value.
  LowSideSource = 1,  ///< Only I_LS on.
  HighSideSource = 2, ///< Only I_HS on.
  Disabled = 3,       ///< No OFF-state diagnosis.
};

/** @brief Feedback update interval (DITHER_CTRL.FAST_MEAS, section 4.10.3). */
enum class FastFeedback : uint8_t {
  DitherPeriod = 0,  ///< One update per dither period. Reset value.
  HalfPeriod = 1,    ///< n_meas_sample = 2.
  QuarterPeriod = 2, ///< n_meas_sample = 4.
};

/** @brief The level of VIO, the SPI output supply (GLOBAL_CONFIG.VIO_SEL, section 4.1.1). */
enum class VioLevel : uint8_t {
  V3V3 = 0, ///< 3.3 V: VIO monitored against the 3.3 V thresholds (Table 9).
  V5V0 = 1, ///< 5.0 V. Reset value.
};

/** @brief Where the PLL takes its reference (CLK_DIV.EXT_CLK, section 4.2.1). */
enum class ClockSource : uint8_t {
  Internal = 0, ///< The internal oscillator; the dividers are ignored. Reset value.
  External = 1, ///< The CLK pin, `Config::external_clock_khz`.
};

/**
 * @brief The integrating current controller of one channel (CTRL, PERIOD, INTEGRATOR_LIMIT,
 * CTRL_INT_THRESH, SETPOINT.AUTO_LIMIT_DIS; section 4.6.2). Defaults are the reset values.
 */
struct IccConfig {
  int8_t min_int_thresh{0};        ///< CTRL.MIN_INT_THRESH, -128 to 127; above 1 keeps the
                                   ///< threshold positive (4.6.2.4). Reset value 0.
  bool skip_falling_dither{false}; ///< CTRL.PWM_PERIOD_CALC_MODE (4.6.2.3).
  uint8_t period_mant{0};          ///< PERIOD.PERIOD_MANT; 0 turns the PWM controller off.
  uint8_t period_exp{0};           ///< PERIOD.PERIOD_EXP, 0 to 7.
  bool low_freq_range{false};      ///< PERIOD.LOW_FREQ_RANGE_EN: the period times 8.
  uint8_t ki{0};                   ///< PERIOD.PWM_CTRL_PARAM, the controller gain, 0 to 15.
  uint16_t limit{1023};            ///< INTEGRATOR_LIMIT.LIM_VALUE_ABS, 0 to 1023.
  uint8_t auto_limit{16};          ///< INTEGRATOR_LIMIT.AUTO_LIM_VALUE_ABS, 0 to 31; must exceed
                                   ///< `min_int_thresh` + 3 (note in 4.6.2.3).
  bool auto_limit_disable{false};  ///< SETPOINT.AUTO_LIMIT_DIS (4.6.2.2).
  uint16_t int_thresh{3};          ///< CTRL_INT_THRESH.INT_THRESH, 0 to 511.
};

/**
 * @brief The dither generator of one channel (DITHER_CLK_DIV, DITHER_STEP, DITHER_CTRL; section
 * 4.7). In current control and direct drive the dither period is also the feedback measurement
 * period (section 4.4.2): with `mant` = 0 the averaged feedback never updates. Defaults are the
 * reset values.
 */
struct DitherConfig {
  uint16_t mant{0};          ///< DITHER_CLK_DIV.MANT of t_ref_clk, 0 to 1023.
  uint8_t exp{0};            ///< DITHER_CLK_DIV.EXP, 0 to 15; also the TON exponent.
  bool pwm_sync{false};      ///< DITHER_CLK_DIV.DITHER_PWM_SYNC_EN (4.7.3).
  bool setpoint_sync{false}; ///< DITHER_CLK_DIV.DITHER_SETPOINT_SYNC_EN (4.7.5).
  uint8_t steps{0};          ///< DITHER_STEP.STEPS per quarter period.
  uint8_t flat{0};           ///< DITHER_STEP.FLAT reference clocks at each plateau.
  uint16_t step_size{0};     ///< DITHER_CTRL.STEP_SIZE, 0 to 4095, in TARGET counts.
  bool deep{false};          ///< DITHER_CTRL.DEEP_DITHER (4.7.7).
  FastFeedback fast{FastFeedback::DitherPeriod}; ///< DITHER_CTRL.FAST_MEAS (4.10.3).
};

/**
 * @brief The diagnosis of one channel (CH_CONFIG, CTRL, TON; section 4.9). Defaults are the reset
 * values. On a channel `ChannelConfig::loaded` does not declare, the driver writes the OFF-state
 * diagnosis off and both open-load thresholds 0, whatever these fields say (handbook KB-CPP-131).
 */
struct DiagConfig {
  DiagCurrent current{DiagCurrent::Ua80};        ///< CH_CONFIG.I_DIAG.
  uint8_t ol_relative_eighths{0};                ///< CH_CONFIG.OL_TH, 0 (off) to 7 eighths of
                                                 ///< the setpoint; current control only.
  uint8_t ol_fixed{0};                           ///< CH_CONFIG.OL_TH_FIXED, 0 to 63, 7.8 mA per
                                                 ///< count (15.6 mA in a pair), Equation 13.
  OffStateDiag off_state{OffStateDiag::Enabled}; ///< CH_CONFIG.OFF_DIAG_CH.
  bool olsg_warning{true};                       ///< CTRL.OLSG_WARN_EN (4.9.3).
  uint8_t olsg_warn_window{3};                   ///< CTRL.OLSG_WARN_WINDOW, 0 to 31.
  uint8_t olsg_timeout{0};                       ///< TON.OLSG_TIMEOUT, 0 to 63 (Equation 15).
};

/** @brief Everything the driver writes for one channel. Defaults are the reset values. */
struct ChannelConfig {
  bool loaded{false};                 ///< A load is connected: the channel may be enabled
                                      ///< and its load-dependent diagnosis runs (driver
                                      ///< setting, KB-CPP-131).
  ChannelMode mode{ChannelMode::Off}; ///< MODE.CH_MODE (Config Mode only).
  SlewRate slew{SlewRate::VPerUs10};  ///< CH_CONFIG.SLEWR.
  uint16_t on_time_mant{0};           ///< TON.TON_MANT, 0 to 1023: direct-drive on-time and
                                      ///< the OFF-state overcurrent check (Equation 17).
  IccConfig icc{};                    ///< Current controller.
  DitherConfig dither{};              ///< Dither and measurement period.
  DiagConfig diag{};                  ///< Diagnosis.
};

/**
 * @brief Which faults pull FAULTN low (FAULT_MASK0..2, sections 5.3.2.14 to 5.3.2.16); `true`
 * reports the fault on the pin. Defaults are the reset values. A channel `loaded` does not declare
 * never reaches the pin (KB-CPP-131).
 */
struct FaultPinMask {
  uint8_t channel_errors{0x3FU};   ///< CHx_ERR_MASK, bit x: OC, SG, OL, OTE, OLSG of channel x.
  uint8_t channel_warnings{0x3FU}; ///< CHx_WARN_MASK, bit x: OTW, I_REG, PWM_REG, OLSG warnings.
  bool en_pin{false};              ///< EN_PIN_MASK: EN low.
  bool supply_internal{true};      ///< SUP_NOK_INT_MASK.
  bool supply_external{true};      ///< SUP_NOK_EXT_MASK.
  bool central_ot_warning{true};   ///< COTWARN_MASK.
  bool central_ot_error{true};     ///< COTERR_MASK.
  bool clock_too_slow{true};       ///< CLK_LOW_MASK.
  bool data_error{true};           ///< DATA_ERR_MASK.
  bool spi_watchdog{true};         ///< SPI_WD_MASK.
};

/**
 * @brief Everything `EnsureInitialized()` writes to the chip, and the driver's own settings.
 *
 * Register fields default to the chip's reset values, so `Config{}` changes nothing on a freshly
 * reset chip (and declares no channel loaded). It has no `operator==`.
 */
struct Config {
  VioLevel vio{VioLevel::V5V0}; ///< GLOBAL_CONFIG.VIO_SEL: set it to the board's VIO, or
                                ///< the VIO monitor keeps the chip in Config Mode.
  bool crc_check{true};         ///< GLOBAL_CONFIG.CRC_EN: the chip checks the driver's
                                ///< frame CRCs. The driver always sends and checks CRCs.
  bool clock_watchdog{true};    ///< GLOBAL_CONFIG.CLK_WD_EN (4.2.2).
  bool spi_watchdog{false};     ///< GLOBAL_CONFIG.SPI_WD_EN (5.2.2).
  uint16_t watchdog_reload{1};  ///< WD_RELOAD.WD_TIME, 1 to 2047 counts of 2^14/f_SYS
                                ///< (0.585 ms), at least `kMinWatchdogReload` with
                                ///< `spi_watchdog`; see `WatchdogReloadCode()`.
  ClockSource clock{ClockSource::Internal}; ///< CLK_DIV.EXT_CLK.
  uint32_t external_clock_khz{0};           ///< f_CLK on the CLK pin with `ClockSource::External`:
                                            ///< 1000 to 8000 kHz in 500 kHz steps (Table 10).
  uint8_t vbat_uv_code{0x19U};              ///< VBAT_TH.VBAT_UV_TH, 162.08 mV per count (4.05 V).
  uint8_t vbat_ov_code{0xFFU};              ///< VBAT_TH.VBAT_OV_TH (41.3 V).
  FaultPinMask fault_pin{};                 ///< FAULT_MASK0..2.
  std::array<bool, kPairCount> parallel{};  ///< CH_CTRL.CH_PAR_x_y, indexed by `ParallelPair`.
  std::array<ChannelConfig, kChannelCount> channels{}; ///< Per channel, indexed by `Channel`.
  uint32_t init_timeout_us{10000}; ///< Driver: longest wait for INIT_DONE after a reset or
                                   ///< a clock change, 100 to 100000 us (t_POR = 10 ms).
  uint8_t max_retries{2};          ///< Driver: retries of a read whose reply failed its
                                   ///< checks, 0 to `kMaxRetries`. Writes never retry.
};

/// @name Limits and timing (datasheet sections 3.2, 4.1.3, 4.6.3, 5.1.4)
/// @{

/// Highest DC setpoint of one channel, in microamps (Table 14, I_set).
inline constexpr uint32_t kMaxSetpointUa = 1'500'000U;
/// Highest setpoint plus dither amplitude of one channel (Table 14, I_set + I_Dither).
inline constexpr uint32_t kMaxSetpointWithDitherUa = 1'800'000U;
/// Measurement range of a parallel pair, setpoint, dither and overshoot included (Table 14,
/// I_LOADx,y,par).
inline constexpr uint32_t kParallelMeasurementRangeUa = 2'700'000U;
/// Highest setpoint plus dither amplitude of a parallel pair. Table 14 gives 2.7 A including the
/// overshoot; the driver keeps the single-channel headroom (1.8 A of the 2.0 A range, 90 %), so
/// 2.43 A. A design decision, **not verified on hardware**.
inline constexpr uint32_t kMaxParallelSetpointUa = (kParallelMeasurementRangeUa / 10U) * 9U;
static_assert(kMaxParallelSetpointUa == 2'430'000U);
/// Full scale of TARGET, one channel: I_set = 2 A * TARGET / (2^15 - 1) (5.3.3.3).
inline constexpr uint32_t kSetpointFullScaleUa = 2'000'000U;
/// Full scale of TARGET in a parallel pair: the LSB doubles (4.5.2).
inline constexpr uint32_t kParallelFullScaleUa = 4'000'000U;
/// Largest TARGET value, 2^15 - 1.
inline constexpr uint32_t kTargetMax = 32767U;
/// f_SYS of the internal oscillator, typical (Table 6).
inline constexpr uint32_t kInternalClockHz = 28'000'000U;
/// Lowest f_SYS (Table 6).
inline constexpr uint32_t kMinSystemClockHz = 27'500'000U;
/// Highest f_SYS (Table 6).
inline constexpr uint32_t kMaxSystemClockHz = 28'500'000U;
/// Lowest PWM frequency of the frequency controller, f_PWM min (Table 6).
inline constexpr uint32_t kMinPwmHz = 110U;
/// Highest PWM frequency, f_PWM max (Table 6).
inline constexpr uint32_t kMaxPwmHz = 4000U;
/// Largest WD_TIME, an 11-bit field (5.3.2.11).
inline constexpr uint16_t kMaxWatchdogReload = 2047U;
/// Smallest WD_TIME with the SPI watchdog on: 9 counts, 5.3 ms at 28 MHz, so the application's
/// first `ReloadWatchdog()` after the start can arrive. A design decision: the reset value 1
/// (0.585 ms) would expire before any caller could reload it.
inline constexpr uint16_t kMinWatchdogReload = 9U;
/// The SPI watchdog counts f_SYS / 2^14 (Table 18).
inline constexpr uint32_t kWatchdogDivider = 16384U;
/// Longest OFF-state overcurrent on-time t_OCon (TON register note, 5.3.3.12), microseconds.
inline constexpr uint32_t kMaxOcOnTimeUs = 300'000U;
/// Logic ready after RESN released, t_RESN max (Table 9).
inline constexpr uint32_t kResetReleaseUs = 100U;
/// Logic ready after a power-on reset, t_POR max (Table 9).
inline constexpr uint32_t kPowerOnResetUs = 10'000U;
/// RESN low time of a reset pulse. The datasheet gives no minimum; 1 ms is the design value
/// (**not verified on hardware**: bench step in docs/hardware.md).
inline constexpr uint32_t kResetPulseUs = 1000U;
/// Interval of the INIT_DONE and self-test polls.
inline constexpr uint32_t kPollIntervalUs = 100U;
/// Shortest `Config::init_timeout_us`.
inline constexpr uint32_t kMinInitTimeoutUs = 100U;
/// Longest `Config::init_timeout_us`.
inline constexpr uint32_t kMaxInitTimeoutUs = 100'000U;
/// Longest wait for SMU_SLF_TST_DONE. The datasheet gives no duration; 10 ms is the design value
/// (**not verified on hardware**).
inline constexpr uint32_t kBistTimeoutUs = 10'000U;
/// Highest `Config::max_retries`.
inline constexpr uint8_t kMaxRetries = 3U;
/// ICVID.MANUFACTURER of every TLE92466ED (5.3.2.19).
inline constexpr uint8_t kManufacturerId = 0xC1U;
/// ICVID.VERSION of design step A11.
inline constexpr uint8_t kVersionA11 = 0xFCU;
/// ICVID.VERSION of design step A13.
inline constexpr uint8_t kVersionA13 = 0xFEU;
/// @}

/**
 * @brief Global faults, each named by its register (bits 7..4: 0 GLOBAL_DIAG0, 1 GLOBAL_DIAG1, 2
 * GLOBAL_DIAG2) and bit (bits 3..0), so `Faults::Has()` needs no table (sections 5.3.2.5 to
 * 5.3.2.7).
 */
enum class GlobalFault : uint8_t {
  VbatUndervoltage = 0x00U, ///< GLOBAL_DIAG0.VBAT_UV.
  VbatOvervoltage = 0x01U,  ///< GLOBAL_DIAG0.VBAT_OV.
  VioUndervoltage = 0x02U,  ///< GLOBAL_DIAG0.VIO_UV.
  VioOvervoltage = 0x03U,   ///< GLOBAL_DIAG0.VIO_OV.
  VddUndervoltage = 0x04U,  ///< GLOBAL_DIAG0.VDD_UV.
  VddOvervoltage = 0x05U,   ///< GLOBAL_DIAG0.VDD_OV.
  ClockFault = 0x06U,       ///< GLOBAL_DIAG0.CLK_NOK.
  CentralOtError = 0x07U,   ///< GLOBAL_DIAG0.COTERR.
  CentralOtWarning = 0x08U, ///< GLOBAL_DIAG0.COTWARN.
  SpiWatchdog = 0x0EU,      ///< GLOBAL_DIAG0.SPI_WD_ERR.
  BiasCurrentLow = 0x10U,   ///< GLOBAL_DIAG1.VR_IREF_UV.
  BiasCurrentHigh = 0x11U,  ///< GLOBAL_DIAG1.VR_IREF_OV.
  Supply2v5Low = 0x12U,     ///< GLOBAL_DIAG1.VDD2V5_UV.
  Supply2v5High = 0x13U,    ///< GLOBAL_DIAG1.VDD2V5_OV.
  ReferenceLow = 0x14U,     ///< GLOBAL_DIAG1.REF_UV.
  ReferenceHigh = 0x15U,    ///< GLOBAL_DIAG1.REF_OV.
  PreRegulatorHigh = 0x16U, ///< GLOBAL_DIAG1.VPRE_OV.
  MonitorAdc = 0x1FU,       ///< GLOBAL_DIAG1.HVADC_ERR.
  RegisterEcc = 0x21U,      ///< GLOBAL_DIAG2.REG_ECC_ERR.
  OtpEcc = 0x23U,           ///< GLOBAL_DIAG2.OTP_ECC_ERR.
  OtpVirgin = 0x24U,        ///< GLOBAL_DIAG2.OTP_VIRGIN.
};

/** @brief The faults and warnings of one channel, decoded (sections 5.3.2.12, 5.3.2.13). */
struct ChannelFaults {
  bool olsg{false};                    ///< OLSG: open load or short to ground in ON-state (4.9.2).
  bool open_load{false};               ///< OL: open load, OFF-state diagnosis (4.9.4).
  bool overcurrent{false};             ///< OC (4.9.6, 4.11.2).
  bool short_to_ground{false};         ///< SG: OFF-state diagnosis (4.9.5).
  bool overtemperature{false};         ///< OTE: the channel was switched off (4.11.1).
  bool pwm_regulation{false};          ///< PWM_REG_WARN (4.6.2.4).
  bool current_regulation{false};      ///< I_REG_WARN (4.6.2.5).
  bool overtemperature_warning{false}; ///< OTW.
  bool olsg_warning{false};            ///< OLSG_WARN (4.9.3).
  bool olsg_check_skipped{false};      ///< OLSG_WARN_CHK_NOK: no voltage check was possible.

  /** @return true when an error (not a warning) is set. */
  [[nodiscard]] constexpr bool AnyError() const noexcept {
    return olsg || open_load || overcurrent || short_to_ground || overtemperature;
  }
  /// Field-by-field equality (KB-CPP20-033: integral members only).
  friend constexpr bool operator==(const ChannelFaults& lhs,
                                   const ChannelFaults& rhs) noexcept = default;
};

/**
 * @brief The diagnosis registers as read: GLOBAL_DIAG0..2, DIAG_ERR_CHGR0..2 and
 * DIAG_WARN_CHGR0..2. Raw words, so a latch is a plain OR; `Has()` and `Of()` decode them.
 */
struct Faults {
  std::array<uint16_t, 3> global{};   ///< GLOBAL_DIAG0, 1, 2.
  std::array<uint16_t, 3> errors{};   ///< DIAG_ERR_CHGR0, 1, 2.
  std::array<uint16_t, 3> warnings{}; ///< DIAG_WARN_CHGR0, 1, 2.

  /**
   * @brief Whether a global fault is set.
   * @param fault The fault.
   * @return The bit.
   */
  [[nodiscard]] constexpr bool Has(GlobalFault fault) const noexcept {
    const auto code = static_cast<uint32_t>(detail::ToUnderlying(fault));
    const uint32_t index = (code >> 4U) % 3U;
    return ((static_cast<uint32_t>(global[index]) >> (code & 0x0FU)) & 1U) != 0U;
  }

  /**
   * @brief The faults of one channel.
   * @param channel The channel.
   * @return The decoded flags.
   */
  [[nodiscard]] constexpr ChannelFaults Of(Channel channel) const noexcept {
    const auto ch = static_cast<uint32_t>(detail::ToUnderlying(channel)) % kChannelCount;
    const uint32_t shift = 8U * (ch % 2U);
    const uint32_t e = (static_cast<uint32_t>(errors[ch / 2U]) >> shift) & 0xFFU;
    const uint32_t w = (static_cast<uint32_t>(warnings[ch / 2U]) >> shift) & 0xFFU;
    ChannelFaults f{};
    f.olsg = (e & 0x01U) != 0U;
    f.open_load = (e & 0x02U) != 0U;
    f.overcurrent = (e & 0x04U) != 0U;
    f.short_to_ground = (e & 0x08U) != 0U;
    f.overtemperature = (e & 0x10U) != 0U;
    f.pwm_regulation = (w & 0x01U) != 0U;
    f.current_regulation = (w & 0x02U) != 0U;
    f.overtemperature_warning = (w & 0x04U) != 0U;
    f.olsg_warning = (w & 0x08U) != 0U;
    f.olsg_check_skipped = (w & 0x10U) != 0U;
    return f;
  }

  /**
   * @return true when any fault or warning is set. POR_EVENT and RES_EVENT (events, not faults)
   * and OLSG_WARN_CHK_NOK (a check that could not run) do not count.
   */
  [[nodiscard]] constexpr bool Any() const noexcept {
    uint32_t bits = (static_cast<uint32_t>(global[0]) & 0x41FFU) |
                    (static_cast<uint32_t>(global[1]) & 0x807FU) |
                    (static_cast<uint32_t>(global[2]) & 0x001AU);
    for (uint32_t g = 0U; g < 3U; ++g) {
      bits |= static_cast<uint32_t>(errors[g]) & 0x1F1FU;
      bits |= static_cast<uint32_t>(warnings[g]) & 0x0F0FU;
    }
    return bits != 0U;
  }

  /// Word-by-word equality (KB-CPP20-033: integral members only).
  friend constexpr bool operator==(const Faults& lhs, const Faults& rhs) noexcept = default;
};

/** @brief FB_STAT, the chip's one-word status summary, decoded (section 5.3.2.21). */
struct DeviceStatus {
  bool init_done{false};           ///< INIT_DONE: clock and logic ready.
  bool spi_watchdog_error{false};  ///< SPI_WD_ERR.
  bool clock_fault{false};         ///< CLK_NOK.
  bool central_ot_error{false};    ///< COTERR.
  bool central_ot_warning{false};  ///< COTWARN.
  bool reset_event{false};         ///< RES_event: RESN reset since the flag was cleared.
  bool power_on_event{false};      ///< POR_event: power-on reset since the flag was cleared.
  bool data_error{false};          ///< DATA_err: OTP ECC, OTP virgin or monitor ADC error.
  bool supply_external{false};     ///< SUP_NOK_EXT: VIO, VDD or VBAT out of range.
  bool supply_internal{false};     ///< SUP_NOK_INT: an internal supply out of range.
  uint8_t error_groups{0};         ///< ERR_CHGR0..2, bit g: a channel error in group g.
  uint8_t warning_groups{0};       ///< DIAG_WARN_CHGR0..2 summary, bit g.
  uint8_t check_skipped_groups{0}; ///< OLSG_WARN_CHK_NOK summary, bit g.
  uint32_t raw{0};                 ///< The 22-bit register.
};

/** @brief The supply and temperature feedback (FB_VOLTAGE1, FB_VOLTAGE2; 5.3.2.22, 5.3.2.23). */
struct SupplyReadings {
  uint32_t vio_mv{0};        ///< VIO = 3.4534 mV * VIO.
  uint32_t vdd_mv{0};        ///< VDD = 3.4534 mV * VDD.
  uint32_t vbat_mv{0};       ///< VBAT = 41.47 V * VBAT / (2^11 - 1).
  int32_t temperature_mc{0}; ///< Central temperature in milli-degrees Celsius (Equation 32),
                             ///< +/-20 degC (Table 16).
};

/** @brief PIN_STAT, decoded (section 5.3.2.20). */
struct PinStatus {
  bool drv0{false};        ///< DRV0 is high.
  bool drv1{false};        ///< DRV1 is high.
  bool enable{false};      ///< EN is high.
  bool fault{false};       ///< An unmasked fault (FAULTN bit 0).
  bool faultn_high{false}; ///< FAULTN_FB: the pin's level is high.
};

/** @brief The result of the safety flip-flop self-test (SFF_BIST, section 4.9.8). */
struct BistResult {
  bool passed{false};               ///< SMU_SLF_TST_FAIL = 0.
  bool uncorrectable_tested{false}; ///< SMU_SLF_TST_UERR.
  bool correctable_tested{false};   ///< SMU_SLF_TST_CERR.
};

/** @brief The unique chip identification, CHIPID0..2 (sections 5.3.2.24 to 5.3.2.26). */
struct ChipId {
  std::array<uint16_t, 3> words{}; ///< ID0, ID1, ID2.
  /// Word-by-word equality.
  friend constexpr bool operator==(const ChipId& lhs, const ChipId& rhs) noexcept = default;
};

/**
 * @brief One measurement period of a channel's feedback, read under the freeze of section 4.10.5
 * and converted with Equations 18 to 22, 30 and 31.
 */
struct ChannelFeedback {
  int32_t average_current_ua{0}; ///< I_avg = 4 A * I_AVG_MANT / TP_MANT (signed). In a pair, the
                                 ///< sum of both channels.
  uint32_t duty_permyriad{0};    ///< TO_MANT / TP_MANT, 0 to 10000.
  uint32_t vbat_mv{0};           ///< VBAT = 41.47 V * VBAT_AVG_MANT / TP_MANT.
  uint32_t period_us{0};         ///< T_meas = TP_MANT * 2^EXP / f_SYS.
  int32_t min_current_ua{0};     ///< I_min = 4 A * IMIN / (2^9 - 1).
  int32_t max_current_ua{0};     ///< I_max = 4 A * IMAX / (2^9 - 1).
  uint32_t min_pwm_period_ns{0}; ///< T_PWM_min = PMIN * 256 / f_SYS.
  uint32_t max_pwm_period_ns{0}; ///< T_PWM_max = PMAX * 256 / f_SYS.
  uint8_t period_count{0};       ///< DITHER_PERIOD_CNT (fast feedback only, 4.10.4).
  uint8_t quad_count{0};         ///< DITHER_QUAD_CNT (fast feedback only).
};

/** @brief Counters of the protocol layer (handbook KB-ARCH-044). No I/O to read them. */
struct BusDiagnostics {
  uint32_t frames{0};           ///< 32-bit frames sent.
  uint32_t retries{0};          ///< Reads repeated after a reply failed its checks.
  uint32_t rejected_replies{0}; ///< Replies that failed a check (CRC, width, echo, anchor).
  uint32_t resyncs{0};          ///< Anchor frames sent alone to restore the pipeline.
};

/// @name Conversions (constexpr, proved below with the datasheet's own numbers)
/// @{

/**
 * @brief f_SYS for a configuration: 28 MHz from the internal oscillator, else
 * f_CLK * PLL_FBDIV / (2 * PLL_REFDIV) (Equation 2).
 * @param config The configuration.
 * @return f_SYS in hertz; 0 for an external clock outside 1 to 8 MHz.
 */
[[nodiscard]] constexpr uint32_t SystemClockHz(const Config& config) noexcept;

/**
 * @brief PLL_REFDIV for an external clock: f_CLK / 1 MHz, rounded down as in Table 10.
 * @param khz f_CLK in kilohertz.
 * @return The divider.
 */
[[nodiscard]] constexpr uint32_t PllRefDiv(uint32_t khz) noexcept {
  return khz / 1000U;
}

/**
 * @brief PLL_FBDIV for an external clock: 56 MHz * PLL_REFDIV / f_CLK, rounded to the nearest
 * as in Table 10 (Equation 3).
 * @param khz f_CLK in kilohertz, at least 1000.
 * @return The divider; 0 for `khz` = 0.
 */
[[nodiscard]] constexpr uint32_t PllFbDiv(uint32_t khz) noexcept {
  return (khz == 0U) ? 0U : (((56'000U * PllRefDiv(khz)) + (khz / 2U)) / khz);
}

constexpr uint32_t SystemClockHz(const Config& config) noexcept {
  if (config.clock == ClockSource::Internal) {
    return kInternalClockHz;
  }
  const uint32_t khz = config.external_clock_khz;
  if ((khz < 1000U) || (khz > 8000U)) {
    return 0U;
  }
  return static_cast<uint32_t>((static_cast<uint64_t>(khz) * 1000U * PllFbDiv(khz)) /
                               (uint64_t{2} * PllRefDiv(khz)));
}

/**
 * @brief The TARGET count of a current, rounded to the nearest (section 5.3.3.3).
 * @param microamps The current.
 * @param parallel true for a parallel pair (4 A full scale).
 * @return TARGET; saturates at `kTargetMax`.
 */
[[nodiscard]] constexpr uint32_t TargetFromMicroamps(uint32_t microamps, bool parallel) noexcept {
  const uint64_t scale = parallel ? kParallelFullScaleUa : kSetpointFullScaleUa;
  const uint64_t target = ((static_cast<uint64_t>(microamps) * kTargetMax) + (scale / 2U)) / scale;
  return (target > kTargetMax) ? kTargetMax : static_cast<uint32_t>(target);
}

/**
 * @brief The current a TARGET count programs, rounded to the nearest microamp.
 * @param target TARGET, 0 to `kTargetMax`.
 * @param parallel true for a parallel pair.
 * @return The current in microamps.
 */
[[nodiscard]] constexpr uint32_t MicroampsFromTarget(uint32_t target, bool parallel) noexcept {
  const uint64_t scale = parallel ? kParallelFullScaleUa : kSetpointFullScaleUa;
  return static_cast<uint32_t>(
      (((static_cast<uint64_t>(target) & kTargetMax) * scale) + (kTargetMax / 2U)) / kTargetMax);
}

/**
 * @brief The dither amplitude I_Dither = STEPS * STEP_SIZE * full scale / (2^15 - 1) (Equation 6).
 * @param dither The dither settings.
 * @param parallel true for a parallel pair (the TARGET LSB doubles).
 * @return The amplitude in microamps.
 */
[[nodiscard]] constexpr uint32_t DitherAmplitudeUa(const DitherConfig& dither,
                                                   bool parallel) noexcept {
  const uint32_t counts = static_cast<uint32_t>(dither.steps) * dither.step_size;
  return MicroampsFromTarget((counts > kTargetMax) ? kTargetMax : counts, parallel);
}

/**
 * @brief The PWM frequency the controller is set to: f_SYS / (PERIOD_MANT * 2^PERIOD_EXP), times
 * 1/8 in the low range (Equations 4, 5).
 * @param icc The controller settings.
 * @param system_hz f_SYS.
 * @return The frequency in hertz, rounded down; 0 when PERIOD_MANT = 0 (controller off).
 */
[[nodiscard]] constexpr uint32_t PwmFrequencyHz(const IccConfig& icc, uint32_t system_hz) noexcept {
  if (icc.period_mant == 0U) {
    return 0U;
  }
  const uint32_t cycles = (static_cast<uint32_t>(icc.period_mant) << (icc.period_exp & 0x07U)) *
                          (icc.low_freq_range ? 8U : 1U);
  return system_hz / cycles;
}

/**
 * @brief WD_TIME for a watchdog timeout: rounddown(t_SPI_WD * f_SYS / 2^14) (5.3.2.11).
 * @param timeout_ms The timeout.
 * @param system_hz f_SYS, `kInternalClockHz` unless an external clock is used.
 * @return The code, or `InvalidArgument` when it would be 0 or above `kMaxWatchdogReload`.
 */
[[nodiscard]] constexpr Result<uint16_t> WatchdogReloadCode(uint32_t timeout_ms,
                                                            uint32_t system_hz) noexcept {
  const uint64_t code =
      (static_cast<uint64_t>(timeout_ms) * system_hz) / (uint64_t{1000} * kWatchdogDivider);
  if ((code == 0U) || (code > kMaxWatchdogReload)) {
    return Result<uint16_t>::Failure(Error::InvalidArgument);
  }
  return Result<uint16_t>::Success(static_cast<uint16_t>(code));
}

/**
 * @brief VBAT_UV_TH or VBAT_OV_TH for a threshold voltage, 162.08 mV per count, rounded to the
 * nearest (Equation 1).
 * @param millivolts The threshold.
 * @return The code, or `InvalidArgument` above 255 counts (41.3 V).
 */
[[nodiscard]] constexpr Result<uint8_t> VbatThresholdCode(uint32_t millivolts) noexcept {
  const uint64_t code = ((static_cast<uint64_t>(millivolts) * 100U) + 8104U) / 16208U;
  if (code > 0xFFU) {
    return Result<uint8_t>::Failure(Error::InvalidArgument);
  }
  return Result<uint8_t>::Success(static_cast<uint8_t>(code));
}
/// @}

// Table 10: every row's dividers and f_SYS within Table 6.
static_assert((PllRefDiv(1500U) == 1U) && (PllFbDiv(1500U) == 37U));
static_assert((PllRefDiv(2500U) == 2U) && (PllFbDiv(2500U) == 45U));
static_assert((PllRefDiv(4500U) == 4U) && (PllFbDiv(4500U) == 50U));
static_assert((PllRefDiv(6500U) == 6U) && (PllFbDiv(6500U) == 52U));
static_assert((PllRefDiv(7500U) == 7U) && (PllFbDiv(7500U) == 52U));
static_assert((PllRefDiv(8000U) == 8U) && (PllFbDiv(8000U) == 56U));
// The reset value of CLK_DIV (0x0438) is REFDIV 2, FBDIV 56: the 2 MHz row.
static_assert((PllRefDiv(2000U) == 2U) && (PllFbDiv(2000U) == 56U));
// TARGET: the 0x6000 saturation limit is 1.5 A (5.3.3.3); 1 mA is 16 counts.
static_assert(MicroampsFromTarget(0x6000U, false) == 1'500'046U);
static_assert(TargetFromMicroamps(1'000U, false) == 16U);
static_assert(TargetFromMicroamps(kSetpointFullScaleUa, false) == kTargetMax);
static_assert(TargetFromMicroamps(2'000U, true) == 16U);
// OL_TH_FIXED full scale 63 * 128 counts is 492 mA (Table 15).
static_assert((MicroampsFromTarget(63U * 128U, false) / 1000U) == 492U);
// The SPI watchdog: 0.585 ms per count at 28 MHz, so 100 ms is 170 counts and 1.2 s is too long.
static_assert(WatchdogReloadCode(100U, kInternalClockHz).value == 170U);
static_assert(!WatchdogReloadCode(1200U, kInternalClockHz).ok());
// VBAT_TH reset codes: 25 is 4.05 V, 255 is 41.33 V.
static_assert(VbatThresholdCode(4052U).value == 25U);
static_assert(VbatThresholdCode(41330U).value == 255U);

namespace detail {
/// Last enumerator of `Verify` (detail::IsEnumerator).
template <>
struct EnumLast<Verify> {
  static constexpr Verify kValue = Verify::ReadBack; ///< The last enumerator.
};
/// Last enumerator of `Channel`.
template <>
struct EnumLast<Channel> {
  static constexpr Channel kValue = Channel::Ch5; ///< The last enumerator.
};
/// Last enumerator of `ParallelPair`.
template <>
struct EnumLast<ParallelPair> {
  static constexpr ParallelPair kValue = ParallelPair::Ch4Ch5; ///< The last enumerator.
};
/// Last enumerator of `ChannelMode`.
template <>
struct EnumLast<ChannelMode> {
  static constexpr ChannelMode kValue = ChannelMode::Measurement; ///< The last enumerator.
};
/// Last enumerator of `SlewRate`.
template <>
struct EnumLast<SlewRate> {
  static constexpr SlewRate kValue = SlewRate::VPerUs10; ///< The last enumerator.
};
/// Last enumerator of `DiagCurrent`.
template <>
struct EnumLast<DiagCurrent> {
  static constexpr DiagCurrent kValue = DiagCurrent::Ua1250; ///< The last enumerator.
};
/// Last enumerator of `OffStateDiag`.
template <>
struct EnumLast<OffStateDiag> {
  static constexpr OffStateDiag kValue = OffStateDiag::Disabled; ///< The last enumerator.
};
/// Last enumerator of `FastFeedback`.
template <>
struct EnumLast<FastFeedback> {
  static constexpr FastFeedback kValue = FastFeedback::QuarterPeriod; ///< The last enumerator.
};
/// Last enumerator of `VioLevel`.
template <>
struct EnumLast<VioLevel> {
  static constexpr VioLevel kValue = VioLevel::V5V0; ///< The last enumerator.
};
/// Last enumerator of `ClockSource`.
template <>
struct EnumLast<ClockSource> {
  static constexpr ClockSource kValue = ClockSource::External; ///< The last enumerator.
};

/// The master of a pair: CH0, CH1, CH4 (section 4.5.2).
[[nodiscard]] constexpr Channel PairMaster(ParallelPair pair) noexcept {
  constexpr std::array<Channel, kPairCount> kMasters = {Channel::Ch0, Channel::Ch1, Channel::Ch4};
  return kMasters[static_cast<uint32_t>(ToUnderlying(pair)) % kPairCount];
}
/// The slave of a pair: CH3, CH2, CH5 (section 4.5.2).
[[nodiscard]] constexpr Channel PairSlave(ParallelPair pair) noexcept {
  constexpr std::array<Channel, kPairCount> kSlaves = {Channel::Ch3, Channel::Ch2, Channel::Ch5};
  return kSlaves[static_cast<uint32_t>(ToUnderlying(pair)) % kPairCount];
}
/// The pair a channel belongs to.
[[nodiscard]] constexpr ParallelPair PairOf(Channel channel) noexcept {
  constexpr std::array<ParallelPair, kChannelCount> kPairs = {
      ParallelPair::Ch0Ch3, ParallelPair::Ch1Ch2, ParallelPair::Ch1Ch2,
      ParallelPair::Ch0Ch3, ParallelPair::Ch4Ch5, ParallelPair::Ch4Ch5};
  return kPairs[static_cast<uint32_t>(ToUnderlying(channel)) % kChannelCount];
}
/// Whether `channel` is the slave of a pair `config` enables.
[[nodiscard]] constexpr bool IsActiveSlave(const Config& config, Channel channel) noexcept {
  const ParallelPair pair = PairOf(channel);
  return config.parallel[ToUnderlying(pair)] && (PairSlave(pair) == channel);
}
/// Whether `channel` is the master of a pair `config` enables.
[[nodiscard]] constexpr bool IsActiveMaster(const Config& config, Channel channel) noexcept {
  const ParallelPair pair = PairOf(channel);
  return config.parallel[ToUnderlying(pair)] && (PairMaster(pair) == channel);
}

/// Every enumeration of a channel holds an enumerator.
[[nodiscard]] constexpr bool ChannelEnumsValid(const ChannelConfig& c) noexcept {
  return IsEnumerator(c.mode) && IsEnumerator(c.slew) && IsEnumerator(c.diag.current) &&
         IsEnumerator(c.diag.off_state) && IsEnumerator(c.dither.fast);
}

/// The controller fields fit their registers and keep Autolimit above the floor (4.6.2.3), and the
/// PWM target is inside f_PWM (Table 6).
[[nodiscard]] constexpr bool IccValid(const IccConfig& icc, uint32_t system_hz) noexcept {
  if ((icc.period_exp > 7U) || (icc.ki > 15U) || (icc.limit > 1023U) || (icc.auto_limit > 31U) ||
      (icc.int_thresh > 511U)) {
    return false;
  }
  if (static_cast<int32_t>(icc.auto_limit) <= (static_cast<int32_t>(icc.min_int_thresh) + 3)) {
    return false;
  }
  const uint32_t hz = PwmFrequencyHz(icc, system_hz);
  return (icc.period_mant == 0U) || ((hz >= kMinPwmHz) && (hz <= kMaxPwmHz));
}

/// The dither fields fit their registers; fast feedback needs FLAT above 2 (note in 4.10.3).
[[nodiscard]] constexpr bool DitherValid(const DitherConfig& dither) noexcept {
  if ((dither.mant > 1023U) || (dither.exp > 15U) || (dither.step_size > 4095U)) {
    return false;
  }
  return (dither.fast == FastFeedback::DitherPeriod) || (dither.flat > 2U);
}

/// The diagnosis fields fit their registers; the relative open-load threshold exists in current
/// control only (note in 4.9.2.1).
[[nodiscard]] constexpr bool DiagValid(const DiagConfig& diag, ChannelMode mode) noexcept {
  if ((diag.ol_relative_eighths > 7U) || (diag.ol_fixed > 63U) || (diag.olsg_warn_window > 31U) ||
      (diag.olsg_timeout > 63U)) {
    return false;
  }
  return (diag.ol_relative_eighths == 0U) || (mode == ChannelMode::CurrentControl);
}

/// One channel's configuration is valid.
[[nodiscard]] constexpr bool ChannelValid(const ChannelConfig& c, uint32_t system_hz) noexcept {
  return ChannelEnumsValid(c) && (c.on_time_mant <= 1023U) && IccValid(c.icc, system_hz) &&
         DitherValid(c.dither) && DiagValid(c.diag, c.mode);
}

/// The global fields are valid: enumerations, watchdog, VBAT thresholds, clock and driver timing.
[[nodiscard]] constexpr bool GlobalValid(const Config& config) noexcept {
  if (!IsEnumerator(config.vio) || !IsEnumerator(config.clock)) {
    return false;
  }
  const uint16_t min_reload = config.spi_watchdog ? kMinWatchdogReload : uint16_t{1};
  if ((config.watchdog_reload < min_reload) || (config.watchdog_reload > kMaxWatchdogReload) ||
      (config.vbat_uv_code >= config.vbat_ov_code) || (config.max_retries > kMaxRetries) ||
      (config.init_timeout_us < kMinInitTimeoutUs) ||
      (config.init_timeout_us > kMaxInitTimeoutUs)) {
    return false;
  }
  if ((config.clock == ClockSource::External) && ((config.external_clock_khz % 500U) != 0U)) {
    return false;
  }
  const uint32_t hz = SystemClockHz(config);
  return (hz >= kMinSystemClockHz) && (hz <= kMaxSystemClockHz) &&
         ((static_cast<uint32_t>(config.fault_pin.channel_errors) & ~0x3FU) == 0U) &&
         ((static_cast<uint32_t>(config.fault_pin.channel_warnings) & ~0x3FU) == 0U);
}
} // namespace detail

/**
 * @brief Check a configuration without touching the chip.
 *
 * Fails when an enumeration holds a value that is not an enumerator, a field does not fit its
 * register, WD_TIME is 0 or above 2047, VBAT_UV_TH is not below VBAT_OV_TH, f_SYS of an external
 * clock is outside 27.5 to 28.5 MHz (Table 6) or not a 500 kHz step of 1 to 8 MHz (Table 10), a
 * PWM target is outside 110 to 4000 Hz (Table 6), AUTO_LIM_VALUE_ABS is not above
 * MIN_INT_THRESH + 3 (4.6.2.3), fast feedback has FLAT of 2 or less (4.10.3), a relative open-load
 * threshold is set outside current control (4.9.2.1), the slave of an enabled pair is declared
 * loaded, or a driver setting is outside its range.
 *
 * @param config The configuration.
 * @return `Error::Ok` or `Error::InvalidArgument`.
 */
constexpr Error Validate(const Config& config) noexcept {
  if (!detail::GlobalValid(config)) {
    return Error::InvalidArgument;
  }
  const uint32_t hz = SystemClockHz(config);
  for (uint32_t ch = 0U; ch < kChannelCount; ++ch) {
    const auto channel = static_cast<Channel>(ch);
    if (!detail::ChannelValid(config.channels[ch], hz) ||
        (detail::IsActiveSlave(config, channel) && config.channels[ch].loaded)) {
      return Error::InvalidArgument;
    }
  }
  return Error::Ok;
}

/**
 * @brief A starting point for one current-controlled channel with feedback: ICC, a 1 kHz PWM target
 * (PERIOD_MANT 219, PERIOD_EXP 7), MIN_INT_THRESH 2 (above 1, section 4.6.2.4), a 1 ms dither
 * reference clock with no dither amplitude so the averaged feedback updates every millisecond
 * (sections 4.4.2, 4.7.1), and the reset values elsewhere. Tune it to the load.
 * @return The channel configuration, declared loaded.
 */
constexpr ChannelConfig CurrentControlChannel() noexcept {
  ChannelConfig c{};
  c.loaded = true;
  c.mode = ChannelMode::CurrentControl;
  c.icc.min_int_thresh = 2;
  c.icc.period_mant = 219U;
  c.icc.period_exp = 7U;
  c.dither.mant = 875U; // 875 * 2^5 / 28 MHz = 1 ms
  c.dither.exp = 5U;
  return c;
}

// Every preset is valid, proved at build time (KB-CPP20-012).
static_assert(Validate(Config{}) == Error::Ok);
static_assert(PwmFrequencyHz(CurrentControlChannel().icc, kInternalClockHz) == 998U);
namespace detail {
/// `Config{}` with CH0 set to `CurrentControlChannel()`, for the proof below.
[[nodiscard]] constexpr Config PresetProof() noexcept {
  Config c{};
  c.channels[0] = CurrentControlChannel();
  return c;
}
} // namespace detail
static_assert(Validate(detail::PresetProof()) == Error::Ok);

/**
 * @brief Reached only when a `CheckedConfig` is built from an invalid configuration: it is never
 * defined, so the build fails and names it (KB-CPP20-013).
 */
void ConfigIsInvalid() noexcept;

/**
 * @brief A configuration proven valid at build time.
 *
 * `constexpr CheckedConfig kBoard{config};` fails to compile, naming `ConfigIsInvalid`, when
 * `Validate()` rejects the configuration. Pass `kBoard.Get()` to the driver; the driver still
 * validates at start-up, for configurations built at run time.
 */
class CheckedConfig {
public:
  /**
   * @brief Check `config` at build time.
   * @param config The configuration; it must pass `Validate()`.
   */
  consteval explicit CheckedConfig(const Config& config) noexcept : config_(config) {
    if (Validate(config) != Error::Ok) {
      ConfigIsInvalid();
    }
  }
  /** @return The checked configuration. */
  [[nodiscard]] constexpr const Config& Get() const& noexcept {
    return config_;
  }
  /** @brief Not on a temporary: the reference would dangle (KB-CPP20-044). */
  void Get() const&& = delete;

private:
  Config config_;
};

/** @return The driver version, e.g. `"0.1.0"`, from the generated `tle92466ed_version.h`. No I/O.
 */
constexpr const char* GetDriverVersion() noexcept {
  return TLE92466ED_VERSION_STRING;
}
/** @return The major version. No I/O. */
constexpr uint32_t GetDriverVersionMajor() noexcept {
  return TLE92466ED_VERSION_MAJOR;
}
/** @return The minor version. No I/O. */
constexpr uint32_t GetDriverVersionMinor() noexcept {
  return TLE92466ED_VERSION_MINOR;
}
/** @return The patch version. No I/O. */
constexpr uint32_t GetDriverVersionPatch() noexcept {
  return TLE92466ED_VERSION_PATCH;
}

/** @} */

} // namespace tle92466ed

#endif // TLE92466ED_TYPES_HPP
