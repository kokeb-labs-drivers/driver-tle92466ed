/**
 * @file test_tle92466ed.cpp
 * @brief Start-up, lifecycle, operation states and configuration against the chip model.
 */
#include <cstdint>

#include "mock_tle92466ed.hpp"
#include "test_harness.hpp"
#include "test_rig.hpp"
#include "tle92466ed.hpp"

using tle92466ed::Channel;
using tle92466ed::Config;
using tle92466ed::Error;
using tle92466ed::Verify;
using tle92466ed_test::FullPort;
using tle92466ed_test::MinimalPort;
using tle92466ed_test::Rig;
namespace chip = tle92466ed_test::chip;

// -------------------------------------------------------------------------------------------------
// Start-up
// -------------------------------------------------------------------------------------------------

TEST_CASE(
    "Start-up: EN low, RESN pulsed, INIT_DONE, identity, every register; ends in Config Mode") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  const auto& m = r.port.state.chip;
  CHECK(!m.MissionMode());
  CHECK(!m.En());                                          // EN stays low in Config Mode
  CHECK(r.port.reset_low_us >= tle92466ed::kResetPulseUs); // the pulse
  CHECK_EQ(r.drv.GetIcVersion(), chip::kVersionA13);
  CHECK_EQ(m.Peek(chip::kGlobalConfig), chip::kGlobalConfigReset); // Config{} keeps reset values
  CHECK_EQ(m.Peek(chip::kWdReload), 1U);
  CHECK_EQ(m.Peek(chip::kVbatTh), chip::kVbatThReset);
  // CH0 and CH1 loaded: their errors and warnings reach FAULTN; the others never do (KB-CPP-131).
  CHECK_EQ(m.Peek(chip::kFaultMask0), 0xC003U);
  CHECK_EQ(m.Peek(chip::kFaultMask1), 0x7003U);
  CHECK_EQ(m.ChannelReg(0, chip::kMode), 1U);
  CHECK_EQ(m.ChannelReg(0, chip::kPeriod), 0x07DBU);
  CHECK_EQ(m.ChannelReg(0, chip::kDitherClkDiv), 0x176BU);
  CHECK_EQ(m.ChannelReg(0, chip::kChConfig), chip::kChConfigReset);
  CHECK_EQ(m.ChannelReg(2, chip::kChConfig), 0xC003U); // unloaded: OFF-state diagnosis off
  CHECK(r.drv.IsResetWatchArmed());
  CHECK(r.drv.GetStartupFaults().global[0] == chip::kGlobalDiag0Reset); // the reset's own events
  CHECK(!r.drv.GetLatchedFaults().Any());
  CHECK_EQ(r.port.error_lines, 0);
}

TEST_CASE("Start-up without RESN waits t_POR and puts the chip in Config Mode first") {
  Rig<MinimalPort> r;
  // A chip left in Mission Mode by an earlier run: the first write must end it (section 4.3.1).
  REQUIRE(r.Start());
  REQUIRE(r.drv.EnterMissionMode().ok());
  Rig<MinimalPort> again;
  again.port.state.chip = r.port.state.chip;
  REQUIRE(again.port.state.chip.MissionMode());
  REQUIRE(again.Start());
  CHECK(again.port.delayed_us >= tle92466ed::kPowerOnResetUs);
  CHECK(!again.port.state.chip.MissionMode());
  int first_write = -1;
  for (int i = 0; i < again.port.state.transfers; ++i) {
    if (again.port.state.IsWrite(i)) {
      first_write = i;
      break;
    }
  }
  REQUIRE(first_write >= 0);
  CHECK_EQ(again.port.state.AddressOf(first_write), chip::kChCtrl);
  CHECK_EQ(again.port.state.mosi[static_cast<std::size_t>(first_write)] & 0xFFFFU, 0U);
}

TEST_CASE("Start-up: a second call returns Ok with no I/O") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  const int before = r.port.state.transfers;
  CHECK(r.drv.EnsureInitialized().ok());
  CHECK_EQ(r.port.state.transfers, before);
  CHECK_EQ(r.port.state.bus_up_calls, 1);
}

TEST_CASE("Start-up: WD_RELOAD is written before GLOBAL_CONFIG enables the SPI watchdog (5.2.2)") {
  Config c = tle92466ed_test::TwoChannels();
  c.spi_watchdog = true;
  c.watchdog_reload = tle92466ed::WatchdogReloadCode(100U, tle92466ed::kInternalClockHz).value;
  Rig<MinimalPort> r(c);
  REQUIRE(r.Start());
  int reload = -1;
  int global = -1;
  for (int i = 0; i < r.port.state.transfers; ++i) {
    if (r.port.state.IsWrite(i) && (r.port.state.AddressOf(i) == chip::kWdReload) && (reload < 0)) {
      reload = i;
    }
    if (r.port.state.IsWrite(i) && (r.port.state.AddressOf(i) == chip::kGlobalConfig)) {
      global = i;
    }
  }
  CHECK((reload >= 0) && (reload < global));
  CHECK_EQ(r.port.state.chip.Peek(chip::kWdReload), 170U);
  CHECK((r.port.state.chip.Peek(chip::kGlobalConfig) & chip::kSpiWdEn) != 0U);
}

TEST_CASE("Start-up: an external clock programs CLK_DIV from Table 10 and waits for INIT_DONE") {
  Config c = tle92466ed_test::TwoChannels();
  c.clock = tle92466ed::ClockSource::External;
  c.external_clock_khz = 4000U;
  Rig<MinimalPort> r(c);
  r.port.state.chip.init_delay_us = 300;
  REQUIRE(r.Start());
  CHECK_EQ(r.port.state.chip.Peek(chip::kClkDiv), 0x8838U);
  CHECK(r.port.delayed_us >= (tle92466ed::kPowerOnResetUs + 300U));
}

TEST_CASE("Start-up: the configuration is checked before any I/O") {
  Config c{};
  c.watchdog_reload = 0;
  Rig<MinimalPort> r(c);
  CHECK_EQ(r.drv.EnsureInitialized().error, Error::InvalidArgument);
  CHECK_EQ(r.port.state.transfers, 0);
  CHECK_EQ(r.port.state.bus_up_calls, 0);
}

TEST_CASE("Start-up: the port's bus failing is Bus") {
  Rig<FullPort> r;
  r.port.state.bus_up_ok = false;
  CHECK_EQ(r.drv.EnsureInitialized().error, Error::Bus);
  CHECK(!r.drv.IsInitialized());
  CHECK(r.port.error_lines > 0);
}

TEST_CASE("Start-up: a failed EN or RESN write is Bus") {
  for (int pin = 0; pin < 3; ++pin) {
    Rig<FullPort> r;
    r.port.state.fail_pin = pin;
    CHECK_EQ(r.drv.EnsureInitialized().error, Error::Bus);
  }
}

TEST_CASE("Start-up: another manufacturer or an unknown version is DeviceIdMismatch") {
  Rig<FullPort> a;
  a.port.state.chip.manufacturer = 0xC2;
  CHECK_EQ(a.drv.EnsureInitialized().error, Error::DeviceIdMismatch);
  Rig<FullPort> b;
  b.port.state.chip.version = 0xFD;
  CHECK_EQ(b.drv.EnsureInitialized().error, Error::DeviceIdMismatch);
  Rig<FullPort> c;
  c.port.state.chip.version = chip::kVersionA11;
  CHECK(c.Start());
  CHECK_EQ(c.drv.GetIcVersion(), chip::kVersionA11);
}

TEST_CASE("Start-up: INIT_DONE that never comes is Timeout after Config::init_timeout_us") {
  Config c{};
  c.init_timeout_us = 1000;
  Rig<FullPort> r(c);
  r.port.state.chip.init_never = true;
  CHECK_EQ(r.drv.EnsureInitialized().error, Error::Timeout);
  const uint64_t polls_us =
      r.port.delayed_us - tle92466ed::kResetPulseUs - tle92466ed::kResetReleaseUs;
  CHECK(polls_us >= 1000U);
  CHECK(polls_us <= 1100U);
}

TEST_CASE("Start-up: INIT_DONE late is waited for, polled every kPollIntervalUs") {
  Rig<FullPort> r;
  r.port.state.chip.init_delay_us = 2000;
  REQUIRE(r.Start());
  CHECK(r.port.state.CountReads(chip::kFbStat) > 10);
}

TEST_CASE("Start-up: an unprogrammed OTP is DeviceFault (4.9.7)") {
  Rig<FullPort> r;
  r.port.state.chip.otp_virgin = true;
  CHECK_EQ(r.drv.EnsureInitialized().error, Error::DeviceFault);
  CHECK(r.drv.GetStartupFaults().Has(tle92466ed::GlobalFault::OtpVirgin));
}

TEST_CASE("Start-up: faults latched before the start are reported and cleared (KB-CPP-058)") {
  Rig<MinimalPort> r;
  r.port.state.chip.SetDataFault(chip::kOtpEccErr);
  r.port.state.chip.SetChannelError(1, chip::kOc);
  r.port.state.chip.SetGlobalFault(chip::kVbatUv);
  REQUIRE(r.Start());
  const auto& f = r.drv.GetStartupFaults();
  CHECK(f.Has(tle92466ed::GlobalFault::OtpEcc));
  CHECK(f.Has(tle92466ed::GlobalFault::VbatUndervoltage));
  CHECK(f.Of(Channel::Ch1).overcurrent);
  // OC and OTP_ECC_ERR needed their second clear (4.9.6, 4.9.7).
  CHECK_EQ(r.port.state.chip.Peek(chip::kGlobalDiag2), 0U);
  CHECK_EQ(r.port.state.chip.Peek(chip::kDiagErr0), 0U);
  CHECK_EQ(r.port.state.chip.Peek(chip::kGlobalDiag0), 0U);
}

TEST_CASE("Start-up: a GLOBAL_CONFIG that does not read back is WriteMismatch (KB-CPP-117)") {
  Config c = tle92466ed_test::TwoChannels();
  c.vio = tle92466ed::VioLevel::V3V3;
  Rig<FullPort> r(c);
  r.port.state.chip.global_config_sticks = true;
  CHECK_EQ(r.drv.EnsureInitialized().error, Error::WriteMismatch);
}

TEST_CASE("Start-up: events that do not clear leave the reset watch off, with a warning") {
  Rig<FullPort> r;
  r.port.state.chip.events_stick = true;
  REQUIRE(r.Start());
  CHECK(!r.drv.IsResetWatchArmed());
  CHECK(r.port.warning_lines > 0);
  CHECK(r.drv.ReadStatus().ok()); // no false UnexpectedReset
}

TEST_CASE("Start-up: a failed start can be retried from the beginning") {
  Rig<FullPort> r;
  r.port.state.chip.init_never = true;
  CHECK(!r.drv.EnsureInitialized().ok());
  r.port.state.chip.init_never = false;
  CHECK(r.Start());
}

// -------------------------------------------------------------------------------------------------
// Before the start
// -------------------------------------------------------------------------------------------------

TEST_CASE("Before EnsureInitialized every I/O call is NotInitialized and sends nothing") {
  Rig<FullPort> r;
  auto& d = r.drv;
  CHECK_EQ(d.EnterMissionMode().error, Error::NotInitialized);
  CHECK_EQ(d.EnterConfigMode().error, Error::NotInitialized);
  CHECK_EQ(d.ReadChannelControl().error, Error::NotInitialized);
  CHECK_EQ(d.SetEnabledChannels(1).error, Error::NotInitialized);
  CHECK_EQ(d.EnableChannel(Channel::Ch0).error, Error::NotInitialized);
  CHECK_EQ(d.DisableChannel(Channel::Ch0).error, Error::NotInitialized);
  CHECK_EQ(d.EnableChannel(tle92466ed_test::NotAnEnumerator<Channel>(9)).error,
           Error::NotInitialized);
  CHECK_EQ(d.DisableChannel(tle92466ed_test::NotAnEnumerator<Channel>(9)).error,
           Error::NotInitialized);
  CHECK_EQ(d.SetCurrent(Channel::Ch0, 1000).error, Error::NotInitialized);
  CHECK_EQ(d.SetDither(Channel::Ch0, {}).error, Error::NotInitialized);
  CHECK_EQ(d.StartOvercurrentCheck(Channel::Ch0).error, Error::NotInitialized);
  CHECK_EQ(d.SetDrivePin(tle92466ed::CtrlPin::Drv0, true).error, Error::NotInitialized);
  CHECK_EQ(d.ReloadWatchdog().error, Error::NotInitialized);
  CHECK_EQ(d.ReadStatus().error, Error::NotInitialized);
  CHECK_EQ(d.ReadFaults().error, Error::NotInitialized);
  CHECK_EQ(d.ClearFaults().error, Error::NotInitialized);
  CHECK_EQ(d.ClearChannelFaults(Channel::Ch0).error, Error::NotInitialized);
  CHECK_EQ(d.ReadFaultPin().error, Error::NotInitialized);
  CHECK_EQ(d.ReadPinStatus().error, Error::NotInitialized);
  CHECK_EQ(d.ReadSupply().error, Error::NotInitialized);
  CHECK_EQ(d.ReadChannelFeedback(Channel::Ch0).error, Error::NotInitialized);
  CHECK_EQ(d.RunBist().error, Error::NotInitialized);
  CHECK_EQ(d.ReadChipId().error, Error::NotInitialized);
  CHECK_EQ(d.ReadRegister(chip::kIcvid).error, Error::NotInitialized);
  CHECK_EQ(d.HardwareReset().error, Error::NotInitialized);
  CHECK_EQ(d.SetParallel(tle92466ed::ParallelPair::Ch0Ch3, true).error, Error::NotInitialized);
  CHECK_EQ(r.port.state.pin_calls, 0);
  CHECK(d.Deinitialize().ok()); // only EN low, the safe level: no SPI
  CHECK_EQ(r.port.state.transfers, 0);
  CHECK_EQ(r.port.state.pin_calls, 1);
  CHECK(!r.port.state.chip.En());
}

TEST_CASE("Before the start, ApplyConfig and ConfigureChannel only store the configuration") {
  Rig<MinimalPort> r(Config{});
  Config c{};
  c.vio = tle92466ed::VioLevel::V3V3;
  CHECK(r.drv.ApplyConfig(c).ok());
  CHECK(r.drv.ConfigureChannel(Channel::Ch3, tle92466ed::CurrentControlChannel()).ok());
  CHECK_EQ(r.port.state.transfers, 0);
  CHECK(r.drv.GetConfig().channels[3].loaded);
  REQUIRE(r.Start());
  CHECK_EQ(r.port.state.chip.Peek(chip::kGlobalConfig), 0x0005U);
  CHECK_EQ(r.port.state.chip.ChannelReg(3, chip::kMode), 1U);
}

// -------------------------------------------------------------------------------------------------
// Operation states
// -------------------------------------------------------------------------------------------------

TEST_CASE("Mission Mode: EN high, OP_MODE set, read back on request") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  CHECK(r.drv.EnterMissionMode().ok());
  CHECK(r.port.state.chip.MissionMode());
  CHECK(r.port.state.chip.En());
  CHECK(r.drv.IsMissionMode());
  const auto ctrl = r.drv.ReadChannelControl();
  CHECK(ctrl.ok() && ((ctrl.value & chip::kOpMode) != 0U));
}

TEST_CASE("Mission Mode is refused, with nothing written, while a fault of Figure 4 holds") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.SetGlobalFault(chip::kVddUv);
  const int before = r.port.state.transfers;
  CHECK_EQ(r.drv.EnterMissionMode().error, Error::MissionBlocked);
  CHECK_EQ(r.port.state.CountWrites(chip::kChCtrl, before), 0);
  CHECK(!r.port.state.chip.En());
  CHECK(r.drv.ClearFaults().ok());
  CHECK(r.drv.EnterMissionMode().ok());
}

TEST_CASE("Mission Mode before INIT_DONE is MissionBlocked") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.init_never = true;
  CHECK_EQ(r.drv.EnterMissionMode().error, Error::MissionBlocked);
}

TEST_CASE("Mission Mode the chip refuses is MissionBlocked, and the copy says Config Mode") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.refuse_mission = true;
  CHECK_EQ(r.drv.EnterMissionMode().error, Error::MissionBlocked);
  CHECK(!r.drv.IsMissionMode());
  CHECK_EQ(r.drv.EnableChannel(Channel::Ch0).error, Error::WrongState);
}

TEST_CASE("Config Mode: every channel off, then EN low") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  CHECK(r.drv.EnterConfigMode().ok());
  CHECK(!r.port.state.chip.MissionMode());
  CHECK_EQ(r.port.state.chip.Peek(chip::kChCtrl) & 0x3FU, 0U);
  CHECK(!r.port.state.chip.En());
  CHECK_EQ(r.drv.GetEnabledChannels(), 0U);
}

TEST_CASE("ReadChannelControl: the copy follows a chip that left Mission Mode on a supply fault") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.EnableChannel(Channel::Ch1).ok());
  r.port.state.chip.SetGlobalFault(chip::kVioUv); // 4.1.2: Config Mode, EN_CH cleared
  const auto ctrl = r.drv.ReadChannelControl();
  REQUIRE(ctrl.ok());
  CHECK_EQ(ctrl.value & chip::kOpMode, 0);
  CHECK(!r.drv.IsMissionMode());
  CHECK_EQ(r.drv.GetEnabledChannels(), 0U);
}

TEST_CASE("The SPI watchdog: reloads keep Mission Mode, a missed reload ends it (5.2.2)") {
  Config c = tle92466ed_test::TwoChannels();
  c.spi_watchdog = true;
  c.watchdog_reload = tle92466ed::WatchdogReloadCode(10U, tle92466ed::kInternalClockHz).value;
  Rig<FullPort> r(c);
  REQUIRE(r.StartMission());
  for (int i = 0; i < 20; ++i) {
    r.port.DelayUs(5000);
    REQUIRE(r.drv.ReloadWatchdog().ok());
  }
  CHECK(r.port.state.chip.MissionMode());
  r.port.DelayUs(20000);
  CHECK(!r.port.state.chip.MissionMode());
  const auto st = r.drv.ReadStatus();
  CHECK(st.ok() && st.value.spi_watchdog_error);
  CHECK_EQ(r.drv.EnterMissionMode().error, Error::MissionBlocked);
  CHECK(r.drv.ReloadWatchdog().ok()); // a non-zero WD_TIME clears SPI_WD_ERR
  CHECK(r.drv.ClearFaults().ok());
  CHECK(r.drv.EnterMissionMode().ok());
}

// -------------------------------------------------------------------------------------------------
// Configuration
// -------------------------------------------------------------------------------------------------

TEST_CASE("ApplyConfig: Config Mode only, checked first, written whole") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  Config c = tle92466ed_test::TwoChannels();
  c.vbat_uv_code = 40;
  CHECK(r.drv.ApplyConfig(c).ok());
  CHECK_EQ(r.port.state.chip.Peek(chip::kVbatTh), 0xFF28U);
  c.vbat_ov_code = 10; // not above UV
  CHECK_EQ(r.drv.ApplyConfig(c).error, Error::InvalidArgument);
  REQUIRE(r.drv.EnterMissionMode().ok());
  const int before = r.port.state.transfers;
  CHECK_EQ(r.drv.ApplyConfig(tle92466ed_test::TwoChannels()).error, Error::WrongState);
  CHECK_EQ(r.port.state.transfers, before);
}

TEST_CASE("ConfigureChannel: Config Mode only; FAULT_MASK follows `loaded`; read back on request") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  auto cc = tle92466ed::CurrentControlChannel();
  cc.slew = tle92466ed::SlewRate::VPerUs2p5;
  cc.diag.ol_fixed = 10;
  CHECK(r.drv.ConfigureChannel(Channel::Ch4, cc, Verify::ReadBack).ok());
  CHECK_EQ(r.port.state.chip.ChannelReg(4, chip::kChConfig), (10U << 7U) | 1U);
  CHECK_EQ(r.port.state.chip.Peek(chip::kFaultMask0) & 0x3FU, 0x13U);
  cc.icc.auto_limit = 2; // not above MIN_INT_THRESH + 3
  CHECK_EQ(r.drv.ConfigureChannel(Channel::Ch4, cc).error, Error::InvalidArgument);
  CHECK_EQ(r.drv.ConfigureChannel(tle92466ed_test::NotAnEnumerator<Channel>(6), cc).error,
           Error::InvalidArgument);
  CHECK_EQ(r.drv
               .ConfigureChannel(Channel::Ch4, tle92466ed::CurrentControlChannel(),
                                 tle92466ed_test::NotAnEnumerator<Verify>(2))
               .error,
           Error::InvalidArgument);
  REQUIRE(r.drv.EnterMissionMode().ok());
  CHECK_EQ(r.drv.ConfigureChannel(Channel::Ch4, tle92466ed::CurrentControlChannel()).error,
           Error::WrongState);
}

TEST_CASE("ConfigureChannel with Verify::ReadBack finds a register the chip did not take") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.setpoint_ignored = true;
  auto cc = tle92466ed::CurrentControlChannel();
  cc.icc.auto_limit_disable = true; // SETPOINT is then not 0
  CHECK_EQ(r.drv.ConfigureChannel(Channel::Ch0, cc, Verify::ReadBack).error, Error::WriteMismatch);
  CHECK(!r.drv.GetConfig().channels[0].icc.auto_limit_disable); // the copy is unchanged
}

TEST_CASE("SetParallel: Config Mode only; splitting rewrites the slave (4.5.2)") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  CHECK(r.drv.SetParallel(tle92466ed::ParallelPair::Ch0Ch3, true).ok());
  CHECK((r.port.state.chip.Peek(chip::kChCtrl) & chip::kPar03) != 0U);
  CHECK(r.drv.SetParallel(tle92466ed::ParallelPair::Ch1Ch2, true).ok());
  CHECK(r.drv.SetParallel(tle92466ed::ParallelPair::Ch1Ch2, false).ok());
  CHECK_EQ(
      r.drv.SetParallel(tle92466ed_test::NotAnEnumerator<tle92466ed::ParallelPair>(3), true).error,
      Error::InvalidArgument);
  const int before = r.port.state.transfers;
  CHECK(r.drv.SetParallel(tle92466ed::ParallelPair::Ch0Ch3, false).ok());
  CHECK(r.port.state.CountWrites(chip::kBase[3] + chip::kMode, before) == 1);
  REQUIRE(r.drv.EnterMissionMode().ok());
  CHECK_EQ(r.drv.SetParallel(tle92466ed::ParallelPair::Ch0Ch3, true).error, Error::WrongState);
}

TEST_CASE("SetParallel refuses a pair whose slave is declared loaded") {
  Config c = tle92466ed_test::TwoChannels();
  c.channels[3] = tle92466ed::CurrentControlChannel();
  Rig<MinimalPort> r(c);
  REQUIRE(r.Start());
  CHECK_EQ(r.drv.SetParallel(tle92466ed::ParallelPair::Ch0Ch3, true).error, Error::InvalidArgument);
}

TEST_CASE("Deinitialize: every channel off, EN low, uninitialised; again does nothing") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  CHECK(r.drv.Deinitialize().ok());
  CHECK(!r.drv.IsInitialized());
  CHECK(!r.port.state.chip.MissionMode());
  CHECK(!r.port.state.chip.En());
  const int before = r.port.state.transfers;
  CHECK(r.drv.Deinitialize().ok());
  CHECK_EQ(r.port.state.transfers, before);
  CHECK_EQ(r.drv.ReadStatus().error, Error::NotInitialized);
  CHECK(r.Start());
}

TEST_CASE("Deinitialize reports a failed EN write, and still ends uninitialised") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.fail_pin = r.port.state.pin_calls;
  CHECK_EQ(r.drv.Deinitialize().error, Error::Bus);
  CHECK(!r.drv.IsInitialized());
}

TEST_CASE("HardwareReset: RESN pulsed again, everything rewritten; needs GpioSet") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  CHECK(r.drv.HardwareReset().ok());
  CHECK(r.drv.IsInitialized());
  CHECK(!r.port.state.chip.MissionMode());
  r.port.state.chip.init_never = true;
  CHECK_EQ(r.drv.HardwareReset().error, Error::Timeout);
  CHECK(!r.drv.IsInitialized());
  Rig<MinimalPort> m;
  REQUIRE(m.Start());
  CHECK_EQ(m.drv.HardwareReset().error, Error::Unsupported);
}

TEST_CASE("An unexpected reset is seen by the next access and leaves the driver uninitialised") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  r.port.state.chip.PowerOn(); // a brown-out: every register back to reset, POR_EVENT set
  // The first reply after the reset carries nothing (NoDevice); the FB_STAT anchors that follow
  // show POR_EVENT.
  Error e = Error::Ok;
  for (int i = 0; (i < 4) && (e != Error::UnexpectedReset); ++i) {
    e = r.drv.SetCurrent(Channel::Ch0, 100'000).error;
  }
  CHECK_EQ(e, Error::UnexpectedReset);
  CHECK(!r.drv.IsInitialized());
  CHECK(r.port.warning_lines > 0);
  CHECK(r.Start());
  CHECK(r.drv.IsResetWatchArmed());
}
