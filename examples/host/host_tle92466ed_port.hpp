/**
 * @file host_tle92466ed_port.hpp
 * @brief The host port: the driver's bus hooks over a simulated TLE92466ED, so the example runs on
 * any PC.
 *
 * `tle92466ed_test::MockPortBase` (tests/mock_tle92466ed_bus.hpp) supplies the required bus hooks
 * over the chip model the host tests use. This class adds what a real port adds: `DelayUs()`
 * (here it advances the simulated chip's clock instead of waiting), `GpioSet()` for RESN and EN,
 * and `Log()` to the console. A port for a real bus on a PC (Linux spidev, a USB bridge)
 * implements the same hooks: see docs/porting.md.
 */
#ifndef TLE92466ED_HOST_TLE92466ED_PORT_HPP
#define TLE92466ED_HOST_TLE92466ED_PORT_HPP

#include <cstdint>
#include <cstdio>

#include "mock_tle92466ed.hpp"

/// The host port: simulated chip, real console.
class HostTle92466edPort : public tle92466ed_test::MockPortBase<HostTle92466edPort> {
public:
  /// The bus settings a real port would use; checked against the driver's bus requirements
  /// (handbook KB-CPP-123) like any port's.
  static constexpr uint32_t kClockHz = 4'000'000;
  static constexpr uint8_t kSpiMode = 1;
  static_assert(kClockHz <= tle92466ed::kMaxClockHz, "clock above the TLE92466ED's f_SCK max");
  static_assert(kSpiMode == tle92466ed::kSpiMode, "the TLE92466ED samples in SPI mode 1");

  /// Time passes in the simulated chip; nothing sleeps.
  void DelayUs(uint32_t us) noexcept {
    state.chip.Elapse(us);
  }

  /// RESN, EN, DRV0 and DRV1 of the simulated chip.
  bool GpioSet(tle92466ed::CtrlPin pin, tle92466ed::GpioSignal signal) noexcept {
    return state.DoPin(pin, signal);
  }

  /// Driver messages (they carry their own `tle92466ed:` prefix) go to the console, one line each.
  void Log(tle92466ed::LogLevel level, const char* message) noexcept {
    (void)level;
    (void)std::puts(message); // plain text: never pass it as a format string
  }
};

#endif // TLE92466ED_HOST_TLE92466ED_PORT_HPP
