/**
 * @file test_bus_faults.cpp
 * @brief Every public operation reports a failed bus transfer as Error::Bus, whichever transfer
 * fails.
 *
 * Each operation is run once on a clean bus to count its transfers, then once per transfer with
 * exactly that transfer failing. This reaches every "the transfer failed, return the error" path in
 * the driver, which a single fault test per operation would not.
 */
#include <cstdio>

#include "mock_tle92466ed.hpp"
#include "test_harness.hpp"
#include "test_rig.hpp"
#include "tle92466ed.hpp"

using tle92466ed::Channel;
using tle92466ed::Error;
using tle92466ed::Verify;
using tle92466ed_test::FullPort;
using tle92466ed_test::Rig;
using Drv = tle92466ed::TLE92466ED<FullPort>;

namespace {

void Nothing(Rig<FullPort>& r) {
  (void)r;
}

/// Mission Mode, CH0 on at 0.5 A, one measurement period passed.
void Driving(Rig<FullPort>& r) {
  REQUIRE(r.drv.EnterMissionMode().ok());
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 500'000).ok());
  r.port.DelayUs(1500);
}

/// Runs `op` with each of its transfers failing in turn; every run must return Error::Bus.
template <typename Setup, typename Op>
void Sweep(const char* name, Setup setup, Op op) {
  int transfers = 0;
  {
    Rig<FullPort> clean;
    REQUIRE(clean.Start());
    setup(clean);
    const int before = clean.port.state.transfers;
    const Error e = op(clean.drv);
    if (e != Error::Ok) {
      std::printf("    %s: clean run returned %s\n", name, tle92466ed::ToString(e));
    }
    CHECK_EQ(e, Error::Ok);
    transfers = clean.port.state.transfers - before;
  }
  CHECK(transfers > 0);
  for (int i = 0; i < transfers; ++i) {
    Rig<FullPort> r;
    REQUIRE(r.Start());
    setup(r);
    r.port.state.fail_transfer = r.port.state.transfers + i;
    const Error e = op(r.drv);
    if (e != Error::Bus) {
      std::printf("    %s: transfer %d of %d failed, got %s\n", name, i + 1, transfers,
                  tle92466ed::ToString(e));
    }
    CHECK_EQ(e, Error::Bus);
  }
}

} // namespace

TEST_CASE("Every transfer of the start-up can fail and is reported as Bus") {
  int transfers = 0;
  {
    Rig<FullPort> clean;
    REQUIRE(clean.Start());
    transfers = clean.port.state.transfers;
  }
  for (int i = 0; i < transfers; ++i) {
    Rig<FullPort> r;
    r.port.state.fail_transfer = i;
    CHECK_EQ(r.drv.EnsureInitialized().error, Error::Bus);
  }
}

TEST_CASE("Every transfer of every state change can fail and is reported as Bus") {
  Sweep("EnterMissionMode", Nothing, [](Drv& d) { return d.EnterMissionMode().error; });
  Sweep("EnterConfigMode", Nothing, [](Drv& d) { return d.EnterConfigMode().error; });
  Sweep("ReadChannelControl", Nothing, [](Drv& d) { return d.ReadChannelControl().error; });
  Sweep("Deinitialize", Nothing, [](Drv& d) { return d.Deinitialize().error; });
  Sweep("HardwareReset", Nothing, [](Drv& d) { return d.HardwareReset().error; });
  Sweep("ApplyConfig", Nothing,
        [](Drv& d) { return d.ApplyConfig(tle92466ed_test::TwoChannels()).error; });
  Sweep("ConfigureChannel", Nothing, [](Drv& d) {
    return d.ConfigureChannel(Channel::Ch2, tle92466ed::CurrentControlChannel(), Verify::ReadBack)
        .error;
  });
  Sweep("SetParallel on", Nothing,
        [](Drv& d) { return d.SetParallel(tle92466ed::ParallelPair::Ch4Ch5, true).error; });
  Sweep(
      "SetParallel off",
      [](Rig<FullPort>& r) {
        REQUIRE(r.drv.SetParallel(tle92466ed::ParallelPair::Ch4Ch5, true).ok());
      },
      [](Drv& d) { return d.SetParallel(tle92466ed::ParallelPair::Ch4Ch5, false).error; });
}

TEST_CASE("Every transfer of every channel operation can fail and is reported as Bus") {
  const auto mission = [](Rig<FullPort>& r) { REQUIRE(r.drv.EnterMissionMode().ok()); };
  Sweep("EnableChannel", mission,
        [](Drv& d) { return d.EnableChannel(Channel::Ch1, Verify::ReadBack).error; });
  Sweep("SetCurrent", Nothing,
        [](Drv& d) { return d.SetCurrent(Channel::Ch1, 10'000, Verify::ReadBack).error; });
  Sweep("SetDither", Nothing, [](Drv& d) {
    return d.SetDither(Channel::Ch1, tle92466ed::CurrentControlChannel().dither, Verify::ReadBack)
        .error;
  });
  Sweep("ReloadWatchdog", Nothing, [](Drv& d) { return d.ReloadWatchdog().error; });
}

TEST_CASE("Every transfer of every diagnosis can fail and is reported as Bus") {
  Sweep("ReadStatus", Nothing, [](Drv& d) { return d.ReadStatus().error; });
  Sweep("ReadFaults", Nothing, [](Drv& d) { return d.ReadFaults().error; });
  Sweep("ClearFaults", Nothing, [](Drv& d) { return d.ClearFaults().error; });
  Sweep("ClearChannelFaults", Nothing,
        [](Drv& d) { return d.ClearChannelFaults(Channel::Ch3).error; });
  Sweep("ReadPinStatus", Nothing, [](Drv& d) { return d.ReadPinStatus().error; });
  Sweep("ReadSupply", Nothing, [](Drv& d) { return d.ReadSupply().error; });
  Sweep("ReadChipId", Nothing, [](Drv& d) { return d.ReadChipId().error; });
  Sweep("ReadRegister", Nothing, [](Drv& d) { return d.ReadRegister(0x0202U).error; });
  Sweep("RunBist", Nothing, [](Drv& d) { return d.RunBist().error; });
  Sweep("ReadChannelFeedback", Driving,
        [](Drv& d) { return d.ReadChannelFeedback(Channel::Ch0).error; });
}

TEST_CASE("Every transfer of the overcurrent check can fail and is reported as Bus") {
  tle92466ed::Config c = tle92466ed_test::TwoChannels();
  c.channels[1].on_time_mant = 10;
  for (int i = 0; i < 4; ++i) {
    Rig<FullPort> r(c);
    REQUIRE(r.StartMission());
    r.port.state.fail_transfer = r.port.state.transfers + i;
    CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch1).error, Error::Bus);
  }
}

TEST_CASE("Every transfer of a clear with flags set can fail and is reported as Bus") {
  const auto flagged = [](Rig<FullPort>& r) {
    auto& m = r.port.state.chip;
    m.SetChannelError(0, tle92466ed_test::chip::kOc);
    m.SetChannelWarning(3, tle92466ed_test::chip::kOtw);
    m.SetDataFault(tle92466ed_test::chip::kOtpEccErr);
    m.SetInternalFault(0x0010U);
    m.SetGlobalFault(tle92466ed_test::chip::kVbatUv);
  };
  Sweep("ClearFaults with flags", flagged, [](Drv& d) { return d.ClearFaults().error; });
  Sweep("ClearChannelFaults with flags", flagged,
        [](Drv& d) { return d.ClearChannelFaults(Channel::Ch0).error; });
}

TEST_CASE(
    "A failed transfer while confirming a critical fault, or in the first start-up write, is Bus") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.EnterCritical(0x80);
  r.port.state.fail_transfer = r.port.state.transfers + 2; // the confirming frame
  CHECK_EQ(r.drv.ReloadWatchdog().error, Error::Bus);
  tle92466ed_test::Rig<tle92466ed_test::MinimalPort> m;
  m.port.state.fail_transfer = 0; // the anchor before the CH_CTRL write without RESN
  CHECK_EQ(m.drv.EnsureInitialized().error, Error::Bus);
}
