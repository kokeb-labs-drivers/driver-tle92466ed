/**
 * @file test_ambiguous_writes.cpp
 * @brief A write whose reply failed may have taken effect. Every copy the driver decides on without
 * I/O then holds the more dangerous of the two possible values, or is unknown, and a guard that
 * depends on an unknown value refuses with WrongState and no frame, until a validated read or a
 * successful write. Switching off and a setpoint of 0 are never refused.
 *
 * Each case first failed on the driver before its fix. Both write-reply readings run them
 * (tle92466ed_host_tests, tle92466ed_host_tests_reply_before_write).
 */
#include <cstdint>

#include "mock_tle92466ed.hpp"
#include "test_harness.hpp"
#include "test_rig.hpp"
#include "tle92466ed.hpp"

using tle92466ed::Channel;
using tle92466ed::Config;
using tle92466ed::Error;
using tle92466ed::ParallelPair;
using tle92466ed_test::FullPort;
using tle92466ed_test::Rig;
namespace chip = tle92466ed_test::chip;

namespace {

/// Two loaded channels; CH0 has an OFF-state overcurrent on-time.
Config OcConfig() {
  Config c = tle92466ed_test::TwoChannels();
  c.channels[0].on_time_mant = 100;
  return c;
}

uint32_t ChipEnables(const Rig<FullPort>& r) {
  return r.port.state.chip.Peek(chip::kChCtrl) & 0x3FU;
}

/// The reply of the next access's request: the anchor frame after it (0-based from now).
void CorruptReplyOfAccess(Rig<FullPort>& r, int access) {
  r.port.state.corrupt_transfer = r.port.state.transfers + (2 * access) + 1;
}

/// The chip never sees the request of the next access `access` (0-based from now).
void LoseRequestOfAccess(Rig<FullPort>& r, int access) {
  r.port.state.lose_transfer = r.port.state.transfers + (2 * access);
}

/// Every guard that refuses in Mission Mode sends nothing.
void CheckConfigOnlyRefused(Rig<FullPort>& r) {
  const int sent = r.port.state.transfers;
  CHECK_EQ(r.drv.ConfigureChannel(Channel::Ch0, tle92466ed_test::TwoChannels().channels[0]).error,
           Error::WrongState);
  CHECK_EQ(r.drv.SetParallel(ParallelPair::Ch4Ch5, true).error, Error::WrongState);
  CHECK_EQ(r.drv.ApplyConfig(OcConfig()).error, Error::WrongState);
  CHECK_EQ(r.drv.RunBist().error, Error::WrongState);
  CHECK_EQ(r.port.state.transfers, sent);
}

} // namespace

TEST_CASE("Ambiguous SETPOINT: the overcurrent check is WrongState with no frame") {
  Rig<FullPort> r(OcConfig());
  REQUIRE(r.StartMission());
  CorruptReplyOfAccess(r, 0); // the SETPOINT write reaches the chip, its reply does not
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 1'000'000).error, Error::Crc);
  REQUIRE(r.port.state.chip.ChannelReg(0, chip::kSetpoint) != 0U); // the chip took it
  CHECK(r.drv.GetCurrentSetpoint(Channel::Ch0) > 0U);              // the larger value is kept
  const int sent = r.port.state.transfers;
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch0).error, Error::WrongState);
  CHECK_EQ(r.port.state.transfers, sent);
  CHECK_EQ(ChipEnables(r), 0U); // no ON state at 1 A
  // A setpoint of 0 is never refused, and a confirmed one clears the doubt.
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 0U).ok());
  CHECK(r.drv.StartOvercurrentCheck(Channel::Ch0).ok());
}

TEST_CASE("Ambiguous SETPOINT, lost frame: the copy still keeps the larger value") {
  Rig<FullPort> r(OcConfig());
  REQUIRE(r.StartMission());
  LoseRequestOfAccess(r, 0); // the chip keeps 0; the driver cannot know
  CHECK(!r.drv.SetCurrent(Channel::Ch0, 1'000'000).ok());
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch0).error, Error::WrongState);
}

TEST_CASE("Ambiguous SETPOINT: the dither check uses the larger setpoint") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 600'000).ok());
  CorruptReplyOfAccess(r, 0);
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 1'400'000).error, Error::Crc);
  tle92466ed::DitherConfig d = tle92466ed::CurrentControlChannel().dither;
  d.steps = 10;
  d.flat = 3;
  d.step_size = 820; // about 0.5 A: fine over 0.6 A, over 1.8 A peak over 1.4 A
  const int sent = r.port.state.transfers;
  CHECK_EQ(r.drv.SetDither(Channel::Ch0, d).error, Error::InvalidArgument);
  CHECK_EQ(r.port.state.transfers, sent);
}

TEST_CASE("Lost CH_CTRL write: a channel asked on earlier still counts as on (wanted_ ORed)") {
  Rig<FullPort> r(OcConfig());
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 0U).ok());
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  REQUIRE(r.drv.EnableChannel(Channel::Ch1).ok());
  LoseRequestOfAccess(r, 1); // the read goes through, the write to clear CH0 never arrives
  CHECK(!r.drv.DisableChannel(Channel::Ch0).ok());
  REQUIRE((ChipEnables(r) & 0x01U) != 0U); // CH0 still on
  const int sent = r.port.state.transfers;
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch0).error, Error::WrongState);
  CHECK_EQ(r.port.state.transfers, sent);
  // A validated read settles it: CH0 is on, so the check stays refused until it is off.
  REQUIRE(r.drv.ReadChannelControl().ok());
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch0).error, Error::WrongState);
  REQUIRE(r.drv.DisableChannel(Channel::Ch0).ok());
  CHECK(r.drv.StartOvercurrentCheck(Channel::Ch0).ok());
}

TEST_CASE("Ambiguous CH_CTRL write with a reply: the earlier enable is not forgotten") {
  Rig<FullPort> r(OcConfig());
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 0U).ok());
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  CorruptReplyOfAccess(r, 1); // the write to enable CH1 lands, its reply does not
  CHECK_EQ(r.drv.EnableChannel(Channel::Ch1).error, Error::Crc);
  const int sent = r.port.state.transfers;
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch0).error, Error::WrongState);
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch1).error, Error::WrongState);
  CHECK_EQ(r.port.state.transfers, sent);
}

TEST_CASE("Lost EnterConfigMode write: every Config Mode guard refuses until the chip says so") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  LoseRequestOfAccess(r, 0);
  CHECK(!r.drv.EnterConfigMode().ok());
  CHECK(!r.port.state.chip.En()); // EN low whatever the SPI result
  REQUIRE(r.port.state.chip.MissionMode());
  CheckConfigOnlyRefused(r);
  REQUIRE(r.drv.ReadChannelControl().ok()); // the chip is still in Mission Mode
  CheckConfigOnlyRefused(r);
  REQUIRE(r.drv.EnterConfigMode().ok());
  CHECK(r.drv.ConfigureChannel(Channel::Ch0, tle92466ed_test::TwoChannels().channels[0]).ok());
}

TEST_CASE("Ambiguous EnterMissionMode write: Config Mode guards refuse until a read") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.corrupt_transfer = -1;
  // FB_STAT and GLOBAL_DIAG2 reads first (2 accesses), then the CH_CTRL write.
  CorruptReplyOfAccess(r, 2);
  CHECK_EQ(r.drv.EnterMissionMode().error, Error::Crc);
  CheckConfigOnlyRefused(r);
  REQUIRE(r.drv.ReadChannelControl().ok());
  CHECK(r.drv.IsMissionMode() == r.port.state.chip.MissionMode());
}

TEST_CASE("Ambiguous SetParallel: the pair refuses current and enables until it is known") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  CorruptReplyOfAccess(r, 1); // the master's SETPOINT, then the CH_CTRL write lands unconfirmed
  CHECK_EQ(r.drv.SetParallel(ParallelPair::Ch0Ch3, true).error, Error::Crc);
  REQUIRE((r.port.state.chip.Peek(chip::kChCtrl) & 0x2000U) != 0U); // the chip joined the pair
  const int sent = r.port.state.transfers;
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 1'000'000).error, Error::WrongState);
  CHECK_EQ(r.drv.SetDither(Channel::Ch0, tle92466ed::CurrentControlChannel().dither).error,
           Error::WrongState);
  CHECK_EQ(r.port.state.transfers, sent);
  CHECK(r.drv.SetCurrent(Channel::Ch0, 0U).ok()); // 0 is 0 in either pairing
  // A read finds the chip joined while the configuration says split: still refused.
  REQUIRE(r.drv.ReadChannelControl().ok());
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 1'000'000).error, Error::WrongState);
  REQUIRE(r.drv.SetParallel(ParallelPair::Ch0Ch3, true).ok());
  CHECK(r.drv.SetCurrent(Channel::Ch0, 1'000'000).ok());
}

TEST_CASE("Lost SetParallel write: a read that finds the configured pairing settles it") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  LoseRequestOfAccess(r, 1);
  CHECK(!r.drv.SetParallel(ParallelPair::Ch0Ch3, true).ok());
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 1'000'000).error, Error::WrongState);
  REQUIRE(r.drv.ReadChannelControl().ok()); // the chip kept the pair split, as configured
  CHECK(r.drv.SetCurrent(Channel::Ch0, 1'000'000).ok());
}

TEST_CASE("Ambiguous ConfigureChannel: the channel refuses enable, current and the check") {
  Rig<FullPort> r(OcConfig());
  REQUIRE(r.Start());
  tle92466ed::ChannelConfig cc = OcConfig().channels[0];
  CorruptReplyOfAccess(r, 0);
  CHECK_EQ(r.drv.ConfigureChannel(Channel::Ch0, cc).error, Error::Crc);
  REQUIRE(r.drv.EnterMissionMode().ok()); // the other channels are not affected
  const int sent = r.port.state.transfers;
  CHECK_EQ(r.drv.EnableChannel(Channel::Ch0).error, Error::WrongState);
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 500'000).error, Error::WrongState);
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch0).error, Error::WrongState);
  CHECK_EQ(r.port.state.transfers, sent);
  CHECK(r.drv.EnableChannel(Channel::Ch1).ok());
  CHECK(r.drv.DisableChannel(Channel::Ch0).ok()); // switching off is never refused
  REQUIRE(r.drv.EnterConfigMode().ok());
  REQUIRE(r.drv.ConfigureChannel(Channel::Ch0, cc).ok());
  REQUIRE(r.drv.EnterMissionMode().ok());
  CHECK(r.drv.EnableChannel(Channel::Ch0).ok());
}

TEST_CASE("Ambiguous SetDither: current and enables refuse until SetDither succeeds") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  tle92466ed::DitherConfig d = tle92466ed::CurrentControlChannel().dither;
  d.steps = 4;
  d.flat = 3;
  d.step_size = 100;
  CorruptReplyOfAccess(r, 2); // DITHER_CTRL, the update event, lands unconfirmed
  CHECK_EQ(r.drv.SetDither(Channel::Ch0, d).error, Error::Crc);
  const int sent = r.port.state.transfers;
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 500'000).error, Error::WrongState);
  CHECK_EQ(r.drv.EnableChannel(Channel::Ch0).error, Error::WrongState);
  CHECK_EQ(r.port.state.transfers, sent);
  REQUIRE(r.drv.SetDither(Channel::Ch0, d).ok());
  CHECK(r.drv.SetCurrent(Channel::Ch0, 500'000).ok());
  CHECK(r.drv.EnableChannel(Channel::Ch0).ok());
}

TEST_CASE("Ambiguous ApplyConfig: Mission Mode refuses until it succeeds") {
  Rig<FullPort> r(OcConfig());
  REQUIRE(r.Start());
  CorruptReplyOfAccess(r, 0); // GLOBAL_CONFIG lands unconfirmed
  CHECK_EQ(r.drv.ApplyConfig(OcConfig()).error, Error::Crc);
  const int sent = r.port.state.transfers;
  CHECK_EQ(r.drv.EnterMissionMode().error, Error::WrongState);
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 500'000).error, Error::WrongState);
  CHECK_EQ(r.port.state.transfers, sent);
  REQUIRE(r.drv.ApplyConfig(OcConfig()).ok());
  CHECK(r.drv.EnterMissionMode().ok());
}

TEST_CASE("Ambiguous SetDither: a mask that keeps the channel still switches the others off") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 300'000).ok());
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  REQUIRE(r.drv.EnableChannel(Channel::Ch1).ok());
  tle92466ed::DitherConfig d = r.drv.GetConfig().channels[0].dither;
  d.steps = 1;
  d.step_size = 16;
  CorruptReplyOfAccess(r, 0);
  CHECK(!r.drv.SetDither(Channel::Ch0, d).ok());
  CHECK(r.drv.SetEnabledChannels(0x01U).ok()); // keep CH0, switch CH1 off
  CHECK_EQ(ChipEnables(r) & 0x02U, 0U);
  CHECK_EQ(ChipEnables(r) & 0x01U, 0x01U); // CH0 left as the chip has it
  // Turning an unknown channel on is still refused, with nothing sent.
  REQUIRE(r.drv.SetEnabledChannels(0U).ok());
  const int sent = r.port.state.transfers;
  CHECK_EQ(r.drv.SetEnabledChannels(0x03U).error, Error::WrongState);
  CHECK_EQ(r.port.state.transfers, sent);
  REQUIRE(r.drv.SetDither(Channel::Ch0, d).ok());
  CHECK(r.drv.SetEnabledChannels(0x03U).ok());
}
