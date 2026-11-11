/**
 * @file tle92466ed.ipp
 * @brief TLE92466ED driver implementation, included by `tle92466ed.hpp`.
 * @copyright Copyright (c) 2026 The driver-tle92466ed authors. SPDX-License-Identifier: MIT
 *
 * Section numbers refer to the TLE92466ED datasheet, Rev. 1.2.
 */
#ifndef TLE92466ED_IPP
#define TLE92466ED_IPP

#include <algorithm>

namespace tle92466ed {

// -------------------------------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> TLE92466ED<Port>::EnsureInitialized() noexcept {
  if (initialized_) {
    return Result<void>::Success();
  }
  if (auto r = initialize(); !r) {
    return r;
  }
  initialized_ = true;
  detail::Log(bus_.GetPort(), LogLevel::Info, "tle92466ed: initialised, Config Mode");
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::initialize() noexcept {
  using detail::operator""_u16;
  const Config config = config_;
  if (Validate(config) != Error::Ok) {
    return fail(Error::InvalidArgument, "tle92466ed: invalid Config");
  }
  if (!bus_.GetPort().EnsureInitialized()) {
    return fail(Error::Bus, "tle92466ed: the port could not bring the bus up");
  }
  reset_watch_ = false;
  bus_.WatchReset(false);
  bus_.ExpectIdentity(false);
  bus_.Desync();
  ch_ctrl_ = 0U;
  wanted_ = 0U;
  dropped_ = 0U;     // a new start: the record does not carry over
  par_unknown_ = 0U; // the start writes every register again
  config_unknown_ = 0U;
  dither_unknown_ = 0U;
  global_unknown_ = false;
  maybe_mission_ = false;
  std::fill(setpoint_.begin(), setpoint_.end(), 0_u16);
  if (auto r = resetChip(); !r) {
    return r;
  }
  if (auto r = waitInitDone(); !r) {
    return r;
  }
  if (auto r = identify(); !r) {
    return r;
  }
  // Config Mode with every channel off before anything else: without RESN the chip keeps what an
  // earlier run wrote (section 4.3.1).
  if (auto r = write(reg::kChCtrl, 0_u16); !r) {
    return fail(r.error, "tle92466ed: CH_CTRL write failed");
  }
  if (auto r = takeStartupFaults(); !r) {
    return r;
  }
  if (auto r = writeAll(config); !r) {
    return fail(r.error, "tle92466ed: configuration write failed");
  }
  if (auto r = armResetWatch(); !r) {
    return r;
  }
  latched_ = Faults{};
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::resetChip() noexcept {
  using detail::operator""_u16;
  Port& port = bus_.GetPort();
  if constexpr (detail::HasGpioSet<Port>) {
    // EN low first, so no channel can switch while the registers return to their reset values;
    // then RESN low (Reset State, section 4.2.3) and released. The logic is ready after t_RESN.
    if (!port.GpioSet(CtrlPin::Enable, GpioSignal::INACTIVE) ||
        !port.GpioSet(CtrlPin::Reset, GpioSignal::ACTIVE)) {
      return fail(Error::Bus, "tle92466ed: EN or RESN could not be driven");
    }
    detail::DelayUs(port, kResetPulseUs);
    if (!port.GpioSet(CtrlPin::Reset, GpioSignal::INACTIVE)) {
      return fail(Error::Bus, "tle92466ed: RESN could not be released");
    }
    detail::DelayUs(port, kResetReleaseUs); // t_RESN, Table 9
  } else {
    // No RESN: a chip an earlier run left in Mission Mode is switched off at once, before the
    // wait (section 4.3.1). A chip still in its power-on reset may not answer, so only a port
    // failure counts here; the start-up writes CH_CTRL again, checked, once INIT_DONE reads 1.
    const auto off = bus_.Access(reg::kChCtrl, true, 0_u16);
    bus_.Desync();
    if (off.error == Error::Bus) {
      return fail(Error::Bus, "tle92466ed: CH_CTRL write failed");
    }
    // The supplies may have come up with the MCU, so wait out t_POR (Table 9).
    detail::DelayUs(port, kPowerOnResetUs);
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::waitInitDone() noexcept {
  // INIT_DONE = 1: clock and logic ready (sections 4.2.1, 5.3.2.21). A correct ICVID is not
  // readiness (handbook KB-CPP-124); the bound is the validated timeout, capped (KB-CPP20-058).
  const uint32_t timeout_us = std::min(config_.init_timeout_us, kMaxInitTimeoutUs);
  const uint32_t polls = (timeout_us / kPollIntervalUs) + 1U;
  for (uint32_t poll = 0U; poll < polls; ++poll) {
    const auto stat = read(reg::kFbStat);
    if (!stat) {
      return fail(stat.error, "tle92466ed: FB_STAT read failed");
    }
    if ((stat.value & reg::kStatInitDone) != 0U) {
      return Result<void>::Success();
    }
    detail::DelayUs(bus_.GetPort(), kPollIntervalUs);
  }
  return fail(Error::Timeout, "tle92466ed: INIT_DONE stayed 0");
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::identify() noexcept {
  const auto id = read(reg::kIcvid);
  if (!id) {
    return fail(id.error, "tle92466ed: ICVID read failed");
  }
  const auto word = static_cast<uint16_t>(id.value);
  const uint32_t version = reg::IcVersion::Extract(word);
  if ((reg::IcManufacturer::Extract(word) != kManufacturerId) ||
      ((version != kVersionA11) && (version != kVersionA13))) {
    return fail(Error::DeviceIdMismatch, "tle92466ed: ICVID is not a TLE92466ED");
  }
  version_ = static_cast<uint8_t>(version);
  bus_.ExpectIdentity(true); // from here on every ICVID anchor must read 0xC1
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::takeStartupFaults() noexcept {
  // Faults latched before the start are reported, then cleared, so later flags are new (handbook
  // KB-CPP-058). OTP_VIRGIN "must always be 0" (section 4.9.7): the trim is missing.
  const auto faults = readFaults();
  if (!faults) {
    return fail(faults.error, "tle92466ed: diagnosis read failed");
  }
  startup_ = faults.value;
  if (startup_.Has(GlobalFault::OtpVirgin)) {
    return fail(Error::DeviceFault, "tle92466ed: OTP_VIRGIN set, the chip is not trimmed");
  }
  if (auto r = clearRead(startup_); !r) {
    return fail(r.error, "tle92466ed: diagnosis clear failed");
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::writeAll(const Config& config) noexcept {
  if (auto r = writeGlobal(config); !r) {
    return r;
  }
  // The pairs in Config Mode, every channel off (section 4.5.2).
  if (auto r = writeChCtrl(detail::PairBits(config), Verify::None); !r) {
    return r;
  }
  for (uint32_t ch = 0U; ch < kChannelCount; ++ch) {
    const auto channel = static_cast<Channel>(ch);
    if (detail::IsActiveSlave(config, channel)) {
      continue; // the chip ignores a slave's MODE and SETPOINT (section 4.5.2)
    }
    if (auto r = writeChannel(channel, config.channels[ch], Verify::None); !r) {
      return r;
    }
  }
  return armWatchdog(config);
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::armWatchdog(const Config& config) noexcept {
  using detail::operator""_u16;
  // The last writes of the start: WD_RELOAD, then SPI_WD_EN, so no earlier write can let the
  // watchdog expire; its reset value 1 would expire within one count (section 5.2.2).
  if (auto r = write(reg::kWdReload, reg::WdTime::Insert(0_u16, config.watchdog_reload)); !r) {
    return r;
  }
  if (!config.spi_watchdog) {
    return Result<void>::Success();
  }
  return writeVerified(reg::kGlobalConfig, detail::GlobalConfigValue(config),
                       reg::kGlobalConfigWritable, Verify::ReadBack);
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::writeGlobal(const Config& config) noexcept {
  // The clock source first: a change clears INIT_DONE until the PLL has locked (section 4.2.1).
  if (auto r = write(reg::kClkDiv, detail::ClkDivValue(config)); !r) {
    return r;
  }
  if (auto r = waitInitDone(); !r) {
    return r;
  }
  // GLOBAL_CONFIG with the SPI watchdog off (it is armed last), read back: the write path proven
  // at every start (handbook KB-CPP-117).
  Config quiet = config;
  quiet.spi_watchdog = false;
  if (auto r = writeVerified(reg::kGlobalConfig, detail::GlobalConfigValue(quiet),
                             reg::kGlobalConfigWritable, Verify::ReadBack);
      !r) {
    return r;
  }
  const std::array<detail::RegWrite, 4> rest = {{
      {.address = reg::kVbatTh, .value = detail::VbatThValue(config)},
      {.address = reg::kFaultMask0, .value = detail::FaultMask0Value(config)},
      {.address = reg::kFaultMask1, .value = detail::FaultMask1Value(config)},
      {.address = reg::kFaultMask2, .value = detail::FaultMask2Value(config)},
  }};
  for (const detail::RegWrite& w : rest) {
    if (auto r = write(w.address, w.value); !r) {
      return r;
    }
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::writeChannel(Channel channel, const ChannelConfig& config,
                                            Verify verify) noexcept {
  const auto ch = static_cast<uint32_t>(detail::ToUnderlying(channel)) % kChannelCount;
  const uint32_t base = reg::kChannelBase[ch];
  const auto words = detail::ChannelWords(config);
  for (uint32_t i = 0U; i < detail::kChannelOffsets.size(); ++i) {
    const auto address = static_cast<uint16_t>(base + detail::kChannelOffsets[i]);
    if (auto r = writeVerified(address, words[i], detail::kChannelCompare[i], verify); !r) {
      return r;
    }
  }
  setpoint_[ch] = 0U;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::armResetWatch() noexcept {
  // POR_EVENT and RES_EVENT are rw: the clear wrote 0 to them (section 4.1.1). Once they read 0,
  // either one set again means the chip reset, and every FB_STAT anchor checks for it.
  const auto diag0 = read(reg::kGlobalDiag0);
  if (!diag0) {
    return fail(diag0.error, "tle92466ed: GLOBAL_DIAG0 read failed");
  }
  if ((diag0.value & static_cast<uint32_t>(reg::kDiag0PorEvent | reg::kDiag0ResEvent)) == 0U) {
    reset_watch_ = true;
    bus_.WatchReset(true);
  } else {
    detail::Log(bus_.GetPort(), LogLevel::Warning,
                "tle92466ed: POR_EVENT or RES_EVENT did not clear; reset watch off");
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::Deinitialize() noexcept {
  // EN low in every case, also after a reset or a critical fault left the driver uninitialised:
  // it is the safe level, and it needs no SPI (section 4.2.3).
  if (!initialized_) {
    return setPin(CtrlPin::Enable, GpioSignal::INACTIVE);
  }
  initialized_ = false;
  const uint16_t value = detail::PairBits(config_);
  const auto r = write(reg::kChCtrl, value);
  if (r) {
    ch_ctrl_ = value;
  }
  const auto pin = setPin(CtrlPin::Enable, GpioSignal::INACTIVE);
  reset_watch_ = false;
  bus_.WatchReset(false);
  return r ? pin : r;
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::HardwareReset() noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if constexpr (!detail::HasGpioSet<Port>) {
    return Result<void>::Failure(Error::Unsupported);
  } else {
    initialized_ = false;
    if (auto r = initialize(); !r) {
      return r;
    }
    initialized_ = true;
    return Result<void>::Success();
  }
}

// -------------------------------------------------------------------------------------------------
// Configuration
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> TLE92466ED<Port>::ApplyConfig(const Config& config) noexcept {
  if (Validate(config) != Error::Ok) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (!initialized_) {
    config_ = config; // applied by the first EnsureInitialized()
    return Result<void>::Success();
  }
  if (maybeMission()) {
    return Result<void>::Failure(Error::WrongState);
  }
  // Until every write is through, any register may hold the old or the new value.
  global_unknown_ = true;
  config_unknown_ = static_cast<uint8_t>(reg::kChCtrlEnMask);
  dither_unknown_ = static_cast<uint8_t>(reg::kChCtrlEnMask);
  if (auto r = writeAll(config); !r) {
    return r;
  }
  config_ = config;
  global_unknown_ = false;
  config_unknown_ = 0U;
  dither_unknown_ = 0U;
  par_unknown_ = 0U; // writeAll() wrote CH_CTRL with these pairs
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::ConfigureChannel(Channel channel, const ChannelConfig& config,
                                                Verify verify) noexcept {
  if (!detail::IsEnumerator(channel) || !detail::IsEnumerator(verify)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  Config next = config_;
  next.channels[detail::ToUnderlying(channel)] = config;
  if (Validate(next) != Error::Ok) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (!initialized_) {
    config_ = next;
    return Result<void>::Success();
  }
  if (maybeMission() || detail::IsActiveSlave(config_, channel)) {
    return Result<void>::Failure(Error::WrongState); // MODE only in Config Mode (section 4.4.1)
  }
  const auto bit = static_cast<uint32_t>(detail::ChannelBit(channel));
  config_unknown_ = static_cast<uint8_t>(static_cast<uint32_t>(config_unknown_) | bit);
  dither_unknown_ = static_cast<uint8_t>(static_cast<uint32_t>(dither_unknown_) | bit);
  if (auto r = writeChannel(channel, config, verify); !r) {
    return r;
  }
  // `loaded` decides the channel's FAULTN bits (KB-CPP-131).
  if (auto r = write(reg::kFaultMask0, detail::FaultMask0Value(next)); !r) {
    return r;
  }
  if (auto r = write(reg::kFaultMask1, detail::FaultMask1Value(next)); !r) {
    return r;
  }
  config_ = next;
  config_unknown_ = static_cast<uint8_t>(static_cast<uint32_t>(config_unknown_) & ~bit);
  dither_unknown_ = static_cast<uint8_t>(static_cast<uint32_t>(dither_unknown_) & ~bit);
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::SetParallel(ParallelPair pair, bool enabled) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (!detail::IsEnumerator(pair)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  Config next = config_;
  next.parallel[detail::ToUnderlying(pair)] = enabled;
  if (Validate(next) != Error::Ok) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (maybeMission()) {
    return Result<void>::Failure(Error::WrongState); // CH_PAR only in Config Mode (5.3.2.3)
  }
  const Channel master = detail::PairMaster(pair);
  const auto m = static_cast<uint32_t>(detail::ToUnderlying(master));
  if (enabled) {
    // Joined, one TARGET count is twice the current (section 4.5.2): the master's setpoint goes to
    // 0 first, so no setpoint is ever doubled behind the caller's back.
    const auto address = static_cast<uint16_t>(reg::kChannelBase[m] + reg::kSetpoint);
    if (auto r = write(address, detail::SetpointValue(config_.channels[m].icc, 0U)); !r) {
      return r;
    }
    setpoint_[m] = 0U;
  }
  if (auto r = writeChCtrl(detail::PairBits(next), Verify::None); !r) {
    return r;
  }
  const Channel slave = detail::PairSlave(pair);
  const auto slave_bit = static_cast<uint32_t>(detail::ChannelBit(slave));
  if (!enabled) {
    // A split slave must be configured again (section 4.5.2).
    config_unknown_ = static_cast<uint8_t>(static_cast<uint32_t>(config_unknown_) | slave_bit);
    dither_unknown_ = static_cast<uint8_t>(static_cast<uint32_t>(dither_unknown_) | slave_bit);
    if (auto r = writeChannel(slave, next.channels[detail::ToUnderlying(slave)], Verify::None);
        !r) {
      return r;
    }
    config_unknown_ = static_cast<uint8_t>(static_cast<uint32_t>(config_unknown_) & ~slave_bit);
    dither_unknown_ = static_cast<uint8_t>(static_cast<uint32_t>(dither_unknown_) & ~slave_bit);
  }
  config_ = next;
  par_unknown_ = static_cast<uint16_t>(
      (static_cast<uint32_t>(ch_ctrl_) ^ static_cast<uint32_t>(detail::PairBits(config_))) &
      reg::kChCtrlParMask);
  return Result<void>::Success();
}

// -------------------------------------------------------------------------------------------------
// Operation state
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> TLE92466ED<Port>::EnterMissionMode() noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (global_unknown_) {
    return Result<void>::Failure(Error::WrongState); // ApplyConfig() did not finish
  }
  if (auto r = checkMissionEntry(); !r) {
    return r;
  }
  if (auto r = setPin(CtrlPin::Enable, GpioSignal::ACTIVE); !r) {
    return r;
  }
  // OP_MODE with every channel off; then CH_CTRL is read back, always: the chip refuses Mission
  // Mode on its own conditions (Figure 4), and the copy must say what the chip did.
  const uint32_t value = static_cast<uint32_t>(detail::PairBits(config_)) | reg::kChCtrlOpMode;
  maybe_mission_ = true; // until the read below says what the chip did
  if (auto r = write(reg::kChCtrl, static_cast<uint16_t>(value)); !r) {
    return r;
  }
  if (auto r = reconcile(); !r) {
    return r;
  }
  if (IsMissionMode()) {
    return Result<void>::Success();
  }
  // Refused: EN back low, as in Config Mode (section 4.2.3).
  if (auto pin = setPin(CtrlPin::Enable, GpioSignal::INACTIVE); !pin) {
    return pin;
  }
  return Result<void>::Failure(Error::MissionBlocked);
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::checkMissionEntry() noexcept {
  // Figure 4: Config Mode to Mission Mode needs INIT_DONE = 1, no supply, clock, watchdog or
  // central temperature fault (FB_STAT), no channel OTW or OTE (DIAG_WARN_CHGR, DIAG_ERR_CHGR,
  // read only for the groups FB_STAT flags) and no REG_ECC_ERR (GLOBAL_DIAG2).
  const auto stat = read(reg::kFbStat);
  if (!stat) {
    return Result<void>::Failure(stat.error);
  }
  if (((stat.value & reg::kStatInitDone) == 0U) ||
      ((stat.value & reg::kStatMissionBlockers) != 0U)) {
    return Result<void>::Failure(Error::MissionBlocked);
  }
  const DeviceStatus s = detail::DecodeStatus(stat.value);
  for (uint32_t g = 0U; g < reg::kChannelGroups; ++g) {
    const auto group = checkGroupTemperature(g, s);
    if (!group) {
      return group;
    }
  }
  const auto diag2 = read(reg::kGlobalDiag2);
  if (!diag2) {
    return Result<void>::Failure(diag2.error);
  }
  latch(reg::kGlobalDiag2, diag2.value);
  return ((diag2.value & static_cast<uint32_t>(reg::kDiag2RegEccErr)) == 0U)
             ? Result<void>::Success()
             : Result<void>::Failure(Error::MissionBlocked);
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::checkGroupTemperature(uint32_t group,
                                                     const DeviceStatus& status) noexcept {
  constexpr uint32_t kOte = static_cast<uint32_t>(reg::kErrOte) * 0x0101U; // both channels
  constexpr uint32_t kOtw = static_cast<uint32_t>(reg::kWarnOtw) * 0x0101U;
  const uint32_t bit = 1U << (group % reg::kChannelGroups);
  if ((static_cast<uint32_t>(status.error_groups) & bit) != 0U) {
    const auto address = static_cast<uint16_t>(reg::kDiagErrChgr0 + group);
    const auto err = read(address);
    if (!err) {
      return Result<void>::Failure(err.error);
    }
    latch(address, err.value);
    if ((err.value & kOte) != 0U) {
      return Result<void>::Failure(Error::MissionBlocked);
    }
  }
  if ((static_cast<uint32_t>(status.warning_groups) & bit) != 0U) {
    const auto address = static_cast<uint16_t>(reg::kDiagWarnChgr0 + group);
    const auto warn = readOnce(address); // clear-on-read (4.9.3): never repeated
    if (!warn) {
      return Result<void>::Failure(warn.error);
    }
    latch(address, warn.value);
    if ((warn.value & kOtw) != 0U) {
      return Result<void>::Failure(Error::MissionBlocked);
    }
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::EnterConfigMode() noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  // EN low whatever the SPI result: it turns every channel off by itself (section 4.2.3).
  const auto r = writeChCtrl(detail::PairBits(config_), Verify::None);
  const auto pin = setPin(CtrlPin::Enable, GpioSignal::INACTIVE);
  return r ? pin : r;
}

template <SpiPort Port>
Result<uint16_t> TLE92466ED<Port>::ReadChannelControl() noexcept {
  if (!initialized_) {
    return Result<uint16_t>::Failure(Error::NotInitialized);
  }
  if (auto r = reconcile(); !r) {
    return Result<uint16_t>::Failure(r.error);
  }
  return Result<uint16_t>::Success(ch_ctrl_);
}

// -------------------------------------------------------------------------------------------------
// Channels
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> TLE92466ED<Port>::SetEnabledChannels(uint8_t mask, Verify verify) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  const auto bits = static_cast<uint32_t>(mask);
  if (!detail::IsEnumerator(verify)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  // A channel already on (or asked on) that the mask keeps is left as the chip has it: an
  // unknown state refuses only a channel the mask newly turns on, never the switch-off of the rest.
  const uint32_t maybe_on = static_cast<uint32_t>(ch_ctrl_) | wanted_;
  uint32_t kept = 0U;
  for (uint32_t ch = 0U; ch < kChannelCount; ++ch) {
    const uint32_t bit = 1U << ch;
    if (((bits & maybe_on & bit) != 0U) &&
        (checkKnown(static_cast<Channel>(ch), true) != Error::Ok)) {
      kept |= bit;
    }
  }
  const uint32_t set = bits & ~kept;
  if (const Error e = checkEnable(set); e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  return writeEnables(set, reg::kChCtrlEnMask & ~bits, verify);
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::EnableChannel(Channel channel, Verify verify) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (!detail::IsEnumerator(channel) || !detail::IsEnumerator(verify)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  const uint32_t bit = detail::ChannelBit(channel);
  if (const Error e = checkEnable(bit); e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  return writeEnables(bit, 0U, verify);
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::DisableChannel(Channel channel, Verify verify) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (!detail::IsEnumerator(channel) || !detail::IsEnumerator(verify)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  return writeEnables(0U, detail::ChannelBit(channel), verify);
}

template <SpiPort Port>
Result<uint32_t> TLE92466ED<Port>::SetCurrent(Channel channel, uint32_t microamps,
                                              Verify verify) noexcept {
  if (!initialized_) {
    return Result<uint32_t>::Failure(Error::NotInitialized);
  }
  if (const Error e = checkChannel(channel); e != Error::Ok) {
    return Result<uint32_t>::Failure(e);
  }
  const auto ch = static_cast<uint32_t>(detail::ToUnderlying(channel));
  const ChannelConfig& cc = config_.channels[ch];
  if (const Error e = checkCurrent(channel, microamps, cc.dither); e != Error::Ok) {
    return Result<uint32_t>::Failure(e);
  }
  if (!detail::IsEnumerator(verify)) {
    return Result<uint32_t>::Failure(Error::InvalidArgument);
  }
  // A setpoint of 0 is never refused; a larger one needs the channel's state known.
  if (const Error e = checkKnown(channel, true); (microamps != 0U) && (e != Error::Ok)) {
    return Result<uint32_t>::Failure(e);
  }
  const bool parallel = detail::IsActiveMaster(config_, channel);
  const uint32_t target = TargetFromMicroamps(microamps, parallel);
  const auto address = static_cast<uint16_t>(reg::kChannelBase[ch] + reg::kSetpoint);
  // Until the write is confirmed the chip may hold the old or the new TARGET: the copy keeps the
  // larger one (the guard of StartOvercurrentCheck(), the dither check).
  const auto previous = static_cast<uint32_t>(setpoint_[ch]);
  setpoint_[ch] = static_cast<uint16_t>((target > previous) ? target : previous);
  if (auto r = writeVerified(address, detail::SetpointValue(cc.icc, target), 0xFFFFU, verify); !r) {
    return Result<uint32_t>::Failure(r.error);
  }
  setpoint_[ch] = static_cast<uint16_t>(target);
  return Result<uint32_t>::Success(MicroampsFromTarget(target, parallel));
}

template <SpiPort Port>
uint32_t TLE92466ED<Port>::GetCurrentSetpoint(Channel channel) const noexcept {
  if (!detail::IsEnumerator(channel)) {
    return 0U;
  }
  return MicroampsFromTarget(setpoint_[detail::ToUnderlying(channel)],
                             detail::IsActiveMaster(config_, channel));
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::SetDither(Channel channel, const DitherConfig& dither,
                                         Verify verify) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (const Error e = checkChannel(channel); e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  const auto ch = static_cast<uint32_t>(detail::ToUnderlying(channel));
  Config next = config_;
  next.channels[ch].dither = dither;
  if ((Validate(next) != Error::Ok) || !detail::IsEnumerator(verify) ||
      (checkCurrent(channel, GetCurrentSetpoint(channel), dither) != Error::Ok)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (const Error e = checkKnown(channel, false); e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  const auto bit = static_cast<uint32_t>(detail::ChannelBit(channel));
  dither_unknown_ = static_cast<uint8_t>(static_cast<uint32_t>(dither_unknown_) | bit);
  // DITHER_CTRL last: its write is the update event (section 4.7.2).
  const uint32_t base = reg::kChannelBase[ch];
  const std::array<detail::RegWrite, 3> writes = {{
      {.address = static_cast<uint16_t>(base + reg::kDitherClkDiv),
       .value = detail::DitherClkDivValue(dither)},
      {.address = static_cast<uint16_t>(base + reg::kDitherStep),
       .value = detail::DitherStepValue(dither)},
      {.address = static_cast<uint16_t>(base + reg::kDitherCtrl),
       .value = detail::DitherCtrlValue(dither)},
  }};
  // Each register compared with its own mask (kChannelCompare: DITHER_CLK_DIV, STEP, CTRL).
  const std::array<uint16_t, 3> compare = {detail::kChannelCompare[5], detail::kChannelCompare[6],
                                           detail::kChannelCompare[7]};
  for (uint32_t i = 0U; i < writes.size(); ++i) {
    if (auto r = writeVerified(writes[i].address, writes[i].value, compare[i], verify); !r) {
      return r;
    }
  }
  config_ = next;
  dither_unknown_ = static_cast<uint8_t>(static_cast<uint32_t>(dither_unknown_) & ~bit);
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::StartOvercurrentCheck(Channel channel) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (const Error e = checkChannel(channel); e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  const auto ch = static_cast<uint32_t>(detail::ToUnderlying(channel));
  const uint32_t bit = detail::ChannelBit(channel);
  // An OFF-state check (section 4.9.6): in Mission Mode, with the channel off and its setpoint 0,
  // so enabling it for the check does not also start the current controller (section 4.9.1).
  // "Off" needs no read: the copy may under-report a channel that is on (writeChCtrl()), but a
  // channel the driver last asked on stays in wanted_ until a validated read finds it off.
  // The setpoint copy holds the larger of two values a failed write leaves possible, and the
  // timing (on_time_mant, dither.exp, the clock) must be known: t_OCon is at most 300 ms.
  const uint32_t maybe_on = static_cast<uint32_t>(ch_ctrl_) | wanted_;
  if (!IsMissionMode() || (setpoint_[ch] != 0U) || ((maybe_on & bit) != 0U) || global_unknown_ ||
      (checkKnown(channel, true) != Error::Ok)) {
    return Result<void>::Failure(Error::WrongState);
  }
  const ChannelConfig& cc = config_.channels[ch];
  const uint32_t system_hz = SystemClockHz(config_); // not 0: the configuration passed Validate()
  if (!cc.loaded || (cc.on_time_mant == 0U) || (system_hz == 0U)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  // t_OCon = (TON_MANT + 1) * 2^EXP / f_SYS, at most 300 ms, TON_MANT not 0 (Equation 17).
  const uint64_t on_us = (((static_cast<uint64_t>(cc.on_time_mant) + 1U)
                           << (static_cast<uint32_t>(cc.dither.exp) & 0x0FU)) *
                          1'000'000U) /
                         system_hz;
  if (on_us > kMaxOcOnTimeUs) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  const auto address = static_cast<uint16_t>(reg::kChannelBase[ch] + reg::kChConfig);
  const auto word =
      static_cast<uint16_t>(static_cast<uint32_t>(detail::ChConfigValue(cc)) | reg::kOcDiagEn);
  if (auto r = write(address, word); !r) {
    return r;
  }
  return writeEnables(bit, 0U, Verify::None);
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::SetDrivePin(CtrlPin pin, [[maybe_unused]] bool high) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if ((pin != CtrlPin::Drv0) && (pin != CtrlPin::Drv1)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if constexpr (!detail::HasGpioSet<Port>) {
    return Result<void>::Failure(Error::Unsupported);
  } else {
    return setPin(pin, high ? GpioSignal::ACTIVE : GpioSignal::INACTIVE);
  }
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::ReloadWatchdog() noexcept {
  using detail::operator""_u16;
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  return write(reg::kWdReload, reg::WdTime::Insert(0_u16, config_.watchdog_reload));
}

// -------------------------------------------------------------------------------------------------
// Diagnostics
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<DeviceStatus> TLE92466ED<Port>::ReadStatus() noexcept {
  if (!initialized_) {
    return Result<DeviceStatus>::Failure(Error::NotInitialized);
  }
  const auto r = read(reg::kFbStat);
  if (!r) {
    return Result<DeviceStatus>::Failure(r.error);
  }
  return Result<DeviceStatus>::Success(detail::DecodeStatus(r.value));
}

template <SpiPort Port>
Result<Faults> TLE92466ED<Port>::ReadFaults() noexcept {
  if (!initialized_) {
    return Result<Faults>::Failure(Error::NotInitialized);
  }
  return readFaults();
}

template <SpiPort Port>
Result<Faults> TLE92466ED<Port>::readFaults() noexcept {
  Faults f{};
  for (uint32_t g = 0U; g < reg::kChannelGroups; ++g) {
    const auto d = read(static_cast<uint16_t>(reg::kGlobalDiag0 + g));
    const auto e = d ? read(static_cast<uint16_t>(reg::kDiagErrChgr0 + g)) : d;
    // DIAG_WARN_CHGR clears OLSG_WARN_CHK_NOK when read (4.9.3): one read, never repeated.
    const auto w = e ? readOnce(static_cast<uint16_t>(reg::kDiagWarnChgr0 + g)) : e;
    if (!w) {
      return Result<Faults>::Failure(w.error);
    }
    f.global[g] = static_cast<uint16_t>(d.value);
    f.errors[g] = static_cast<uint16_t>(e.value);
    f.warnings[g] = static_cast<uint16_t>(w.value);
  }
  latchAll(f);
  return Result<Faults>::Success(f);
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::ClearFaults() noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  return clearAll();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::clearAll() noexcept {
  // Read every register, latch it, then clear exactly the flags read: a flag raised since the last
  // read is seen before it is cleared (handbook KB-CPP-119).
  const auto faults = readFaults();
  if (!faults) {
    return Result<void>::Failure(faults.error);
  }
  const auto events = static_cast<uint32_t>(reg::kDiag0PorEvent) | reg::kDiag0ResEvent;
  if (reset_watch_ && ((static_cast<uint32_t>(faults.value.global[0]) & events) != 0U)) {
    track(Error::UnexpectedReset); // never clear the evidence of a reset
    return Result<void>::Failure(Error::UnexpectedReset);
  }
  return clearRead(faults.value);
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::clearRead(const Faults& f) noexcept {
  // The rwh flags are cleared by a 1 where they read 1 and a 0 elsewhere; the rw ones (POR_EVENT,
  // RES_EVENT, VDD2V5_UV/OV) take the 0 written (sections 4.1.2, 4.9.1, 5.3.1). The datasheet says
  // only "cleared on write", and 4.9.8 clears the SFF_BIST results by writing 0: the polarity is
  // an interpretation (bench step 1). Writing 1 only where a flag read 1, and nothing to a
  // register with no flag set, keeps every flag seen under either reading. OC and
  // OTP_ECC_ERR need two clears (4.9.6, 4.9.7); OTP_VIRGIN is never cleared (4.9.7). A register
  // with nothing to clear is not written. The channel groups go first, GLOBAL_DIAG0 last.
  for (uint32_t g = 0U; g < reg::kChannelGroups; ++g) {
    const auto err = static_cast<uint16_t>(f.errors[g] & detail::kErrClear);
    if (auto r = clearTwice(static_cast<uint16_t>(reg::kDiagErrChgr0 + g), err, detail::kOcBoth);
        !r) {
      return r;
    }
    const auto warn = static_cast<uint16_t>(f.warnings[g] & detail::kWarnClear);
    if (auto r = clearTwice(static_cast<uint16_t>(reg::kDiagWarnChgr0 + g), warn, 0U); !r) {
      return r;
    }
  }
  const auto diag2 = static_cast<uint16_t>(f.global[2] & detail::kDiag2Clear);
  if (auto r = clearTwice(reg::kGlobalDiag2, diag2, reg::kDiag2OtpEccErr); !r) {
    return r;
  }
  const bool diag1 = (static_cast<uint32_t>(f.global[1]) & reg::kDiag1Flags) != 0U;
  const auto rwh1 = static_cast<uint16_t>(f.global[1] & reg::kDiag1RwhFlags);
  if (auto r = diag1 ? write(reg::kGlobalDiag1, rwh1) : Result<void>::Success(); !r) {
    return r;
  }
  const bool diag0 = (static_cast<uint32_t>(f.global[0]) & detail::kDiag0Any) != 0U;
  const auto rwh0 = static_cast<uint16_t>(f.global[0] & reg::kDiag0Flags);
  return diag0 ? write(reg::kGlobalDiag0, rwh0) : Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::clearTwice(uint16_t address, uint16_t bits,
                                          uint16_t twice) noexcept {
  if (bits == 0U) {
    return Result<void>::Success();
  }
  if (auto r = write(address, bits); !r) {
    return r;
  }
  const auto again = static_cast<uint16_t>(bits & twice);
  return (again == 0U) ? Result<void>::Success() : write(address, again);
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::ClearChannelFaults(Channel channel) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (!detail::IsEnumerator(channel)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  // Both channels of the group are read and latched first, so nothing of the other channel can be
  // lost whatever a write does to its byte; then only this channel's flags that read 1 get a 1.
  const auto ch = static_cast<uint32_t>(detail::ToUnderlying(channel));
  const uint32_t byte = 0xFFU << (8U * (ch % 2U));
  const auto err_address = static_cast<uint16_t>(reg::kDiagErrChgr0 + (ch / 2U));
  const auto warn_address = static_cast<uint16_t>(reg::kDiagWarnChgr0 + (ch / 2U));
  const auto err = read(err_address);
  const auto warn = err ? readOnce(warn_address) : err;
  if (!warn) {
    return Result<void>::Failure(warn.error);
  }
  latch(err_address, err.value);
  latch(warn_address, warn.value);
  const auto err_bits = static_cast<uint16_t>(err.value & detail::kErrClear & byte);
  if (auto r = clearTwice(err_address, err_bits, detail::kOcBoth); !r) {
    return r;
  }
  return clearTwice(warn_address, static_cast<uint16_t>(warn.value & detail::kWarnClear & byte),
                    0U);
}

template <SpiPort Port>
Result<bool> TLE92466ED<Port>::ReadFaultPin() noexcept {
  if (!initialized_) {
    return Result<bool>::Failure(Error::NotInitialized);
  }
  if constexpr (detail::HasGpioRead<Port>) {
    GpioSignal signal = GpioSignal::INACTIVE;
    if (!bus_.GetPort().GpioRead(CtrlPin::Fault, signal)) {
      return Result<bool>::Failure(Error::Bus);
    }
    return Result<bool>::Success(signal == GpioSignal::ACTIVE);
  } else {
    return Result<bool>::Failure(Error::Unsupported);
  }
}

template <SpiPort Port>
Result<PinStatus> TLE92466ED<Port>::ReadPinStatus() noexcept {
  if (!initialized_) {
    return Result<PinStatus>::Failure(Error::NotInitialized);
  }
  const auto r = read(reg::kPinStat);
  if (!r) {
    return Result<PinStatus>::Failure(r.error);
  }
  return Result<PinStatus>::Success(detail::DecodePins(r.value));
}

template <SpiPort Port>
Result<SupplyReadings> TLE92466ED<Port>::ReadSupply() noexcept {
  if (!initialized_) {
    return Result<SupplyReadings>::Failure(Error::NotInitialized);
  }
  const auto v1 = read(reg::kFbVoltage1);
  if (!v1) {
    return Result<SupplyReadings>::Failure(v1.error);
  }
  const auto v2 = read(reg::kFbVoltage2);
  if (!v2) {
    return Result<SupplyReadings>::Failure(v2.error);
  }
  return Result<SupplyReadings>::Success(detail::DecodeSupply(v1.value, v2.value));
}

template <SpiPort Port>
Result<ChannelFeedback> TLE92466ED<Port>::ReadChannelFeedback(Channel channel) noexcept {
  using detail::operator""_u16;
  if (!initialized_) {
    return Result<ChannelFeedback>::Failure(Error::NotInitialized);
  }
  if (const Error e = checkChannel(channel); e != Error::Ok) {
    return Result<ChannelFeedback>::Failure(e);
  }
  const uint16_t bit = detail::ChannelBit(channel);
  const auto upd = read(reg::kFbUpd);
  if (!upd) {
    return Result<ChannelFeedback>::Failure(upd.error);
  }
  if ((upd.value & static_cast<uint32_t>(bit)) == 0U) {
    return Result<ChannelFeedback>::Failure(Error::NotReady);
  }
  // Figure 22: freeze, read, release (the release also clears the FB_UPD bit, 4.10.5).
  if (auto r = write(reg::kFbFrz, bit); !r) {
    return Result<ChannelFeedback>::Failure(r.error);
  }
  std::array<uint32_t, 5> words{};
  const auto got = readFeedbackSet(reg::kChannelBase[detail::ToUnderlying(channel)], words);
  const auto released = write(reg::kFbFrz, 0_u16);
  if (!got) {
    return Result<ChannelFeedback>::Failure(got.error);
  }
  if (!released) {
    return Result<ChannelFeedback>::Failure(released.error);
  }
  return detail::DecodeFeedback(words, SystemClockHz(config_));
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::readFeedbackSet(uint16_t base,
                                               std::array<uint32_t, 5>& words) noexcept {
  for (uint32_t i = 0U; i < detail::kFeedbackOffsets.size(); ++i) {
    const auto r = read(static_cast<uint16_t>(base + detail::kFeedbackOffsets[i]));
    if (!r) {
      return Result<void>::Failure(r.error);
    }
    words[i] = r.value;
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<BistResult> TLE92466ED<Port>::RunBist() noexcept {
  using detail::operator""_u16;
  if (!initialized_) {
    return Result<BistResult>::Failure(Error::NotInitialized);
  }
  if (maybeMission()) {
    return Result<BistResult>::Failure(Error::WrongState); // Config Mode only (4.9.8)
  }
  if (auto r = write(reg::kSffBist, reg::kBistEn); !r) {
    return Result<BistResult>::Failure(r.error);
  }
  const auto done = waitBist();
  // SFF_BIST is cleared by a write of 0 (4.9.8). GLOBAL_DIAG2 is read and latched, then only the
  // REG_ECC_ERR the test sets on purpose is cleared: never OTP_VIRGIN or OTP_ECC_ERR (4.9.7).
  if (auto r = write(reg::kSffBist, 0_u16); !r) {
    return Result<BistResult>::Failure(r.error);
  }
  const auto diag2 = read(reg::kGlobalDiag2);
  if (!diag2) {
    return Result<BistResult>::Failure(diag2.error);
  }
  latch(reg::kGlobalDiag2, diag2.value);
  const auto ecc = static_cast<uint16_t>(diag2.value & reg::kDiag2RegEccErr);
  if (auto r = clearTwice(reg::kGlobalDiag2, ecc, 0U); !r) {
    return Result<BistResult>::Failure(r.error);
  }
  if (!done) {
    return Result<BistResult>::Failure(done.error);
  }
  BistResult result{};
  result.passed = (done.value & reg::kBistFail) == 0U;
  result.uncorrectable_tested = (done.value & reg::kBistUerr) != 0U;
  result.correctable_tested = (done.value & reg::kBistCerr) != 0U;
  return Result<BistResult>::Success(result);
}

template <SpiPort Port>
Result<uint32_t> TLE92466ED<Port>::waitBist() noexcept {
  constexpr uint32_t kPolls = (kBistTimeoutUs / kPollIntervalUs) + 1U;
  for (uint32_t poll = 0U; poll < kPolls; ++poll) {
    const auto r = read(reg::kSffBist);
    if (!r) {
      return r;
    }
    if ((r.value & reg::kBistDone) != 0U) {
      return r;
    }
    detail::DelayUs(bus_.GetPort(), kPollIntervalUs);
  }
  return Result<uint32_t>::Failure(Error::Timeout);
}

template <SpiPort Port>
Result<ChipId> TLE92466ED<Port>::ReadChipId() noexcept {
  if (!initialized_) {
    return Result<ChipId>::Failure(Error::NotInitialized);
  }
  ChipId id{};
  for (uint32_t i = 0U; i < id.words.size(); ++i) {
    const auto r = read(static_cast<uint16_t>(reg::kChipId0 + i));
    if (!r) {
      return Result<ChipId>::Failure(r.error);
    }
    id.words[i] = static_cast<uint16_t>(r.value & reg::kData16Mask);
  }
  return Result<ChipId>::Success(id);
}

template <SpiPort Port>
Result<uint32_t> TLE92466ED<Port>::ReadRegister(uint16_t address) noexcept {
  if (!initialized_) {
    return Result<uint32_t>::Failure(Error::NotInitialized);
  }
  if (!reg::IsRegister(address)) {
    return Result<uint32_t>::Failure(Error::InvalidArgument);
  }
  const auto r = bus_.Access(address, false, 0U); // single shot: no retry
  if (!r) {
    track(r.error);
    return r;
  }
  latch(address, r.value);
  return r;
}

// -------------------------------------------------------------------------------------------------
// Private
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> TLE92466ED<Port>::writeChCtrl(uint16_t value, Verify verify) noexcept {
  // Section 5.2.1 does not say whether the reply to a write carries CH_CTRL before or after the
  // write. Either way the copy may only under-report: an EN_CH or OP_MODE bit is set in the copy
  // only when both the value and the reply have it, and CH_PAR, written in Config Mode only, is
  // taken from the value. A failed write takes the previous copy as the reply. Dropped channels
  // are recorded only from a validated read (reconcile(), Verify::ReadBack), never from a reply.
  constexpr uint32_t kLive = static_cast<uint32_t>(reg::kChCtrlEnMask) | reg::kChCtrlOpMode;
  const uint32_t pairs = static_cast<uint32_t>(value) & reg::kChCtrlWritable & ~kLive;
  const auto before = static_cast<uint32_t>(ch_ctrl_);
  const auto reply = bus_.Access(reg::kChCtrl, true, value);
  const uint32_t seen =
      reply ? static_cast<uint32_t>(reply.value) : static_cast<uint32_t>(ch_ctrl_);
  ch_ctrl_ = static_cast<uint16_t>((static_cast<uint32_t>(value) & seen & kLive) | pairs);
  const uint32_t asked = static_cast<uint32_t>(value) & reg::kChCtrlEnMask;
  const uint32_t new_pars =
      (static_cast<uint32_t>(value) ^ detail::PairBits(config_)) & reg::kChCtrlParMask;
  if (!reply) {
    // The frame may or may not have taken effect: keep both possibilities (on rather than off,
    // Mission rather than Config, either pairing).
    wanted_ = static_cast<uint8_t>(static_cast<uint32_t>(wanted_) | asked);
    maybe_mission_ =
        maybe_mission_ || (((before | static_cast<uint32_t>(value)) & reg::kChCtrlOpMode) != 0U);
    par_unknown_ = static_cast<uint16_t>(static_cast<uint32_t>(par_unknown_) | new_pars);
    track(reply.error);
    return Result<void>::Failure(reply.error);
  }
  // The chip took the frame: it holds value (it may still refuse OP_MODE or an EN_CH bit).
  wanted_ = static_cast<uint8_t>(asked);
  maybe_mission_ = (static_cast<uint32_t>(value) & reg::kChCtrlOpMode) != 0U;
  par_unknown_ = static_cast<uint16_t>(new_pars);
  if (verify != Verify::ReadBack) {
    return Result<void>::Success();
  }
  const auto back = read(reg::kChCtrl);
  if (!back) {
    return Result<void>::Failure(back.error);
  }
  adopt(static_cast<uint16_t>(back.value & reg::kChCtrlWritable)); // the copy follows the chip
  if (ch_ctrl_ != value) {
    return Result<void>::Failure(Error::WriteMismatch);
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::setPin([[maybe_unused]] CtrlPin pin,
                                      [[maybe_unused]] GpioSignal signal) noexcept {
  if constexpr (detail::HasGpioSet<Port>) {
    if (!bus_.GetPort().GpioSet(pin, signal)) {
      return Result<void>::Failure(Error::Bus);
    }
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::reconcile() noexcept {
  // The chip clears EN_CH on its own (a channel fault, 4.9.2 to 4.9.6, 4.11) and leaves Mission
  // Mode on its own (a supply, clock or watchdog fault, 4.1.2, 4.2.2, 5.2.2): the copy is taken
  // from a validated read of CH_CTRL before it is used for a write (handbook KB-CPP-122).
  const auto r = read(reg::kChCtrl);
  if (!r) {
    return Result<void>::Failure(r.error);
  }
  adopt(static_cast<uint16_t>(r.value & reg::kChCtrlWritable));
  return Result<void>::Success();
}

template <SpiPort Port>
void TLE92466ED<Port>::adopt(uint16_t chip) noexcept {
  // A validated read of CH_CTRL: a channel written on that reads off was switched off by the chip.
  noteDropped(static_cast<uint32_t>(wanted_) & ~static_cast<uint32_t>(chip));
  wanted_ = static_cast<uint8_t>(static_cast<uint32_t>(wanted_) & chip);
  ch_ctrl_ = chip;
  maybe_mission_ = false; // the copy's OP_MODE is the chip's
  par_unknown_ = static_cast<uint16_t>((static_cast<uint32_t>(chip) ^ detail::PairBits(config_)) &
                                       reg::kChCtrlParMask);
}

template <SpiPort Port>
void TLE92466ED<Port>::noteDropped(uint32_t bits) noexcept {
  const uint32_t channels = bits & reg::kChCtrlEnMask;
  if (channels == 0U) {
    return;
  }
  dropped_ = static_cast<uint8_t>(static_cast<uint32_t>(dropped_) | channels);
  detail::Log(bus_.GetPort(), LogLevel::Warning, "tle92466ed: the chip switched a channel off");
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::writeEnables(uint32_t set, uint32_t clear, Verify verify) noexcept {
  // Read-modify-write from the chip's own CH_CTRL: no EN_CH bit the call did not name is set, and
  // OP_MODE is only ever kept, never set: only EnterMissionMode() sets it.
  if (auto r = reconcile(); !r) {
    if (set != 0U) {
      return r;
    }
    // Switching off never depends on a read: clearing is always safe, and the chip keeps a
    // fault-latched channel off anyway (4.9.2 to 4.9.6). The read's error is still reported.
    const auto off = static_cast<uint16_t>(static_cast<uint32_t>(ch_ctrl_) & ~clear);
    if (const auto w = writeChCtrl(off, Verify::None); !w) {
      return r; // the copy keeps the channels on: the next call tries again
    }
    return r;
  }
  if ((set != 0U) && !IsMissionMode()) {
    return Result<void>::Failure(Error::MissionBlocked); // the chip left Mission Mode
  }
  const uint32_t value = (static_cast<uint32_t>(ch_ctrl_) & ~clear) | set;
  return writeChCtrl(static_cast<uint16_t>(value), verify);
}

template <SpiPort Port>
Error TLE92466ED<Port>::checkEnable(uint32_t bits) const noexcept {
  if ((bits & ~static_cast<uint32_t>(reg::kChCtrlEnMask)) != 0U) {
    return Error::InvalidArgument;
  }
  for (uint32_t ch = 0U; ch < kChannelCount; ++ch) {
    if ((bits & (1U << ch)) == 0U) {
      continue;
    }
    if (detail::IsActiveSlave(config_, static_cast<Channel>(ch))) {
      return Error::WrongState;
    }
    if (!config_.channels[ch].loaded) {
      return Error::InvalidArgument; // KB-CPP-131
    }
    if (const Error e = checkKnown(static_cast<Channel>(ch), true); e != Error::Ok) {
      return e;
    }
  }
  // EN_CH only in Mission Mode (5.3.2.3).
  return ((bits != 0U) && !IsMissionMode()) ? Error::WrongState : Error::Ok;
}

template <SpiPort Port>
Result<uint32_t> TLE92466ED<Port>::readOnce(uint16_t address) noexcept {
  const auto r = bus_.Access(address, false, 0U);
  if (!r) {
    track(r.error);
  }
  return r;
}

template <SpiPort Port>
void TLE92466ED<Port>::latchAll(const Faults& f) noexcept {
  for (uint32_t g = 0U; g < reg::kChannelGroups; ++g) {
    latch(static_cast<uint16_t>(reg::kGlobalDiag0 + g), f.global[g]);
    latch(static_cast<uint16_t>(reg::kDiagErrChgr0 + g), f.errors[g]);
    latch(static_cast<uint16_t>(reg::kDiagWarnChgr0 + g), f.warnings[g]);
  }
}

template <SpiPort Port>
Result<uint32_t> TLE92466ED<Port>::read(uint16_t address) noexcept {
  const auto r = bus_.Read(address, config_.max_retries);
  if (!r) {
    track(r.error);
  }
  return r;
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::write(uint16_t address, uint16_t value) noexcept {
  const auto r = bus_.Access(address, true, value);
  if (!r) {
    track(r.error);
    return Result<void>::Failure(r.error);
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::writeVerified(uint16_t address, uint16_t value, uint16_t compare,
                                             Verify verify) noexcept {
  if (auto r = write(address, value); !r) {
    return r;
  }
  if (verify != Verify::ReadBack) {
    return Result<void>::Success();
  }
  const auto back = read(address);
  if (!back) {
    return Result<void>::Failure(back.error);
  }
  const uint32_t differ = (back.value ^ static_cast<uint32_t>(value)) & compare;
  return (differ == 0U) ? Result<void>::Success() : Result<void>::Failure(Error::WriteMismatch);
}

template <SpiPort Port>
Error TLE92466ED<Port>::checkChannel(Channel channel) const noexcept {
  if (!detail::IsEnumerator(channel)) {
    return Error::InvalidArgument;
  }
  // A slave's SETPOINT, MODE and feedback are not used (section 4.5.2).
  return detail::IsActiveSlave(config_, channel) ? Error::WrongState : Error::Ok;
}

template <SpiPort Port>
Error TLE92466ED<Port>::checkKnown(Channel channel, bool dither) const noexcept {
  // After a write whose reply failed, the chip may hold the old or the new value: a decision
  // that depends on it is refused, with no I/O, until a validated read or a successful write.
  constexpr std::array<uint16_t, kPairCount> kParBit = {reg::kChCtrlPar03, reg::kChCtrlPar12,
                                                        reg::kChCtrlPar45};
  const auto bit = static_cast<uint32_t>(detail::ChannelBit(channel));
  const uint32_t pair =
      kParBit[static_cast<uint32_t>(detail::ToUnderlying(detail::PairOf(channel))) % kPairCount];
  const bool unknown = ((static_cast<uint32_t>(config_unknown_) & bit) != 0U) ||
                       (dither && ((static_cast<uint32_t>(dither_unknown_) & bit) != 0U)) ||
                       ((static_cast<uint32_t>(par_unknown_) & pair) != 0U);
  return unknown ? Error::WrongState : Error::Ok;
}

template <SpiPort Port>
bool TLE92466ED<Port>::maybeMission() const noexcept {
  return IsMissionMode() || maybe_mission_;
}

template <SpiPort Port>
Error TLE92466ED<Port>::checkCurrent(Channel channel, uint32_t microamps,
                                     const DitherConfig& dither) const noexcept {
  const bool parallel = detail::IsActiveMaster(config_, channel);
  const uint64_t amplitude = DitherAmplitudeUa(dither, parallel);
  const uint64_t peak = static_cast<uint64_t>(microamps) + amplitude;
  const bool over = parallel ? (peak > kMaxParallelSetpointUa)
                             : ((microamps > kMaxSetpointUa) || (peak > kMaxSetpointWithDitherUa));
  // No negative dither amplitude: I_set - I_Dither > 0 (section 4.7.1); dither is off at 0.
  const bool negative = (microamps != 0U) && (amplitude >= microamps);
  return (over || negative) ? Error::InvalidArgument : Error::Ok;
}

template <SpiPort Port>
void TLE92466ED<Port>::latch(uint16_t address, uint32_t value) noexcept {
  const auto a = static_cast<uint32_t>(address);
  const uint32_t v = value & reg::kData16Mask;
  if ((a >= reg::kGlobalDiag0) && (a <= reg::kGlobalDiag2)) {
    uint16_t& w = latched_.global[a - reg::kGlobalDiag0];
    w = static_cast<uint16_t>(static_cast<uint32_t>(w) | v);
  } else if ((a >= reg::kDiagErrChgr0) && (a < (reg::kDiagErrChgr0 + reg::kChannelGroups))) {
    uint16_t& w = latched_.errors[a - reg::kDiagErrChgr0];
    w = static_cast<uint16_t>(static_cast<uint32_t>(w) | v);
  } else if ((a >= reg::kDiagWarnChgr0) && (a < (reg::kDiagWarnChgr0 + reg::kChannelGroups))) {
    uint16_t& w = latched_.warnings[a - reg::kDiagWarnChgr0];
    w = static_cast<uint16_t>(static_cast<uint32_t>(w) | v);
  } else {
    // Not a diagnosis register: nothing to latch.
  }
}

template <SpiPort Port>
void TLE92466ED<Port>::track(Error error) noexcept {
  if ((error != Error::UnexpectedReset) && (error != Error::CriticalFault)) {
    return;
  }
  // The chip lost its configuration, or sits in the Critical Fault state that only a reset leaves
  // (section 4.3.1): EN low, and start again with EnsureInitialized().
  initialized_ = false;
  reset_watch_ = false;
  bus_.WatchReset(false);
  detail::Log(bus_.GetPort(), LogLevel::Warning,
              (error == Error::CriticalFault) ? "tle92466ed: critical fault, EN low"
                                              : "tle92466ed: the chip reset unexpectedly");
  if (!setPin(CtrlPin::Enable, GpioSignal::INACTIVE)) {
    detail::Log(bus_.GetPort(), LogLevel::Error, "tle92466ed: EN could not be driven low");
  }
}

template <SpiPort Port>
Result<void> TLE92466ED<Port>::fail(Error error, const char* message) noexcept {
  detail::Log(bus_.GetPort(), LogLevel::Error, message);
  return Result<void>::Failure(error);
}

} // namespace tle92466ed

#endif // TLE92466ED_IPP
