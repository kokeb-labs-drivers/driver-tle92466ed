/**
 * @file test_channels.cpp
 * @brief Channel enables, setpoints, dither, the OFF-state overcurrent check and the DRV pins.
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

TEST_CASE("Enables: one channel at a time, the others kept (KB-CPP-121)") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  CHECK(r.drv.EnableChannel(Channel::Ch0, Verify::ReadBack).ok());
  CHECK(r.drv.EnableChannel(Channel::Ch1, Verify::ReadBack).ok());
  CHECK_EQ(r.port.state.chip.Peek(chip::kChCtrl) & 0x3FU, 0x03U);
  CHECK(r.drv.DisableChannel(Channel::Ch0).ok());
  CHECK_EQ(r.port.state.chip.Peek(chip::kChCtrl) & 0x3FU, 0x02U);
  CHECK_EQ(r.drv.GetEnabledChannels(), 0x02U);
  CHECK(r.port.state.chip.MissionMode()); // OP_MODE kept in every write
}

TEST_CASE("Enables are refused, with nothing sent, outside their state or on an unloaded channel") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  const int before = r.port.state.transfers;
  CHECK_EQ(r.drv.EnableChannel(Channel::Ch0).error, Error::WrongState); // Config Mode
  REQUIRE(r.drv.EnterMissionMode().ok());
  const int mission = r.port.state.transfers;
  CHECK_EQ(r.drv.EnableChannel(Channel::Ch2).error, Error::InvalidArgument); // not loaded
  CHECK_EQ(r.drv.SetEnabledChannels(0x40U).error, Error::InvalidArgument);
  CHECK_EQ(r.drv.SetEnabledChannels(0x01U, tle92466ed_test::NotAnEnumerator<Verify>(5)).error,
           Error::InvalidArgument);
  CHECK_EQ(r.drv.EnableChannel(tle92466ed_test::NotAnEnumerator<Channel>(6)).error,
           Error::InvalidArgument);
  CHECK_EQ(r.drv.DisableChannel(tle92466ed_test::NotAnEnumerator<Channel>(6)).error,
           Error::InvalidArgument);
  CHECK_EQ(r.port.state.transfers - mission, 0);
  CHECK(before < mission);
  CHECK(r.drv.SetEnabledChannels(0U).ok()); // switching off is allowed
}

TEST_CASE("A slave channel cannot be enabled or driven (4.5.2)") {
  Config c = tle92466ed_test::TwoChannels();
  c.parallel[0] = true; // CH0 master, CH3 slave
  Rig<FullPort> r(c);
  REQUIRE(r.StartMission());
  CHECK_EQ(r.drv.EnableChannel(Channel::Ch3).error, Error::WrongState);
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch3, 1000).error, Error::WrongState);
  CHECK_EQ(r.drv.SetDither(Channel::Ch3, {}).error, Error::WrongState);
  CHECK_EQ(r.drv.ReadChannelFeedback(Channel::Ch3).error, Error::WrongState);
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch3).error, Error::WrongState);
  CHECK_EQ(r.drv.ConfigureChannel(Channel::Ch3, {}).error, Error::WrongState);
}

TEST_CASE("A channel kept off by a latched fault is WriteMismatch with Verify::ReadBack (4.9.2)") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  r.port.state.chip.SetChannelError(0, chip::kOlsg);
  CHECK_EQ(r.drv.EnableChannel(Channel::Ch0, Verify::ReadBack).error, Error::WriteMismatch);
  CHECK_EQ(r.drv.GetEnabledChannels(), 0U); // the copy follows the chip
  CHECK(r.drv.ClearChannelFaults(Channel::Ch0).ok());
  CHECK(r.drv.EnableChannel(Channel::Ch0, Verify::ReadBack).ok());
}

TEST_CASE("SetCurrent: rounded to TARGET, the programmed value returned (KB-CPP-126)") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  const auto s = r.drv.SetCurrent(Channel::Ch0, 500'000, Verify::ReadBack);
  REQUIRE(s.ok());
  CHECK_EQ(r.port.state.chip.ChannelReg(0, chip::kSetpoint), 8192U); // 0.5 A * 32767 / 2 A
  CHECK_EQ(s.value, 500'015U);
  CHECK_EQ(r.drv.GetCurrentSetpoint(Channel::Ch0), 500'015U);
  CHECK_EQ(r.drv.GetCurrentSetpoint(tle92466ed_test::NotAnEnumerator<Channel>(9)), 0U);
  CHECK(r.drv.SetCurrent(Channel::Ch0, tle92466ed::kMaxSetpointUa).ok());
  CHECK(r.drv.SetCurrent(Channel::Ch0, 0).ok());
}

TEST_CASE("SetCurrent: outside the operating range is InvalidArgument, never clamped") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  const int before = r.port.state.transfers;
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, tle92466ed::kMaxSetpointUa + 1U).error,
           Error::InvalidArgument);
  CHECK_EQ(r.drv.SetCurrent(tle92466ed_test::NotAnEnumerator<Channel>(6), 1000).error,
           Error::InvalidArgument);
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 1000, tle92466ed_test::NotAnEnumerator<Verify>(3)).error,
           Error::InvalidArgument);
  CHECK_EQ(r.port.state.transfers, before);
}

TEST_CASE(
    "SetCurrent with dither: peak up to 1.8 A, and the setpoint above the amplitude (4.7.1)") {
  Config c = tle92466ed_test::TwoChannels();
  c.channels[0].dither.steps = 10;
  c.channels[0].dither.step_size = 819; // 10 * 819 counts: 0.5 A amplitude
  Rig<MinimalPort> r(c);
  REQUIRE(r.Start());
  CHECK(r.drv.SetCurrent(Channel::Ch0, 700'000).ok());
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 1'400'000).error, Error::InvalidArgument); // 1.9 A peak
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 400'000).error, Error::InvalidArgument);   // below dither
  CHECK(r.drv.SetCurrent(Channel::Ch0, 0).ok());                                     // dither off
}

TEST_CASE("SetCurrent on a parallel master: 4 A full scale, up to 2.7 A") {
  Config c = tle92466ed_test::TwoChannels();
  c.parallel[1] = true; // CH1 master, CH2 slave
  Rig<MinimalPort> r(c);
  REQUIRE(r.Start());
  const auto s = r.drv.SetCurrent(Channel::Ch1, 2'000'000);
  REQUIRE(s.ok());
  CHECK_EQ(r.port.state.chip.ChannelReg(1, chip::kSetpoint), 16384U);
  CHECK_EQ(r.drv.GetCurrentSetpoint(Channel::Ch1), s.value);
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch1, 2'700'001).error, Error::InvalidArgument);
}

TEST_CASE("SetCurrent with Verify::ReadBack finds a SETPOINT the chip did not take") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.setpoint_ignored = true;
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 100'000, Verify::ReadBack).error, Error::WriteMismatch);
  // The copy keeps the larger of the old and the requested setpoint until a write succeeds.
  CHECK(r.drv.GetCurrentSetpoint(Channel::Ch0) > 0U);
  r.port.state.chip.setpoint_ignored = false;
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 0U, Verify::ReadBack).ok());
  CHECK_EQ(r.drv.GetCurrentSetpoint(Channel::Ch0), 0U);
}

TEST_CASE("SetDither: DITHER_CTRL written last, the update event (4.7.2)") {
  Rig<MinimalPort> r;
  REQUIRE(r.StartMission());
  tle92466ed::DitherConfig d = tle92466ed::CurrentControlChannel().dither;
  d.steps = 4;
  d.flat = 3;
  d.step_size = 100;
  const int before = r.port.state.transfers;
  CHECK(r.drv.SetDither(Channel::Ch1, d, Verify::ReadBack).ok());
  int last = -1;
  for (int i = before; i < r.port.state.transfers; ++i) {
    if (r.port.state.IsWrite(i)) {
      last = i;
    }
  }
  CHECK_EQ(r.port.state.AddressOf(last), chip::kBase[1] + chip::kDitherCtrl);
  CHECK_EQ(r.port.state.chip.ChannelReg(1, chip::kDitherStep), 0x0403U);
  CHECK_EQ(r.drv.GetConfig().channels[1].dither.step_size, 100U);
  d.fast = tle92466ed::FastFeedback::HalfPeriod;
  d.flat = 2; // fast feedback needs FLAT above 2 (4.10.3)
  CHECK_EQ(r.drv.SetDither(Channel::Ch1, d).error, Error::InvalidArgument);
  d.flat = 3;
  CHECK_EQ(r.drv.SetDither(Channel::Ch1, d, tle92466ed_test::NotAnEnumerator<Verify>(4)).error,
           Error::InvalidArgument);
  CHECK_EQ(r.drv.SetDither(tle92466ed_test::NotAnEnumerator<Channel>(8), d).error,
           Error::InvalidArgument);
  REQUIRE(r.drv.SetCurrent(Channel::Ch1, 100'000).ok());
  d.steps = 100;
  d.step_size = 200; // 1.2 A amplitude over 0.1 A
  CHECK_EQ(r.drv.SetDither(Channel::Ch1, d).error, Error::InvalidArgument);
}

TEST_CASE("The OFF-state overcurrent check finds a short to the battery (4.9.6)") {
  Config c = tle92466ed_test::TwoChannels();
  c.channels[0].on_time_mant = 100; // t_OCon = 101 * 2^5 / 28 MHz = 115 us
  Rig<FullPort> r(c);
  REQUIRE(r.StartMission());
  r.port.state.chip.SetOvercurrentLoad(0, true);
  CHECK(r.drv.StartOvercurrentCheck(Channel::Ch0).ok());
  const auto f = r.drv.ReadFaults();
  REQUIRE(f.ok());
  CHECK(f.value.Of(Channel::Ch0).overcurrent);
  CHECK_EQ(r.port.state.chip.ChannelReg(0, chip::kChConfig) & chip::kOcDiagEn, 0);
  // Needs a loaded channel with an on-time of at most 300 ms, in Mission Mode.
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch1).error, Error::InvalidArgument); // TON 0
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch2).error, Error::InvalidArgument); // unloaded
  CHECK_EQ(r.drv.StartOvercurrentCheck(tle92466ed_test::NotAnEnumerator<Channel>(7)).error,
           Error::InvalidArgument);
  REQUIRE(r.drv.EnterConfigMode().ok());
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch0).error, Error::WrongState);
}

TEST_CASE("The OFF-state overcurrent on-time above 300 ms is InvalidArgument") {
  Config c = tle92466ed_test::TwoChannels();
  c.channels[0].on_time_mant = 1023;
  c.channels[0].dither.exp = 15; // 1024 * 2^15 / 28 MHz = 1.2 s
  Rig<FullPort> r(c);
  REQUIRE(r.StartMission());
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch0).error, Error::InvalidArgument);
}

TEST_CASE("SetDrivePin drives DRV0 and DRV1 only; needs GpioSet") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  CHECK(r.drv.SetDrivePin(tle92466ed::CtrlPin::Drv1, true).ok());
  CHECK(r.port.state.chip.Drv(1));
  CHECK(r.drv.SetDrivePin(tle92466ed::CtrlPin::Drv1, false).ok());
  CHECK(!r.port.state.chip.Drv(1));
  CHECK_EQ(r.drv.SetDrivePin(tle92466ed::CtrlPin::Reset, true).error, Error::InvalidArgument);
  r.port.state.fail_pin = r.port.state.pin_calls;
  CHECK_EQ(r.drv.SetDrivePin(tle92466ed::CtrlPin::Drv0, true).error, Error::Bus);
  Rig<MinimalPort> m;
  REQUIRE(m.Start());
  CHECK_EQ(m.drv.SetDrivePin(tle92466ed::CtrlPin::Drv0, true).error, Error::Unsupported);
}

TEST_CASE("Mission Mode with EN that cannot be driven is Bus, OP_MODE not written") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.fail_pin = r.port.state.pin_calls;
  const int before = r.port.state.transfers;
  CHECK_EQ(r.drv.EnterMissionMode().error, Error::Bus);
  CHECK_EQ(r.port.state.CountWrites(chip::kChCtrl, before), 0);
}
