/**
 * @file mock_tle92466ed_bus.hpp
 * @brief SPI mock ports over the chip model, with the line faults a real bus shows.
 *
 * `MockPortBase<Self>` implements the required hooks of the SPI port over a `MockBusState`. Derive
 * from it to build a port with any set of optional hooks:
 *
 * | Port | Hooks |
 * | --- | --- |
 * | `MinimalPort` | the required ones plus `DelayUs()` (needed by every wait) |
 * | `FullPort` | also `GpioSet()` (RESN, EN, DRV0, DRV1), `GpioRead()` (FAULTN) and `Log()` |
 */
#ifndef TLE92466ED_MOCK_TLE92466ED_BUS_HPP
#define TLE92466ED_MOCK_TLE92466ED_BUS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "mock_tle92466ed.hpp"
#include "tle92466ed_spi_interface.hpp"
#include "tle92466ed_types.hpp"

namespace tle92466ed_test {

/// What MISO does, whatever the chip drives.
enum class Miso : uint8_t {
  Chip = 0,      ///< The chip's reply.
  StuckLow = 1,  ///< All zeros: no VIO, no chip, a broken trace with a pull-down.
  StuckHigh = 2, ///< All ones: SO floating with a pull-up.
  Loopback = 3,  ///< MOSI shorted to MISO: every frame echoed.
};

/// Shared state of the mock ports: the chip, fault injection and counters.
struct MockBusState {
  Tle92466edModel chip;
  bool bus_up_ok = true;         ///< EnsureInitialized() result.
  int fail_transfer = -1;        ///< Fail the transfer with this index (0-based); -1 never.
  int lose_transfer = -1;        ///< The chip never sees this transfer; MISO reads 0xFFFFFFFF.
  int corrupt_transfer = -1;     ///< Flip bit 0 of what MISO carried in this transfer.
  int corrupt_count = 1;         ///< How many transfers from `corrupt_transfer` on are corrupted.
  int corrupt_mosi = -1;         ///< Flip bit 0 of what the chip receives in this transfer.
  int replace_transfer = -1;     ///< MISO carries `replace_word` in this transfer instead.
  int replace_count = 1;         ///< How many transfers from `replace_transfer` on are replaced.
  uint32_t clock_hz = 4'000'000; ///< SCK: each frame takes 32 clocks plus 1 us of CSN high.
  int ignore_transfer = -1;      ///< The chip ignores this frame (wrong clock count): MISO carries
                                 ///< the pending reply and the chip's state does not change.
  uint32_t replace_word = 0;
  Miso miso = Miso::Chip;
  int fail_pin = -1;     ///< Fail the GpioSet() call with this index.
  int transfers = 0;     ///< Transfers so far.
  int pin_calls = 0;     ///< GpioSet() calls so far.
  int bus_up_calls = 0;  ///< EnsureInitialized() calls so far.
  int bad_transfers = 0; ///< Transfers outside the port contract (not 4 bytes, no buffer).
  std::array<uint32_t, 4096> mosi{}; ///< Every frame sent.

  bool DoTransfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    const int index = transfers++;
    if ((tx == nullptr) || (rx == nullptr) || (len != 4U)) {
      bad_transfers++;
      return false;
    }
    const uint32_t out = (static_cast<uint32_t>(tx[0]) << 24U) |
                         (static_cast<uint32_t>(tx[1]) << 16U) |
                         (static_cast<uint32_t>(tx[2]) << 8U) | static_cast<uint32_t>(tx[3]);
    if (static_cast<std::size_t>(index) < mosi.size()) {
      mosi[static_cast<std::size_t>(index)] = out;
    }
    if (index == fail_transfer) {
      return false;
    }
    uint32_t in = 0xFFFF'FFFFU;
    if (index == ignore_transfer) {
      in = chip.Pending();
    } else if (index != lose_transfer) {
      in = chip.Frame((index == corrupt_mosi) ? (out ^ 1U) : out);
    }
    if ((index >= replace_transfer) && (index < (replace_transfer + replace_count))) {
      in = replace_word;
    }
    chip.Elapse(((32U * 1'000'000U) / clock_hz) + 1U); // the frame's own time on the bus
    if ((index >= corrupt_transfer) && (index < (corrupt_transfer + corrupt_count))) {
      in ^= 1U;
    }
    switch (miso) {
    case Miso::StuckLow:
      in = 0U;
      break;
    case Miso::StuckHigh:
      in = 0xFFFF'FFFFU;
      break;
    case Miso::Loopback:
      in = out;
      break;
    case Miso::Chip:
    default:
      break;
    }
    for (std::size_t i = 0; i < 4U; ++i) {
      rx[i] = static_cast<uint8_t>(in >> (24U - (8U * i)));
    }
    return true;
  }

  bool DoPin(tle92466ed::CtrlPin pin, tle92466ed::GpioSignal signal) noexcept {
    const int index = pin_calls++;
    if (index == fail_pin) {
      return false;
    }
    const bool active = signal == tle92466ed::GpioSignal::ACTIVE;
    switch (pin) {
    case tle92466ed::CtrlPin::Reset:
      chip.SetResn(!active); // RESN is active low
      return true;
    case tle92466ed::CtrlPin::Enable:
      chip.SetEn(active);
      return true;
    case tle92466ed::CtrlPin::Drv0:
      chip.SetDrv(0, active);
      return true;
    case tle92466ed::CtrlPin::Drv1:
      chip.SetDrv(1, active);
      return true;
    case tle92466ed::CtrlPin::Fault:
    default:
      break;
    }
    return false; // FAULTN is an input
  }

  /// The address field of frame `index`: bits 15..0 of a read, 23..17 of a write.
  [[nodiscard]] uint16_t AddressOf(int index) const noexcept {
    const uint32_t f = mosi[static_cast<std::size_t>(index)];
    return static_cast<uint16_t>(((f & 0x10000U) != 0U) ? ((f >> 17U) & 0x7FU) : (f & 0xFFFFU));
  }
  /// Whether frame `index` was a write.
  [[nodiscard]] bool IsWrite(int index) const noexcept {
    return (mosi[static_cast<std::size_t>(index)] & 0x10000U) != 0U;
  }
  /// How many writes to `address` from transfer `from` on.
  [[nodiscard]] int CountWrites(uint16_t address, int from = 0) const noexcept {
    int n = 0;
    for (int i = from; (i < transfers) && (static_cast<std::size_t>(i) < mosi.size()); ++i) {
      n += (IsWrite(i) && (AddressOf(i) == address)) ? 1 : 0;
    }
    return n;
  }
  /// How many reads of `address` from transfer `from` on.
  [[nodiscard]] int CountReads(uint16_t address, int from = 0) const noexcept {
    int n = 0;
    for (int i = from; (i < transfers) && (static_cast<std::size_t>(i) < mosi.size()); ++i) {
      n += (!IsWrite(i) && (AddressOf(i) == address)) ? 1 : 0;
    }
    return n;
  }
};

/// The required SPI hooks over a MockBusState. Derive `class P : public MockPortBase<P>`.
template <typename Self>
class MockPortBase : public tle92466ed::SpiInterface<Self> {
public:
  MockBusState state;

  bool EnsureInitialized() noexcept {
    state.bus_up_calls++;
    return state.bus_up_ok;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return state.DoTransfer(tx, rx, len);
  }
};

/// Required hooks plus the delay; time passes in the model as the driver waits. Without GpioSet
/// the board ties EN high and RESN high.
class MinimalPort : public MockPortBase<MinimalPort> {
public:
  uint64_t delayed_us = 0; ///< Sum of DelayUs() arguments.

  MinimalPort() noexcept {
    state.chip.SetEn(true);
  }

  void DelayUs(uint32_t us) noexcept {
    delayed_us += us;
    state.chip.Elapse(us);
  }
};

/// Every optional hook, with counters.
class FullPort : public MockPortBase<FullPort> {
public:
  uint64_t delayed_us = 0; ///< Sum of DelayUs() arguments.
  int log_lines = 0;       ///< Log() calls.
  int error_lines = 0;     ///< Log() calls at LogLevel::Error.
  int warning_lines = 0;   ///< Log() calls at LogLevel::Warning.
  bool fault_read_ok = true;
  uint64_t reset_low_us = 0; ///< Time RESN was last held low.

  void DelayUs(uint32_t us) noexcept {
    delayed_us += us;
    if (state.chip.InReset()) {
      reset_low_us += us;
    }
    state.chip.Elapse(us);
  }
  bool GpioSet(tle92466ed::CtrlPin pin, tle92466ed::GpioSignal signal) noexcept {
    if ((pin == tle92466ed::CtrlPin::Reset) && (signal == tle92466ed::GpioSignal::ACTIVE)) {
      reset_low_us = 0;
    }
    return state.DoPin(pin, signal);
  }
  bool GpioRead(tle92466ed::CtrlPin pin, tle92466ed::GpioSignal& signal) noexcept {
    if ((pin != tle92466ed::CtrlPin::Fault) || !fault_read_ok) {
      return false;
    }
    signal = state.chip.FaultPinLow() ? tle92466ed::GpioSignal::ACTIVE
                                      : tle92466ed::GpioSignal::INACTIVE;
    return true;
  }
  void Log(tle92466ed::LogLevel level, const char* message) noexcept {
    (void)message;
    log_lines++;
    error_lines += (level == tle92466ed::LogLevel::Error) ? 1 : 0;
    warning_lines += (level == tle92466ed::LogLevel::Warning) ? 1 : 0;
  }
};

} // namespace tle92466ed_test

#endif // TLE92466ED_MOCK_TLE92466ED_BUS_HPP
