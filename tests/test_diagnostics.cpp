/**
 * @file test_diagnostics.cpp
 * @brief Status, faults and their latch, clearing, feedback, supplies, pins, self-test, chip ID
 * and the raw register read.
 */
#include <cstdint>

#include "mock_tle92466ed.hpp"
#include "test_harness.hpp"
#include "test_rig.hpp"
#include "tle92466ed.hpp"

using tle92466ed::Channel;
using tle92466ed::Config;
using tle92466ed::Error;
using tle92466ed::GlobalFault;
using tle92466ed_test::FullPort;
using tle92466ed_test::MinimalPort;
using tle92466ed_test::Rig;
namespace chip = tle92466ed_test::chip;

TEST_CASE("ReadStatus decodes FB_STAT") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  auto s = r.drv.ReadStatus();
  REQUIRE(s.ok());
  CHECK(s.value.init_done);
  CHECK(!s.value.power_on_event && !s.value.reset_event);
  r.port.state.chip.SetChannelError(3, chip::kSg);
  r.port.state.chip.SetGlobalFault(chip::kCotWarn | chip::kVbatOv);
  r.port.state.chip.SetInternalFault(0x0010U); // REF_UV
  s = r.drv.ReadStatus();
  REQUIRE(s.ok());
  CHECK_EQ(s.value.error_groups, 0x2U); // CH3 is in group 1
  CHECK(s.value.central_ot_warning && s.value.supply_external && s.value.supply_internal);
  CHECK(!s.value.clock_fault && !s.value.central_ot_error && !s.value.spi_watchdog_error);
}

TEST_CASE("ReadFaults decodes every register and feeds the latch (KB-CPP-119)") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  r.port.state.chip.SetChannelError(1, chip::kOl | chip::kOte);
  r.port.state.chip.SetChannelWarning(4, chip::kIRegWarn | chip::kOtw);
  r.port.state.chip.SetDataFault(chip::kRegEccErr);
  const auto f = r.drv.ReadFaults();
  REQUIRE(f.ok());
  const auto ch1 = f.value.Of(Channel::Ch1);
  CHECK(ch1.open_load && ch1.overtemperature && !ch1.olsg && ch1.AnyError());
  const auto ch4 = f.value.Of(Channel::Ch4);
  CHECK(ch4.current_regulation && ch4.overtemperature_warning && !ch4.AnyError());
  CHECK(f.value.Has(GlobalFault::RegisterEcc));
  CHECK(!f.value.Has(GlobalFault::VbatUndervoltage));
  CHECK(f.value.Any());
  // The latch keeps them after the chip's flags are cleared.
  REQUIRE(r.drv.ClearFaults().ok());
  const auto again = r.drv.ReadFaults();
  REQUIRE(again.ok());
  CHECK(!again.value.Any());
  CHECK(r.drv.GetLatchedFaults().Of(Channel::Ch1).open_load);
  CHECK(!(r.drv.GetLatchedFaults() == tle92466ed::Faults{}));
  r.drv.ClearLatchedFaults();
  CHECK(!r.drv.GetLatchedFaults().Any());
}

TEST_CASE("OLSG_WARN_CHK_NOK is cleared by a read; the latch keeps it (4.9.3)") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.SetChannelWarning(0, chip::kOlsgChkNok);
  CHECK(r.drv.ReadFaults().value.Of(Channel::Ch0).olsg_check_skipped);
  CHECK(!r.drv.ReadFaults().value.Of(Channel::Ch0).olsg_check_skipped);
  CHECK(r.drv.GetLatchedFaults().Of(Channel::Ch0).olsg_check_skipped);
  CHECK(!r.drv.GetLatchedFaults().Any()); // a check that could not run is not a fault
}

TEST_CASE("ClearFaults: two clears for OC and OTP_ECC_ERR, the events written 0") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  r.port.state.chip.SetChannelError(5, chip::kOc);
  r.port.state.chip.SetDataFault(chip::kOtpEccErr);
  r.port.state.chip.SetGlobalFault(chip::kClkNok);
  r.port.state.chip.SetInternalFault(0x000CU); // VDD2V5 UV and OV, rw bits
  CHECK(r.drv.ClearFaults().ok());
  CHECK_EQ(r.port.state.chip.Peek(chip::kDiagErr0 + 2U), 0U);
  CHECK_EQ(r.port.state.chip.Peek(chip::kGlobalDiag2), 0U);
  CHECK_EQ(r.port.state.chip.Peek(chip::kGlobalDiag1), 0U);
  CHECK_EQ(r.port.state.chip.Peek(chip::kGlobalDiag0), 0U);
  CHECK(r.drv.IsInitialized()); // no false reset
}

TEST_CASE("ClearChannelFaults clears one channel and leaves its group partner alone") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  r.port.state.chip.SetChannelError(0, chip::kOc | chip::kOlsg);
  r.port.state.chip.SetChannelError(1, chip::kSg);
  CHECK(r.drv.ClearChannelFaults(Channel::Ch0).ok());
  CHECK_EQ(r.port.state.chip.Peek(chip::kDiagErr0), static_cast<uint16_t>(chip::kSg << 8U));
  CHECK_EQ(r.drv.ClearChannelFaults(tle92466ed_test::NotAnEnumerator<Channel>(6)).error,
           Error::InvalidArgument);
}

TEST_CASE("ReadFaultPin follows FAULTN through GpioRead") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  auto p = r.drv.ReadFaultPin();
  CHECK(p.ok() && !p.value);
  r.port.state.chip.SetChannelError(0, chip::kOl); // CH0 loaded: unmasked
  p = r.drv.ReadFaultPin();
  CHECK(p.ok() && p.value);
  r.port.fault_read_ok = false;
  CHECK_EQ(r.drv.ReadFaultPin().error, Error::Bus);
  Rig<MinimalPort> m;
  REQUIRE(m.Start());
  CHECK_EQ(m.drv.ReadFaultPin().error, Error::Unsupported);
}

TEST_CASE("A fault on a channel not declared loaded never reaches FAULTN (KB-CPP-131)") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.SetChannelError(5, chip::kOl);
  const auto p = r.drv.ReadPinStatus();
  REQUIRE(p.ok());
  CHECK(!p.value.fault);
  CHECK(p.value.faultn_high);
}

TEST_CASE("ReadPinStatus decodes PIN_STAT") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.SetDrivePin(tle92466ed::CtrlPin::Drv0, true).ok());
  r.port.state.chip.SetChannelError(0, chip::kOl);
  const auto p = r.drv.ReadPinStatus();
  REQUIRE(p.ok());
  CHECK(p.value.drv0 && !p.value.drv1 && p.value.enable && p.value.fault && !p.value.faultn_high);
}

TEST_CASE("ReadSupply converts FB_VOLTAGE1 and FB_VOLTAGE2 (Equation 32)") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.vio_raw = 956;   // 3.30 V
  r.port.state.chip.temp_raw = 1112; // about 100 degC
  const auto s = r.drv.ReadSupply();
  REQUIRE(s.ok());
  CHECK_EQ(s.value.vio_mv, 3301U);
  CHECK_EQ(s.value.vdd_mv, 5000U);
  CHECK_EQ(s.value.vbat_mv, 13998U);
  CHECK_EQ(s.value.temperature_mc, 99'740);
}

TEST_CASE("Feedback: NotReady before a period ends, then one frozen set (4.10.5)") {
  Rig<MinimalPort> r;
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 750'000).ok());
  CHECK_EQ(r.drv.ReadChannelFeedback(Channel::Ch0).error, Error::NotReady);
  r.port.DelayUs(1500);
  const auto f = r.drv.ReadChannelFeedback(Channel::Ch0);
  REQUIRE(f.ok());
  CHECK((f.value.average_current_ua > 745'000) && (f.value.average_current_ua < 755'000));
  CHECK_EQ(f.value.duty_permyriad, 4000U);
  CHECK_EQ(f.value.period_us, 1000U);
  CHECK((f.value.vbat_mv > 13'900U) && (f.value.vbat_mv < 14'100U));
  CHECK(f.value.max_current_ua > f.value.min_current_ua);
  CHECK(f.value.min_pwm_period_ns > 900'000U);
  CHECK_EQ(r.port.state.chip.Peek(chip::kFbFrz), 0U); // released
  CHECK_EQ(r.port.state.chip.Peek(chip::kFbUpd), 0U); // and its update flag cleared
  CHECK_EQ(r.drv.ReadChannelFeedback(Channel::Ch0).error, Error::NotReady); // nothing new yet
  CHECK_EQ(r.drv.ReadChannelFeedback(tle92466ed_test::NotAnEnumerator<Channel>(6)).error,
           Error::InvalidArgument);
}

TEST_CASE(
    "Feedback with no measurement period (TP_MANT = 0) is NotReady, never 0 mA (KB-CPP-127)") {
  const std::array<uint32_t, 5> words = {0U, 0U, 0U, 0U, 0U};
  CHECK_EQ(tle92466ed::detail::DecodeFeedback(words, tle92466ed::kInternalClockHz).error,
           Error::NotReady);
  // A channel with a dither clock of 0 never publishes (section 4.4.2).
  Config c = tle92466ed_test::TwoChannels();
  c.channels[1].dither.mant = 0;
  Rig<MinimalPort> r(c);
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.EnableChannel(Channel::Ch1).ok());
  REQUIRE(r.drv.SetCurrent(Channel::Ch1, 100'000).ok());
  r.port.DelayUs(50'000);
  CHECK_EQ(r.drv.ReadChannelFeedback(Channel::Ch1).error, Error::NotReady);
}

TEST_CASE("Feedback of a negative current is signed (Equation 22)") {
  Rig<MinimalPort> r;
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 10'000).ok());
  r.port.state.chip.load_ua[0] = -20'000;
  r.port.DelayUs(1500);
  const auto f = r.drv.ReadChannelFeedback(Channel::Ch0);
  REQUIRE(f.ok());
  CHECK(f.value.average_current_ua < -15'000);
  CHECK(f.value.min_current_ua < 0);
}

TEST_CASE("RunBist: Config Mode only; result read; SFF_BIST and its REG_ECC_ERR cleared (4.9.8)") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  auto b = r.drv.RunBist();
  REQUIRE(b.ok());
  CHECK(b.value.passed && b.value.uncorrectable_tested && b.value.correctable_tested);
  CHECK_EQ(r.port.state.chip.Peek(chip::kSffBist), 0U);
  CHECK_EQ(r.port.state.chip.Peek(chip::kGlobalDiag2), 0U);
  CHECK(r.drv.EnterMissionMode().ok());
  CHECK_EQ(r.drv.RunBist().error, Error::WrongState);
}

TEST_CASE("RunBist: a failed test is data, a test that never ends is Timeout") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.bist_fails = true;
  const auto b = r.drv.RunBist();
  CHECK(b.ok() && !b.value.passed);
  r.port.state.chip.bist_never_done = true;
  CHECK_EQ(r.drv.RunBist().error, Error::Timeout);
  CHECK_EQ(r.port.state.chip.Peek(chip::kSffBist), 0U); // still cleared
}

TEST_CASE("ReadChipId reads CHIPID0..2") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  const auto id = r.drv.ReadChipId();
  REQUIRE(id.ok());
  const tle92466ed::ChipId want{{0x1234U, 0x5678U, 0x9ABCU}};
  CHECK(id.value == want);
}

TEST_CASE(
    "ReadRegister: one access, no retry, any listed register; diagnosis reads feed the latch") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  const auto icvid = r.drv.ReadRegister(chip::kIcvid);
  REQUIRE(icvid.ok());
  CHECK_EQ(icvid.value, 0xC1FEU);
  CHECK_EQ(r.drv.GetLastReply() & 0xFFFFU, 0xC1FEU);
  CHECK_EQ(r.drv.ReadRegister(0x0001U).error, Error::InvalidArgument);
  CHECK_EQ(r.drv.ReadRegister(0x0248U).error, Error::InvalidArgument);
  const auto fb = r.drv.ReadRegister(static_cast<uint16_t>(chip::kBase[0] + chip::kFbIntThresh));
  CHECK(fb.ok());
  r.port.state.chip.SetGlobalFault(chip::kVbatUv);
  r.port.state.chip.SetChannelError(2, chip::kOl);
  r.port.state.chip.SetChannelWarning(3, chip::kOtw);
  CHECK(r.drv.ReadRegister(chip::kGlobalDiag0).ok());
  CHECK(r.drv.ReadRegister(chip::kDiagErr0 + 1U).ok());
  CHECK(r.drv.ReadRegister(chip::kDiagWarn0 + 1U).ok());
  CHECK(r.drv.GetLatchedFaults().Has(GlobalFault::VbatUndervoltage));
  CHECK(r.drv.GetLatchedFaults().Of(Channel::Ch2).open_load);
  CHECK(r.drv.GetLatchedFaults().Of(Channel::Ch3).overtemperature_warning);
  // A corrupted reply is reported, not retried.
  const auto before = r.drv.GetBusDiagnostics().retries;
  r.port.state.corrupt_transfer = r.port.state.transfers + 1;
  CHECK_EQ(r.drv.ReadRegister(chip::kIcvid).error, Error::Crc);
  CHECK_EQ(r.drv.GetBusDiagnostics().retries, before);
}

TEST_CASE("Faults decoders: Has() and Of() on each bit, Any() leaves events out") {
  tle92466ed::Faults f{};
  f.global[0] = chip::kPorEvent | chip::kResEvent;
  f.warnings[0] = 0x1010U;
  CHECK(!f.Any());
  f.global[1] = 0x8000U;
  CHECK(f.Has(GlobalFault::MonitorAdc) && f.Any());
  f.errors[2] = static_cast<uint16_t>(chip::kOlsg << 8U);
  CHECK(f.Of(Channel::Ch5).olsg && !f.Of(Channel::Ch4).olsg);
  f.warnings[1] = chip::kPwmRegWarn | chip::kOlsgWarn;
  CHECK(f.Of(Channel::Ch2).pwm_regulation && f.Of(Channel::Ch2).olsg_warning);
}
