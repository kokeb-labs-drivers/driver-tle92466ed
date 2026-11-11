/**
 * @file tle92466ed.hpp
 * @brief TLE92466ED driver: the one header to include.
 * @copyright Copyright (c) 2026 The driver-tle92466ed authors. SPDX-License-Identifier: MIT
 *
 * @details
 * `tle92466ed::TLE92466ED<Port>` drives one Infineon TLE92466ED six-channel low-side solenoid
 * driver through a platform port you write (see `tle92466ed_spi_interface.hpp`). The driver has
 * no platform code, allocates nothing, throws nothing, takes no lock and is not thread-safe: give
 * each object one owner, or guard it with one mutex. Section numbers refer to the TLE92466ED
 * datasheet, Rev. 1.2.
 */
#ifndef TLE92466ED_HPP
#define TLE92466ED_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "tle92466ed_detail.hpp"
#include "tle92466ed_registers.hpp"
#include "tle92466ed_spi_interface.hpp"
#include "tle92466ed_types.hpp"
#include "tle92466ed_version.h"

namespace tle92466ed {

namespace detail {

// Optional hooks: a `Declares*` concept only tests the name, the `Has*` concept the exact
// signature, `noexcept` and the result type; the driver rejects a hook that is declared but does
// not match (KB-CPP20-007).

/// The port declares something called `GpioSet` (used to reject a misdeclared hook).
template <typename Port>
concept DeclaresGpioSet =
    requires(Port& port) { port.GpioSet(CtrlPin::Reset, GpioSignal::ACTIVE); };

/// The port drives control pins: `bool GpioSet(CtrlPin, GpioSignal) noexcept`.
template <typename Port>
concept HasGpioSet =
    requires { ExactHook<Port, bool(CtrlPin, GpioSignal) noexcept>::Take(&Port::GpioSet); };

/// The port declares something called `GpioRead`.
template <typename Port>
concept DeclaresGpioRead =
    requires(Port& port, GpioSignal& signal) { port.GpioRead(CtrlPin::Fault, signal); };

/// The port reads pins: `bool GpioRead(CtrlPin, GpioSignal&) noexcept`.
template <typename Port>
concept HasGpioRead =
    requires { ExactHook<Port, bool(CtrlPin, GpioSignal&) noexcept>::Take(&Port::GpioRead); };

/// The port declares something called `DelayUs`.
template <typename Port>
concept DeclaresDelayUs = requires(Port& port) { port.DelayUs(1_u32); };

/// The port waits: `void DelayUs(uint32_t us) noexcept`. A narrower parameter would truncate long
/// waits.
template <typename Port>
concept HasDelayUs = requires { ExactHook<Port, void(uint32_t) noexcept>::Take(&Port::DelayUs); };

/// The port declares something called `Log`.
template <typename Port>
concept DeclaresLog = requires(Port& port) { port.Log(LogLevel::Info, ""); };

/// The port takes log lines: `void Log(LogLevel, const char* message) noexcept`.
template <typename Port>
concept HasLog =
    requires { ExactHook<Port, void(LogLevel, const char*) noexcept>::Take(&Port::Log); };

/// Wait through the port. A port without `DelayUs()` fails to compile at the first call that needs
/// a wait: the chip's reset and start-up times are requirements, never skipped.
template <typename Port>
void DelayUs(Port& port, uint32_t us) noexcept {
  if constexpr (HasDelayUs<Port>) {
    port.DelayUs(us);
  } else {
    static_assert(kAlwaysFalse<Port>,
                  "tle92466ed: the port must implement `void DelayUs(uint32_t us) noexcept`");
  }
}

/// Log through the port, or nothing when the port has no `Log()`. The driver never formats text.
template <typename Port>
void Log(Port& port, [[maybe_unused]] LogLevel level,
         [[maybe_unused]] const char* message) noexcept {
  if constexpr (HasLog<Port>) {
    port.Log(level, message);
  } else {
    (void)port;
  }
}

// Register encoders: free constexpr functions, emitted once whatever the port type, and proved
// below with static_assert (KB-CPP20-009, -012).

/// One bit per channel.
[[nodiscard]] constexpr uint16_t ChannelBit(Channel channel) noexcept {
  return static_cast<uint16_t>(
      1U << (static_cast<uint32_t>(ToUnderlying(channel)) % kChannelCount));
}

/// The channels `config` declares loaded, one bit each.
[[nodiscard]] constexpr uint32_t LoadedMask(const Config& config) noexcept {
  uint32_t mask = 0U;
  for (uint32_t ch = 0U; ch < kChannelCount; ++ch) {
    mask |= config.channels[ch].loaded ? (1U << ch) : 0U;
  }
  return mask;
}

/// The CH_PAR bits of CH_CTRL (section 5.3.2.3).
[[nodiscard]] constexpr uint16_t PairBits(const Config& config) noexcept {
  uint32_t bits = config.parallel[0] ? reg::kChCtrlPar03 : 0U;
  bits |= config.parallel[1] ? reg::kChCtrlPar12 : 0U;
  bits |= config.parallel[2] ? reg::kChCtrlPar45 : 0U;
  return static_cast<uint16_t>(bits);
}

/// GLOBAL_CONFIG (section 5.3.2.4); the test bits stay 0.
[[nodiscard]] constexpr uint16_t GlobalConfigValue(const Config& config) noexcept {
  uint32_t v = config.clock_watchdog ? reg::kGlobalConfigClkWdEn : 0U;
  v |= config.spi_watchdog ? reg::kGlobalConfigSpiWdEn : 0U;
  v |= config.crc_check ? reg::kGlobalConfigCrcEn : 0U;
  v |= (config.vio == VioLevel::V5V0) ? reg::kGlobalConfigVioSel : 0U;
  return static_cast<uint16_t>(v);
}

/// CLK_DIV (section 5.3.2.17): the reset dividers for the internal clock, Table 10's for a CLK pin.
[[nodiscard]] constexpr uint16_t ClkDivValue(const Config& config) noexcept {
  if (config.clock == ClockSource::Internal) {
    return reg::kClkDivReset;
  }
  const uint32_t khz = config.external_clock_khz;
  auto v = reg::PllFbDiv::Insert(reg::kClkDivExtClk, PllFbDiv(khz));
  return reg::PllRefDiv::Insert(v, PllRefDiv(khz));
}

/// VBAT_TH (section 5.3.2.8).
[[nodiscard]] constexpr uint16_t VbatThValue(const Config& config) noexcept {
  const auto v = reg::VbatUvTh::Insert(0_u16, config.vbat_uv_code);
  return reg::VbatOvTh::Insert(v, config.vbat_ov_code);
}

/// FAULT_MASK0 (5.3.2.14): channel errors of loaded channels only (KB-CPP-131).
[[nodiscard]] constexpr uint16_t FaultMask0Value(const Config& config) noexcept {
  const FaultPinMask& m = config.fault_pin;
  uint32_t v = static_cast<uint32_t>(m.channel_errors) & LoadedMask(config);
  v |= m.en_pin ? reg::kMask0EnPin : 0U;
  v |= m.supply_internal ? reg::kMask0SupInt : 0U;
  v |= m.supply_external ? reg::kMask0SupExt : 0U;
  return static_cast<uint16_t>(v);
}

/// FAULT_MASK1 (5.3.2.15): channel warnings of loaded channels only.
[[nodiscard]] constexpr uint16_t FaultMask1Value(const Config& config) noexcept {
  const FaultPinMask& m = config.fault_pin;
  uint32_t v = static_cast<uint32_t>(m.channel_warnings) & LoadedMask(config);
  v |= m.central_ot_warning ? reg::kMask1CotWarn : 0U;
  v |= m.central_ot_error ? reg::kMask1CotErr : 0U;
  v |= m.clock_too_slow ? reg::kMask1ClkLow : 0U;
  return static_cast<uint16_t>(v);
}

/// FAULT_MASK2 (5.3.2.16).
[[nodiscard]] constexpr uint16_t FaultMask2Value(const Config& config) noexcept {
  uint32_t v = config.fault_pin.data_error ? reg::kMask2DataErr : 0U;
  v |= config.fault_pin.spi_watchdog ? reg::kMask2SpiWd : 0U;
  return static_cast<uint16_t>(v);
}

/// CH_MODE for a mode (5.3.3.11): Measurement is CH.
[[nodiscard]] constexpr uint16_t ModeValue(ChannelMode mode) noexcept {
  const auto code = static_cast<uint32_t>(ToUnderlying(mode));
  return static_cast<uint16_t>((mode == ChannelMode::Measurement) ? reg::kChModeMeasurement
                                                                  : (code & 0x7U));
}

/// CH_CONFIG (5.3.3.10). An unloaded channel gets no load-dependent diagnosis: OFF-state diagnosis
/// off and both open-load thresholds 0 (KB-CPP-131). OC_DIAG_EN is written 0.
[[nodiscard]] constexpr uint16_t ChConfigValue(const ChannelConfig& c) noexcept {
  auto v = reg::SlewRate::Insert(0_u16, ToUnderlying(c.slew));
  v = reg::IDiag::Insert(v, ToUnderlying(c.diag.current));
  if (c.loaded) {
    v = reg::OlTh::Insert(v, c.diag.ol_relative_eighths);
    v = reg::OlThFixed::Insert(v, c.diag.ol_fixed);
    return reg::OffDiagCh::Insert(v, ToUnderlying(c.diag.off_state));
  }
  return reg::OffDiagCh::Insert(v, reg::kOffDiagDisabled);
}

/// CTRL (5.3.3.4).
[[nodiscard]] constexpr uint16_t CtrlValue(const ChannelConfig& c) noexcept {
  auto v = reg::MinIntThresh::Insert(
      0_u16, static_cast<uint32_t>(static_cast<uint8_t>(c.icc.min_int_thresh)));
  v = static_cast<uint16_t>(static_cast<uint32_t>(v) |
                            (c.icc.skip_falling_dither ? reg::kPwmPeriodCalcMode : 0U));
  v = reg::OlsgWarnWindow::Insert(v, c.diag.olsg_warn_window);
  return static_cast<uint16_t>(static_cast<uint32_t>(v) |
                               (c.diag.olsg_warning ? reg::kOlsgWarnEn : 0U));
}

/// PERIOD (5.3.3.5).
[[nodiscard]] constexpr uint16_t PeriodValue(const IccConfig& icc) noexcept {
  auto v = reg::PeriodMant::Insert(0_u16, icc.period_mant);
  v = reg::PeriodExp::Insert(v, icc.period_exp);
  v = static_cast<uint16_t>(static_cast<uint32_t>(v) |
                            (icc.low_freq_range ? reg::kLowFreqRangeEn : 0U));
  return reg::PwmCtrlParam::Insert(v, icc.ki);
}

/// INTEGRATOR_LIMIT (5.3.3.6).
[[nodiscard]] constexpr uint16_t IntegratorLimitValue(const IccConfig& icc) noexcept {
  return reg::AutoLimValueAbs::Insert(reg::LimValueAbs::Insert(0_u16, icc.limit), icc.auto_limit);
}

/// DITHER_CLK_DIV (5.3.3.7).
[[nodiscard]] constexpr uint16_t DitherClkDivValue(const DitherConfig& d) noexcept {
  auto v = reg::DitherExp::Insert(reg::DitherMant::Insert(0_u16, d.mant), d.exp);
  uint32_t bits = static_cast<uint32_t>(v) | (d.pwm_sync ? reg::kDitherPwmSyncEn : 0U);
  bits |= d.setpoint_sync ? reg::kDitherSetpointSyncEn : 0U;
  return static_cast<uint16_t>(bits);
}

/// DITHER_STEP (5.3.3.8).
[[nodiscard]] constexpr uint16_t DitherStepValue(const DitherConfig& d) noexcept {
  return reg::DitherSteps::Insert(reg::DitherFlat::Insert(0_u16, d.flat), d.steps);
}

/// DITHER_CTRL (5.3.3.9).
[[nodiscard]] constexpr uint16_t DitherCtrlValue(const DitherConfig& d) noexcept {
  auto v = reg::DitherStepSize::Insert(0_u16, d.step_size);
  v = static_cast<uint16_t>(static_cast<uint32_t>(v) | (d.deep ? reg::kDeepDither : 0U));
  return reg::FastMeas::Insert(v, ToUnderlying(d.fast));
}

/// TON (5.3.3.12).
[[nodiscard]] constexpr uint16_t TonValue(const ChannelConfig& c) noexcept {
  return reg::OlsgTimeout::Insert(reg::TonMant::Insert(0_u16, c.on_time_mant), c.diag.olsg_timeout);
}

/// SETPOINT with `target` (5.3.3.3).
[[nodiscard]] constexpr uint16_t SetpointValue(const IccConfig& icc, uint32_t target) noexcept {
  const auto v = reg::Target::Insert(0_u16, target);
  return static_cast<uint16_t>(static_cast<uint32_t>(v) |
                               (icc.auto_limit_disable ? reg::kAutoLimitDis : 0U));
}

/// The channel registers the driver writes, in the order it writes them (Table 24); SETPOINT last.
inline constexpr std::array<uint16_t, 11> kChannelOffsets = {
    reg::kMode,         reg::kChConfig,   reg::kCtrl,       reg::kPeriod, reg::kIntegratorLimit,
    reg::kDitherClkDiv, reg::kDitherStep, reg::kDitherCtrl, reg::kTon,    reg::kCtrlIntThresh,
    reg::kSetpoint};

/// The bits of each register in `kChannelOffsets` that a read-back compares: CH_CONFIG leaves out
/// its rwh fields OC_DIAG_EN and OFF_DIAG_CH, which the chip changes itself.
inline constexpr std::array<uint16_t, 11> kChannelCompare = {0x000FU, 0x1FFFU, 0x7FFFU, 0xFFFFU,
                                                             0x7FFFU, 0xFFFFU, 0xFFFFU, 0xEFFFU,
                                                             0xFFFFU, 0x01FFU, 0xFFFFU};

/// The words of `kChannelOffsets` for a channel, SETPOINT with TARGET 0.
[[nodiscard]] constexpr std::array<uint16_t, 11> ChannelWords(const ChannelConfig& c) noexcept {
  return {ModeValue(c.mode),
          ChConfigValue(c),
          CtrlValue(c),
          PeriodValue(c.icc),
          IntegratorLimitValue(c.icc),
          DitherClkDivValue(c.dither),
          DitherStepValue(c.dither),
          DitherCtrlValue(c.dither),
          TonValue(c),
          reg::IntThresh::Insert(0_u16, c.icc.int_thresh),
          SetpointValue(c.icc, 0U)};
}

/// The CH_PAR bits of CH_CTRL.
inline constexpr uint32_t kPairMask = 0x7000U;

/// One register write of a fixed sequence.
struct RegWrite {
  uint16_t address{0}; ///< The register.
  uint16_t value{0};   ///< The word written.
};

/// The flags of a DIAG_ERR_CHGR word the driver clears: OLSG, OL, OC, SG, OTE of both channels.
inline constexpr uint16_t kErrClear = 0x1F1FU;
/// The OC flags of both channels: they need a second clear (4.9.6, 4.11.2).
inline constexpr uint16_t kOcBoth = 0x0404U;
/// The flags of a DIAG_WARN_CHGR word the driver clears; OLSG_WARN_CHK_NOK clears on read (4.9.3).
inline constexpr uint16_t kWarnClear = 0x0F0FU;
/// The GLOBAL_DIAG2 flags the driver clears: REG_ECC_ERR and OTP_ECC_ERR, never OTP_VIRGIN (4.9.7).
inline constexpr uint16_t kDiag2Clear = 0x000AU;
/// Every bit of GLOBAL_DIAG0 a clear handles: the rwh flags and the two rw events.
inline constexpr uint32_t kDiag0Any = 0x47FFU;
static_assert(kOcBoth == (static_cast<uint32_t>(reg::kErrOc) * 0x0101U));
static_assert(kDiag2Clear == (static_cast<uint32_t>(reg::kDiag2RegEccErr) | reg::kDiag2OtpEccErr));
static_assert(kDiag0Any == (static_cast<uint32_t>(reg::kDiag0Flags) | reg::kDiag0PorEvent |
                            reg::kDiag0ResEvent));

/// The feedback registers one read takes, as offsets from the channel base (Table 24).
inline constexpr std::array<uint16_t, 5> kFeedbackOffsets = {
    reg::kFbDc, reg::kFbVbat, reg::kFbIAvg, reg::kFbIminImax, reg::kFbPeriodMinMax};

/// FB_STAT, decoded (section 5.3.2.21).
[[nodiscard]] constexpr DeviceStatus DecodeStatus(uint32_t raw) noexcept {
  DeviceStatus s{};
  s.init_done = (raw & reg::kStatInitDone) != 0U;
  s.spi_watchdog_error = (raw & reg::kStatSpiWdErr) != 0U;
  s.clock_fault = (raw & reg::kStatClkNok) != 0U;
  s.central_ot_error = (raw & reg::kStatCotErr) != 0U;
  s.central_ot_warning = (raw & reg::kStatCotWarn) != 0U;
  s.reset_event = (raw & reg::kStatResEvent) != 0U;
  s.power_on_event = (raw & reg::kStatPorEvent) != 0U;
  s.data_error = (raw & reg::kStatDataErr) != 0U;
  s.supply_external = (raw & reg::kStatSupNokExt) != 0U;
  s.supply_internal = (raw & reg::kStatSupNokInt) != 0U;
  s.error_groups = static_cast<uint8_t>((raw >> 14U) & 0x7U);
  s.warning_groups = static_cast<uint8_t>(raw & 0x7U);
  s.check_skipped_groups = static_cast<uint8_t>((raw >> 3U) & 0x7U);
  s.raw = raw & reg::kData22Mask;
  return s;
}

/// PIN_STAT, decoded (section 5.3.2.20). FAULTN bit 0 means a fault.
[[nodiscard]] constexpr PinStatus DecodePins(uint32_t raw) noexcept {
  PinStatus p{};
  p.drv0 = (raw & reg::kPinDrv0) != 0U;
  p.drv1 = (raw & reg::kPinDrv1) != 0U;
  p.enable = (raw & reg::kPinEn) != 0U;
  p.fault = (raw & reg::kPinFaultn) == 0U;
  p.faultn_high = (raw & reg::kPinFaultnFb) != 0U;
  return p;
}

/// FB_VOLTAGE1 and FB_VOLTAGE2 in millivolts and milli-degrees (5.3.2.22, 5.3.2.23, Eq. 32):
/// T = (TEMP * 0.000593 - 0.819) / -0.0016 = 511.875 - 0.370625 * TEMP degrees Celsius.
[[nodiscard]] constexpr SupplyReadings DecodeSupply(uint32_t v1, uint32_t v2) noexcept {
  SupplyReadings s{};
  s.vio_mv = (reg::VioRaw::Extract(v1) * 34534U) / 10000U;
  s.vdd_mv = (reg::VddRaw::Extract(v1) * 34534U) / 10000U;
  s.vbat_mv = (reg::VbatRaw::Extract(v2) * 41470U) / 2047U;
  const auto temp = static_cast<int64_t>(reg::TempRaw::Extract(v2));
  s.temperature_mc = static_cast<int32_t>((511'875'000 - (370'625 * temp)) / 1000);
  return s;
}

/// One feedback set (FB_DC, FB_VBAT, FB_I_AVG, FB_IMIN_IMAX, FB_PERIOD_MIN_MAX) converted with
/// Equations 18 to 22, 30 and 31. TP_MANT = 0 is no measurement period: `NotReady`, never a value
/// (handbook KB-CPP-127).
[[nodiscard]] constexpr Result<ChannelFeedback> DecodeFeedback(const std::array<uint32_t, 5>& w,
                                                               uint32_t system_hz) noexcept {
  const uint32_t tp = reg::TpMant::Extract(w[0]);
  if ((tp == 0U) || (system_hz == 0U)) {
    return Result<ChannelFeedback>::Failure(Error::NotReady);
  }
  const uint32_t to = reg::ToMant::Extract(w[0]);
  ChannelFeedback f{};
  f.average_current_ua = static_cast<int32_t>(
      (4'000'000 * static_cast<int64_t>(SignExtend<12U>(reg::IAvgMant::Extract(w[2])))) /
      static_cast<int64_t>(tp));
  const uint32_t duty = (to * 10000U) / tp;
  f.duty_permyriad =
      (duty < 10000U) ? duty : 10000U; // TO_MANT never exceeds TP_MANT on a real chip
  f.vbat_mv = (reg::VbatAvgMant::Extract(w[1]) * 41470U) / tp;
  f.period_us = static_cast<uint32_t>(
      ((static_cast<uint64_t>(tp) << reg::FbExp::Extract(w[1])) * 1'000'000U) / system_hz);
  f.min_current_ua = (4'000'000 * SignExtend<10U>(reg::IMin::Extract(w[3]))) / 511;
  f.max_current_ua = (4'000'000 * SignExtend<10U>(reg::IMax::Extract(w[3]))) / 511;
  f.min_pwm_period_ns = static_cast<uint32_t>(
      (static_cast<uint64_t>(reg::PMin::Extract(w[4])) * 256'000'000'000U) / system_hz);
  f.max_pwm_period_ns = static_cast<uint32_t>(
      (static_cast<uint64_t>(reg::PMax::Extract(w[4])) * 256'000'000'000U) / system_hz);
  f.period_count = static_cast<uint8_t>(reg::DitherPeriodCnt::Extract(w[1]));
  f.quad_count = static_cast<uint8_t>(reg::DitherQuadCnt::Extract(w[1]));
  return Result<ChannelFeedback>::Success(f);
}

// Decoders on worked values: 25 degC is TEMP 1313.6 (Equation 32); 0 A at mid-scale.
static_assert(DecodeSupply(0U, 1314U).temperature_mc == 24'873);
static_assert(DecodeSupply(1448U, 0U).vio_mv == 5000U);
static_assert(DecodeFeedback({0x0000'0000U, 0U, 0U, 0U, 0U}, kInternalClockHz).error ==
              Error::NotReady);
static_assert(DecodeFeedback({(1000U << 11U) | 2000U, 0x5000U | 1000U, 0x0FFFU, 0U, 0U},
                             kInternalClockHz)
                  .value.average_current_ua == -2000);
static_assert(DecodeFeedback({(1000U << 11U) | 2000U, 0x5000U | 1000U, 0x0FFFU, 0U, 0U},
                             kInternalClockHz)
                  .value.duty_permyriad == 5000U);
static_assert(DecodeFeedback({(1000U << 11U) | 2000U, 0x5000U | 1000U, 0x0FFFU, 0U, 0U},
                             kInternalClockHz)
                  .value.period_us == 2285U);

/// `Config{}` with every channel declared loaded: the encoders' reset values come out of it.
[[nodiscard]] constexpr Config AllLoaded() noexcept {
  Config c{};
  for (ChannelConfig& ch : c.channels) {
    ch.loaded = true;
  }
  return c;
}

// The reset configuration encodes to the reset values of section 5.3.
static_assert(GlobalConfigValue(Config{}) == reg::kGlobalConfigReset);
static_assert(ClkDivValue(Config{}) == reg::kClkDivReset);
static_assert(VbatThValue(Config{}) == reg::kVbatThReset);
static_assert((FaultMask0Value(AllLoaded()) == reg::kMask0Reset) &&
              (FaultMask1Value(AllLoaded()) == reg::kMask1Reset) &&
              (FaultMask2Value(Config{}) == reg::kMask2Reset));
static_assert((ChConfigValue(AllLoaded().channels[0]) == reg::kChConfigReset) &&
              (CtrlValue(ChannelConfig{}) == reg::kCtrlReset) &&
              (IntegratorLimitValue(IccConfig{}) == reg::kIntegratorLimitReset) &&
              (ChannelWords(ChannelConfig{})[9] == reg::kCtrlIntThreshReset));
static_assert((ModeValue(ChannelMode::Off) == 0U) && (PeriodValue(IccConfig{}) == 0U) &&
              (DitherClkDivValue(DitherConfig{}) == 0U) && (TonValue(ChannelConfig{}) == 0U));
// An unloaded channel never reaches FAULTN and runs no OFF-state diagnosis (KB-CPP-131).
static_assert((FaultMask0Value(Config{}) == 0xC000U) && (FaultMask1Value(Config{}) == 0x7000U));
static_assert(reg::OffDiagCh::Extract(ChConfigValue(ChannelConfig{})) == reg::kOffDiagDisabled);
static_assert(ModeValue(ChannelMode::Measurement) == 0xCU);
// The fields land where Table 24's registers put them.
static_assert(CtrlValue(CurrentControlChannel()) == 0x4602U);
static_assert(PeriodValue(CurrentControlChannel().icc) == 0x07DBU);
static_assert(DitherClkDivValue(CurrentControlChannel().dither) == 0x176BU);
static_assert(ClkDivValue(Config{.clock = ClockSource::External, .external_clock_khz = 4000U}) ==
              0x8838U);

} // namespace detail

/**
 * @defgroup tle92466ed_driver TLE92466ED driver
 * @brief The driver class.
 * @{
 */

// clang-format off: the table below must keep one row per line.
/**
 * @brief Driver for one TLE92466ED on a platform port.
 *
 * Lifecycle: construct with the configuration (no I/O), then `EnsureInitialized()`, the one way
 * to start (reset, readiness, identity, start-up faults, every register), which leaves the chip in
 * Config Mode; `EnterMissionMode()` lets the channels switch. Every call that touches the chip
 * returns `Error::NotInitialized` until `EnsureInitialized()` has succeeded; no call starts the
 * chip as a side effect.
 *
 * Optional port hooks, found at compile time:
 *
 * | Hook | Used for | Without it |
 * | --- | --- | --- |
 * | `void DelayUs(uint32_t us) noexcept` | t_POR, t_RESN, the INIT_DONE and self-test polls | Compile error: the waits are required |
 * | `bool GpioSet(CtrlPin, GpioSignal) noexcept` | RESN pulse at start-up, EN with the operation state, DRV0/DRV1 | Start-up waits t_POR and rewrites every register; `HardwareReset()` and `SetDrivePin()` return `Unsupported` |
 * | `bool GpioRead(CtrlPin, GpioSignal&) noexcept` | `ReadFaultPin()` | `ReadFaultPin()` returns `Unsupported` |
 * | `void Log(LogLevel, const char* message) noexcept` | One line per lifecycle event or start-up failure | Nothing is logged |
 *
 * A hook declared with the wrong signature (for example `void GpioSet(...)`) is a compile error.
 *
 * @tparam Port Your class derived from `SpiInterface<Port>`.
 */
// clang-format on
template <SpiPort Port>
class TLE92466ED {
  static_assert(!detail::DeclaresGpioSet<Port> || detail::HasGpioSet<Port>,
                "tle92466ed: the optional GpioSet hook must be `bool GpioSet(CtrlPin, GpioSignal) "
                "noexcept`, so that a failed pin write is reported");
  static_assert(!detail::DeclaresGpioRead<Port> || detail::HasGpioRead<Port>,
                "tle92466ed: the optional GpioRead hook must be `bool GpioRead(CtrlPin, "
                "GpioSignal&) noexcept`");
  static_assert(
      !detail::DeclaresDelayUs<Port> || detail::HasDelayUs<Port>,
      "tle92466ed: the DelayUs hook must be `void DelayUs(uint32_t us) noexcept`, so that no "
      "wait is truncated");
  static_assert(
      !detail::DeclaresLog<Port> || detail::HasLog<Port>,
      "tle92466ed: the optional Log hook must be `void Log(LogLevel, const char* message) "
      "noexcept`");

public:
  /**
   * @brief Bind the driver to a port and store the configuration. No I/O, cannot fail.
   * @param port The port; it must outlive the driver.
   * @param config What `EnsureInitialized()` will apply; checked there, before any I/O.
   */
  explicit TLE92466ED(Port& port, const Config& config = {}) noexcept
      : bus_(port), config_(config) {}

  /** @brief One driver object per chip. */
  TLE92466ED(const TLE92466ED&) = delete;
  /** @brief One driver object per chip. */
  TLE92466ED& operator=(const TLE92466ED&) = delete;

  /// @name Lifecycle
  /// @{

  /**
   * @brief Start the chip, once: the only way to initialise the driver.
   *
   * In order: check the configuration; bring the port up; reset the chip (EN low, RESN low for
   * `kResetPulseUs`, then t_RESN; without `GpioSet()` write CH_CTRL to 0 at once, so a chip an
   * earlier run left in Mission Mode is switched off, then wait t_POR, section 4.1.3); poll FB_STAT
   * until INIT_DONE = 1, at most `Config::init_timeout_us` (4.2.1); check ICVID (manufacturer 0xC1,
   * version A11 or A13); write CH_CTRL to Config Mode with every channel off; read, keep
   * (`GetStartupFaults()`) and clear the diagnosis flags it found, refusing an unprogrammed OTP
   * (4.9.7); write CLK_DIV and wait for INIT_DONE again; write GLOBAL_CONFIG with the SPI watchdog
   * off and read it back; write VBAT_TH, FAULT_MASK0..2, the pairs and every register of every
   * channel that is not an enabled pair's slave; write WD_RELOAD, and last, with
   * `Config::spi_watchdog`, GLOBAL_CONFIG with SPI_WD_EN, read back (5.2.2); then arm the reset
   * watch if POR_EVENT and RES_EVENT read 0 (4.1.1). It ends in Config Mode. 181 frames on a clean
   * chip with every channel written, plus the reset; later calls return `Ok` at once with no I/O.
   * After a failure the driver stays
   * uninitialised and the next call starts again from the beginning.
   *
   * @return `Ok`, `InvalidArgument` (no I/O), `Bus`, `Timeout`, `DeviceIdMismatch`, `DeviceFault`
   *         (OTP_VIRGIN), `WriteMismatch` (GLOBAL_CONFIG), or a reply error (`NoDevice`,
   *         `CriticalFault`, `Crc`, `ReplyMismatch`, `FrameRejected`).
   */
  Result<void> EnsureInitialized() noexcept;

  /** @return true after a successful `EnsureInitialized()` and before `Deinitialize()`. No I/O. */
  [[nodiscard]] bool IsInitialized() const noexcept {
    return initialized_;
  }

  /**
   * @brief Switch every channel off: Config Mode (CH_CTRL with OP_MODE and every EN_CH 0), then EN
   * low. The driver is uninitialised afterwards even if a step failed. On an uninitialised driver
   * (never started, or left by a reset or a critical fault) only EN is driven low, with no SPI.
   * @return `Ok`, `Bus` or a reply error.
   */
  Result<void> Deinitialize() noexcept;

  /**
   * @brief Pulse RESN and start again as `EnsureInitialized()` does, with the stored configuration.
   * @return `Ok`, `NotInitialized`, `Unsupported` (no `GpioSet()`), or the errors of
   *         `EnsureInitialized()`.
   */
  Result<void> HardwareReset() noexcept;

  /// @}
  /// @name Configuration
  /// @{

  /**
   * @brief Change the whole configuration. Before the start it is only stored, with no I/O; on a
   * started driver it is written as at start-up (no reset), which needs Config Mode.
   * @param config Checked with `Validate()` before any I/O.
   * @return `Ok`, `InvalidArgument`, `NotInitialized` never, `WrongState` (Mission Mode),
   *         `WriteMismatch`, `Bus` or a reply error.
   */
  Result<void> ApplyConfig(const Config& config) noexcept;

  /** @return The configuration last applied or stored. No I/O. */
  [[nodiscard]] const Config& GetConfig() const& noexcept {
    return config_;
  }
  /** @brief Not on a temporary driver: the reference would dangle (KB-CPP20-044). */
  void GetConfig() const&& = delete;

  /**
   * @brief Write one channel's registers (MODE is writable in Config Mode only, section 4.4.1).
   * Before the start only stored. SETPOINT is written with TARGET 0.
   * @param channel The channel; not the slave of an enabled pair.
   * @param config Its configuration; the whole `Config` with it must pass `Validate()`.
   * @param verify `ReadBack`: read the 11 registers back and compare, 22 more frames.
   * @return `Ok`, `InvalidArgument`, `WrongState` (Mission Mode, or a slave), `WriteMismatch`,
   *         `Bus` or a reply error. 26 frames: the 11 registers and FAULT_MASK0..1.
   */
  Result<void> ConfigureChannel(Channel channel, const ChannelConfig& config,
                                Verify verify = Verify::None) noexcept;

  /**
   * @brief Join or split a pair (CH_CTRL.CH_PAR, Config Mode only, section 4.5.2). Joining sets the
   * master's SETPOINT to 0 first, since one TARGET count is then twice the current; set it again
   * with `SetCurrent()`, checked against the pair's limit. Splitting rewrites the slave's
   * registers, which the chip requires.
   * @param pair The pair.
   * @param enabled Join or split.
   * @return `Ok`, `NotInitialized`, `InvalidArgument` (the slave is declared loaded),
   *         `WrongState` (Mission Mode), `Bus` or a reply error.
   */
  Result<void> SetParallel(ParallelPair pair, bool enabled) noexcept;

  /// @}
  /// @name Operation state (section 4.3)
  /// @{

  /**
   * @brief Enter Mission Mode, as Figure 4 allows it. First the entry conditions: FB_STAT
   * (INIT_DONE = 1; no SPI watchdog, clock, central temperature, external or internal supply
   * fault), the DIAG_ERR_CHGR and DIAG_WARN_CHGR groups FB_STAT flags (no channel OTE or OTW),
   * GLOBAL_DIAG2 (no REG_ECC_ERR); any of them set is `MissionBlocked` with nothing written. Then
   * EN high, OP_MODE with every channel off, and CH_CTRL read back, always: the copy is what the
   * chip did, and an OP_MODE the chip refused is `MissionBlocked`. VPRE_OV blocks entry too, a
   * fail-safe choice: the datasheet does not list it in Figure 4. 8 frames, 2 more per flagged
   * group.
   * @return `Ok`, `NotInitialized`, `MissionBlocked`, `Bus` or a reply error.
   */
  Result<void> EnterMissionMode() noexcept;

  /**
   * @brief Enter Config Mode: OP_MODE 0, which turns every channel off (section 4.3.1), then EN
   * low, whatever the SPI result; the first error is returned. Two frames.
   * @return `Ok`, `NotInitialized`, `Bus` or a reply error.
   */
  Result<void> EnterConfigMode() noexcept;

  /** @return true when the driver last set Mission Mode and saw nothing leave it. No I/O. */
  [[nodiscard]] bool IsMissionMode() const noexcept {
    return (static_cast<uint32_t>(ch_ctrl_) & reg::kChCtrlOpMode) != 0U;
  }

  /**
   * @brief Read CH_CTRL: the operation state and the enabled channels as the chip has them. A
   * supply, clock or watchdog fault ends Mission Mode and clears EN_CH (sections 4.1.2, 4.2.2,
   * 5.2.2), a channel fault clears its EN_CH (4.9.2 to 4.9.6): the driver's copy follows this read.
   * Every enable change does this read first. Two frames.
   * @return CH_CTRL, or `NotInitialized`, `Bus` or a reply error.
   */
  Result<uint16_t> ReadChannelControl() noexcept;

  /// @}
  /// @name Channels
  /// @{

  /**
   * @brief Switch channels on and off together. CH_CTRL is read first, so the copy is the chip's
   * (a channel a fault switched off stays off), then written whole with only the EN_CH bits the
   * mask names changed; OP_MODE is kept as the chip has it, never set (handbook KB-CPP-121, -122).
   * @param mask Bit x for channel x, 0 to 0x3F. A channel the mask enables must be declared loaded
   * and not be a slave, and Mission Mode is needed to enable one (section 5.3.2.3). A channel
   * already on whose state a failed write left unknown is kept as the chip has it, so the mask
   * still switches the others off; only a channel it newly turns on is refused (`WrongState`).
   * @param verify `ReadBack`: read CH_CTRL back, two more frames.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `WrongState` (nothing sent),
   *         `MissionBlocked` (the chip left Mission Mode on its own), `WriteMismatch` (the chip
   * keeps a channel with a latched fault off, section 4.9.2), `Bus` or a reply error. 4 frames.
   */
  Result<void> SetEnabledChannels(uint8_t mask, Verify verify = Verify::None) noexcept;

  /**
   * @brief Switch one channel on, leaving the others as they are. As `SetEnabledChannels()`.
   * @param channel The channel.
   * @param verify As `SetEnabledChannels()`.
   * @return As `SetEnabledChannels()`.
   */
  Result<void> EnableChannel(Channel channel, Verify verify = Verify::None) noexcept;

  /**
   * @brief Switch one channel off, leaving the others as they are. Allowed in any state. When the
   * CH_CTRL read fails, the write still goes out, from the copy with the channel cleared, and the
   * read's error is returned: switching off never depends on a read.
   * @param channel The channel.
   * @param verify As `SetEnabledChannels()`.
   * @return As `SetEnabledChannels()`.
   */
  Result<void> DisableChannel(Channel channel, Verify verify = Verify::None) noexcept;

  /**
   * @return The channels the chip switched off on its own since `ClearDroppedChannels()`, bit x
   * for channel x: a channel the driver last asked on that a validated read of CH_CTRL (the read
   * before every channel call, or `Verify::ReadBack`) found off: a refused enable or a latched
   * fault (4.9.2 to 4.9.6), or a supply, clock or watchdog fault (4.1.2, 4.2.2, 5.2.2). Never
   * taken from a write's reply. Each is also logged. Emptied by every start
   * (`EnsureInitialized()`, `HardwareReset()`). No I/O.
   */
  [[nodiscard]] uint8_t GetDroppedChannels() const noexcept {
    return dropped_;
  }
  /** @brief Empty the record of `GetDroppedChannels()`. No I/O. */
  void ClearDroppedChannels() noexcept {
    dropped_ = 0U;
  }

  /**
   * @return The EN_CH bits of the driver's copy of CH_CTRL. No I/O. After a write without
   * `Verify::ReadBack` a bit is set only when both the value written and the write's reply have
   * it, so the copy can under-report a channel that is on, never report one that is off.
   */
  [[nodiscard]] uint8_t GetEnabledChannels() const noexcept {
    return static_cast<uint8_t>(static_cast<uint32_t>(ch_ctrl_) & reg::kChCtrlEnMask);
  }

  /**
   * @brief Set a channel's average current setpoint (SETPOINT.TARGET, section 4.6.1).
   *
   * The current is checked against the operating range, never clamped (handbook KB-CPP-126): up to
   * 1.5 A, and with the configured dither amplitude up to 1.8 A; for the master of a pair, up to
   * 2.7 A with dither (Table 14). With dither, a non-zero setpoint must exceed the amplitude
   * (section 4.7.1). The value is rounded to the nearest TARGET count; the call returns what was
   * programmed. Allowed in either state: the channel switches when it is enabled in Mission Mode.
   * @param channel The channel; not a slave.
   * @param microamps The setpoint.
   * @param verify `ReadBack`: read SETPOINT back, two more frames.
   * @return The programmed current in microamps, or `NotInitialized`, `InvalidArgument`,
   *         `WrongState` (a slave), `WriteMismatch`, `Bus` or a reply error. Two frames.
   */
  Result<uint32_t> SetCurrent(Channel channel, uint32_t microamps,
                              Verify verify = Verify::None) noexcept;

  /**
   * @param channel The channel.
   * @return The setpoint last programmed, in microamps; 0 for an invalid channel. After a failed
   *         `SetCurrent()` the larger of the old and the requested setpoint, which the chip may
   *         hold, until a `SetCurrent()` succeeds. No I/O.
   */
  [[nodiscard]] uint32_t GetCurrentSetpoint(Channel channel) const noexcept;

  /**
   * @brief Change a channel's dither at run time (section 4.7.2): DITHER_CLK_DIV (takes effect at
   * once), DITHER_STEP, then DITHER_CTRL, whose write starts the update at the next dither period.
   * @param channel The channel; not a slave.
   * @param dither The settings; checked as `Validate()` does and against the current setpoint.
   * @param verify `ReadBack`: read the three registers back, six more frames.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `WrongState`, `WriteMismatch`, `Bus` or a
   *         reply error. Six frames.
   */
  Result<void> SetDither(Channel channel, const DitherConfig& dither,
                         Verify verify = Verify::None) noexcept;

  /**
   * @brief Run the OFF-state overcurrent check once (section 4.9.6): set CH_CONFIG.OC_DIAG_EN and
   * the channel's EN_CH; the chip switches the stage on for t_OCon and clears OC_DIAG_EN. Read the
   * result with `ReadFaults()` after t_OCon. The channel must be off with setpoint 0, so the check
   * does not start the current controller; afterwards it stays enabled with setpoint 0, which is
   * the OFF-state diagnosis condition (4.9.1): `DisableChannel()` when that is not wanted.
   * @param channel A loaded channel, not a slave, with `on_time_mant` not 0 and t_OCon at most
   * 300 ms (TON register note).
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `WrongState` (Config Mode, a slave, a
   *         setpoint, or the channel on or asked on since the last read of CH_CTRL: nothing
   *         sent), `MissionBlocked`, `Bus` or a reply error. 6 frames.
   */
  Result<void> StartOvercurrentCheck(Channel channel) noexcept;

  /**
   * @brief Drive DRV0 or DRV1, the inputs of the direct-drive channels (section 4.8.1).
   * @param pin `CtrlPin::Drv0` or `CtrlPin::Drv1`.
   * @param high The level.
   * @return `Ok`, `NotInitialized`, `InvalidArgument` (another pin), `Unsupported` (no `GpioSet()`)
   *         or `Bus`.
   */
  Result<void> SetDrivePin(CtrlPin pin, [[maybe_unused]] bool high) noexcept;

  /// @}
  /// @name Watchdog (section 5.2.2)
  /// @{

  /**
   * @brief Reload the SPI watchdog: write `Config::watchdog_reload` to WD_RELOAD. Call it more
   * often than the timeout while `Config::spi_watchdog` is set. Not verifiable: the counter runs.
   * Two frames.
   * @return `Ok`, `NotInitialized`, `Bus` or a reply error.
   */
  Result<void> ReloadWatchdog() noexcept;

  /// @}
  /// @name Diagnostics
  /// @{

  /**
   * @brief Read FB_STAT, the one-register summary (section 5.3.2.21). Two frames.
   * @return The status, or `NotInitialized`, `Bus` or a reply error.
   */
  Result<DeviceStatus> ReadStatus() noexcept;

  /**
   * @brief Read the nine diagnosis registers (GLOBAL_DIAG0..2, DIAG_ERR_CHGR0..2,
   * DIAG_WARN_CHGR0..2). Reading DIAG_WARN_CHGR clears OLSG_WARN_CHK_NOK on the chip (4.9.3); every
   * flag read is also ORed into the latch (`GetLatchedFaults()`). 18 frames.
   * @return The registers, or `NotInitialized`, `Bus` or a reply error.
   */
  Result<Faults> ReadFaults() noexcept;

  /**
   * @brief Clear the diagnosis flags on the chip: every diagnosis register is read and latched
   * (`GetLatchedFaults()`), then exactly the flags read are cleared, so a flag raised since the
   * last read is never lost (handbook KB-CPP-119). An rwh flag is cleared by a 1 where it read 1,
   * an rw flag (POR_EVENT, RES_EVENT, VDD2V5_UV/OV) by a 0 (sections 4.1.2, 4.9.1, 5.3.1); OC and
   * OTP_ECC_ERR get the two clears they need (4.9.6, 4.9.7); OTP_VIRGIN is never cleared. A
   * register with nothing set is not written. With the reset watch armed, POR_EVENT or RES_EVENT
   * read 1 is `UnexpectedReset`, nothing cleared. A flag set again at once means the cause is still
   * there. Not verified. 18 frames, 2 more per register written (4 with OC or OTP_ECC_ERR).
   * @return `Ok`, `NotInitialized`, `UnexpectedReset`, `Bus` or a reply error.
   */
  Result<void> ClearFaults() noexcept;

  /**
   * @brief Clear one channel's flags and warnings: its DIAG_ERR_CHGR and DIAG_WARN_CHGR group read
   * and latched (both channels), then 1 written to the flags of this channel that read 1 (twice
   * for OC), 0 to everything else. A channel switched off by a fault stays off until its flags are
   * cleared and EN_CH is set again (sections 4.9.2 to 4.9.6). 4 frames, 2 more per write.
   * @param channel The channel.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `Bus` or a reply error.
   */
  Result<void> ClearChannelFaults(Channel channel) noexcept;

  /**
   * @return Every flag any read of a diagnosis register saw since the last `ClearLatchedFaults()`,
   * the reads inside `ReadFaults()` and `ReadRegister()` included (handbook KB-CPP-119). No I/O.
   */
  [[nodiscard]] const Faults& GetLatchedFaults() const& noexcept {
    return latched_;
  }
  /** @brief Not on a temporary driver. */
  void GetLatchedFaults() const&& = delete;

  /** @brief Empty the latch of `GetLatchedFaults()`. No I/O. */
  void ClearLatchedFaults() noexcept {
    latched_ = Faults{};
  }

  /**
   * @return The diagnosis registers as the start-up found them, before it cleared them (POR_EVENT
   * and RES_EVENT of the start-up's own reset included). No I/O.
   */
  [[nodiscard]] const Faults& GetStartupFaults() const& noexcept {
    return startup_;
  }
  /** @brief Not on a temporary driver. */
  void GetStartupFaults() const&& = delete;

  /**
   * @brief Read the FAULTN pin through the port's `GpioRead()`.
   * @return true while a fault pulls FAULTN low, or `NotInitialized`, `Unsupported` or `Bus`.
   */
  Result<bool> ReadFaultPin() noexcept;

  /**
   * @brief Read PIN_STAT (section 5.3.2.20). Two frames.
   * @return The levels, or `NotInitialized`, `Bus` or a reply error.
   */
  Result<PinStatus> ReadPinStatus() noexcept;

  /**
   * @brief Read FB_VOLTAGE1 and FB_VOLTAGE2: VIO, VDD, VBAT and the central temperature
   * (sections 4.1.2, 4.11.1). Four frames.
   * @return The readings, or `NotInitialized`, `Bus` or a reply error.
   */
  Result<SupplyReadings> ReadSupply() noexcept;

  /**
   * @brief Read one measurement period of a channel's feedback, as section 4.10.5 recommends: read
   * FB_UPD; when the channel has new values, freeze it (FB_FRZ), read FB_DC, FB_VBAT, FB_I_AVG,
   * FB_IMIN_IMAX and FB_PERIOD_MIN_MAX, and release the freeze, which clears its FB_UPD bit. The
   * conversions use f_SYS of the configuration (handbook KB-CPP-128). 16 frames; 2 when there is
   * nothing new.
   * @param channel The channel; not a slave (its feedback is meaningless, section 4.5.2).
   * @return The feedback, or `NotInitialized`, `InvalidArgument`, `WrongState`, `NotReady` (no new
   *         values, or TP_MANT = 0: no measurement period, as with a dither clock of 0), `Bus` or a
   *         reply error.
   */
  Result<ChannelFeedback> ReadChannelFeedback(Channel channel) noexcept;

  /**
   * @brief Run the safety flip-flop self-test (section 4.9.8, Config Mode only): start it, poll
   * SMU_SLF_TST_DONE every `kPollIntervalUs` for at most `kBistTimeoutUs`, read the result, clear
   * SFF_BIST (a write of 0), read and latch GLOBAL_DIAG2 and clear only the REG_ECC_ERR the test
   * sets on purpose.
   * @return The result, or `NotInitialized`, `WrongState`, `Timeout`, `Bus` or a reply error.
   */
  Result<BistResult> RunBist() noexcept;

  /**
   * @brief Read CHIPID0..2, the unique chip identification. Six frames.
   * @return The ID, or `NotInitialized`, `Bus` or a reply error.
   */
  Result<ChipId> ReadChipId() noexcept;

  /** @return ICVID.VERSION read by the last start-up (0xFC A11, 0xFE A13); 0 before. No I/O. */
  [[nodiscard]] uint8_t GetIcVersion() const noexcept {
    return version_;
  }

  /**
   * @return true while a reset of the chip is detected on every access: the start-up cleared
   * POR_EVENT and RES_EVENT and saw them read 0. No I/O.
   */
  [[nodiscard]] bool IsResetWatchArmed() const noexcept {
    return reset_watch_;
  }

  /// @}
  /// @name Raw access (handbook KB-CPP-129)
  /// @{

  /**
   * @brief Read one register once: two frames, no retry, no decoding. Reading DIAG_WARN_CHGR clears
   * OLSG_WARN_CHK_NOK on the chip; a diagnosis register read here feeds the latch too.
   * @param address A register of Tables 19 and 24 (`reg::IsRegister()`).
   * @return The 16 or 22 data bits, or `NotInitialized`, `InvalidArgument` (no such register;
   *         nothing sent), `Bus` or a reply error.
   */
  Result<uint32_t> ReadRegister(uint16_t address) noexcept;

  /** @return The last word clocked in as a reply, by any call; 0 before. No I/O. */
  [[nodiscard]] uint32_t GetLastReply() const noexcept {
    return bus_.LastReply();
  }

  /**
   * @return The flags byte of the last critical fault frame (section 5.2.3.5): bits 7..5 are 1 when
   * the 1V5, 2V5 and bandgap are fine, bits 4..0 are clock faults. 0 before the first. No I/O.
   */
  [[nodiscard]] uint8_t GetLastCriticalFault() const noexcept {
    return bus_.LastCritical();
  }

  /** @return The protocol counters: frames, retries, rejected replies, resyncs. No I/O. */
  [[nodiscard]] BusDiagnostics GetBusDiagnostics() const noexcept {
    return bus_.Diagnostics();
  }

  /// @}

private:
  Result<void> initialize() noexcept;
  Result<void> resetChip() noexcept;
  Result<void> waitInitDone() noexcept;
  Result<void> identify() noexcept;
  Result<void> takeStartupFaults() noexcept;
  Result<Faults> readFaults() noexcept;
  Result<void> clearAll() noexcept;
  Result<void> clearRead(const Faults& f) noexcept;
  Result<void> clearTwice(uint16_t address, uint16_t bits, uint16_t twice) noexcept;
  Result<void> armWatchdog(const Config& config) noexcept;
  Result<void> checkMissionEntry() noexcept;
  Result<void> checkGroupTemperature(uint32_t group, const DeviceStatus& status) noexcept;
  Result<void> reconcile() noexcept;
  void adopt(uint16_t chip) noexcept;
  Result<void> writeEnables(uint32_t set, uint32_t clear, Verify verify) noexcept;
  [[nodiscard]] Error checkEnable(uint32_t bits) const noexcept;
  Result<uint32_t> readOnce(uint16_t address) noexcept;
  void latchAll(const Faults& f) noexcept;
  void noteDropped(uint32_t bits) noexcept;
  Result<uint32_t> waitBist() noexcept;
  Result<void> writeAll(const Config& config) noexcept;
  Result<void> writeGlobal(const Config& config) noexcept;
  Result<void> writeChannel(Channel channel, const ChannelConfig& config, Verify verify) noexcept;
  Result<void> armResetWatch() noexcept;
  Result<void> writeChCtrl(uint16_t value, Verify verify) noexcept;
  Result<void> setPin([[maybe_unused]] CtrlPin pin, [[maybe_unused]] GpioSignal signal) noexcept;
  Result<void> readFeedbackSet(uint16_t base, std::array<uint32_t, 5>& words) noexcept;
  Result<uint32_t> read(uint16_t address) noexcept;
  Result<void> write(uint16_t address, uint16_t value) noexcept;
  Result<void> writeVerified(uint16_t address, uint16_t value, uint16_t compare,
                             Verify verify) noexcept;
  [[nodiscard]] Error checkChannel(Channel channel) const noexcept;
  [[nodiscard]] Error checkKnown(Channel channel, bool dither) const noexcept;
  [[nodiscard]] bool maybeMission() const noexcept;
  [[nodiscard]] Error checkCurrent(Channel channel, uint32_t microamps,
                                   const DitherConfig& dither) const noexcept;
  void latch(uint16_t address, uint32_t value) noexcept;
  void track(Error error) noexcept;
  Result<void> fail(Error error, const char* message) noexcept;

  detail::Transport<Port> bus_;
  Config config_{};
  Faults latched_{};
  Faults startup_{};
  std::array<uint16_t, kChannelCount> setpoint_{}; ///< TARGET last written per channel.
  uint16_t ch_ctrl_{0};                            ///< The copy of CH_CTRL.
  uint8_t version_{0};
  uint8_t wanted_{0}; ///< EN_CH bits last asked for that no validated read has found off since.
  // What a write whose reply failed may have changed (it may have taken effect): each guard that
  // depends on one refuses with WrongState and no I/O until a validated read or a successful write.
  uint16_t par_unknown_{0};    ///< CH_PAR bits the chip may hold other than PairBits(config_).
  uint8_t config_unknown_{0};  ///< Channels whose registers may differ from config_.channels.
  uint8_t dither_unknown_{0};  ///< Channels whose dither registers may differ from config_.
  bool global_unknown_{false}; ///< GLOBAL_CONFIG, VBAT_TH, FAULT_MASK, WD_RELOAD may differ.
  bool maybe_mission_{false};  ///< The chip may be in Mission Mode although the copy says not.
  uint8_t dropped_{0}; ///< Channels the chip switched off on its own (GetDroppedChannels()).
  bool reset_watch_{false};
  bool initialized_{false};
};

/** @} */

} // namespace tle92466ed

// codeql::misra_deviation_next_line(MD-CPP-009)
#include "../src/tle92466ed.ipp" // IWYU pragma: export

#endif // TLE92466ED_HPP
