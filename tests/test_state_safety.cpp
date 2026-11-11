/**
 * @file test_state_safety.cpp
 * @brief The driver's copy of the chip's state never overrides what the chip did: channels a
 * fault switched off stay off, Mission Mode is entered only as Figure 4 allows, flags are never
 * cleared unseen, and a critical fault or a reset leaves every output off.
 *
 * Each case first failed on the driver before its fix.
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
using tle92466ed_test::MinimalPort;
using tle92466ed_test::Rig;
namespace chip = tle92466ed_test::chip;

namespace {

/// CH0 and CH1 on, CH0 at 0.5 A, in Mission Mode.
bool Driving(Rig<FullPort>& r) {
  return r.StartMission() && r.drv.SetCurrent(Channel::Ch0, 500'000).ok() &&
         r.drv.EnableChannel(Channel::Ch0).ok() && r.drv.EnableChannel(Channel::Ch1).ok();
}

uint32_t ChipEnables(const Rig<FullPort>& r) {
  return r.port.state.chip.Peek(chip::kChCtrl) & 0x3FU;
}

} // namespace

TEST_CASE("B1: a channel the chip switched off for OC stays off when another channel changes") {
  Rig<FullPort> r;
  REQUIRE(Driving(r));
  r.port.state.chip.SetChannelError(0, chip::kOc); // 4.9.6: the chip clears EN_CH0
  REQUIRE(r.drv.ClearFaults().ok());
  CHECK(r.drv.DisableChannel(Channel::Ch1).ok());
  CHECK_EQ(ChipEnables(r), 0U); // CH0 not switched back on
  CHECK_EQ(r.drv.GetEnabledChannels(), 0U);
  CHECK(r.drv.EnableChannel(Channel::Ch1).ok());
  CHECK_EQ(ChipEnables(r), 0x02U);
}

TEST_CASE("B1: after a supply fault ended Mission Mode, no write sets OP_MODE or an enable again") {
  Rig<FullPort> r;
  REQUIRE(Driving(r));
  r.port.state.chip.SetGlobalFault(chip::kVddUv); // 4.1.2: Config Mode, every EN_CH cleared
  REQUIRE(r.drv.ClearFaults().ok());
  CHECK(r.drv.DisableChannel(Channel::Ch1).ok());
  CHECK(!r.port.state.chip.MissionMode());
  CHECK_EQ(ChipEnables(r), 0U);
  CHECK(!r.drv.IsMissionMode());
  // Enabling now is refused: it needs Mission Mode first.
  CHECK_EQ(r.drv.EnableChannel(Channel::Ch0).error, Error::WrongState);
  CHECK(r.drv.EnterMissionMode().ok());
  CHECK_EQ(ChipEnables(r), 0U); // Mission Mode with every channel off
}

TEST_CASE("B1: an enable that finds the chip back in Config Mode is MissionBlocked") {
  Rig<FullPort> r;
  REQUIRE(Driving(r));
  r.port.state.chip.SetGlobalFault(chip::kClkNok);
  CHECK_EQ(r.drv.EnableChannel(Channel::Ch1).error, Error::MissionBlocked);
  CHECK(!r.port.state.chip.MissionMode());
}

TEST_CASE("B1: the model switches both channels of a pair off on an OC in either (4.5.2)") {
  Config c = tle92466ed_test::TwoChannels();
  c.parallel[0] = true;
  Rig<FullPort> r(c);
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  r.port.state.chip.SetChannelError(3, chip::kOc);
  CHECK_EQ(ChipEnables(r), 0U);
  CHECK(r.drv.EnableChannel(Channel::Ch1).ok());
  CHECK_EQ(ChipEnables(r), 0x02U);                            // CH0 not switched on again
  CHECK_EQ(r.drv.GetEnabledChannels() & ~ChipEnables(r), 0U); // the copy never over-reports
  REQUIRE(r.drv.ReadChannelControl().ok());
  CHECK_EQ(r.drv.GetEnabledChannels(), 0x02U);
}

TEST_CASE("B2: joining a pair sets the master's setpoint to 0 instead of doubling it") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 1'400'000).ok());
  CHECK(r.drv.SetParallel(ParallelPair::Ch0Ch3, true).ok());
  CHECK_EQ(r.port.state.chip.ChannelReg(0, chip::kSetpoint) & 0x7FFFU, 0U);
  CHECK_EQ(r.drv.GetCurrentSetpoint(Channel::Ch0), 0U);
  // The pair's limit then applies, dither included.
  CHECK(r.drv.SetCurrent(Channel::Ch0, 2'400'000).ok());
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch0, 2'500'000).error, Error::InvalidArgument);
}

TEST_CASE("M1: the OFF-state overcurrent check needs the channel off with setpoint 0") {
  Config c = tle92466ed_test::TwoChannels();
  c.channels[0].on_time_mant = 100;
  Rig<FullPort> r(c);
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 1'000'000).ok());
  const int before = r.port.state.transfers;
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch0).error, Error::WrongState);
  CHECK_EQ(r.port.state.transfers, before); // nothing sent
  CHECK_EQ(ChipEnables(r), 0U);
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 0).ok());
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch0).error, Error::WrongState); // already on
  REQUIRE(r.drv.DisableChannel(Channel::Ch0).ok());
  const int sent = r.port.state.transfers;
  CHECK(r.drv.StartOvercurrentCheck(Channel::Ch0).ok());
  CHECK_EQ(r.port.state.transfers - sent, 6); // CH_CONFIG, then the CH_CTRL read and write
  CHECK_EQ(r.port.state.chip.ChannelReg(0, chip::kSetpoint) & 0x7FFFU, 0U); // no ON state
}

TEST_CASE("M1: a channel asked on counts as on for the overcurrent check until a read") {
  // The copy may under-report a channel that is on; the check still sends nothing (WrongState)
  // for a channel the driver asked on since the last read of CH_CTRL.
  Config c = tle92466ed_test::TwoChannels();
  c.channels[0].on_time_mant = 100;
  Rig<FullPort> r(c);
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 0).ok());
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  int sent = r.port.state.transfers;
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch0).error, Error::WrongState); // on
  CHECK_EQ(r.port.state.transfers, sent);
  REQUIRE(r.drv.DisableChannel(Channel::Ch0).ok());
  r.port.state.chip.SetChannelError(0, chip::kOl); // CH0 refused while the fault is latched
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  REQUIRE(ChipEnables(r) == 0U);
  sent = r.port.state.transfers;
  CHECK_EQ(r.drv.StartOvercurrentCheck(Channel::Ch0).error, Error::WrongState); // asked on
  CHECK_EQ(r.port.state.transfers, sent);
  REQUIRE(r.drv.ReadChannelControl().ok()); // the read finds it off
  CHECK(r.drv.StartOvercurrentCheck(Channel::Ch0).ok());
}

TEST_CASE("M2: a flag raised between ReadFaults and ClearFaults reaches the latch") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.ReadFaults().ok());
  r.port.state.chip.SetChannelWarning(1, chip::kOtw);
  REQUIRE(r.drv.ClearFaults().ok());
  CHECK(r.drv.GetLatchedFaults().Of(Channel::Ch1).overtemperature_warning);
}

TEST_CASE("M2: ClearChannelFaults latches its group partner, and clears nothing it did not read") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  r.port.state.chip.SetChannelError(1, chip::kSg);
  r.port.state.chip.SetChannelError(0, chip::kOl);
  CHECK(r.drv.ClearChannelFaults(Channel::Ch0).ok());
  CHECK(r.drv.GetLatchedFaults().Of(Channel::Ch1).short_to_ground);
  CHECK_EQ(r.port.state.chip.Peek(chip::kDiagErr0), chip::kSg << 8U);
  // Nothing set: nothing written.
  const int before = r.port.state.CountWrites(chip::kDiagErr0 + 1U);
  CHECK(r.drv.ClearChannelFaults(Channel::Ch2).ok());
  CHECK_EQ(r.port.state.CountWrites(chip::kDiagErr0 + 1U), before);
}

TEST_CASE("M2, N4: OTP_VIRGIN is never cleared; ClearFaults writes only registers with flags") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.SetDataFault(chip::kOtpVirgin);
  const int before = r.port.state.transfers;
  REQUIRE(r.drv.ClearFaults().ok());
  CHECK(r.drv.ReadFaults().value.Has(tle92466ed::GlobalFault::OtpVirgin));
  CHECK_EQ(r.port.state.CountWrites(chip::kGlobalDiag0, before), 0);
  CHECK_EQ(r.port.state.CountWrites(chip::kDiagWarn0, before), 0);
}

TEST_CASE("M2: ClearFaults that finds POR_EVENT with the reset watch armed is UnexpectedReset") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  r.port.state.chip.SetGlobalFault(0U);
  r.port.state.chip.PowerOn(); // every register reset, POR_EVENT set
  r.port.state.chip.init_delay_us = 0;
  Error e = Error::Ok;
  for (int i = 0; (i < 3) && (e != Error::UnexpectedReset); ++i) {
    e = r.drv.ClearFaults().error;
  }
  CHECK_EQ(e, Error::UnexpectedReset);
  CHECK((r.port.state.chip.Peek(chip::kGlobalDiag0) & chip::kPorEvent) != 0U); // kept
}

TEST_CASE("M4: Mission Mode is refused before any write on REG_ECC_ERR or a channel OTW") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.SetDataFault(chip::kRegEccErr);
  int before = r.port.state.transfers;
  CHECK_EQ(r.drv.EnterMissionMode().error, Error::MissionBlocked);
  CHECK_EQ(r.port.state.CountWrites(chip::kChCtrl, before), 0);
  CHECK(!r.drv.IsMissionMode());
  REQUIRE(r.drv.ClearFaults().ok());
  r.port.state.chip.SetChannelWarning(4, chip::kOtw);
  before = r.port.state.transfers;
  CHECK_EQ(r.drv.EnterMissionMode().error, Error::MissionBlocked);
  CHECK_EQ(r.port.state.CountWrites(chip::kChCtrl, before), 0);
  REQUIRE(r.drv.ClearFaults().ok());
  r.port.state.chip.SetChannelError(5, chip::kOte);
  CHECK_EQ(r.drv.EnterMissionMode().error, Error::MissionBlocked);
  REQUIRE(r.drv.ClearFaults().ok());
  CHECK(r.drv.EnterMissionMode().ok());
}

TEST_CASE("M5: one critical fault frame from noise is not a critical fault; the read retries") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  r.port.state.replace_transfer = r.port.state.transfers + 1;
  r.port.state.replace_word = chip::CriticalFrame(0x00);
  CHECK(r.drv.ReadStatus().ok());
  CHECK(r.drv.IsInitialized());
}

TEST_CASE("M5: a confirmed critical fault drives EN low and leaves the driver uninitialised") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  REQUIRE(r.port.state.chip.En());
  r.port.state.chip.EnterCritical(0xE8);
  CHECK_EQ(r.drv.ReadStatus().error, Error::CriticalFault);
  CHECK(!r.drv.IsInitialized());
  CHECK(!r.port.state.chip.En());
  CHECK(r.Start()); // RESN leaves the state (Figure 4)
}

TEST_CASE("M6: a clear-on-read DIAG_WARN_CHGR read is never repeated") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.SetChannelWarning(2, chip::kOlsgChkNok);
  // Corrupt the reply to the DIAG_WARN_CHGR0 read (the third register ReadFaults reads).
  r.port.state.corrupt_transfer = r.port.state.transfers + 5;
  const int before = r.port.state.transfers;
  CHECK_EQ(r.drv.ReadFaults().error, Error::Crc);
  CHECK_EQ(r.port.state.CountReads(chip::kDiagWarn0, before), 1);
}

TEST_CASE("M7: a pair's setpoint with dither keeps the single-channel headroom: 2.43 A") {
  static_assert(tle92466ed::kMaxParallelSetpointUa == 2'430'000U);
  Config c = tle92466ed_test::TwoChannels();
  c.parallel[1] = true;
  Rig<MinimalPort> r(c);
  REQUIRE(r.Start());
  CHECK(r.drv.SetCurrent(Channel::Ch1, 2'430'000).ok());
  CHECK_EQ(r.drv.SetCurrent(Channel::Ch1, 2'430'100).error, Error::InvalidArgument);
}

TEST_CASE(
    "N1: SPI_WD_EN is the last start-up write, and a watchdog too short to reload is refused") {
  Config c = tle92466ed_test::TwoChannels();
  c.spi_watchdog = true;
  CHECK_EQ(tle92466ed::Validate(c), Error::InvalidArgument); // WD_TIME 1: 0.585 ms
  c.watchdog_reload = tle92466ed::kMinWatchdogReload;
  Rig<FullPort> r(c);
  REQUIRE(r.Start());
  int last_write = -1;
  for (int i = 0; i < r.port.state.transfers; ++i) {
    last_write = r.port.state.IsWrite(i) ? i : last_write;
  }
  REQUIRE(last_write >= 0);
  CHECK_EQ(r.port.state.AddressOf(last_write), chip::kGlobalConfig);
  CHECK((r.port.state.mosi[static_cast<std::size_t>(last_write)] & chip::kSpiWdEn) != 0U);
  CHECK(r.drv.EnterMissionMode().ok()); // the watchdog has not expired during the start
}

TEST_CASE(
    "N1: the model's clock runs with the frames: an unreloaded watchdog expires during traffic") {
  Config c = tle92466ed_test::TwoChannels();
  c.spi_watchdog = true;
  c.watchdog_reload = 20; // 11.7 ms
  Rig<FullPort> r(c);
  REQUIRE(r.StartMission());
  for (int i = 0; i < 1000; ++i) { // 2000 frames of 9 us, no reload: 18 ms
    (void)r.drv.ReadStatus();
  }
  CHECK(!r.port.state.chip.MissionMode());
}

TEST_CASE("N2: without RESN a chip left in Mission Mode is switched off before the t_POR wait") {
  Rig<MinimalPort> first;
  REQUIRE(first.StartMission());
  REQUIRE(first.drv.EnableChannel(Channel::Ch0).ok());
  Rig<MinimalPort> again;
  again.port.state.chip = first.port.state.chip;
  REQUIRE(again.port.state.chip.MissionMode());
  REQUIRE(again.drv.EnsureInitialized().ok());
  // Frame 0 is the anchor that starts the pipeline, frame 1 the CH_CTRL write, before any wait.
  CHECK(again.port.state.IsWrite(1));
  CHECK_EQ(again.port.state.AddressOf(1), chip::kChCtrl);
  CHECK_EQ(again.port.state.mosi[1] & 0xFFFFU, 0U);
  CHECK(!again.port.state.chip.MissionMode());
}

TEST_CASE("N3: an unexpected reset drives EN low; Deinitialize afterwards drives it low too") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  r.port.state.chip.PowerOn();
  Error e = Error::Ok;
  for (int i = 0; (i < 4) && (e != Error::UnexpectedReset); ++i) {
    e = r.drv.ReloadWatchdog().error;
  }
  REQUIRE(e == Error::UnexpectedReset);
  CHECK(!r.port.state.chip.En());
  r.port.state.chip.SetEn(true);
  const int transfers = r.port.state.transfers;
  CHECK(r.drv.Deinitialize().ok());
  CHECK(!r.port.state.chip.En());
  CHECK_EQ(r.port.state.transfers, transfers); // no SPI
}

TEST_CASE("N4: RunBist reads and latches GLOBAL_DIAG2 before it clears REG_ECC_ERR") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  REQUIRE(r.drv.RunBist().ok());
  CHECK(r.drv.GetLatchedFaults().Has(tle92466ed::GlobalFault::RegisterEcc));
  CHECK_EQ(r.port.state.chip.Peek(chip::kGlobalDiag2) & chip::kRegEccErr, 0U);
}

TEST_CASE("N5: SetDither's read-back compares DITHER_STEP with all 16 bits") {
  Rig<MinimalPort> r;
  REQUIRE(r.Start());
  tle92466ed::DitherConfig d = tle92466ed::CurrentControlChannel().dither;
  d.steps = 0x10; // STEPS bit 4 is register bit 12
  CHECK(r.drv.SetDither(Channel::Ch0, d, tle92466ed::Verify::ReadBack).ok());
  CHECK_EQ(r.port.state.chip.ChannelReg(0, chip::kDitherStep), 0x1000U);
  // A read-back that differs only in bit 12 is a mismatch. Each register is a write (2 frames) and
  // a read (2 frames); DITHER_STEP's read reply is the 8th frame.
  r.port.state.replace_transfer = r.port.state.transfers + 7;
  r.port.state.replace_word = chip::Reply16(0x0000U, false, 0U);
  CHECK_EQ(r.drv.SetDither(Channel::Ch0, d, tle92466ed::Verify::ReadBack).error,
           Error::WriteMismatch);
}

TEST_CASE("N7: the frame counts the docs give") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  CHECK_EQ(r.port.state.transfers, 181); // EnsureInitialized on a clean chip (docs: Contract)
  int before = r.port.state.transfers;
  REQUIRE(r.drv.ClearFaults().ok());
  CHECK_EQ(r.port.state.transfers - before, 18); // nothing to clear: the 9 reads
  before = r.port.state.transfers;
  REQUIRE(r.drv.ConfigureChannel(Channel::Ch2, tle92466ed::CurrentControlChannel()).ok());
  CHECK_EQ(r.port.state.transfers - before, 26);
  before = r.port.state.transfers;
  REQUIRE(r.drv.EnterMissionMode().ok());
  CHECK_EQ(r.port.state.transfers - before, 8);
  before = r.port.state.transfers;
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  CHECK_EQ(r.port.state.transfers - before, 4);
}

TEST_CASE("Model: COTERR switches every stage off and keeps Mission Mode; COTWARN does nothing") {
  Rig<FullPort> r;
  REQUIRE(Driving(r));
  r.port.state.chip.SetGlobalFault(chip::kCotWarn);
  CHECK_EQ(ChipEnables(r), 0x03U);
  r.port.state.chip.SetGlobalFault(chip::kCotErr);
  CHECK_EQ(ChipEnables(r), 0U);
  CHECK(r.port.state.chip.MissionMode());
}

TEST_CASE("Model: a TARGET above 0x6000 regulates 1.5 A (5.3.3.3)") {
  Rig<MinimalPort> r;
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.EnableChannel(Channel::Ch0).ok());
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 1'000'000).ok());
  r.port.state.chip.SetTargetForTest(0, 0x7000U);
  r.port.DelayUs(1500);
  const auto f = r.drv.ReadChannelFeedback(Channel::Ch0);
  REQUIRE(f.ok());
  CHECK((f.value.average_current_ua > 1'490'000) && (f.value.average_current_ua < 1'510'000));
}

TEST_CASE("M5: a critical fault in the anchor slot is confirmed too; an EN that cannot go low is "
          "logged") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  r.port.state.replace_transfer = r.port.state.transfers; // the first slot, then the next frames
  r.port.state.replace_count = 3;
  r.port.state.replace_word = chip::CriticalFrame(0x40);
  r.port.state.fail_pin = r.port.state.pin_calls; // EN low fails
  const int errors = r.port.error_lines;
  CHECK_EQ(r.drv.ReloadWatchdog().error, Error::CriticalFault);
  CHECK_EQ(r.drv.GetLastCriticalFault(), 0x40U);
  CHECK(r.port.error_lines > errors);
}

TEST_CASE("N-A: with MISO stuck high, switching off still reaches the chip and reports the error") {
  Rig<FullPort> r;
  REQUIRE(Driving(r));
  r.port.state.miso = tle92466ed_test::Miso::StuckHigh; // the CH_CTRL read fails, MOSI still works
  CHECK_EQ(r.drv.DisableChannel(Channel::Ch0).error, Error::NoDevice);
  // CH0 off. CH1 stays on when the write's reply showed it on; when the reply shows CH_CTRL before
  // the write that enabled it, the copy under-reports CH1 and the fallback switches it off too.
  CHECK_EQ(ChipEnables(r), tle92466ed_test::kDefaultReplyBeforeWrite ? 0U : 0x02U);
  CHECK_EQ(r.drv.SetEnabledChannels(0U).error, Error::NoDevice);
  CHECK_EQ(ChipEnables(r), 0U);
  CHECK_EQ(r.drv.EnableChannel(Channel::Ch0).error, Error::NoDevice); // enabling needs the read
  CHECK_EQ(ChipEnables(r), 0U);
  CHECK_EQ(r.drv.EnterConfigMode().error, Error::NoDevice);
  CHECK(!r.port.state.chip.MissionMode());
  CHECK(!r.port.state.chip.En()); // EN low whatever the SPI result
}

TEST_CASE("N-A: with a corrupted CH_CTRL read, a disable still switches the channel off") {
  Rig<FullPort> r;
  REQUIRE(Driving(r));
  r.port.state.corrupt_transfer = r.port.state.transfers + 1; // the read's reply
  r.port.state.corrupt_count = 20;                            // every retry and the write reply
  CHECK_EQ(r.drv.DisableChannel(Channel::Ch1).error, Error::Crc);
  CHECK_EQ(ChipEnables(r), 0x01U);
}

TEST_CASE("N-B: channels the chip switched off are reported until cleared") {
  Rig<FullPort> r;
  REQUIRE(Driving(r));
  CHECK_EQ(r.drv.GetDroppedChannels(), 0U); // no write's reply counts as a drop
  r.port.state.chip.SetChannelError(1, chip::kOlsg);
  const int warnings = r.port.warning_lines;
  REQUIRE(r.drv.ReadChannelControl().ok());
  CHECK_EQ(r.drv.GetDroppedChannels(), 0x02U);
  CHECK(r.port.warning_lines > warnings);
  r.drv.ClearDroppedChannels();
  CHECK_EQ(r.drv.GetDroppedChannels(), 0U);
}

TEST_CASE(
    "N-B: an enable the chip refuses is reported from the next validated read, not the reply") {
  Rig<FullPort> r;
  REQUIRE(Driving(r));
  r.port.state.chip.SetChannelError(1, chip::kOlsg); // CH1 off, the fault latched (4.9.4)
  REQUIRE(r.drv.ReadChannelControl().ok());
  r.drv.ClearDroppedChannels();
  CHECK(r.drv.EnableChannel(Channel::Ch1).ok()); // refused, no read-back
  CHECK_EQ(r.drv.GetDroppedChannels(), 0U);      // the write's reply is not used for the record
  REQUIRE(r.drv.ReadChannelControl().ok());
  CHECK_EQ(r.drv.GetDroppedChannels(), 0x02U);
  r.drv.ClearDroppedChannels();
  CHECK_EQ(r.drv.EnableChannel(Channel::Ch1, tle92466ed::Verify::ReadBack).error,
           Error::WriteMismatch);
  CHECK_EQ(r.drv.GetDroppedChannels(), 0x02U); // the read-back is a validated read
}

TEST_CASE("N-B: a new start empties the dropped-channel record") {
  Rig<FullPort> r;
  REQUIRE(Driving(r));
  r.port.state.chip.SetChannelError(1, chip::kOlsg);
  REQUIRE(r.drv.ReadChannelControl().ok());
  REQUIRE(r.drv.GetDroppedChannels() == 0x02U);
  REQUIRE(r.drv.HardwareReset().ok());
  CHECK_EQ(r.drv.GetDroppedChannels(), 0U);
}

TEST_CASE("N-C: the CH_CTRL copy never shows a channel on that the chip refused") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  r.port.state.chip.SetChannelError(0, chip::kOl); // CH0 kept off by a latched fault (4.9.4)
  const int before = r.port.state.transfers;
  CHECK(r.drv.EnableChannel(Channel::Ch0).ok()); // no read-back asked for
  CHECK_EQ(r.port.state.transfers - before, 4);  // the read and the write only
  CHECK_EQ(r.drv.GetEnabledChannels(), 0U);
}

TEST_CASE("N-C: after any write the copy reports no channel on that the chip has off") {
  Rig<FullPort> r;
  REQUIRE(r.StartMission());
  REQUIRE(r.drv.SetCurrent(Channel::Ch0, 500'000).ok());
  for (const auto ch : {Channel::Ch0, Channel::Ch1}) {
    REQUIRE(r.drv.EnableChannel(ch).ok());
    CHECK_EQ(r.drv.GetEnabledChannels() & ~ChipEnables(r), 0U);
  }
  for (const auto ch : {Channel::Ch1, Channel::Ch0}) {
    REQUIRE(r.drv.DisableChannel(ch).ok());
    CHECK_EQ(r.drv.GetEnabledChannels() & ~ChipEnables(r), 0U);
  }
  REQUIRE(r.drv.SetEnabledChannels(0x03U).ok());
  CHECK_EQ(r.drv.GetEnabledChannels() & ~ChipEnables(r), 0U);
  CHECK(r.drv.IsMissionMode() == r.port.state.chip.MissionMode());
}

TEST_CASE("N-C: a switch-off that cannot read CH_CTRL never turns a disabled channel back on") {
  // Disable CH0 with the bus working, then CH1 with MISO broken: the fallback write is built from
  // the copy, which must not still hold CH0 from the reply to the first disable.
  Rig<FullPort> r;
  REQUIRE(Driving(r));
  REQUIRE(r.drv.DisableChannel(Channel::Ch0).ok());
  REQUIRE((ChipEnables(r) & 0x01U) == 0U);
  CHECK_EQ(r.drv.GetEnabledChannels() & 0x01U, 0U);
  r.port.state.miso = tle92466ed_test::Miso::StuckHigh;
  CHECK_EQ(r.drv.DisableChannel(Channel::Ch1).error, Error::NoDevice);
  CHECK_EQ(ChipEnables(r), 0U);
}

TEST_CASE("N-C: Mission Mode is taken from the chip, not from a write's reply") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  REQUIRE(r.drv.EnterMissionMode().ok()); // the read after the write sets the copy
  CHECK(r.drv.IsMissionMode());
  CHECK(r.port.state.chip.MissionMode());
  REQUIRE(r.drv.EnterConfigMode().ok());
  CHECK(!r.drv.IsMissionMode());
  CHECK(!r.port.state.chip.MissionMode());
}

TEST_CASE("N-C: the model answers a write with CH_CTRL after or before it, as set") {
  using tle92466ed::detail::EncodeWrite;
  for (const bool before_write : {false, true}) {
    tle92466ed_test::Tle92466edModel m;
    m.write_reply_before_write = before_write;
    (void)m.Frame(EncodeWrite(chip::kChCtrl, 0x1000U));
    const uint32_t reply = m.Frame(EncodeWrite(chip::kChCtrl, 0x3000U));
    CHECK_EQ(m.Peek(chip::kChCtrl), 0x3000U);
    CHECK_EQ(reply & 0xFFFFU, before_write ? 0U : 0x1000U); // the reply to the first write
    const uint32_t second = m.Frame(EncodeWrite(chip::kChCtrl, 0x3000U));
    CHECK_EQ(second & 0xFFFFU, before_write ? 0x1000U : 0x3000U);
  }
  Rig<FullPort> r;
  CHECK(r.port.state.chip.write_reply_before_write == tle92466ed_test::kDefaultReplyBeforeWrite);
}

TEST_CASE("N-D: a refused Mission Mode drives EN low again") {
  Rig<FullPort> r;
  REQUIRE(r.Start());
  r.port.state.chip.refuse_mission = true;
  CHECK_EQ(r.drv.EnterMissionMode().error, Error::MissionBlocked);
  CHECK(!r.port.state.chip.En());
  r.port.state.chip.refuse_mission = false;
  CHECK(r.drv.EnterMissionMode().ok());
  CHECK(r.port.state.chip.En());
}
