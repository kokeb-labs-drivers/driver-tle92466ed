/**
 * @file test_errors.cpp
 * @brief The `Error` enum follows the standard, and its values are returned by real calls.
 *
 * - The base set (0 to 9) has the names and values every driver of the org shares.
 * - Every chip-specific value (16 and up) MUST be returned by some call: the enumerators are found
 *   through `ToString()` (-Wswitch keeps it complete), so a new value fails this test until a
 *   scenario below produces it.
 * - Base values MAY go unreturned; `kRequireEveryBaseValue` says whether this driver returns them
 *   all. The TLE92466ED is never busy to SPI, so `Busy` is listed as not returned on the API guide.
 */
#include <array>
#include <cstdio>
#include <cstring>

#include "mock_tle92466ed.hpp"
#include "test_harness.hpp"
#include "test_rig.hpp"
#include "tle92466ed.hpp"

using tle92466ed::Channel;
using tle92466ed::Error;
using tle92466ed_test::FullPort;
using tle92466ed_test::MinimalPort;
using tle92466ed_test::Rig;
namespace chip = tle92466ed_test::chip;

static_assert(static_cast<int>(Error::Ok) == 0 && static_cast<int>(Error::NotInitialized) == 1 &&
                  static_cast<int>(Error::InvalidArgument) == 2 &&
                  static_cast<int>(Error::Bus) == 3 && static_cast<int>(Error::Timeout) == 4 &&
                  static_cast<int>(Error::DeviceIdMismatch) == 5 &&
                  static_cast<int>(Error::Crc) == 6 && static_cast<int>(Error::Busy) == 7 &&
                  static_cast<int>(Error::DeviceFault) == 8 &&
                  static_cast<int>(Error::Unsupported) == 9,
              "the base Error set has fixed values (standards/driver.md)");

namespace {

constexpr bool kRequireEveryBaseValue = false;
constexpr int kFirstChipSpecific = 16;

// Which Error values some scenario returned; written by Record() while the scenarios run.
std::array<bool, 256> g_seen{}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void Record(Error e) {
  g_seen[static_cast<uint8_t>(e)] = true;
}

void StartUpScenarios() {
  { // Ok, NotInitialized
    Rig<MinimalPort> r;
    Record(r.drv.ReadStatus().error);
    Record(r.drv.EnsureInitialized().error);
  }
  { // InvalidArgument
    tle92466ed::Config c{};
    c.max_retries = 9;
    Rig<MinimalPort> r(c);
    Record(r.drv.EnsureInitialized().error);
  }
  { // Bus
    Rig<MinimalPort> r;
    r.port.state.bus_up_ok = false;
    Record(r.drv.EnsureInitialized().error);
  }
  { // Timeout
    Rig<MinimalPort> r;
    r.port.state.chip.init_never = true;
    Record(r.drv.EnsureInitialized().error);
  }
  { // DeviceIdMismatch
    Rig<MinimalPort> r;
    r.port.state.chip.version = 0x01;
    Record(r.drv.EnsureInitialized().error);
  }
  { // DeviceFault
    Rig<FullPort> r;
    r.port.state.chip.otp_virgin = true;
    Record(r.drv.EnsureInitialized().error);
  }
  { // NoDevice
    Rig<MinimalPort> r;
    r.port.state.miso = tle92466ed_test::Miso::StuckLow;
    Record(r.drv.EnsureInitialized().error);
  }
  { // WriteMismatch
    tle92466ed::Config c{};
    c.vio = tle92466ed::VioLevel::V3V3;
    Rig<MinimalPort> r(c);
    r.port.state.chip.global_config_sticks = true;
    Record(r.drv.EnsureInitialized().error);
  }
}

void RunningScenarios() {
  Rig<FullPort> r;
  if (!r.Start()) {
    return;
  }
  Record(r.drv.EnableChannel(Channel::Ch0).error); // WrongState: Config Mode
  r.port.state.corrupt_transfer = r.port.state.transfers + 1;
  Record(r.drv.ReloadWatchdog().error); // Crc
  r.port.state.replace_transfer = r.port.state.transfers + 2;
  r.port.state.replace_word = chip::Reply16(0U, true, 1U);
  Record(r.drv.ReloadWatchdog().error); // FrameRejected
  REQUIRE(r.drv.ReadStatus().ok());
  r.port.state.replace_transfer = r.port.state.transfers + 1;
  r.port.state.replace_word = chip::Reply16(0x1234U, false, 0U);
  Record(r.drv.ReloadWatchdog().error);                  // ReplyMismatch
  Record(r.drv.ReadChannelFeedback(Channel::Ch0).error); // NotReady
  r.port.state.chip.SetGlobalFault(chip::kVddUv);
  Record(r.drv.EnterMissionMode().error); // MissionBlocked
  REQUIRE(r.drv.ClearFaults().ok());
  r.port.state.chip.EnterCritical(0xE0);
  Record(r.drv.ReadStatus().error);        // CriticalFault: the driver is uninitialised
  REQUIRE(r.drv.EnsureInitialized().ok()); // RESN leaves the Critical Fault state
  Record(r.drv.HardwareReset().error);     // Ok
  r.port.state.chip.PowerOn();
  for (int i = 0; i < 4; ++i) {
    Record(r.drv.ReloadWatchdog().error); // NoDevice, then UnexpectedReset
  }
  Rig<MinimalPort> m;
  if (m.Start()) {
    Record(m.drv.HardwareReset().error); // Unsupported
  }
}

} // namespace

TEST_CASE("Every chip-specific Error value is returned by some call") {
  StartUpScenarios();
  RunningScenarios();
  int enumerators = 0;
  for (int v = 0; v < 256; ++v) {
    const auto e = static_cast<Error>(v);
    if (std::strcmp(tle92466ed::ToString(e), "Unknown") == 0) {
      continue;
    }
    ++enumerators;
    const bool required = (v >= kFirstChipSpecific) || kRequireEveryBaseValue || (e != Error::Busy);
    const bool seen = g_seen[static_cast<std::size_t>(v)];
    if (!seen) {
      std::printf("    %s: Error::%s is never returned\n", required ? "FAIL" : "note",
                  tle92466ed::ToString(e));
    }
    CHECK(seen || !required);
    CHECK((v < 10) || (v >= kFirstChipSpecific)); // 10 to 15 are reserved for the base set
  }
  CHECK_EQ(enumerators, 19);
}

TEST_CASE("ToString names every enumerator and nothing else") {
  CHECK(std::strcmp(tle92466ed::ToString(Error::UnexpectedReset), "UnexpectedReset") == 0);
  CHECK(std::strcmp(tle92466ed::ToString(Error::Busy), "Busy") == 0);
  CHECK(std::strcmp(tle92466ed::ToString(tle92466ed_test::NotAnEnumerator<Error>(12)), "Unknown") ==
        0);
}
