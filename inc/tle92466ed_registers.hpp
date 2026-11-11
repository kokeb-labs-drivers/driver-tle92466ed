/**
 * @file tle92466ed_registers.hpp
 * @brief TLE92466ED register map: addresses, bit fields, reset values and the frame CRC
 * (datasheet section 5.3).
 * @copyright Copyright (c) 2026 The driver-tle92466ed authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Registers hold 16 bits; the feedback and status registers at offset 0x200 and above answer with
 * 22 bits (section 5.2.3.4). Central registers sit at fixed addresses (Table 19); each channel has
 * a bank of control registers at its base address and a bank of read-only feedback registers at the
 * base plus 0x200 (Tables 24 and 25). A write frame carries a 7-bit address (section 5.2.3.1), so
 * only the registers below 0x80 are writable; a read frame carries the full 16-bit address
 * (section 5.2.3.2).
 */
#ifndef TLE92466ED_REGISTERS_HPP
#define TLE92466ED_REGISTERS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "tle92466ed_detail.hpp"

/** @brief The TLE92466ED register map. */
namespace tle92466ed::reg {

/**
 * @defgroup tle92466ed_registers Register map
 * @brief Addresses, fields and reset values.
 * @{
 */

/// @name Central registers (section 5.3.2, Table 19)
/// @{
inline constexpr uint16_t kChCtrl = 0x0000U;        ///< CH_CTRL (rw): enables, pairs, OP_MODE.
inline constexpr uint16_t kGlobalConfig = 0x0002U;  ///< GLOBAL_CONFIG (rw, Config Mode only).
inline constexpr uint16_t kGlobalDiag0 = 0x0003U;   ///< GLOBAL_DIAG0: supply, clock, OT, events.
inline constexpr uint16_t kGlobalDiag1 = 0x0004U;   ///< GLOBAL_DIAG1: internal supplies, ADC.
inline constexpr uint16_t kGlobalDiag2 = 0x0005U;   ///< GLOBAL_DIAG2: register and OTP ECC.
inline constexpr uint16_t kVbatTh = 0x0006U;        ///< VBAT_TH: VBAT under/overvoltage codes.
inline constexpr uint16_t kFbFrz = 0x0007U;         ///< FB_FRZ: freeze the feedback of a channel.
inline constexpr uint16_t kFbUpd = 0x0008U;         ///< FB_UPD (rh): new feedback per channel.
inline constexpr uint16_t kWdReload = 0x0009U;      ///< WD_RELOAD: SPI watchdog reload value.
inline constexpr uint16_t kDiagErrChgr0 = 0x000AU;  ///< DIAG_ERR_CHGR0..2 at 0x000A + group.
inline constexpr uint16_t kDiagWarnChgr0 = 0x0010U; ///< DIAG_WARN_CHGR0..2 at 0x0010 + group.
inline constexpr uint16_t kFaultMask0 = 0x0016U;    ///< FAULT_MASK0: channel errors, supplies.
inline constexpr uint16_t kFaultMask1 = 0x0017U;    ///< FAULT_MASK1: warnings, OT, clock.
inline constexpr uint16_t kFaultMask2 = 0x0018U;    ///< FAULT_MASK2: data error, SPI watchdog.
inline constexpr uint16_t kClkDiv = 0x0019U;        ///< CLK_DIV (Config Mode only): PLL, source.
inline constexpr uint16_t kSffBist = 0x003FU;       ///< SFF_BIST (Config Mode only): self-test.
inline constexpr uint16_t kIcvid = 0x0200U;         ///< ICVID (rh): manufacturer and version.
inline constexpr uint16_t kPinStat = 0x0201U;       ///< PIN_STAT (rh): DRV0/1, EN, FAULTN levels.
inline constexpr uint16_t kFbStat = 0x0202U;        ///< FB_STAT (rh, 22 bits): status summary.
inline constexpr uint16_t kFbVoltage1 = 0x0203U;    ///< FB_VOLTAGE1 (rh, 22 bits): VIO, VDD.
inline constexpr uint16_t kFbVoltage2 = 0x0204U;    ///< FB_VOLTAGE2 (rh, 22 bits): VBAT, temp.
inline constexpr uint16_t kChipId0 = 0x0205U;       ///< CHIPID0..2 (rh, 22 bits): unique ID.
/// Channel groups of the DIAG_ERR_CHGR and DIAG_WARN_CHGR registers: group g holds channels 2g
/// (bits 7..0) and 2g + 1 (bits 15..8) (sections 5.3.2.12, 5.3.2.13).
inline constexpr uint16_t kChannelGroups = 3U;
/// @}

/// @name Channel registers: offsets from the channel's base (section 5.3.3, Table 24)
/// @{
inline constexpr uint16_t kSetpoint = 0x0000U;        ///< SETPOINT: TARGET, AUTO_LIMIT_DIS.
inline constexpr uint16_t kCtrl = 0x0001U;            ///< CTRL: ICC threshold, OLSG warning.
inline constexpr uint16_t kPeriod = 0x0002U;          ///< PERIOD: PWM frequency controller.
inline constexpr uint16_t kIntegratorLimit = 0x0003U; ///< INTEGRATOR_LIMIT: ICC limits.
inline constexpr uint16_t kDitherClkDiv = 0x0004U;    ///< DITHER_CLK_DIV: t_ref_clk, sync.
inline constexpr uint16_t kDitherStep = 0x0005U;      ///< DITHER_STEP: STEPS, FLAT.
inline constexpr uint16_t kDitherCtrl = 0x0006U;      ///< DITHER_CTRL: STEP_SIZE, fast feedback.
inline constexpr uint16_t kChConfig = 0x0007U;        ///< CH_CONFIG: slew, diagnosis, OL limits.
inline constexpr uint16_t kMode = 0x000CU;            ///< MODE (Config Mode only): CH_MODE.
inline constexpr uint16_t kTon = 0x000DU;             ///< TON: on-time, OLSG time-out.
inline constexpr uint16_t kCtrlIntThresh = 0x000EU;   ///< CTRL_INT_THRESH: ICC start threshold.
inline constexpr uint16_t kFbDc = 0x0200U;            ///< FB_DC (22 bits): TP_MANT, TO_MANT.
inline constexpr uint16_t kFbVbat = 0x0201U;          ///< FB_VBAT (22 bits): VBAT, EXP, counters.
inline constexpr uint16_t kFbIAvg = 0x0202U;          ///< FB_I_AVG (22 bits): I_AVG_MANT, EXP.
inline constexpr uint16_t kFbIminImax = 0x0203U;      ///< FB_IMIN_IMAX (22 bits).
inline constexpr uint16_t kFbIAvgS16 = 0x0204U;       ///< FB_I_AVG_s16 (22 bits).
inline constexpr uint16_t kFbIntThresh = 0x0205U;     ///< FB_INT_THRESH (22 bits).
inline constexpr uint16_t kFbPeriodMinMax = 0x0206U;  ///< FB_PERIOD_MIN_MAX (22 bits).
/// Base address of each channel's register bank, CH0 to CH5 (Table 25).
inline constexpr std::array<uint16_t, 6> kChannelBase = {0x0040U, 0x0050U, 0x0060U,
                                                         0x0070U, 0x0020U, 0x0030U};
/// @}

/// @name CH_CTRL (section 5.3.2.3); reset value 0x0000
/// @{
inline constexpr uint16_t kChCtrlEnMask = 0x003FU;   ///< EN_CH0..5, bits 5..0.
inline constexpr uint16_t kChCtrlPar45 = 0x1000U;    ///< CH_PAR_4_5 (Config Mode only).
inline constexpr uint16_t kChCtrlPar03 = 0x2000U;    ///< CH_PAR_0_3 (Config Mode only).
inline constexpr uint16_t kChCtrlPar12 = 0x4000U;    ///< CH_PAR_1_2 (Config Mode only).
inline constexpr uint16_t kChCtrlOpMode = 0x8000U;   ///< OP_MODE: 0 Config Mode, 1 Mission Mode.
inline constexpr uint16_t kChCtrlParMask = 0x7000U;  ///< CH_PAR_4_5, CH_PAR_0_3, CH_PAR_1_2.
inline constexpr uint16_t kChCtrlWritable = 0xF03FU; ///< Every bit that is not reserved.
/// @}

/// @name GLOBAL_CONFIG (section 5.3.2.4); reset value 0x4005
/// @{
inline constexpr uint16_t kGlobalConfigClkWdEn = 0x0001U; ///< CLK_WD_EN: clock watchdog.
inline constexpr uint16_t kGlobalConfigSpiWdEn = 0x0002U; ///< SPI_WD_EN: SPI watchdog.
inline constexpr uint16_t kGlobalConfigCrcEn = 0x0004U;   ///< CRC_EN: check MOSI frame CRCs.
inline constexpr uint16_t kGlobalConfigVioSel = 0x4000U;  ///< VIO_SEL: 0 = 3.3 V, 1 = 5.0 V.
inline constexpr uint16_t kGlobalConfigReset = 0x4005U;   ///< After reset.
/// Bits the driver writes; the test bits (V1V5_UV/OV_TEST, OT_TEST, UV_OV_SWAP) stay 0.
inline constexpr uint16_t kGlobalConfigWritable = 0x4007U;
/// @}

/// @name GLOBAL_DIAG0 (section 5.3.2.5); reset value 0x0600
/// @{
inline constexpr uint16_t kDiag0VbatUv = 0x0001U;   ///< VBAT_UV (rwh).
inline constexpr uint16_t kDiag0VbatOv = 0x0002U;   ///< VBAT_OV (rwh).
inline constexpr uint16_t kDiag0VioUv = 0x0004U;    ///< VIO_UV (rwh).
inline constexpr uint16_t kDiag0VioOv = 0x0008U;    ///< VIO_OV (rwh).
inline constexpr uint16_t kDiag0VddUv = 0x0010U;    ///< VDD_UV (rwh).
inline constexpr uint16_t kDiag0VddOv = 0x0020U;    ///< VDD_OV (rwh).
inline constexpr uint16_t kDiag0ClkNok = 0x0040U;   ///< CLK_NOK (rwh).
inline constexpr uint16_t kDiag0CotErr = 0x0080U;   ///< COTERR (rwh).
inline constexpr uint16_t kDiag0CotWarn = 0x0100U;  ///< COTWARN (rwh).
inline constexpr uint16_t kDiag0ResEvent = 0x0200U; ///< RES_EVENT (rw): RESN reset happened.
inline constexpr uint16_t kDiag0PorEvent = 0x0400U; ///< POR_EVENT (rw): power-on reset happened.
inline constexpr uint16_t kDiag0SpiWdErr = 0x4000U; ///< SPI_WD_ERR (rwh).
inline constexpr uint16_t kDiag0Reset = 0x0600U;    ///< After reset: both events set.
/// The rwh fault flags, cleared by a write of 1 (section 4.1.2).
inline constexpr uint16_t kDiag0Flags = 0x41FFU;
/// @}

/// @name GLOBAL_DIAG1 (section 5.3.2.6); reset value 0x0000
/// @{
inline constexpr uint16_t kDiag1VrIrefUv = 0x0001U; ///< VR_IREF_UV (rwh).
inline constexpr uint16_t kDiag1VrIrefOv = 0x0002U; ///< VR_IREF_OV (rwh).
inline constexpr uint16_t kDiag1Vdd2v5Uv = 0x0004U; ///< VDD2V5_UV (rw).
inline constexpr uint16_t kDiag1Vdd2v5Ov = 0x0008U; ///< VDD2V5_OV (rw).
inline constexpr uint16_t kDiag1RefUv = 0x0010U;    ///< REF_UV (rwh).
inline constexpr uint16_t kDiag1RefOv = 0x0020U;    ///< REF_OV (rwh).
inline constexpr uint16_t kDiag1VpreOv = 0x0040U;   ///< VPRE_OV (rwh).
inline constexpr uint16_t kDiag1HvadcErr = 0x8000U; ///< HVADC_ERR (rwh).
/// Every flag of GLOBAL_DIAG1.
inline constexpr uint16_t kDiag1Flags = 0x807FU;
/// The rwh flags, cleared by a write of 1; the two rw flags (VDD2V5) are cleared by a 0.
inline constexpr uint16_t kDiag1RwhFlags = 0x8073U;
/// @}

/// @name GLOBAL_DIAG2 (section 5.3.2.7, Table 21); reset value 0x0000 (bit 0 undefined)
/// @{
inline constexpr uint16_t kDiag2RegEccErr = 0x0002U; ///< REG_ECC_ERR (rwh).
inline constexpr uint16_t kDiag2OtpEccErr = 0x0008U; ///< OTP_ECC_ERR (rwh): two clears (4.9.7).
inline constexpr uint16_t kDiag2OtpVirgin = 0x0010U; ///< OTP_VIRGIN (rwh): must always be 0.
inline constexpr uint16_t kDiag2Flags = 0x001AU;     ///< Every flag of GLOBAL_DIAG2.
/// @}

/// @name VBAT_TH (section 5.3.2.8); reset value 0xFF19
/// @{
using VbatUvTh = detail::Field<uint16_t, 0U, 8U>; ///< VBAT_UV_TH, 162.08 mV per count.
using VbatOvTh = detail::Field<uint16_t, 8U, 8U>; ///< VBAT_OV_TH, 162.08 mV per count.
inline constexpr uint16_t kVbatThReset = 0xFF19U; ///< After reset: UV 25 (4.05 V), OV 255.
/// @}

/// @name FB_FRZ, FB_UPD (sections 5.3.2.9, 5.3.2.10); reset values 0x0000
/// @{
inline constexpr uint16_t kChannelBits = 0x003FU; ///< One bit per channel, CH0 at bit 0.
/// @}

/// @name WD_RELOAD (section 5.3.2.11); reset value 0x0001
/// @{
using WdTime = detail::Field<uint16_t, 0U, 11U>;    ///< WD_TIME, counts of 2^14 / f_SYS.
inline constexpr uint16_t kWdReloadReset = 0x0001U; ///< After reset: one count.
/// @}

/// @name DIAG_ERR_CHGR, per channel byte (section 5.3.2.12); reset value 0x0000
/// @{
inline constexpr uint16_t kErrOlsg = 0x0001U;  ///< OLSG: open load or short to ground, on-state.
inline constexpr uint16_t kErrOl = 0x0002U;    ///< OL: open load, off-state.
inline constexpr uint16_t kErrOc = 0x0004U;    ///< OC: overcurrent; two clears (4.9.6, 4.11.2).
inline constexpr uint16_t kErrSg = 0x0008U;    ///< SG: short to ground, off-state.
inline constexpr uint16_t kErrOte = 0x0010U;   ///< OTE: channel overtemperature error.
inline constexpr uint16_t kErrFlags = 0x001FU; ///< Every flag of one channel.
/// @}

/// @name DIAG_WARN_CHGR, per channel byte (section 5.3.2.13); reset value 0x1010
/// @{
inline constexpr uint16_t kWarnPwmReg = 0x0001U;     ///< PWM_REG_WARN: ICC PWM regulation.
inline constexpr uint16_t kWarnIReg = 0x0002U;       ///< I_REG_WARN: ICC current regulation.
inline constexpr uint16_t kWarnOtw = 0x0004U;        ///< OTW: channel overtemperature warning.
inline constexpr uint16_t kWarnOlsg = 0x0008U;       ///< OLSG_WARN.
inline constexpr uint16_t kWarnOlsgChkNok = 0x0010U; ///< OLSG_WARN_CHK_NOK: cleared by a read.
inline constexpr uint16_t kWarnFlags = 0x001FU;      ///< Every flag of one channel.
inline constexpr uint16_t kDiagWarnReset = 0x1010U;  ///< After reset: both checks "not possible".
/// @}

/// @name FAULT_MASK0..2 (sections 5.3.2.14 to 5.3.2.16)
/// @{
inline constexpr uint16_t kMask0EnPin = 0x2000U;   ///< EN_PIN_MASK.
inline constexpr uint16_t kMask0SupInt = 0x4000U;  ///< SUP_NOK_INT_MASK.
inline constexpr uint16_t kMask0SupExt = 0x8000U;  ///< SUP_NOK_EXT_MASK.
inline constexpr uint16_t kMask0Reset = 0xC03FU;   ///< After reset.
inline constexpr uint16_t kMask1CotWarn = 0x1000U; ///< COTWARN_MASK.
inline constexpr uint16_t kMask1CotErr = 0x2000U;  ///< COTERR_MASK.
inline constexpr uint16_t kMask1ClkLow = 0x4000U;  ///< CLK_LOW_MASK.
inline constexpr uint16_t kMask1Reset = 0x703FU;   ///< After reset.
inline constexpr uint16_t kMask2DataErr = 0x4000U; ///< DATA_ERR_MASK.
inline constexpr uint16_t kMask2SpiWd = 0x8000U;   ///< SPI_WD_MASK.
inline constexpr uint16_t kMask2Reset = 0xC000U;   ///< After reset.
/// @}

/// @name CLK_DIV (section 5.3.2.17); reset value 0x0438
/// @{
using PllFbDiv = detail::Field<uint16_t, 0U, 9U>;  ///< PLL_FBDIV.
using PllRefDiv = detail::Field<uint16_t, 9U, 6U>; ///< PLL_REFDIV.
inline constexpr uint16_t kClkDivExtClk = 0x8000U; ///< EXT_CLK: 1 = clock from the CLK pin.
inline constexpr uint16_t kClkDivReset = 0x0438U;  ///< After reset: REFDIV 2, FBDIV 56, internal.
/// @}

/// @name SFF_BIST (section 5.3.2.18); reset value 0x0000
/// @{
inline constexpr uint16_t kBistEn = 0x0001U;   ///< SMU_SLF_TST_EN: start the self-test.
inline constexpr uint16_t kBistDone = 0x0002U; ///< SMU_SLF_TST_DONE.
inline constexpr uint16_t kBistFail = 0x0004U; ///< SMU_SLF_TST_FAIL.
inline constexpr uint16_t kBistUerr = 0x0008U; ///< SMU_SLF_TST_UERR: uncorrectable errors tested.
inline constexpr uint16_t kBistCerr = 0x0010U; ///< SMU_SLF_TST_CERR: correctable errors tested.
/// @}

/// @name ICVID, PIN_STAT (sections 5.3.2.19, 5.3.2.20)
/// @{
using IcVersion = detail::Field<uint16_t, 0U, 8U>;      ///< VERSION.
using IcManufacturer = detail::Field<uint16_t, 8U, 8U>; ///< MANUFACTURER.
inline constexpr uint16_t kPinDrv0 = 0x0001U;           ///< DRV0 level.
inline constexpr uint16_t kPinDrv1 = 0x0002U;           ///< DRV1 level.
inline constexpr uint16_t kPinEn = 0x0010U;             ///< EN level.
inline constexpr uint16_t kPinFaultn = 0x0020U;         ///< FAULTN internal: 0 = fault.
inline constexpr uint16_t kPinFaultnFb = 0x0040U;       ///< FAULTN pin level.
/// @}

/// @name FB_STAT (section 5.3.2.21, Table 23), 22 bits
/// @{
inline constexpr uint32_t kStatWarnGroups = 0x000007U;   ///< DIAG_WARN_CHGR0..2 summary.
inline constexpr uint32_t kStatChkNokGroups = 0x000038U; ///< OLSG_WARN_CHK_NOK summary.
inline constexpr uint32_t kStatClkNok = 0x000040U;       ///< CLK_NOK.
inline constexpr uint32_t kStatCotErr = 0x000080U;       ///< COTERR.
inline constexpr uint32_t kStatCotWarn = 0x000100U;      ///< COTWARN.
inline constexpr uint32_t kStatResEvent = 0x000200U;     ///< RES_event.
inline constexpr uint32_t kStatPorEvent = 0x000400U;     ///< POR_event.
inline constexpr uint32_t kStatDataErr = 0x000800U;      ///< DATA_err: OTP ECC, OTP virgin, HVADC.
inline constexpr uint32_t kStatSupNokExt = 0x001000U;    ///< SUP_NOK_EXT.
inline constexpr uint32_t kStatSupNokInt = 0x002000U;    ///< SUP_NOK_INT.
inline constexpr uint32_t kStatErrGroups = 0x01C000U;    ///< DIAG_ERR_CHGR0..2 summary.
inline constexpr uint32_t kStatSpiWdErr = 0x100000U;     ///< SPI_WD_ERR.
inline constexpr uint32_t kStatInitDone = 0x200000U;     ///< INIT_DONE.
/// Faults that keep the chip out of Mission Mode (Figure 4).
inline constexpr uint32_t kStatMissionBlockers = 0x1031C0U;
/// @}

/// @name SETPOINT (section 5.3.3.3)
/// @{
using Target = detail::Field<uint16_t, 0U, 15U>;       ///< TARGET, 2 A / (2^15 - 1) per count.
inline constexpr uint16_t kAutoLimitDis = 0x8000U;     ///< AUTO_LIMIT_DIS.
inline constexpr uint32_t kTargetSaturation = 0x6000U; ///< Higher TARGET values are saturated.
/// @}

/// @name CTRL (section 5.3.3.4); reset value 0x4600
/// @{
using MinIntThresh = detail::Field<uint16_t, 0U, 8U>;   ///< MIN_INT_THRESH, signed.
inline constexpr uint16_t kPwmPeriodCalcMode = 0x0100U; ///< PWM_PERIOD_CALC_MODE.
using OlsgWarnWindow = detail::Field<uint16_t, 9U, 5U>; ///< OLSG_WARN_WINDOW.
inline constexpr uint16_t kOlsgWarnEn = 0x4000U;        ///< OLSG_WARN_EN.
inline constexpr uint16_t kCtrlReset = 0x4600U;         ///< After reset.
/// @}

/// @name PERIOD (section 5.3.3.5); reset value 0x0000
/// @{
using PeriodMant = detail::Field<uint16_t, 0U, 8U>;    ///< PERIOD_MANT; 0 stops the controller.
using PeriodExp = detail::Field<uint16_t, 8U, 3U>;     ///< PERIOD_EXP.
inline constexpr uint16_t kLowFreqRangeEn = 0x0800U;   ///< LOW_FREQ_RANGE_EN: period times 8.
using PwmCtrlParam = detail::Field<uint16_t, 12U, 4U>; ///< PWM_CTRL_PARAM, the gain KI.
/// @}

/// @name INTEGRATOR_LIMIT (section 5.3.3.6); reset value 0x43FF
/// @{
using LimValueAbs = detail::Field<uint16_t, 0U, 10U>;      ///< LIM_VALUE_ABS.
using AutoLimValueAbs = detail::Field<uint16_t, 10U, 5U>;  ///< AUTO_LIM_VALUE_ABS.
inline constexpr uint16_t kIntegratorLimitReset = 0x43FFU; ///< After reset: 1023 and 16.
/// @}

/// @name DITHER_CLK_DIV, DITHER_STEP, DITHER_CTRL (sections 5.3.3.7 to 5.3.3.9)
/// @{
using DitherMant = detail::Field<uint16_t, 0U, 10U>;       ///< MANT of t_ref_clk.
using DitherExp = detail::Field<uint16_t, 10U, 4U>;        ///< EXP of t_ref_clk and TON.
inline constexpr uint16_t kDitherPwmSyncEn = 0x4000U;      ///< DITHER_PWM_SYNC_EN.
inline constexpr uint16_t kDitherSetpointSyncEn = 0x8000U; ///< DITHER_SETPOINT_SYNC_EN.
using DitherFlat = detail::Field<uint16_t, 0U, 8U>;        ///< FLAT.
using DitherSteps = detail::Field<uint16_t, 8U, 8U>;       ///< STEPS.
using DitherStepSize = detail::Field<uint16_t, 0U, 12U>;   ///< STEP_SIZE.
inline constexpr uint16_t kDeepDither = 0x2000U;           ///< DEEP_DITHER.
using FastMeas = detail::Field<uint16_t, 14U, 2U>;         ///< FAST_MEAS.
/// @}

/// @name CH_CONFIG (section 5.3.3.10); reset value 0x0003
/// @{
using SlewRate = detail::Field<uint16_t, 0U, 2U>;   ///< SLEWR.
using IDiag = detail::Field<uint16_t, 2U, 2U>;      ///< I_DIAG.
using OlTh = detail::Field<uint16_t, 4U, 3U>;       ///< OL_TH, eighths of the setpoint.
using OlThFixed = detail::Field<uint16_t, 7U, 6U>;  ///< OL_TH_FIXED.
inline constexpr uint16_t kOcDiagEn = 0x2000U;      ///< OC_DIAG_EN (self-clearing, rwh).
using OffDiagCh = detail::Field<uint16_t, 14U, 2U>; ///< OFF_DIAG_CH.
inline constexpr uint32_t kOffDiagDisabled = 3U;    ///< OFF_DIAG_CH = 11B: diagnosis off.
inline constexpr uint16_t kChConfigReset = 0x0003U; ///< After reset: 10 V/us, diagnosis on.
/// @}

/// @name MODE, TON, CTRL_INT_THRESH (sections 5.3.3.11 to 5.3.3.13)
/// @{
using ChMode = detail::Field<uint16_t, 0U, 4U>;       ///< CH_MODE.
inline constexpr uint32_t kChModeMeasurement = 0xCU;  ///< CH_MODE = CH: free-running measurement.
using TonMant = detail::Field<uint16_t, 0U, 10U>;     ///< TON_MANT.
using OlsgTimeout = detail::Field<uint16_t, 10U, 6U>; ///< OLSG_TIMEOUT.
using IntThresh = detail::Field<uint16_t, 0U, 9U>;    ///< INT_THRESH.
inline constexpr uint16_t kCtrlIntThreshReset = 0x0003U; ///< After reset.
/// @}

/// @name Feedback registers (sections 5.3.3.14 to 5.3.3.20), 22 bits
/// @{
using TpMant = detail::Field<uint32_t, 0U, 11U>;          ///< FB_DC.TP_MANT: measurement period.
using ToMant = detail::Field<uint32_t, 11U, 11U>;         ///< FB_DC.TO_MANT: on-time.
using VbatAvgMant = detail::Field<uint32_t, 0U, 11U>;     ///< FB_VBAT.VBAT_AVG_MANT.
using FbExp = detail::Field<uint32_t, 12U, 4U>;           ///< FB_VBAT.EXP, FB_I_AVG.EXP.
using DitherQuadCnt = detail::Field<uint32_t, 16U, 2U>;   ///< FB_VBAT.DITHER_QUAD_CNT.
using DitherPeriodCnt = detail::Field<uint32_t, 18U, 2U>; ///< FB_VBAT.DITHER_PERIOD_CNT.
using IAvgMant = detail::Field<uint32_t, 0U, 12U>;        ///< FB_I_AVG.I_AVG_MANT, signed.
using IMin = detail::Field<uint32_t, 0U, 10U>;            ///< FB_IMIN_IMAX.IMIN, signed.
using IMax = detail::Field<uint32_t, 10U, 10U>;           ///< FB_IMIN_IMAX.IMAX, signed.
using PMin = detail::Field<uint32_t, 0U, 10U>;            ///< FB_PERIOD_MIN_MAX.PMIN.
using PMax = detail::Field<uint32_t, 10U, 10U>;           ///< FB_PERIOD_MIN_MAX.PMAX.
/// @}

/// @name FB_VOLTAGE1, FB_VOLTAGE2 (sections 5.3.2.22, 5.3.2.23), 22 bits
/// @{
using VioRaw = detail::Field<uint32_t, 0U, 11U>;   ///< VIO, 3.4534 mV per count.
using VddRaw = detail::Field<uint32_t, 11U, 11U>;  ///< VDD, 3.4534 mV per count.
using TempRaw = detail::Field<uint32_t, 0U, 11U>;  ///< TEMP_VALUE.
using VbatRaw = detail::Field<uint32_t, 11U, 11U>; ///< VBAT, 41.47 V / (2^11 - 1) per count.
/// @}

/// @name SPI frames (section 5.2.3)
/// @{
inline constexpr uint32_t kFrameRw = 0x0001'0000U;    ///< R/W bit 16: 1 = write.
inline constexpr uint32_t kWriteAddressShift = 17U;   ///< Write frame: address in bits 23..17.
inline constexpr uint32_t kWriteAddressMask = 0x7FU;  ///< A write frame's 7-bit address.
inline constexpr uint32_t kReplyModeShift = 22U;      ///< Reply mode in bits 23..22.
inline constexpr uint32_t kReplyMode16 = 0U;          ///< 00B: 16-bit reply frame.
inline constexpr uint32_t kReplyMode22 = 1U;          ///< 01B: 22-bit reply frame.
inline constexpr uint32_t kReplyModeCritical = 2U;    ///< 10B: critical fault frame.
inline constexpr uint32_t kReplyStatusShift = 17U;    ///< 16-bit reply: status in bits 21..17.
inline constexpr uint32_t kReplyStatusMask = 0x1FU;   ///< Five status bits.
inline constexpr uint32_t kStatusFrameError = 1U;     ///< SPI frame error.
inline constexpr uint32_t kStatusCrcError = 2U;       ///< The chip found a wrong MOSI CRC.
inline constexpr uint32_t kStatusWriteReadOnly = 3U;  ///< Write to a read-only register.
inline constexpr uint32_t kData16Mask = 0x0000'FFFFU; ///< Data of a 16-bit reply.
inline constexpr uint32_t kData22Mask = 0x003F'FFFFU; ///< Data of a 22-bit reply.
inline constexpr uint32_t kCrcPolynomial = 0x1DU;     ///< SAE J1850: x^8 + x^4 + x^3 + x^2 + 1.
inline constexpr uint32_t kCrcInit = 0xFFU;           ///< Initial value.
inline constexpr uint32_t kCrcXorOut = 0xFFU;         ///< The result is XORed with 0xFF.
/// @}

/**
 * @brief CRC-8 SAE J1850 of a frame's lower 24 bits, in the chip's byte order: bits 7..0 first,
 * then 15..8, then 23..16 (section 5.1.2).
 * @param frame The frame; bits 31..24 are ignored.
 * @return The CRC byte that belongs in bits 31..24.
 */
[[nodiscard]] constexpr uint8_t FrameCrc(uint32_t frame) noexcept {
  uint32_t crc = kCrcInit; // all arithmetic in uint32_t (KB-CPP20-021)
  for (uint32_t byte = 0U; byte < 3U; ++byte) {
    crc ^= (frame >> (8U * byte)) & 0xFFU;
    for (uint32_t bit = 0U; bit < 8U; ++bit) {
      crc = ((crc & 0x80U) != 0U) ? ((crc << 1U) ^ kCrcPolynomial) : (crc << 1U);
      crc &= 0xFFU;
    }
  }
  return static_cast<uint8_t>(crc ^ kCrcXorOut);
}

// Worked by hand from section 5.1.2: a read of CH_CTRL is the 24-bit word 0x000000 and a read of
// ICVID 0x000200 (bytes 0x00, 0x02, 0x00 in the CRC's order).
static_assert(FrameCrc(0x00'0000U) == 0xF1U);
static_assert(FrameCrc(0x00'0200U) == 0x69U);
static_assert(FrameCrc(0xFF00'0000U) == FrameCrc(0x00'0000U)); // bits 31..24 do not count

/**
 * @brief Whether a read of `address` is answered with a 22-bit reply (section 5.2.3.4): the central
 * registers from FB_STAT (0x0202) on and every channel feedback register. The datasheet draws each
 * of them with a bits 21..16 row and a six-digit reset value.
 * @param address The register address.
 * @return true for a 22-bit register, false for a 16-bit one.
 */
[[nodiscard]] constexpr bool Is22Bit(uint16_t address) noexcept {
  return (static_cast<uint32_t>(address) >= 0x0202U) &&
         ((static_cast<uint32_t>(address) <= 0x0207U) ||
          ((static_cast<uint32_t>(address) >= 0x0220U) &&
           (static_cast<uint32_t>(address) <= 0x0276U)));
}
static_assert(!Is22Bit(kIcvid) && !Is22Bit(kPinStat) && Is22Bit(kFbStat) && Is22Bit(0x0207U));
static_assert(!Is22Bit(0x0208U) && Is22Bit(0x0240U + kFbPeriodMinMax - 0x0200U));

/**
 * @brief Whether `address` names a register of the datasheet: a central register of Table 19, or a
 * channel register of Table 24 at a base of Table 25.
 * @param address The address.
 * @return true when the chip has a register there.
 */
[[nodiscard]] constexpr bool IsRegister(uint16_t address) noexcept {
  const auto a = static_cast<uint32_t>(address);
  // CH_CTRL to CLK_DIV, one bit per listed address of Table 19.
  constexpr uint32_t kCentral = 0x03C7'1FFDU;
  if (a <= 0x0019U) {
    return ((kCentral >> a) & 1U) != 0U;
  }
  if ((a == kSffBist) || ((a >= kIcvid) && (a <= 0x0207U))) {
    return true;
  }
  const uint32_t bank = a & 0xFDF0U; // the channel base, from either bank
  const uint32_t offset = a & 0x000FU;
  const bool is_base = (bank >= 0x0020U) && (bank <= 0x0070U);
  const bool control =
      (a < 0x0200U) && ((offset <= 0x7U) || ((offset >= 0xCU) && (offset <= 0xEU)));
  const bool feedback = (a >= 0x0200U) && (offset <= 0x6U);
  return is_base && (control || feedback);
}
static_assert(IsRegister(kChCtrl) && !IsRegister(0x0001U) && IsRegister(kSffBist));
static_assert(IsRegister(0x0040U) && IsRegister(0x004EU) && !IsRegister(0x0048U));
static_assert(IsRegister(0x0246U) && !IsRegister(0x0247U) && !IsRegister(0x0010U + 3U));

/** @} */

} // namespace tle92466ed::reg

#endif // TLE92466ED_REGISTERS_HPP
