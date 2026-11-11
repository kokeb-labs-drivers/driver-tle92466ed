/**
 * @file gpio_read_by_value.cpp
 * @brief Must NOT compile: a GpioRead hook that takes the level by value could never report it, so
 * the driver rejects it instead of ignoring it (handbook KB-CPP20-007).
 *
 * EXPECT: GpioRead hook must be
 */
#include "mock_tle92466ed.hpp"
#include "tle92466ed.hpp"

class ByValuePort : public tle92466ed_test::MockPortBase<ByValuePort> {
public:
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
  bool GpioRead(tle92466ed::CtrlPin pin, tle92466ed::GpioSignal signal) noexcept {
    (void)pin;
    return signal == tle92466ed::GpioSignal::ACTIVE;
  }
};

int main() {
  ByValuePort port;
  tle92466ed::TLE92466ED<ByValuePort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
