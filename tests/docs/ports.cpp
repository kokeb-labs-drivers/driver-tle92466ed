/**
 * @file ports.cpp
 * @brief The port with every optional hook, as shown on the Porting page. Compiled, never run.
 */
#include <cstddef>
#include <cstdint>

#include "tle92466ed.hpp"

bool platform_spi_init() noexcept;
bool platform_spi_transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept;
void platform_delay_us(uint32_t us) noexcept;
bool platform_gpio_write(int pin, bool high) noexcept;
bool platform_gpio_read(int pin, bool& high) noexcept;
void platform_log(const char* line) noexcept;

// --8<-- [start:full-port]
class FullPort : public tle92466ed::SpiInterface<FullPort> {
public:
  bool EnsureInitialized() noexcept {
    // Before the bus: RESN low (in reset), EN, DRV0 and DRV1 low (handbook KB-HW-018).
    return platform_gpio_write(kResn, false) && platform_gpio_write(kEn, false) &&
           platform_gpio_write(kDrv0, false) && platform_gpio_write(kDrv1, false) &&
           platform_spi_init();
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return platform_spi_transfer(tx, rx, len);
  }

  // Required by every call that waits (reset, INIT_DONE, self-test).
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

  // Optional: RESN, EN, DRV0, DRV1. ACTIVE on RESN means "in reset", a low level on the pin.
  bool GpioSet(tle92466ed::CtrlPin pin, tle92466ed::GpioSignal signal) noexcept {
    const bool active = signal == tle92466ed::GpioSignal::ACTIVE;
    switch (pin) {
    case tle92466ed::CtrlPin::Reset:
      return platform_gpio_write(kResn, !active);
    case tle92466ed::CtrlPin::Enable:
      return platform_gpio_write(kEn, active);
    case tle92466ed::CtrlPin::Drv0:
      return platform_gpio_write(kDrv0, active);
    case tle92466ed::CtrlPin::Drv1:
      return platform_gpio_write(kDrv1, active);
    default:
      return false; // FAULTN is an input
    }
  }

  // Optional: FAULTN, open drain with a pull-up. ACTIVE (a fault) is a low level.
  bool GpioRead(tle92466ed::CtrlPin pin, tle92466ed::GpioSignal& signal) noexcept {
    bool high = true;
    if ((pin != tle92466ed::CtrlPin::Fault) || !platform_gpio_read(kFaultn, high)) {
      return false;
    }
    signal = high ? tle92466ed::GpioSignal::INACTIVE : tle92466ed::GpioSignal::ACTIVE;
    return true;
  }

  // Optional: one line per lifecycle event or failure. The driver never formats; the port does.
  void Log(tle92466ed::LogLevel level, const char* message) noexcept {
    if (level > tle92466ed::LogLevel::Warning) {
      return;
    }
    platform_log(message); // plain text: never use it as a format string
  }

private:
  static constexpr int kResn = 7;
  static constexpr int kEn = 8;
  static constexpr int kDrv0 = 9;
  static constexpr int kDrv1 = 10;
  static constexpr int kFaultn = 11;
};
// --8<-- [end:full-port]

// --8<-- [start:hardware-reset]
tle92466ed::Error RecoverFromCriticalFault(tle92466ed::TLE92466ED<FullPort>& driver) {
  // Only RESN or a power cycle leaves the Critical Fault state; without GpioSet() this call
  // returns Error::Unsupported.
  return driver.HardwareReset().error;
}
// --8<-- [end:hardware-reset]
