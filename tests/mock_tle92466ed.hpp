/**
 * @file mock_tle92466ed.hpp
 * @brief A protocol and register model of the TLE92466ED for the host tests, plus the mock ports in
 * mock_tle92466ed_bus.hpp.
 *
 * The model behaves like the datasheet (Rev. 1.2): 32-bit frames with a CRC-8 SAE J1850 that it
 * checks while CRC_EN is set (5.1.2); the reply to each frame sent during the next one (5.2.1), in
 * the 16-bit, 22-bit or critical fault format (5.2.3); the reset values of section 5.3; Config and
 * Mission Mode with their rules (4.3.1, Figure 4: EN_CH only in Mission Mode, MODE, GLOBAL_CONFIG,
 * CLK_DIV, CH_PAR and SFF_BIST only in Config Mode); clear-on-write flags (a 1 clears an rwh flag,
 * an rw bit takes the value written), OC and OTP_ECC_ERR needing two clears (4.9.6, 4.9.7),
 * OLSG_WARN_CHK_NOK cleared by a read (4.9.3); a channel kept off while it has a fault (4.9.2);
 * slave channels that ignore MODE, SETPOINT and EN_CH (4.5.2); INIT_DONE after a reset or a clock
 * change (4.2.1); the SPI watchdog counting at f_SYS / 2^14 (5.2.2); the feedback freeze (4.10.5)
 * with a measurement period from the dither clock (4.4.2); the self-test (4.9.8). Time passes
 * only through `Elapse()`. Faults are injected through the public fields and methods.
 *
 * The model is written from the datasheet on its own (handbook KB-TST-030): it does not include the
 * driver's headers, but carries its own register map, reset values, timing and CRC below (namespace
 * `chip`, each with its datasheet section), so a wrong constant in the driver cannot hide behind
 * the same wrong constant in the model. tests/test_model.cpp checks the two against each other.
 */
#ifndef TLE92466ED_MOCK_TLE92466ED_HPP
#define TLE92466ED_MOCK_TLE92466ED_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace tle92466ed_test {

/// The model's default for `write_reply_before_write`: the build defines
/// TLE92466ED_MODEL_REPLY_BEFORE_WRITE for the test binary that runs under that reading.
#ifdef TLE92466ED_MODEL_REPLY_BEFORE_WRITE
inline constexpr bool kDefaultReplyBeforeWrite = true;
#else
inline constexpr bool kDefaultReplyBeforeWrite = false;
#endif

/// The chip as the datasheet describes it, independent of the driver's headers.
namespace chip {
// Central registers (Table 19).
inline constexpr uint32_t kChCtrl = 0x0000;
inline constexpr uint32_t kGlobalConfig = 0x0002;
inline constexpr uint32_t kGlobalDiag0 = 0x0003;
inline constexpr uint32_t kGlobalDiag1 = 0x0004;
inline constexpr uint32_t kGlobalDiag2 = 0x0005;
inline constexpr uint32_t kVbatTh = 0x0006;
inline constexpr uint32_t kFbFrz = 0x0007;
inline constexpr uint32_t kFbUpd = 0x0008;
inline constexpr uint32_t kWdReload = 0x0009;
inline constexpr uint32_t kDiagErr0 = 0x000A;  // + group 0..2
inline constexpr uint32_t kDiagWarn0 = 0x0010; // + group 0..2
inline constexpr uint32_t kFaultMask0 = 0x0016;
inline constexpr uint32_t kFaultMask1 = 0x0017;
inline constexpr uint32_t kFaultMask2 = 0x0018;
inline constexpr uint32_t kClkDiv = 0x0019;
inline constexpr uint32_t kSffBist = 0x003F;
inline constexpr uint32_t kIcvid = 0x0200;
inline constexpr uint32_t kPinStat = 0x0201;
inline constexpr uint32_t kFbStat = 0x0202;
inline constexpr uint32_t kFbVoltage1 = 0x0203;
inline constexpr uint32_t kFbVoltage2 = 0x0204;
inline constexpr uint32_t kChipId0 = 0x0205;
// Channel banks (Tables 24, 25): CH0..CH5 bases; feedback at base + 0x200.
inline constexpr std::array<uint16_t, 6> kBase = {0x40, 0x50, 0x60, 0x70, 0x20, 0x30};
inline constexpr uint32_t kSetpoint = 0x0;
inline constexpr uint32_t kCtrl = 0x1;
inline constexpr uint32_t kPeriod = 0x2;
inline constexpr uint32_t kIntegratorLimit = 0x3;
inline constexpr uint32_t kDitherClkDiv = 0x4;
inline constexpr uint32_t kDitherStep = 0x5;
inline constexpr uint32_t kDitherCtrl = 0x6;
inline constexpr uint32_t kChConfig = 0x7;
inline constexpr uint32_t kMode = 0xC;
inline constexpr uint32_t kTon = 0xD;
inline constexpr uint32_t kCtrlIntThresh = 0xE;
inline constexpr uint32_t kFbDc = 0x200;
inline constexpr uint32_t kFbVbat = 0x201;
inline constexpr uint32_t kFbIAvg = 0x202;
inline constexpr uint32_t kFbIminImax = 0x203;
inline constexpr uint32_t kFbIAvgS16 = 0x204;
inline constexpr uint32_t kFbIntThresh = 0x205;
inline constexpr uint32_t kFbPeriodMinMax = 0x206;
// Reset values (section 5.3).
inline constexpr uint32_t kGlobalConfigReset = 0x4005;
inline constexpr uint32_t kGlobalDiag0Reset = 0x0600;
inline constexpr uint32_t kVbatThReset = 0xFF19;
inline constexpr uint32_t kWdReloadReset = 0x0001;
inline constexpr uint32_t kDiagWarnReset = 0x1010;
inline constexpr uint32_t kFaultMask0Reset = 0xC03F;
inline constexpr uint32_t kFaultMask1Reset = 0x703F;
inline constexpr uint32_t kFaultMask2Reset = 0xC000;
inline constexpr uint32_t kClkDivReset = 0x0438;
inline constexpr uint32_t kCtrlReset = 0x4600;
inline constexpr uint32_t kIntegratorLimitReset = 0x43FF;
inline constexpr uint32_t kChConfigReset = 0x0003;
inline constexpr uint32_t kCtrlIntThreshReset = 0x0003;
// CH_CTRL (5.3.2.3).
inline constexpr uint32_t kOpMode = 0x8000;
inline constexpr uint32_t kPar12 = 0x4000;
inline constexpr uint32_t kPar03 = 0x2000;
inline constexpr uint32_t kPar45 = 0x1000;
// GLOBAL_CONFIG (5.3.2.4).
inline constexpr uint32_t kClkWdEn = 0x0001;
inline constexpr uint32_t kSpiWdEn = 0x0002;
inline constexpr uint32_t kCrcEn = 0x0004;
inline constexpr uint32_t kVioSel = 0x4000;
// GLOBAL_DIAG0 (5.3.2.5): rwh flags, rw events.
inline constexpr uint32_t kVbatUv = 0x0001;
inline constexpr uint32_t kVbatOv = 0x0002;
inline constexpr uint32_t kVioUv = 0x0004;
inline constexpr uint32_t kVioOv = 0x0008;
inline constexpr uint32_t kVddUv = 0x0010;
inline constexpr uint32_t kVddOv = 0x0020;
inline constexpr uint32_t kClkNok = 0x0040;
inline constexpr uint32_t kCotErr = 0x0080;
inline constexpr uint32_t kCotWarn = 0x0100;
inline constexpr uint32_t kResEvent = 0x0200;
inline constexpr uint32_t kPorEvent = 0x0400;
inline constexpr uint32_t kSpiWdErr = 0x4000;
inline constexpr uint32_t kDiag0Rwh = 0x41FF;
// GLOBAL_DIAG1 (5.3.2.6): VDD2V5_UV/OV are rw, the rest rwh.
inline constexpr uint32_t kDiag1Rwh = 0x8073;
inline constexpr uint32_t kDiag1Rw = 0x000C;
// GLOBAL_DIAG2 (5.3.2.7).
inline constexpr uint32_t kRegEccErr = 0x0002;
inline constexpr uint32_t kOtpEccErr = 0x0008;
inline constexpr uint32_t kOtpVirgin = 0x0010;
// DIAG_ERR_CHGR (5.3.2.12) and DIAG_WARN_CHGR (5.3.2.13), per channel byte.
inline constexpr uint32_t kOlsg = 0x01;
inline constexpr uint32_t kOl = 0x02;
inline constexpr uint32_t kOc = 0x04;
inline constexpr uint32_t kSg = 0x08;
inline constexpr uint32_t kOte = 0x10;
inline constexpr uint32_t kPwmRegWarn = 0x01;
inline constexpr uint32_t kIRegWarn = 0x02;
inline constexpr uint32_t kOtw = 0x04;
inline constexpr uint32_t kOlsgWarn = 0x08;
inline constexpr uint32_t kOlsgChkNok = 0x10;
// SFF_BIST (5.3.2.18).
inline constexpr uint32_t kBistEn = 0x01;
inline constexpr uint32_t kBistDone = 0x02;
inline constexpr uint32_t kBistFail = 0x04;
inline constexpr uint32_t kBistUerr = 0x08;
inline constexpr uint32_t kBistCerr = 0x10;
// CH_CONFIG (5.3.3.10).
inline constexpr uint32_t kOcDiagEn = 0x2000;
// FB_STAT (5.3.2.21), 22 bits.
inline constexpr uint32_t kStatInitDone = 1U << 21U;
inline constexpr uint32_t kStatSpiWdErr = 1U << 20U;
inline constexpr uint32_t kStatPorEvent = 1U << 10U;
inline constexpr uint32_t kStatResEvent = 1U << 9U;
// Identity (5.3.2.19).
inline constexpr uint8_t kManufacturer = 0xC1;
inline constexpr uint8_t kVersionA11 = 0xFC;
inline constexpr uint8_t kVersionA13 = 0xFE;
// Timing (Tables 6, 9, 18).
inline constexpr uint32_t kSystemHz = 28'000'000;
inline constexpr uint32_t kTporUs = 10'000;
inline constexpr uint32_t kTresnUs = 100;
inline constexpr uint32_t kWatchdogDivider = 16384; // f_SPI,WD = f_SYS / 2^14

/// CRC-8 SAE J1850 of a frame (5.1.2): polynomial 0x1D, initial value 0xFF, result XORed with
/// 0xFF, over bits 7..0, 15..8, 23..16 in that order. Written out here, not taken from the driver.
constexpr uint8_t Crc(uint32_t frame) noexcept {
  uint32_t crc = 0xFF;
  const std::array<uint32_t, 3> bytes = {frame & 0xFFU, (frame >> 8U) & 0xFFU,
                                         (frame >> 16U) & 0xFFU};
  for (const uint32_t b : bytes) {
    crc ^= b;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80U) != 0U ? ((crc << 1U) ^ 0x1DU) & 0xFFU : (crc << 1U) & 0xFFU;
    }
  }
  return static_cast<uint8_t>(crc ^ 0xFFU);
}

/// A 16-bit reply frame (5.2.3.3).
constexpr uint32_t Reply16(uint32_t data, bool write, uint32_t status) noexcept {
  const uint32_t w = ((status & 0x1FU) << 17U) | (write ? (1U << 16U) : 0U) | (data & 0xFFFFU);
  return (static_cast<uint32_t>(Crc(w)) << 24U) | w;
}
/// A 22-bit reply frame (5.2.3.4).
constexpr uint32_t Reply22(uint32_t data) noexcept {
  const uint32_t w = (1U << 22U) | (data & 0x3FFFFFU);
  return (static_cast<uint32_t>(Crc(w)) << 24U) | w;
}
/// A critical fault frame (5.2.3.5); bits 31..24 are don't care, here 0xA5.
constexpr uint32_t CriticalFrame(uint8_t flags) noexcept {
  return 0xA500'0000U | (2U << 22U) | flags;
}
} // namespace chip

/// The register and protocol model of one TLE92466ED.
class Tle92466edModel {
public:
  // --- Injection knobs (tests set these) ---
  uint8_t version = chip::kVersionA13; ///< ICVID.VERSION.
  uint8_t manufacturer = chip::kManufacturer;
  bool otp_virgin = false;           ///< OTP_VIRGIN after every reset.
  bool events_stick = false;         ///< POR_EVENT and RES_EVENT cannot be cleared.
  bool init_never = false;           ///< INIT_DONE never sets.
  uint32_t init_delay_us = 50;       ///< INIT_DONE after a reset or a clock change.
  bool bist_never_done = false;      ///< SMU_SLF_TST_DONE never sets.
  bool bist_fails = false;           ///< SMU_SLF_TST_FAIL set by the self-test.
  bool refuse_mission = false;       ///< A register ECC error the clear cannot remove.
  bool global_config_sticks = false; ///< GLOBAL_CONFIG writes are ignored (a broken write path).
  bool setpoint_ignored = false;     ///< SETPOINT writes are ignored.
  /// The reply to a write carries the register as it was before the write, not after it: section
  /// 5.2.1 allows either reading. The default is kDefaultReplyBeforeWrite, so a whole test binary
  /// can run under the other reading.
  bool write_reply_before_write = kDefaultReplyBeforeWrite;
  uint32_t vio_raw = 1448;  ///< FB_VOLTAGE1.VIO: 5.0 V.
  uint32_t vdd_raw = 1448;  ///< FB_VOLTAGE1.VDD: 5.0 V.
  uint32_t vbat_raw = 691;  ///< FB_VOLTAGE2.VBAT: 14.0 V.
  uint32_t temp_raw = 1314; ///< FB_VOLTAGE2.TEMP_VALUE: 25 degC.
  std::array<uint16_t, 3> chip_id = {0x1234, 0x5678, 0x9ABC};
  std::array<int32_t, 6> load_ua{}; ///< Measured current; 0: the setpoint.
  uint32_t duty_permyriad = 4000;   ///< Duty cycle the feedback reports.

  Tle92466edModel() noexcept {
    PowerOn();
  }

  /// Power-on reset: every register to its reset value, POR_EVENT set (4.1.1).
  void PowerOn() noexcept {
    resetRegisters();
    diag0_ = chip::kGlobalDiag0Reset;
    init_at_us_ = now_us_ + init_delay_us;
  }

  /// RESN level: false holds the chip in reset (4.2.3). Released, it runs its reset.
  void SetResn(bool high) noexcept {
    if (!high) {
      in_reset_ = true;
      return;
    }
    if (in_reset_) {
      in_reset_ = false;
      resetRegisters();
      diag0_ = chip::kGlobalDiag0Reset; // the reset value has both events set
      init_at_us_ = now_us_ + init_delay_us;
    }
  }
  /// EN level (4.2.3): low turns every channel off.
  void SetEn(bool high) noexcept {
    en_ = high;
  }
  /// DRV0 or DRV1 level.
  void SetDrv(uint32_t pin, bool high) noexcept {
    drv_[pin % 2U] = high;
  }

  /// Time passes: INIT_DONE, the SPI watchdog, the self-test and the feedback move on.
  void Elapse(uint32_t us) noexcept {
    now_us_ += us;
    if (bist_running_ && !bist_never_done && (now_us_ >= bist_done_at_us_)) {
      bist_running_ = false;
      bist_ = static_cast<uint16_t>(chip::kBistDone | chip::kBistUerr | chip::kBistCerr |
                                    (bist_fails ? chip::kBistFail : 0U));
      diag2_ = static_cast<uint16_t>(diag2_ | chip::kRegEccErr); // the tested error (4.9.8)
    }
    runWatchdog(us);
    runFeedback(us);
  }

  /// One 32-bit frame: returns MISO (the reply to the previous frame) and takes MOSI.
  uint32_t Frame(uint32_t mosi) noexcept {
    frames++;
    if (in_reset_) {
      return 0U; // SO is driven only when the logic runs
    }
    const uint32_t out = pending_;
    if (critical_) {
      pending_ = chip::CriticalFrame(critical_flags_);
      return out;
    }
    const bool write = (mosi & (1U << 16U)) != 0U;
    if (((global_config_ & chip::kCrcEn) != 0U) && (chip::Crc(mosi) != (mosi >> 24U))) {
      crc_errors++;
      pending_ = chip::Reply16(0, write, 2); // status 00010B (5.1.2)
      return out;
    }
    if (write) {
      const auto address = static_cast<uint16_t>((mosi >> 17U) & 0x7FU);
      const uint16_t before = Peek(address);
      const auto status = applyWrite(address, static_cast<uint16_t>(mosi & 0xFFFFU));
      pending_ = chip::Reply16(write_reply_before_write ? before : Peek(address), true, status);
    } else {
      const auto address = static_cast<uint16_t>(mosi & 0xFFFFU);
      pending_ = is22(address) ? chip::Reply22(readReg(address))
                               : chip::Reply16(readReg(address), false, 0);
    }
    return out;
  }

  /// The reply the next frame will carry, as a frame the chip ignores leaves it.
  [[nodiscard]] uint32_t Pending() const noexcept {
    return in_reset_ ? 0U : pending_;
  }

  // --- Fault injection ---

  /// Set GLOBAL_DIAG0 flags (4.1.2): a VDD or VIO fault, the clock, central OT or the watchdog
  /// also ends Mission Mode and clears every EN_CH.
  /// VIO and VDD under/overvoltage, the clock and the SPI watchdog end Mission Mode (4.1.2, 4.2.2,
  /// 5.2.2); COTERR only switches every stage off (4.11.1); COTWARN and VBAT are reported only.
  void SetGlobalFault(uint16_t bits) noexcept {
    diag0_ = static_cast<uint16_t>(diag0_ | bits);
    constexpr uint32_t kToConfig =
        chip::kVioUv | chip::kVioOv | chip::kVddUv | chip::kVddOv | chip::kClkNok | chip::kSpiWdErr;
    if ((static_cast<uint32_t>(bits) & kToConfig) != 0U) {
      leaveMission();
    } else if ((static_cast<uint32_t>(bits) & chip::kCotErr) != 0U) {
      offAll();
    } else {
      // VBAT and COTWARN: reported only.
    }
  }
  /// Set GLOBAL_DIAG1 flags (an internal supply). An internal under/overvoltage switches every
  /// stage off and ends Mission Mode, except VPRE_OV, which only switches them off (4.1.2).
  void SetInternalFault(uint16_t bits) noexcept {
    diag1_ = static_cast<uint16_t>(diag1_ | bits);
    if ((static_cast<uint32_t>(bits) & 0x003FU) != 0U) {
      leaveMission();
    } else if ((static_cast<uint32_t>(bits) & 0x0040U) != 0U) {
      offAll();
    } else {
      // HVADC_ERR: reported only.
    }
  }
  /// Set GLOBAL_DIAG2 flags.
  void SetDataFault(uint16_t bits) noexcept {
    diag2_ = static_cast<uint16_t>(diag2_ | bits);
    if ((bits & chip::kOtpEccErr) != 0U) {
      otp_clears_ = 0;
    }
  }
  /// A channel error (OLSG, OL, OC, SG, OTE): the channel is switched off and stays off.
  void SetChannelError(uint32_t ch, uint16_t bits) noexcept {
    err_[ch / 2U] = static_cast<uint16_t>(static_cast<uint32_t>(err_[ch / 2U]) |
                                          (static_cast<uint32_t>(bits) << (8U * (ch % 2U))));
    ch_ctrl_ = static_cast<uint16_t>(ch_ctrl_ & ~(1U << ch));
    if ((bits & chip::kOc) != 0U) {
      oc_clears_[ch] = 0;
      // An OC on either channel of a pair disables both (4.5.2).
      const uint32_t partner = partnerOf(ch);
      if (partner < 6U) {
        ch_ctrl_ = static_cast<uint16_t>(ch_ctrl_ & ~(1U << partner));
      }
    }
  }
  /// A channel warning.
  void SetChannelWarning(uint32_t ch, uint16_t bits) noexcept {
    warn_[ch / 2U] = static_cast<uint16_t>(static_cast<uint32_t>(warn_[ch / 2U]) |
                                           (static_cast<uint32_t>(bits) << (8U * (ch % 2U))));
  }
  /// The Critical Fault state (Figure 4): every reply is a critical fault frame until a reset.
  void EnterCritical(uint8_t flags) noexcept {
    critical_ = true;
    critical_flags_ = flags;
    leaveMission();
  }
  /// Write a raw SETPOINT, as only a test can (the driver never exceeds 0x6000).
  void SetTargetForTest(uint32_t ch, uint32_t target) noexcept {
    bank_[ch % 6U][chip::kSetpoint] = static_cast<uint16_t>(target & 0x7FFFU);
  }
  /// An overcurrent at the LOAD pin, seen by the next OFF-state OC check (4.9.6).
  void SetOvercurrentLoad(uint32_t ch, bool shorted) noexcept {
    oc_load_[ch] = shorted;
  }

  // --- Inspection (tests read these) ---

  [[nodiscard]] uint16_t Peek(uint16_t address) const noexcept {
    return static_cast<uint16_t>(peekWide(address) & 0xFFFFU);
  }
  [[nodiscard]] bool MissionMode() const noexcept {
    return (ch_ctrl_ & chip::kOpMode) != 0U;
  }
  [[nodiscard]] uint16_t ChannelReg(uint32_t ch, uint16_t offset) const noexcept {
    return bank_[ch][offset & 0xFU];
  }
  [[nodiscard]] bool InReset() const noexcept {
    return in_reset_;
  }
  [[nodiscard]] bool En() const noexcept {
    return en_;
  }
  [[nodiscard]] bool Drv(uint32_t pin) const noexcept {
    return drv_[pin % 2U];
  }
  [[nodiscard]] uint32_t WatchdogCount() const noexcept {
    return wd_count_;
  }
  [[nodiscard]] uint32_t NowUs() const noexcept {
    return now_us_;
  }
  /// FAULTN pin low (4.2.3): Critical Fault, RESN low, or an unmasked fault.
  [[nodiscard]] bool FaultPinLow() const noexcept {
    return critical_ || in_reset_ || unmaskedFault();
  }

  int frames = 0;     ///< Frames clocked while out of reset.
  int crc_errors = 0; ///< Frames refused for their CRC.
  int writes = 0;     ///< Write frames applied.

private:
  void resetRegisters() noexcept {
    ch_ctrl_ = 0;
    global_config_ = chip::kGlobalConfigReset;
    diag1_ = 0;
    diag2_ = otp_virgin ? chip::kOtpVirgin : 0;
    vbat_th_ = chip::kVbatThReset;
    fb_frz_ = 0;
    fb_upd_ = 0;
    wd_reload_ = chip::kWdReloadReset;
    wd_count_ = chip::kWdReloadReset;
    err_ = {};
    warn_ = {chip::kDiagWarnReset, chip::kDiagWarnReset, chip::kDiagWarnReset};
    mask_ = {chip::kFaultMask0Reset, chip::kFaultMask1Reset, chip::kFaultMask2Reset};
    clk_div_ = chip::kClkDivReset;
    bist_ = 0;
    bist_running_ = false;
    critical_ = false;
    for (auto& b : bank_) {
      b = {};
      b[chip::kCtrl] = chip::kCtrlReset;
      b[chip::kIntegratorLimit] = chip::kIntegratorLimitReset;
      b[chip::kChConfig] = chip::kChConfigReset;
      b[chip::kCtrlIntThresh] = chip::kCtrlIntThreshReset;
    }
    fb_ = {};
    meas_us_ = {};
    pending_ = 0; // the first reply after a reset carries nothing
  }

  static bool is22(uint16_t a) noexcept {
    return ((a >= 0x202U) && (a <= 0x207U)) || ((a >= 0x220U) && (a <= 0x276U));
  }

  /// Channel index of a bank address, or -1.
  static int channelOf(uint16_t a) noexcept {
    const uint32_t base = a & 0xFDF0U;
    for (uint32_t ch = 0; ch < 6; ++ch) {
      if (chip::kBase[ch] == base) {
        return static_cast<int>(ch);
      }
    }
    return -1;
  }

  [[nodiscard]] bool slaveOf(uint32_t ch) const noexcept {
    return ((ch == 3) && ((ch_ctrl_ & chip::kPar03) != 0U)) ||
           ((ch == 2) && ((ch_ctrl_ & chip::kPar12) != 0U)) ||
           ((ch == 5) && ((ch_ctrl_ & chip::kPar45) != 0U));
  }

  [[nodiscard]] uint32_t fbStat() const noexcept {
    uint32_t s = 0;
    for (uint32_t g = 0; g < 3; ++g) {
      s |= ((warn_[g] & 0x0F0FU) != 0U) ? (1U << g) : 0U;
      s |= ((warn_[g] & 0x1010U) != 0U) ? (1U << (3U + g)) : 0U;
      s |= (err_[g] != 0U) ? (1U << (14U + g)) : 0U;
    }
    s |= (diag0_ &
          (chip::kClkNok | chip::kCotErr | chip::kCotWarn | chip::kResEvent | chip::kPorEvent));
    s |= ((diag2_ & (chip::kOtpEccErr | chip::kOtpVirgin)) != 0U || (diag1_ & 0x8000U) != 0U)
             ? (1U << 11U)
             : 0U;
    s |= ((diag0_ & 0x003FU) != 0U) ? (1U << 12U) : 0U;
    s |= ((diag1_ & 0x007FU) != 0U) ? (1U << 13U) : 0U;
    s |= ((diag0_ & chip::kSpiWdErr) != 0U) ? chip::kStatSpiWdErr : 0U;
    s |= (!init_never && (now_us_ >= init_at_us_)) ? chip::kStatInitDone : 0U;
    return s;
  }

  [[nodiscard]] bool unmaskedFault() const noexcept {
    for (uint32_t ch = 0; ch < 6; ++ch) {
      const uint32_t e = (static_cast<uint32_t>(err_[ch / 2U]) >> (8U * (ch % 2U))) & 0x1FU;
      const uint32_t w = (static_cast<uint32_t>(warn_[ch / 2U]) >> (8U * (ch % 2U))) & 0x0FU;
      if (((e != 0U) && ((static_cast<uint32_t>(mask_[0]) >> ch) & 1U) != 0U) ||
          ((w != 0U) && ((static_cast<uint32_t>(mask_[1]) >> ch) & 1U) != 0U)) {
        return true;
      }
    }
    return ((diag0_ & chip::kSpiWdErr) != 0U) && ((mask_[2] & 0x8000U) != 0U);
  }

  [[nodiscard]] uint32_t peekWide(uint16_t a) const noexcept {
    switch (a) {
    case chip::kChCtrl:
      return ch_ctrl_;
    case chip::kGlobalConfig:
      return global_config_;
    case chip::kGlobalDiag0:
      return diag0_;
    case chip::kGlobalDiag1:
      return diag1_;
    case chip::kGlobalDiag2:
      return diag2_;
    case chip::kVbatTh:
      return vbat_th_;
    case chip::kFbFrz:
      return fb_frz_;
    case chip::kFbUpd:
      return fb_upd_;
    case chip::kWdReload:
      return wd_reload_;
    case chip::kFaultMask0:
    case chip::kFaultMask1:
    case chip::kFaultMask2:
      return mask_[a - chip::kFaultMask0];
    case chip::kClkDiv:
      return clk_div_;
    case chip::kSffBist:
      return bist_;
    case chip::kIcvid:
      return (static_cast<uint32_t>(manufacturer) << 8U) | version;
    case chip::kPinStat:
      return (drv_[0] ? 1U : 0U) | (drv_[1] ? 2U : 0U) | (en_ ? 0x10U : 0U) |
             (unmaskedFault() ? 0U : 0x20U) | (FaultPinLow() ? 0U : 0x40U);
    case chip::kFbStat:
      return fbStat();
    case chip::kFbVoltage1:
      return ((vdd_raw & 0x7FFU) << 11U) | (vio_raw & 0x7FFU);
    case chip::kFbVoltage2:
      return ((vbat_raw & 0x7FFU) << 11U) | (temp_raw & 0x7FFU);
    case chip::kChipId0:
    case chip::kChipId0 + 1:
    case chip::kChipId0 + 2:
      return chip_id[a - chip::kChipId0];
    default:
      break;
    }
    if ((a >= chip::kDiagErr0) && (a < chip::kDiagErr0 + 3U)) {
      return err_[a - chip::kDiagErr0];
    }
    if ((a >= chip::kDiagWarn0) && (a < chip::kDiagWarn0 + 3U)) {
      return warn_[a - chip::kDiagWarn0];
    }
    const int ch = channelOf(a);
    if (ch < 0) {
      return 0;
    }
    const auto c = static_cast<uint32_t>(ch);
    if (a >= 0x200U) {
      return fb_[c][(a & 0xFU) % 7U];
    }
    const bool slave_blank =
        slaveOf(c) && (((a & 0xFU) == chip::kMode) || ((a & 0xFU) == chip::kSetpoint));
    return slave_blank ? 0U : bank_[c][a & 0xFU]; // a slave reads back zero (4.5.2)
  }

  /// A read and its side effect: OLSG_WARN_CHK_NOK is cleared by reading (4.9.3).
  uint32_t readReg(uint16_t a) noexcept {
    const uint32_t v = peekWide(a);
    if ((a >= chip::kDiagWarn0) && (a < chip::kDiagWarn0 + 3U)) {
      warn_[a - chip::kDiagWarn0] = static_cast<uint16_t>(warn_[a - chip::kDiagWarn0] & ~0x1010U);
    }
    return v;
  }

  void leaveMission() noexcept {
    ch_ctrl_ = static_cast<uint16_t>(ch_ctrl_ & ~(chip::kOpMode | 0x3FU));
  }

  [[nodiscard]] bool missionAllowed() const noexcept {
    // Figure 4, Config Mode to Mission Mode: no SPI watchdog, clock, VIO, VDD or VBAT fault, no
    // central OT, no internal supply fault, no REG_ECC_ERR, no channel OTW or OTE.
    const bool supplies = (diag0_ & 0x41FFU) == 0U;
    const bool internal = (diag1_ & 0x003FU) == 0U && (diag2_ & chip::kRegEccErr) == 0U;
    bool channels = true;
    for (uint32_t g = 0; g < 3; ++g) {
      channels = channels && ((err_[g] & 0x1010U) == 0U) && ((warn_[g] & 0x0404U) == 0U);
    }
    return !refuse_mission && supplies && internal && channels &&
           ((fbStat() & chip::kStatInitDone) != 0U);
  }

  /// Every EN_CH off, Mission Mode kept.
  void offAll() noexcept {
    ch_ctrl_ = static_cast<uint16_t>(static_cast<uint32_t>(ch_ctrl_) & ~0x3FU);
  }

  /// The other channel of a channel's active pair, or 6 when it is not paired (4.5.2).
  [[nodiscard]] uint32_t partnerOf(uint32_t ch) const noexcept {
    constexpr std::array<uint32_t, 6> kPartner = {3, 2, 1, 0, 5, 4};
    constexpr std::array<uint32_t, 6> kPairBit = {chip::kPar03, chip::kPar12, chip::kPar12,
                                                  chip::kPar03, chip::kPar45, chip::kPar45};
    return ((ch_ctrl_ & kPairBit[ch % 6U]) != 0U) ? kPartner[ch % 6U] : 6U;
  }

  /// The status of a write frame (5.2.3.3): 0, or 3 for a read-only register.
  uint32_t applyWrite(uint16_t a, uint16_t v) noexcept {
    writes++;
    if ((a == chip::kFbUpd) || (a == 0x0001U)) {
      return 3;
    }
    if (writeCentral(a, v)) {
      return 0;
    }
    if ((a >= chip::kDiagErr0) && (a < chip::kDiagErr0 + 3U)) {
      clearErr(a - chip::kDiagErr0, v);
      return 0;
    }
    if ((a >= chip::kDiagWarn0) && (a < chip::kDiagWarn0 + 3U)) {
      uint16_t& w = warn_[a - chip::kDiagWarn0];
      const auto cleared_warn =
          static_cast<uint32_t>(w) & static_cast<uint32_t>(v) & 0x0808U; // OLSG_WARN cleared
      w = static_cast<uint16_t>(w & ~static_cast<uint32_t>(v));
      w = static_cast<uint16_t>(static_cast<uint32_t>(w) |
                                (cleared_warn << 1U)); // CHK_NOK set after a cleared warning
      return 0;
    }
    const int ch = channelOf(a);
    if (ch >= 0) {
      writeChannel(static_cast<uint32_t>(ch), static_cast<uint16_t>(a & 0xFU), v);
    }
    return 0;
  }

  bool writeCentral(uint16_t a, uint16_t v) noexcept {
    const bool config_mode = !MissionMode();
    switch (a) {
    case chip::kChCtrl:
      writeChCtrl(v);
      return true;
    case chip::kGlobalConfig:
      if (config_mode && !global_config_sticks) {
        global_config_ = static_cast<uint16_t>(v & 0x7037U);
      }
      return true;
    case chip::kGlobalDiag0:
      diag0_ = static_cast<uint16_t>(
          diag0_ & ~static_cast<uint32_t>(v & chip::kDiag0Rwh)); // rwh: a 1 clears
      if (!events_stick) {
        // rw events take the value written
        diag0_ = static_cast<uint16_t>((diag0_ & ~0x0600U) | (v & 0x0600U));
      }
      return true;
    case chip::kGlobalDiag1:
      diag1_ = static_cast<uint16_t>((diag1_ & ~static_cast<uint32_t>(v & chip::kDiag1Rwh) &
                                      ~static_cast<uint32_t>(chip::kDiag1Rw)) |
                                     (v & chip::kDiag1Rw));
      return true;
    case chip::kGlobalDiag2:
      writeDiag2(v);
      return true;
    case chip::kVbatTh:
      vbat_th_ = v;
      return true;
    case chip::kFbFrz:
      fb_upd_ = static_cast<uint16_t>(
          fb_upd_ & (v | ~static_cast<uint32_t>(fb_frz_))); // releasing clears FB_UPD (4.10.5)
      fb_frz_ = static_cast<uint16_t>(v & 0x3FU);
      return true;
    case chip::kWdReload:
      wd_reload_ = static_cast<uint16_t>(v & 0x7FFU);
      wd_count_ = wd_reload_;
      if (wd_reload_ != 0U) {
        diag0_ = static_cast<uint16_t>(diag0_ & ~static_cast<uint32_t>(chip::kSpiWdErr)); // 5.2.2
      }
      return true;
    case chip::kFaultMask0:
    case chip::kFaultMask1:
    case chip::kFaultMask2:
      mask_[a - chip::kFaultMask0] = v;
      return true;
    case chip::kClkDiv:
      if (config_mode) {
        if (v != clk_div_) {
          init_at_us_ = now_us_ + init_delay_us; // INIT_DONE cleared until the PLL locks (4.2.1)
        }
        clk_div_ = v;
      }
      return true;
    case chip::kSffBist:
      writeBist(v, config_mode);
      return true;
    default:
      break;
    }
    return false;
  }

  void writeChCtrl(uint16_t v) noexcept {
    const bool was_config = !MissionMode();
    uint16_t next = ch_ctrl_;
    if (was_config) {
      next = static_cast<uint16_t>((next & ~0x7000U) | (v & 0x7000U)); // CH_PAR in Config Mode
    }
    const bool mission = ((v & chip::kOpMode) != 0U) && (MissionMode() || missionAllowed());
    next = static_cast<uint16_t>((next & ~static_cast<uint32_t>(chip::kOpMode)) |
                                 (mission ? chip::kOpMode : 0U));
    uint16_t en = 0;
    if (mission) {
      for (uint32_t ch = 0; ch < 6; ++ch) {
        const bool want = ((static_cast<uint32_t>(v) >> ch) & 1U) != 0U;
        const uint32_t e = (static_cast<uint32_t>(err_[ch / 2U]) >> (8U * (ch % 2U))) & 0x1FU;
        ch_ctrl_ = next; // slaveOf() looks at the new pairs
        if (want && !slaveOf(ch) && (e == 0U)) {
          en = static_cast<uint16_t>(en | (1U << ch));
          ocCheck(ch);
        }
      }
    }
    ch_ctrl_ = static_cast<uint16_t>((next & ~0x3FU) | en);
    en = static_cast<uint16_t>(ch_ctrl_ & 0x3FU);
    for (uint32_t ch = 0; ch < 6; ++ch) {
      if (((static_cast<uint32_t>(en) >> ch) & 1U) == 0U) {
        meas_us_[ch] = 0;
      }
    }
  }

  /// The OFF-state overcurrent check (4.9.6): with OC_DIAG_EN set, enabling the channel turns
  /// the stage on for t_OCon; a short sets OC and clears EN_CH. OC_DIAG_EN resets afterwards.
  void ocCheck(uint32_t ch) noexcept {
    uint16_t& cfg = bank_[ch][chip::kChConfig];
    if ((cfg & chip::kOcDiagEn) == 0U) {
      return;
    }
    cfg = static_cast<uint16_t>(cfg & ~static_cast<uint32_t>(chip::kOcDiagEn));
    if (oc_load_[ch]) {
      SetChannelError(ch, chip::kOc);
    }
  }

  void writeDiag2(uint16_t v) noexcept {
    diag2_ = static_cast<uint16_t>(
        diag2_ & ~static_cast<uint32_t>(v & (chip::kRegEccErr | chip::kOtpVirgin)));
    if ((v & chip::kOtpEccErr) != 0U) {
      // Two clear commands (4.9.7).
      otp_clears_++;
      if (otp_clears_ >= 2) {
        diag2_ = static_cast<uint16_t>(diag2_ & ~static_cast<uint32_t>(chip::kOtpEccErr));
      }
    }
    if (refuse_mission) {
      diag2_ = static_cast<uint16_t>(diag2_ | chip::kRegEccErr);
    }
  }

  void clearErr(uint32_t g, uint16_t v) noexcept {
    uint16_t keep = 0;
    for (uint32_t half = 0; half < 2; ++half) {
      const uint32_t ch = (2U * g) + half;
      const uint32_t shift = 8U * half;
      if (((static_cast<uint32_t>(v) >> shift) & chip::kOc) != 0U) {
        oc_clears_[ch]++;
        if (oc_clears_[ch] < 2) {
          keep = static_cast<uint16_t>(keep | (chip::kOc << shift)); // OC needs two (4.9.6)
        }
      }
    }
    err_[g] = static_cast<uint16_t>((static_cast<uint32_t>(err_[g]) & ~static_cast<uint32_t>(v)) |
                                    (static_cast<uint32_t>(err_[g]) & keep));
  }

  void writeBist(uint16_t v, bool config_mode) noexcept {
    if (!config_mode) {
      return;
    }
    if ((v & chip::kBistEn) != 0U) {
      bist_ = chip::kBistEn;
      bist_running_ = true;
      bist_done_at_us_ = now_us_ + 500U;
    } else {
      bist_ = 0; // results cleared by a write of 0 (4.9.8)
      bist_running_ = false;
    }
  }

  void writeChannel(uint32_t ch, uint16_t offset, uint16_t v) noexcept {
    if ((offset == chip::kMode) && (MissionMode() || slaveOf(ch))) {
      return; // MODE only in Config Mode; a slave ignores it (4.4.1, 4.5.2)
    }
    if ((offset == chip::kSetpoint) && (slaveOf(ch) || setpoint_ignored)) {
      return;
    }
    if ((offset > chip::kChConfig) && (offset < chip::kMode)) {
      return; // no register
    }
    bank_[ch][offset] = v;
  }

  void runWatchdog(uint32_t us) noexcept {
    if ((global_config_ & chip::kSpiWdEn) == 0U) {
      wd_us_ = 0;
      return;
    }
    const uint64_t tick_us = (uint64_t{chip::kWatchdogDivider} * 1'000'000U) / chip::kSystemHz;
    wd_us_ += us;
    while ((wd_us_ >= tick_us) && (wd_count_ > 0U)) {
      wd_us_ -= tick_us;
      wd_count_--;
      if (wd_count_ == 0U) {
        SetGlobalFault(chip::kSpiWdErr); // 1 to 0: error, Config Mode (5.2.2)
      }
    }
  }

  void runFeedback(uint32_t us) noexcept {
    for (uint32_t ch = 0; ch < 6; ++ch) {
      const auto& b = bank_[ch];
      const uint32_t mant = b[chip::kDitherClkDiv] & 0x3FFU;
      const uint32_t exp = (static_cast<uint32_t>(b[chip::kDitherClkDiv]) >> 10U) & 0xFU;
      const bool on = MissionMode() && en_ &&
                      (((static_cast<uint32_t>(ch_ctrl_) >> ch) & 1U) != 0U) &&
                      ((b[chip::kSetpoint] & 0x7FFFU) != 0U) && (mant != 0U);
      if (!on) {
        continue;
      }
      // T_meas = T_Dither = (4 STEPS + 2 FLAT) t_ref_clk, or t_ref_clk (Equations 7, 8, note).
      const uint32_t steps = (static_cast<uint32_t>(b[chip::kDitherStep]) >> 8U) & 0xFFU;
      const uint32_t flat = b[chip::kDitherStep] & 0xFFU;
      const uint32_t refs = ((steps == 0U) && (flat == 0U)) ? 1U : ((4U * steps) + (2U * flat));
      const uint64_t cycles = (static_cast<uint64_t>(mant) << exp) * refs;
      const uint64_t period_us = (cycles * 1'000'000U) / chip::kSystemHz;
      meas_us_[ch] += us;
      if ((period_us == 0U) || (meas_us_[ch] < period_us)) {
        continue;
      }
      meas_us_[ch] = 0;
      if (((static_cast<uint32_t>(fb_frz_) >> ch) & 1U) == 0U) {
        publish(ch, cycles);
      }
    }
  }

  /// New feedback values for one measurement period of `cycles` f_SYS cycles (Equations 18-22).
  void publish(uint32_t ch, uint64_t cycles) noexcept {
    uint32_t e = 0;
    while (((cycles >> e) > 2047U) && (e < 15U)) {
      e++;
    }
    const auto tp = static_cast<uint32_t>(cycles >> e);
    const bool par = ((ch == 0) && ((ch_ctrl_ & chip::kPar03) != 0U)) ||
                     ((ch == 1) && ((ch_ctrl_ & chip::kPar12) != 0U)) ||
                     ((ch == 4) && ((ch_ctrl_ & chip::kPar45) != 0U));
    // TARGET values above 0x6000 are saturated (5.3.3.3).
    const int64_t raw_target = bank_[ch][chip::kSetpoint] & 0x7FFF;
    const int64_t target = (raw_target > 0x6000) ? 0x6000 : raw_target;
    const int64_t ua =
        (load_ua[ch] != 0) ? load_ua[ch] : (target * (par ? 4'000'000 : 2'000'000)) / 32767;
    const int64_t i_mant = (ua * static_cast<int64_t>(tp)) / 4'000'000;
    const uint32_t to = (tp * duty_permyriad) / 10000U;
    const uint32_t vbat_mant = (vbat_raw * tp) / 2047U; // VBAT = 41.47 V * mant / TP
    fb_[ch][0] = ((to & 0x7FFU) << 11U) | (tp & 0x7FFU);
    fb_[ch][1] = ((fb_[ch][1] + (1U << 18U)) & (3U << 18U)) | (e << 12U) | (vbat_mant & 0x7FFU);
    fb_[ch][2] = (e << 12U) | (static_cast<uint32_t>(i_mant) & 0xFFFU);
    const int64_t i9 = (ua * 511) / 4'000'000;
    fb_[ch][3] = ((static_cast<uint32_t>(i9 + 8) & 0x3FFU) << 10U) |
                 (static_cast<uint32_t>(i9 - 8) & 0x3FFU);
    fb_[ch][6] = (110U << 10U) | 108U; // PWM periods of about 1 ms
    fb_upd_ = static_cast<uint16_t>(fb_upd_ | (1U << ch));
  }

  uint32_t now_us_ = 0;
  uint32_t init_at_us_ = 0;
  uint32_t bist_done_at_us_ = 0;
  uint64_t wd_us_ = 0;
  uint32_t pending_ = 0;
  uint16_t ch_ctrl_ = 0;
  uint16_t global_config_ = chip::kGlobalConfigReset;
  uint16_t diag0_ = chip::kGlobalDiag0Reset;
  uint16_t diag1_ = 0;
  uint16_t diag2_ = 0;
  uint16_t vbat_th_ = chip::kVbatThReset;
  uint16_t fb_frz_ = 0;
  uint16_t fb_upd_ = 0;
  uint16_t wd_reload_ = chip::kWdReloadReset;
  uint32_t wd_count_ = chip::kWdReloadReset;
  uint16_t clk_div_ = chip::kClkDivReset;
  uint16_t bist_ = 0;
  bool bist_running_ = false;
  bool critical_ = false;
  uint8_t critical_flags_ = 0;
  bool in_reset_ = false;
  bool en_ = false;
  std::array<bool, 2> drv_{};
  int otp_clears_ = 0;
  std::array<int, 6> oc_clears_{};
  std::array<bool, 6> oc_load_{};
  std::array<uint16_t, 3> err_{};
  std::array<uint16_t, 3> warn_{};
  std::array<uint16_t, 3> mask_{};
  std::array<std::array<uint16_t, 16>, 6> bank_{};
  std::array<std::array<uint32_t, 7>, 6> fb_{};
  std::array<uint64_t, 6> meas_us_{};
};

} // namespace tle92466ed_test

#include "mock_tle92466ed_bus.hpp"

#endif // TLE92466ED_MOCK_TLE92466ED_HPP
