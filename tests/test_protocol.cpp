/**
 * @file test_protocol.cpp
 * @brief The SPI protocol of section 5.2 and the hardware lessons of the handbook: frame layout,
 * reply validity, stuck and looped-back lines, ignored and lost frames, resynchronisation, bounded
 * retries of reads only, critical fault frames and the status field.
 */
#include <cstdint>

#include "mock_tle92466ed.hpp"
#include "test_harness.hpp"
#include "test_rig.hpp"
#include "tle92466ed.hpp"

using tle92466ed::Channel;
using tle92466ed::Config;
using tle92466ed::Error;
using tle92466ed_test::FullPort;
using tle92466ed_test::MinimalPort;
using tle92466ed_test::Miso;
using tle92466ed_test::Rig;
namespace chip = tle92466ed_test::chip;
namespace detail = tle92466ed::detail;

TEST_CASE("Frames: 32 bits each, MSB first, a valid CRC, the layout of 5.2.3") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  CHECK_EQ(r.port.state.bad_transfers, 0);
  CHECK_EQ(r.port.state.chip.crc_errors, 0); // CRC_EN is on from reset: every frame was checked
  for (int i = 0; i < r.port.state.transfers; ++i) {
    const uint32_t f = r.port.state.mosi[static_cast<std::size_t>(i)];
    CHECK_EQ(chip::Crc(f), f >> 24U);
    if (!r.port.state.IsWrite(i)) {
      CHECK_EQ((f >> 22U) & 3U, 3U); // don't-care bits 23..22 sent as 1: an echo is never a reply
    }
  }
  CHECK_EQ(detail::EncodeWrite(0x0041U, 0x1234U) & 0x00FF'FFFFU, (0x41U << 17U) | 0x1'1234U);
}

TEST_CASE("Bus requirements: SPI mode 1, MSB first, 8 MHz, the CSN timing of Table 18") {
  static_assert(tle92466ed::kSpiMode == 1U);
  static_assert(tle92466ed::kMsbFirst);
  static_assert(tle92466ed::kMaxClockHz == 8'000'000U);
  static_assert(tle92466ed::kMinCsHighNs == 600U);
  static_assert((tle92466ed::kCsSetupNs == 250U) && (tle92466ed::kCsHoldNs == 250U));
  static_assert(tle92466ed::kFrameBytes == 4U);
  CHECK(!tle92466ed::kContinuousWord);
}

TEST_CASE("A stuck or looped-back MISO is NoDevice, never data (KB-CPP-116)") {
  for (const Miso m : {Miso::StuckLow, Miso::StuckHigh, Miso::Loopback}) {
    Rig<FullPort> r;
    r.port.state.miso = m;
    CHECK_EQ(r.drv.EnsureInitialized().error, Error::NoDevice);
    CHECK(!r.drv.IsInitialized());
  }
}

TEST_CASE("A line that sticks while running: reads retried, bounded and counted, then NoDevice") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  const auto before = r.drv.GetBusDiagnostics();
  r.port.state.miso = Miso::StuckHigh;
  CHECK_EQ(r.drv.ReadStatus().error, Error::NoDevice);
  const auto after = r.drv.GetBusDiagnostics();
  CHECK_EQ(after.retries - before.retries, 2U); // Config::max_retries
  CHECK_EQ(after.rejected_replies - before.rejected_replies, 3U);
  r.port.state.miso = Miso::Chip;
  CHECK(r.drv.ReadStatus().ok()); // resynchronised
}

TEST_CASE("A corrupted reply is retried once and the read succeeds") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  const auto before = r.drv.GetBusDiagnostics();
  r.port.state.corrupt_transfer = r.port.state.transfers + 1; // the reply slot
  const auto s = r.drv.ReadSupply();
  CHECK(s.ok());
  CHECK_EQ(s.value.vdd_mv, 5000U);
  const auto after = r.drv.GetBusDiagnostics();
  CHECK_EQ(after.retries - before.retries, 1U);
  CHECK_EQ(after.resyncs - before.resyncs, 1U);
}

TEST_CASE("Retries stop at Config::max_retries; 0 means none") {
  Config c = tle92466ed_test::TwoChannels();
  c.max_retries = 0;
  Rig<MinimalPort> r(c);
  REQUIRE(r.Start());
  r.port.state.corrupt_transfer = r.port.state.transfers + 1;
  CHECK_EQ(r.drv.ReadStatus().error, Error::Crc);
  CHECK_EQ(r.drv.GetBusDiagnostics().retries, 0U);
  Config d = tle92466ed_test::TwoChannels();
  d.max_retries = 3;
  Rig<MinimalPort> q(d);
  REQUIRE(q.Start());
  q.port.state.corrupt_transfer = q.port.state.transfers;
  q.port.state.corrupt_count = 1000;
  CHECK(!q.drv.ReadStatus().ok());
  CHECK_EQ(q.drv.GetBusDiagnostics().retries, 3U);
}

TEST_CASE("A write is never retried: its outcome is unknown (KB-ARCH-044)") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  const int before = r.port.state.transfers;
  r.port.state.corrupt_transfer = before + 1; // the write's reply
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 200'000).error, Error::Crc);
  CHECK_EQ(r.port.state.CountWrites(chip::kBase[0] + chip::kSetpoint, before), 1);
  // The chip may hold it: the copy keeps the larger of the old and the new setpoint.
  CHECK(r.drv.GetCurrentSetpoint(Channel::Ch0) > 0U);
}

TEST_CASE("A request the chip ignored repeats the previous reply and is refused, then retried") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  // A 16-bit read after a 16-bit access: the pending anchor is FB_STAT, 22 bits.
  r.port.state.ignore_transfer = r.port.state.transfers;
  const auto pins = r.drv.ReadPinStatus();
  CHECK(pins.ok());
  // A 22-bit read after a 16-bit access: same width as the pending FB_STAT anchor, so only the
  // repeated word gives the ignored frame away.
  r.port.state.ignore_transfer = r.port.state.transfers;
  const auto before = r.drv.GetBusDiagnostics().rejected_replies;
  const auto s = r.drv.ReadSupply();
  REQUIRE(s.ok());
  CHECK_EQ(s.value.vio_mv, 5000U); // FB_VOLTAGE1, not FB_STAT's word
  CHECK_EQ(r.drv.GetBusDiagnostics().rejected_replies - before, 1U);
}

TEST_CASE("A lost frame breaks the anchor chain and is detected") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  r.port.state.lose_transfer = r.port.state.transfers + 1; // the anchor the chip never sees
  CHECK(r.drv.ReloadWatchdog().ok() == false);
  CHECK(r.drv.ReadStatus().ok());
}

TEST_CASE("An anchor ICVID that is not manufacturer 0xC1 means the chain is out of step") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  REQUIRE(r.drv.ReadStatus().ok()); // a 22-bit read: the pending anchor is ICVID
  r.port.state.replace_transfer = r.port.state.transfers + 1; // the next access's first slot
  r.port.state.replace_word = chip::Reply16(0x1234U, false, 0U);
  CHECK_EQ(r.drv.ReloadWatchdog().error, Error::ReplyMismatch);
}

TEST_CASE("Status field: CRC error, frame error, read-only write, internal bus fault (5.2.3.3)") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  r.port.state.corrupt_mosi = r.port.state.transfers; // the chip sees a bad CRC in the write
  CHECK_EQ(r.drv.ReloadWatchdog().error, Error::Crc);
  const std::array<std::pair<uint32_t, Error>, 3> cases = {
      {{1U, Error::FrameRejected}, {3U, Error::FrameRejected}, {4U, Error::DeviceFault}}};
  for (const auto& [status, error] : cases) {
    r.port.state.replace_transfer = r.port.state.transfers + 2; // after the resync frame
    r.port.state.replace_word = chip::Reply16(0U, true, status);
    CHECK_EQ(r.drv.ReloadWatchdog().error, error);
  }
  CHECK(r.drv.ReloadWatchdog().ok());
}

TEST_CASE("The reserved reply mode and a wrong R/W echo are ReplyMismatch") {
  CHECK_EQ(detail::DecodeReply(0x00C0'0000U | (uint32_t{chip::Crc(0xC0'0000U)} << 24U), 0U,
                               detail::Width::Bits16, false)
               .error,
           Error::ReplyMismatch);
  CHECK_EQ(detail::DecodeReply(chip::Reply16(5U, true, 0U), 0U, detail::Width::Bits16, false).error,
           Error::ReplyMismatch);
  CHECK_EQ(detail::DecodeReply(chip::Reply22(5U), 0U, detail::Width::Bits22, true).error,
           Error::ReplyMismatch);
  CHECK_EQ(detail::DecodeReply(chip::Reply22(5U), 0U, detail::Width::Bits22, false).data, 5U);
}

TEST_CASE("A critical fault frame is CriticalFault, never retried; its flags are kept (5.2.3.5)") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  r.port.state.chip.EnterCritical(0xD8); // 1V5 and 2V5 fine, bandgap fine? 0xD8: clock too slow
  const auto before = r.drv.GetBusDiagnostics();
  CHECK_EQ(r.drv.ReadStatus().error, Error::CriticalFault);
  CHECK_EQ(r.drv.GetBusDiagnostics().retries, before.retries);
  CHECK_EQ(r.drv.GetLastCriticalFault(), 0xD8U);
  CHECK(!r.drv.IsInitialized()); // EN low, the driver uninitialised
  CHECK_EQ(r.drv.EnableChannel(Channel::Ch0).error, Error::NotInitialized);
  // Only a reset leaves the state (Figure 4): the start pulses RESN.
  CHECK(r.drv.EnsureInitialized().ok());
  CHECK(r.drv.ReadStatus().ok());
}

TEST_CASE("A critical fault frame in the anchor slot is CriticalFault too, once confirmed") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  r.port.state.replace_transfer = r.port.state.transfers + 1; // the reply slot and the next frame
  r.port.state.replace_count = 2;
  r.port.state.replace_word = chip::CriticalFrame(0x20);
  CHECK_EQ(r.drv.ReloadWatchdog().error, Error::CriticalFault);
  CHECK_EQ(r.drv.GetLastCriticalFault(), 0x20U);
  // One frame alone is noise, not a critical fault: a write is reported, not retried.
  Rig<MinimalPort> q;
  REQUIRE(q.Start());
  q.port.state.replace_transfer = q.port.state.transfers + 1;
  q.port.state.replace_word = chip::CriticalFrame(0x20);
  CHECK_EQ(q.drv.ReloadWatchdog().error, Error::ReplyMismatch);
  CHECK(q.drv.IsInitialized());
}

TEST_CASE("With CRC_EN off the chip skips the check; the driver still sends and checks CRCs") {
  Config c = tle92466ed_test::TwoChannels();
  c.crc_check = false;
  Rig<MinimalPort> r(c);
  REQUIRE(r.Start());
  CHECK_EQ(r.port.state.chip.Peek(chip::kGlobalConfig) & chip::kCrcEn, 0);
  r.port.state.corrupt_mosi = r.port.state.transfers;
  CHECK(r.drv.ReloadWatchdog().ok()); // not checked by the chip
  r.port.state.corrupt_transfer = r.port.state.transfers + 1;
  CHECK_EQ(r.drv.ReloadWatchdog().error, Error::Crc);
}

TEST_CASE("After any failure the next access starts with one anchor frame alone (KB-CPP-118)") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  r.port.state.fail_transfer = r.port.state.transfers;
  CHECK_EQ(r.drv.ReloadWatchdog().error, Error::Bus);
  const int next = r.port.state.transfers;
  const auto resyncs = r.drv.GetBusDiagnostics().resyncs;
  CHECK(r.drv.ReloadWatchdog().ok());
  CHECK_EQ(r.port.state.AddressOf(next), chip::kIcvid);
  CHECK(!r.port.state.IsWrite(next));
  CHECK_EQ(r.drv.GetBusDiagnostics().resyncs, resyncs + 1U);
}
