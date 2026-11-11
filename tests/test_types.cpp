/**
 * @file test_types.cpp
 * @brief Validate() against every documented range, and the conversions, at run time (the
 * static_asserts in the headers prove the reset values; these reach every refusal).
 */
#include <cstdint>

#include "test_harness.hpp"
#include "test_rig.hpp"
#include "tle92466ed.hpp"

using tle92466ed::Config;
using tle92466ed::Error;

namespace {

/// A valid configuration with CH0 in current control, changed by `change`.
template <typename Change>
Error ValidateWith(Change change) {
  Config c{};
  c.channels[0] = tle92466ed::CurrentControlChannel();
  change(c);
  return tle92466ed::Validate(c);
}

} // namespace

TEST_CASE("Validate: the global fields and the driver's own settings") {
  CHECK_EQ(ValidateWith([](Config& c) { (void)c; }), Error::Ok);
  CHECK_EQ(ValidateWith([](Config& c) {
             c.vio = tle92466ed_test::NotAnEnumerator<tle92466ed::VioLevel>(2);
           }),
           Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](Config& c) {
             c.clock = tle92466ed_test::NotAnEnumerator<tle92466ed::ClockSource>(2);
           }),
           Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](Config& c) { c.watchdog_reload = 2048; }), Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](Config& c) { c.vbat_uv_code = c.vbat_ov_code; }),
           Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](Config& c) { c.max_retries = 4; }), Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](Config& c) { c.init_timeout_us = 99; }), Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](Config& c) { c.init_timeout_us = 100'001; }), Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](Config& c) { c.fault_pin.channel_errors = 0x40; }),
           Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](Config& c) { c.fault_pin.channel_warnings = 0x80; }),
           Error::InvalidArgument);
}

TEST_CASE("Validate: an external clock must be a Table 10 step giving f_SYS of Table 6") {
  const auto ext = [](uint32_t khz) {
    return ValidateWith([khz](Config& c) {
      c.clock = tle92466ed::ClockSource::External;
      c.external_clock_khz = khz;
    });
  };
  for (uint32_t khz = 1000U; khz <= 8000U; khz += 500U) {
    CHECK_EQ(ext(khz), Error::Ok);
  }
  CHECK_EQ(ext(0U), Error::InvalidArgument);
  CHECK_EQ(ext(1250U), Error::InvalidArgument);
  CHECK_EQ(ext(8500U), Error::InvalidArgument);
  Config c{};
  c.clock = tle92466ed::ClockSource::External;
  c.external_clock_khz = 9000U;
  CHECK_EQ(tle92466ed::SystemClockHz(c), 0U);
  c.external_clock_khz = 1500U;
  CHECK_EQ(tle92466ed::SystemClockHz(c), 27'750'000U);
}

TEST_CASE("Validate: the current controller (4.6.2)") {
  using C = Config;
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].icc.period_exp = 8; }), Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].icc.ki = 16; }), Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].icc.limit = 1024; }), Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].icc.auto_limit = 32; }), Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].icc.int_thresh = 512; }), Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].icc.min_int_thresh = 13; }),
           Error::InvalidArgument); // AUTO_LIM 16 must exceed MIN_INT_THRESH + 3
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].icc.min_int_thresh = -128; }), Error::Ok);
  // f_PWM 110 Hz to 4 kHz (Table 6).
  CHECK_EQ(ValidateWith([](C& c) {
             c.channels[0].icc.period_mant = 6;
             c.channels[0].icc.period_exp = 0;
           }),
           Error::InvalidArgument); // 4.67 MHz
  CHECK_EQ(ValidateWith([](C& c) {
             c.channels[0].icc.period_mant = 255;
             c.channels[0].icc.period_exp = 7;
             c.channels[0].icc.low_freq_range = true;
           }),
           Error::InvalidArgument); // 107 Hz / 8
  CHECK_EQ(ValidateWith([](C& c) {
             c.channels[0].icc.period_mant = 219;
             c.channels[0].icc.period_exp = 4;
             c.channels[0].icc.low_freq_range = true;
           }),
           Error::Ok); // 998 Hz
}

TEST_CASE("Validate: dither, diagnosis and the channel enumerations") {
  using C = Config;
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].dither.mant = 1024; }), Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].dither.exp = 16; }), Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].dither.step_size = 4096; }),
           Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) {
             c.channels[0].dither.fast = tle92466ed::FastFeedback::QuarterPeriod;
             c.channels[0].dither.flat = 3;
           }),
           Error::Ok);
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].diag.ol_relative_eighths = 8; }),
           Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].diag.ol_fixed = 64; }), Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].diag.olsg_warn_window = 32; }),
           Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].diag.olsg_timeout = 64; }),
           Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) {
             c.channels[0].mode = tle92466ed::ChannelMode::DirectDriveSpi;
             c.channels[0].diag.ol_relative_eighths = 2; // current control only (4.9.2.1)
           }),
           Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) { c.channels[0].on_time_mant = 1024; }), Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) {
             c.channels[0].mode = tle92466ed_test::NotAnEnumerator<tle92466ed::ChannelMode>(6);
           }),
           Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) {
             c.channels[0].slew = tle92466ed_test::NotAnEnumerator<tle92466ed::SlewRate>(4);
           }),
           Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) {
             c.channels[0].diag.current =
                 tle92466ed_test::NotAnEnumerator<tle92466ed::DiagCurrent>(4);
           }),
           Error::InvalidArgument);
  CHECK_EQ(ValidateWith([](C& c) {
             c.channels[0].diag.off_state =
                 tle92466ed_test::NotAnEnumerator<tle92466ed::OffStateDiag>(4);
           }),
           Error::InvalidArgument);
}

TEST_CASE("Conversions at run time: watchdog, VBAT, TARGET, dither, PWM") {
  CHECK(!tle92466ed::WatchdogReloadCode(0U, tle92466ed::kInternalClockHz).ok());
  CHECK_EQ(tle92466ed::WatchdogReloadCode(1000U, tle92466ed::kInternalClockHz).value, 1708U);
  CHECK(!tle92466ed::VbatThresholdCode(42'000U).ok());
  CHECK_EQ(tle92466ed::VbatThresholdCode(7'000U).value, 43U);
  CHECK_EQ(tle92466ed::TargetFromMicroamps(3'000'000U, false), tle92466ed::kTargetMax);
  CHECK_EQ(tle92466ed::MicroampsFromTarget(16384U, true), 2'000'061U);
  tle92466ed::DitherConfig d{};
  d.steps = 200;
  d.step_size = 4095; // more than 2^15 counts: saturates
  CHECK_EQ(tle92466ed::DitherAmplitudeUa(d, false), tle92466ed::kSetpointFullScaleUa);
  tle92466ed::IccConfig icc{};
  CHECK_EQ(tle92466ed::PwmFrequencyHz(icc, tle92466ed::kInternalClockHz), 0U);
  icc.period_mant = 100;
  icc.period_exp = 8; // masked to the field: 0
  CHECK_EQ(tle92466ed::PwmFrequencyHz(icc, tle92466ed::kInternalClockHz), 280'000U);
}

TEST_CASE("CheckedConfig and the version getters") {
  constexpr tle92466ed::CheckedConfig kBoard{Config{.vio = tle92466ed::VioLevel::V3V3}};
  CHECK_EQ(kBoard.Get().vio, tle92466ed::VioLevel::V3V3);
  CHECK(tle92466ed::GetDriverVersion()[0] != '\0');
  CHECK_EQ(tle92466ed::GetDriverVersionMajor(), 0U);
  CHECK((tle92466ed::GetDriverVersionMinor() + tle92466ed::GetDriverVersionPatch()) >= 1U);
}

TEST_CASE("The decoders at run time: status, pins, supply") {
  const auto s = tle92466ed::detail::DecodeStatus(0x3F'FFFFU);
  CHECK(s.init_done && s.spi_watchdog_error && s.data_error && s.reset_event);
  CHECK_EQ(s.error_groups, 7U);
  const auto p = tle92466ed::detail::DecodePins(0U);
  CHECK(p.fault && !p.faultn_high && !p.enable);
  const auto v = tle92466ed::detail::DecodeSupply(0U, 2047U);
  CHECK_EQ(v.temperature_mc, -246'794);
}
