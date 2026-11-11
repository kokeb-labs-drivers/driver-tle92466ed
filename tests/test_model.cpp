/**
 * @file test_model.cpp
 * @brief The chip model's register map against the driver's, at compile time (handbook KB-TST-030).
 *
 * tests/mock_tle92466ed.hpp is written from the datasheet without the driver's headers, so a
 * constant the driver gets wrong is not copied into the model. These checks make the two agree, or
 * the build fails: a difference is a mistake in one of them, found against the datasheet section
 * the model names.
 */
#include <cstdint>

#include "mock_tle92466ed.hpp"
#include "test_harness.hpp"
#include "tle92466ed.hpp"

namespace {

namespace chip = tle92466ed_test::chip;
namespace reg = tle92466ed::reg;

// --8<-- [start:cross-check]
// Central registers (Table 19).
static_assert(chip::kChCtrl == reg::kChCtrl && chip::kGlobalConfig == reg::kGlobalConfig &&
              chip::kGlobalDiag0 == reg::kGlobalDiag0 && chip::kGlobalDiag1 == reg::kGlobalDiag1 &&
              chip::kGlobalDiag2 == reg::kGlobalDiag2 && chip::kVbatTh == reg::kVbatTh);
static_assert(chip::kFbFrz == reg::kFbFrz && chip::kFbUpd == reg::kFbUpd &&
              chip::kWdReload == reg::kWdReload && chip::kDiagErr0 == reg::kDiagErrChgr0 &&
              chip::kDiagWarn0 == reg::kDiagWarnChgr0 && chip::kFaultMask0 == reg::kFaultMask0 &&
              chip::kFaultMask1 == reg::kFaultMask1 && chip::kFaultMask2 == reg::kFaultMask2);
static_assert(chip::kClkDiv == reg::kClkDiv && chip::kSffBist == reg::kSffBist &&
              chip::kIcvid == reg::kIcvid && chip::kPinStat == reg::kPinStat &&
              chip::kFbStat == reg::kFbStat && chip::kFbVoltage1 == reg::kFbVoltage1 &&
              chip::kFbVoltage2 == reg::kFbVoltage2 && chip::kChipId0 == reg::kChipId0);
// Channel banks (Tables 24, 25).
static_assert(chip::kBase == reg::kChannelBase);
static_assert(chip::kSetpoint == reg::kSetpoint && chip::kCtrl == reg::kCtrl &&
              chip::kPeriod == reg::kPeriod && chip::kIntegratorLimit == reg::kIntegratorLimit &&
              chip::kDitherClkDiv == reg::kDitherClkDiv && chip::kDitherStep == reg::kDitherStep &&
              chip::kDitherCtrl == reg::kDitherCtrl && chip::kChConfig == reg::kChConfig &&
              chip::kMode == reg::kMode && chip::kTon == reg::kTon &&
              chip::kCtrlIntThresh == reg::kCtrlIntThresh);
static_assert(chip::kFbDc == reg::kFbDc && chip::kFbVbat == reg::kFbVbat &&
              chip::kFbIAvg == reg::kFbIAvg && chip::kFbIminImax == reg::kFbIminImax &&
              chip::kFbIAvgS16 == reg::kFbIAvgS16 && chip::kFbIntThresh == reg::kFbIntThresh &&
              chip::kFbPeriodMinMax == reg::kFbPeriodMinMax);
// Reset values (section 5.3).
static_assert(chip::kGlobalConfigReset == reg::kGlobalConfigReset &&
              chip::kGlobalDiag0Reset == reg::kDiag0Reset &&
              chip::kVbatThReset == reg::kVbatThReset &&
              chip::kWdReloadReset == reg::kWdReloadReset &&
              chip::kDiagWarnReset == reg::kDiagWarnReset);
static_assert(chip::kFaultMask0Reset == reg::kMask0Reset &&
              chip::kFaultMask1Reset == reg::kMask1Reset &&
              chip::kFaultMask2Reset == reg::kMask2Reset &&
              chip::kClkDivReset == reg::kClkDivReset && chip::kCtrlReset == reg::kCtrlReset &&
              chip::kIntegratorLimitReset == reg::kIntegratorLimitReset &&
              chip::kChConfigReset == reg::kChConfigReset &&
              chip::kCtrlIntThreshReset == reg::kCtrlIntThreshReset);
// Fields the driver writes and the model acts on.
static_assert(chip::kOpMode == reg::kChCtrlOpMode && chip::kPar03 == reg::kChCtrlPar03 &&
              chip::kPar12 == reg::kChCtrlPar12 && chip::kPar45 == reg::kChCtrlPar45);
static_assert(chip::kClkWdEn == reg::kGlobalConfigClkWdEn &&
              chip::kSpiWdEn == reg::kGlobalConfigSpiWdEn &&
              chip::kCrcEn == reg::kGlobalConfigCrcEn && chip::kVioSel == reg::kGlobalConfigVioSel);
static_assert(chip::kDiag0Rwh == reg::kDiag0Flags && chip::kPorEvent == reg::kDiag0PorEvent &&
              chip::kResEvent == reg::kDiag0ResEvent && chip::kDiag1Rwh == reg::kDiag1RwhFlags &&
              chip::kRegEccErr == reg::kDiag2RegEccErr &&
              chip::kOtpEccErr == reg::kDiag2OtpEccErr && chip::kOtpVirgin == reg::kDiag2OtpVirgin);
static_assert(chip::kOlsg == reg::kErrOlsg && chip::kOl == reg::kErrOl &&
              chip::kOc == reg::kErrOc && chip::kSg == reg::kErrSg && chip::kOte == reg::kErrOte &&
              chip::kOlsgChkNok == reg::kWarnOlsgChkNok);
static_assert(chip::kBistEn == reg::kBistEn && chip::kBistDone == reg::kBistDone &&
              chip::kBistFail == reg::kBistFail && chip::kOcDiagEn == reg::kOcDiagEn);
static_assert(chip::kStatInitDone == reg::kStatInitDone &&
              chip::kStatSpiWdErr == reg::kStatSpiWdErr &&
              chip::kStatPorEvent == reg::kStatPorEvent &&
              chip::kStatResEvent == reg::kStatResEvent);
// Identity and timing (sections 5.3.2.19, 4.1.3, 5.1.4).
static_assert(chip::kManufacturer == tle92466ed::kManufacturerId &&
              chip::kVersionA11 == tle92466ed::kVersionA11 &&
              chip::kVersionA13 == tle92466ed::kVersionA13);
static_assert(chip::kSystemHz == tle92466ed::kInternalClockHz &&
              chip::kTporUs == tle92466ed::kPowerOnResetUs &&
              chip::kTresnUs == tle92466ed::kResetReleaseUs &&
              chip::kWatchdogDivider == tle92466ed::kWatchdogDivider);
// --8<-- [end:cross-check]

// The two CRC implementations and reply builders agree.
static_assert(chip::Crc(0x00'0000U) == reg::FrameCrc(0x00'0000U));
static_assert(chip::Reply16(0xC1FEU, false, 0U) == tle92466ed::detail::Reply16(0xC1FEU, false, 0U));

} // namespace

TEST_CASE("Model: the CRCs of the model and the driver agree on every 24-bit pattern sampled") {
  for (uint32_t w = 0; w < 0x0100'0000U; w += 0x1F3BU) {
    CHECK_EQ(chip::Crc(w), reg::FrameCrc(w));
  }
}

TEST_CASE("Model: Is22Bit and the model's reply widths agree on every address") {
  for (uint32_t a = 0; a < 0x0300U; ++a) {
    const auto address = static_cast<uint16_t>(a);
    tle92466ed_test::Tle92466edModel m;
    (void)m.Frame(tle92466ed::detail::EncodeRead(address));
    const uint32_t reply = m.Frame(tle92466ed::detail::EncodeRead(reg::kIcvid));
    const bool is22 = ((reply >> 22U) & 3U) == 1U;
    CHECK_EQ(is22, reg::Is22Bit(address));
  }
}
