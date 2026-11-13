/**
 * @file fuzz_tle92466ed.cpp
 * @brief Fuzz target: the driver over a bus whose behaviour the fuzzer controls.
 *
 * What the driver reads comes from the chip over the bus, so read data is the untrusted input. The
 * fuzzer's bytes choose a configuration, then a sequence of driver calls; on every transfer they
 * also choose what the bus does: pass it to the chip model, fail it, flip bits of the reply or
 * replace it. They can also change the chip: another identity, an unprogrammed OTP, INIT_DONE that
 * never comes, supply, clock, channel and data faults, the Critical Fault state, a brown-out, a
 * refused Mission Mode, time passing.
 *
 * After every call the target checks the driver's promises (docs/contract.md), and aborts, naming
 * the line, when one breaks:
 *
 * - a call returns an `Error` the enum defines, and a failed one `T{}`;
 * - before `EnsureInitialized()` succeeds, no call makes a transfer;
 * - `InvalidArgument` and `WrongState` mean nothing was sent;
 * - `UnexpectedReset` leaves the driver uninitialised;
 * - a programmed setpoint is inside the operating range and is what `GetCurrentSetpoint()` says;
 * - feedback is a duty of at most 100 % over a measurement period that is not 0;
 * - every transfer is one 32-bit frame with both buffers, and every log line is non-empty text.
 *
 * Built two ways (tests/fuzz/CMakeLists.txt): with libFuzzer (Clang,
 * `-DTLE92466ED_BUILD_FUZZERS=ON`), and as a plain executable that replays the corpus in
 * tests/fuzz/corpus/ as a CTest on any compiler.
 */
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "mock_tle92466ed.hpp" // the chip model, then the bus mock in mock_tle92466ed_bus.hpp
#include "tle92466ed.hpp"

namespace {

using tle92466ed::Channel;
using tle92466ed::Error;
namespace chip = tle92466ed_test::chip;

/// The fuzzer's bytes, read front to back; zeros once they run out.
class Input {
public:
  Input(const uint8_t* data, std::size_t size) noexcept : data_(data), size_(size) {}

  [[nodiscard]] bool Empty() const noexcept {
    return pos_ >= size_;
  }
  uint8_t Byte() noexcept {
    return pos_ < size_ ? data_[pos_++] : uint8_t{0};
  }
  uint16_t Word() noexcept {
    const auto high = Byte();
    return static_cast<uint16_t>((static_cast<uint32_t>(high) << 8U) | Byte());
  }

private:
  const uint8_t* data_;
  std::size_t size_;
  std::size_t pos_ = 0;
};

/// A broken promise: name the line and abort, so libFuzzer saves the input as a crash.
void Check(bool promise, int line = __builtin_LINE()) noexcept {
  if (!promise) {
    (void)std::fprintf(stderr, "fuzz_tle92466ed: promise broken at line %d\n", line);
    std::abort();
  }
}

/// What the bus does with one transfer, chosen by the fuzzer; any other value passes it.
constexpr uint8_t kFail = 0xFF;    ///< The transfer fails; it never reached the chip.
constexpr uint8_t kFlip = 0xFE;    ///< The reply's bits are flipped (noise).
constexpr uint8_t kReplace = 0xFD; ///< The reply is replaced (another device answered).

/// The port: the chip model of the host tests behind a bus the fuzzer controls.
class FuzzPort : public tle92466ed::SpiInterface<FuzzPort> {
public:
  explicit FuzzPort(Input& in) noexcept : in_(in) {}

  [[nodiscard]] bool EnsureInitialized() const noexcept {
    return bus_up_ok_;
  }

  /// One 32-bit frame; the reply is what the fuzzer lets through.
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    Check((tx != nullptr) && (rx != nullptr) && (len == 4U));
    const uint8_t action = nextAction();
    if (action == kFail) {
      return false;
    }
    (void)bus_.DoTransfer(tx, rx, len);
    for (std::size_t i = 0; i < len; ++i) {
      if (action == kFlip) {
        rx[i] = static_cast<uint8_t>(rx[i] ^ in_.Byte());
      } else if (action == kReplace) {
        rx[i] = in_.Byte();
      } else {
        // Pass the byte through unchanged.
      }
    }
    return true;
  }

  void DelayUs(uint32_t us) noexcept {
    bus_.chip.Elapse(us);
  }

  /// RESN, EN, DRV0 and DRV1 of the model; the fuzzer may fail the pin write.
  bool GpioSet(tle92466ed::CtrlPin pin, tle92466ed::GpioSignal signal) noexcept {
    Check(pin != tle92466ed::CtrlPin::Fault);
    if (nextAction() == kFail) {
      return false;
    }
    return bus_.DoPin(pin, signal);
  }

  /// FAULTN; the fuzzer may fail the read.
  bool GpioRead(tle92466ed::CtrlPin pin, tle92466ed::GpioSignal& signal) noexcept {
    Check(pin == tle92466ed::CtrlPin::Fault);
    if (nextAction() == kFail) {
      return false;
    }
    signal =
        bus_.chip.FaultPinLow() ? tle92466ed::GpioSignal::ACTIVE : tle92466ed::GpioSignal::INACTIVE;
    return true;
  }

  /// The driver logs plain text: one non-empty line, never a format string.
  void Log(tle92466ed::LogLevel level, const char* message) noexcept {
    (void)level;
    Check((message != nullptr) && (message[0] != '\0'));
  }

  tle92466ed_test::Tle92466edModel& Chip() noexcept {
    return bus_.chip;
  }
  /// Transfers and pin accesses so far.
  [[nodiscard]] int Transfers() const noexcept {
    return transfers_;
  }
  void SetBusUp(bool ok) noexcept {
    bus_up_ok_ = ok;
  }

private:
  /// Count one bus access and let the fuzzer decide what happens to it.
  uint8_t nextAction() noexcept {
    transfers_++;
    return in_.Byte();
  }

  Input& in_;
  tle92466ed_test::MockBusState bus_;
  int transfers_ = 0;
  bool bus_up_ok_ = true;
};

using Driver = tle92466ed::TLE92466ED<FuzzPort>;

bool KnownError(Error e) noexcept {
  return std::strcmp(tle92466ed::ToString(e), "Unknown") != 0;
}

/// A channel from the fuzzer; 6 and 7 are not channels.
Channel FuzzChannel(Input& in) noexcept {
  return static_cast<Channel>(in.Byte() % 8U);
}

/// A channel configuration from the fuzzer's bytes; sometimes invalid on purpose.
tle92466ed::ChannelConfig FuzzChannelConfig(Input& in) noexcept {
  tle92466ed::ChannelConfig c = tle92466ed::CurrentControlChannel();
  const uint8_t flags = in.Byte();
  c.loaded = (flags & 0x01U) != 0U;
  if ((flags & 0x02U) != 0U) {
    c.mode = static_cast<tle92466ed::ChannelMode>(in.Byte() % 7U); // 6 is not an enumerator
    c.icc.min_int_thresh = static_cast<int8_t>(in.Byte());
    c.icc.period_mant = in.Byte();
    c.icc.period_exp = static_cast<uint8_t>(in.Byte() % 9U);
    c.dither.mant = static_cast<uint16_t>(in.Word() % 1100U);
    c.dither.exp = static_cast<uint8_t>(in.Byte() % 17U);
    c.dither.steps = in.Byte();
    c.dither.flat = in.Byte();
    c.dither.step_size = static_cast<uint16_t>(in.Word() % 4200U);
    c.dither.fast = static_cast<tle92466ed::FastFeedback>(in.Byte() % 4U);
    c.diag.ol_relative_eighths = static_cast<uint8_t>(in.Byte() % 9U);
    c.on_time_mant = static_cast<uint16_t>(in.Word() % 1100U);
  }
  return c;
}

/// A configuration from the fuzzer's bytes; sometimes invalid on purpose.
tle92466ed::Config FuzzConfig(Input& in) noexcept {
  tle92466ed::Config c{};
  const uint8_t flags = in.Byte();
  c.vio = ((flags & 0x01U) != 0U) ? tle92466ed::VioLevel::V3V3 : tle92466ed::VioLevel::V5V0;
  c.crc_check = (flags & 0x02U) == 0U;
  c.spi_watchdog = (flags & 0x04U) != 0U;
  c.watchdog_reload = static_cast<uint16_t>(in.Word() % 2100U); // 0 and above 2047 are invalid
  if ((flags & 0x08U) != 0U) {
    c.clock = tle92466ed::ClockSource::External;
    c.external_clock_khz = 250U * (in.Byte() % 40U);
  }
  c.parallel[0] = (flags & 0x10U) != 0U;
  c.parallel[1] = (flags & 0x20U) != 0U;
  c.vbat_uv_code = in.Byte();
  c.vbat_ov_code = in.Byte();
  c.max_retries = static_cast<uint8_t>(in.Byte() % 5U);
  c.init_timeout_us = 100U * (in.Byte() % 12U); // short, so a stuck chip ends fast
  for (auto& ch : c.channels) {
    if ((in.Byte() & 0x03U) == 0U) {
      ch = FuzzChannelConfig(in);
    }
  }
  return c;
}

/// Change the chip between two calls.
void FuzzChip(tle92466ed_test::Tle92466edModel& m, Input& in) noexcept {
  const uint8_t arg = in.Byte();
  switch (in.Byte() % 12U) {
  case 0:
    m.version = ((arg & 1U) != 0U) ? arg : chip::kVersionA13;
    break;
  case 1:
    m.otp_virgin = !m.otp_virgin;
    break;
  case 2:
    m.init_never = !m.init_never;
    break;
  case 3:
    m.events_stick = !m.events_stick;
    break;
  case 4:
    m.SetGlobalFault(static_cast<uint16_t>(1U << (arg % 15U)));
    break;
  case 5:
    m.SetChannelError(arg % 6U, static_cast<uint16_t>(1U << ((arg >> 3U) % 5U)));
    break;
  case 6:
    m.SetChannelWarning(arg % 6U, static_cast<uint16_t>(1U << ((arg >> 3U) % 5U)));
    break;
  case 7:
    m.EnterCritical(arg);
    break;
  case 8:
    m.refuse_mission = !m.refuse_mission;
    m.bist_never_done = (arg & 1U) != 0U;
    break;
  case 9:
    m.Elapse(100U * arg);
    break;
  case 10:
    m.SetDataFault(static_cast<uint16_t>(arg & 0x1AU));
    break;
  default:
    m.PowerOn(); // a brown-out: reset values, POR_EVENT set
    break;
  }
}

/// The promises of any call.
template <typename R>
void CheckCommon(const Driver& d, const R& r, bool was_up, int sent) noexcept {
  Check(KnownError(r.error));
  if (!was_up) {
    Check((r.error == Error::NotInitialized) && (sent == 0) && !d.IsInitialized());
  }
  if ((r.error == Error::InvalidArgument) || (r.error == Error::WrongState)) {
    Check(sent == 0);
  }
  if (r.error == Error::UnexpectedReset) {
    Check(!d.IsInitialized());
  }
}

/// A failed read returns `T{}`.
template <typename T>
void CheckEmpty(const tle92466ed::Result<T>& r) noexcept {
  if (!r) {
    Check(r.value == T{});
  }
}

void CheckFeedback(const tle92466ed::Result<tle92466ed::ChannelFeedback>& r) noexcept {
  if (r) {
    Check((r.value.duty_permyriad <= 10000U) && (r.value.period_us < 3'000'000U));
  }
}

void CheckSetpoint(const Driver& d, Channel ch, const tle92466ed::Result<uint32_t>& r) noexcept {
  CheckEmpty(r);
  if (r) {
    Check((r.value <= tle92466ed::kMaxParallelSetpointUa) && (d.GetCurrentSetpoint(ch) == r.value));
  }
}

/// A start or reset: initialised exactly when it succeeded.
void CheckStarted(const Driver& d, const tle92466ed::Result<void>& r) noexcept {
  Check(KnownError(r.error) && (r.ok() == d.IsInitialized()));
}

void RunLifecycle(Driver& d, Input& in, uint8_t op, bool was_up, int sent_before,
                  FuzzPort& port) noexcept {
  switch (op) {
  case 0: {
    const auto r = d.EnsureInitialized();
    CheckStarted(d, r);
    if (was_up) {
      Check(r.ok() && (port.Transfers() == sent_before));
    }
    break;
  }
  case 1:
    Check(KnownError(d.Deinitialize().error) && !d.IsInitialized());
    break;
  case 2: {
    const auto r = d.HardwareReset();
    CheckCommon(d, r, was_up, port.Transfers() - sent_before);
    CheckStarted(d, r);
    break;
  }
  case 3: {
    const auto r = d.ApplyConfig(FuzzConfig(in));
    Check(KnownError(r.error));
    if (!was_up || (r.error == Error::InvalidArgument)) {
      Check(port.Transfers() == sent_before);
    }
    break;
  }
  default: {
    const auto r = d.ConfigureChannel(FuzzChannel(in), FuzzChannelConfig(in));
    Check(KnownError(r.error));
    if (!was_up || (r.error == Error::InvalidArgument) || (r.error == Error::WrongState)) {
      Check(port.Transfers() == sent_before);
    }
    break;
  }
  }
}

template <typename R>
void Done(const Driver& d, const R& r, bool was_up, int sent) noexcept {
  CheckCommon(d, r, was_up, sent);
}

void RunControl(Driver& d, Input& in, uint8_t op, bool was_up, int before,
                FuzzPort& port) noexcept {
  const auto sent = [&port, before]() { return port.Transfers() - before; };
  switch (op) {
  case 5: {
    const auto r = d.EnterMissionMode();
    Done(d, r, was_up, sent());
    break;
  }
  case 6: {
    const auto r = d.EnterConfigMode();
    Done(d, r, was_up, sent());
    break;
  }
  case 7: {
    const auto r = d.SetEnabledChannels(in.Byte(), static_cast<tle92466ed::Verify>(in.Byte() % 3U));
    Done(d, r, was_up, sent());
    break;
  }
  case 8: {
    const auto r = d.DisableChannel(FuzzChannel(in));
    Done(d, r, was_up, sent());
    break;
  }
  case 9: {
    const Channel ch = FuzzChannel(in);
    const auto r =
        d.SetCurrent(ch, 12'000U * in.Byte(), static_cast<tle92466ed::Verify>(in.Byte() % 3U));
    Done(d, r, was_up, sent());
    CheckSetpoint(d, ch, r);
    break;
  }
  case 10: {
    const auto r = d.SetDither(FuzzChannel(in), FuzzChannelConfig(in).dither);
    Done(d, r, was_up, sent());
    break;
  }
  case 11: {
    const auto r = d.StartOvercurrentCheck(FuzzChannel(in));
    Done(d, r, was_up, sent());
    break;
  }
  case 12: {
    const auto r = d.SetParallel(static_cast<tle92466ed::ParallelPair>(in.Byte() % 4U),
                                 (in.Byte() & 1U) != 0U);
    Done(d, r, was_up, sent());
    break;
  }
  case 13: {
    const auto r = d.ReloadWatchdog();
    Done(d, r, was_up, sent());
    break;
  }
  default: {
    const auto r = d.ReadChannelControl();
    Done(d, r, was_up, sent());
    CheckEmpty(r);
    break;
  }
  }
}

void RunDiagnostics(Driver& d, Input& in, uint8_t op, bool was_up, int before,
                    FuzzPort& port) noexcept {
  const auto sent = [&port, before]() { return port.Transfers() - before; };
  switch (op) {
  case 15: {
    const auto r = d.ReadStatus();
    Done(d, r, was_up, sent());
    break;
  }
  case 16: {
    const auto r = d.ReadFaults();
    Done(d, r, was_up, sent());
    CheckEmpty(r);
    break;
  }
  case 17: {
    const auto r = d.ClearFaults();
    Done(d, r, was_up, sent());
    break;
  }
  case 18: {
    const auto r = d.ClearChannelFaults(FuzzChannel(in));
    Done(d, r, was_up, sent());
    break;
  }
  case 19: {
    const auto r = d.ReadChannelFeedback(FuzzChannel(in));
    Done(d, r, was_up, sent());
    CheckFeedback(r);
    break;
  }
  case 20: {
    const auto r = d.ReadSupply();
    Done(d, r, was_up, sent());
    break;
  }
  case 21: {
    const auto r = d.ReadPinStatus();
    Done(d, r, was_up, sent());
    break;
  }
  case 22: {
    const auto r = d.RunBist();
    Done(d, r, was_up, sent());
    break;
  }
  case 23: {
    const auto r = d.ReadChipId();
    Done(d, r, was_up, sent());
    CheckEmpty(r);
    break;
  }
  case 24: {
    const auto r = d.ReadRegister(in.Word() % 0x0300U);
    Done(d, r, was_up, sent());
    CheckEmpty(r);
    break;
  }
  case 25: {
    const auto r = d.ReadFaultPin();
    Done(d, r, was_up, sent());
    break;
  }
  case 26: {
    const auto r =
        d.SetDrivePin(static_cast<tle92466ed::CtrlPin>(in.Byte() % 5U), (in.Byte() & 1U) != 0U);
    Done(d, r, was_up, sent());
    break;
  }
  default:
    FuzzChip(port.Chip(), in);
    port.SetBusUp((in.Byte() & 0x07U) != 0U);
    break;
  }
}

void RunOne(Driver& d, FuzzPort& port, Input& in) noexcept {
  const auto op = static_cast<uint8_t>(in.Byte() % 28U);
  const bool was_up = d.IsInitialized();
  const int before = port.Transfers();
  if (op < 5U) {
    RunLifecycle(d, in, op, was_up, before, port);
  } else if (op < 15U) {
    RunControl(d, in, op, was_up, before, port);
  } else {
    RunDiagnostics(d, in, op, was_up, before, port);
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  Input in(data, size);
  FuzzPort port(in);
  Driver driver(port, FuzzConfig(in));
  for (int calls = 0; (calls < 1024) && !in.Empty(); ++calls) {
    RunOne(driver, port, in);
  }
  return 0;
}
