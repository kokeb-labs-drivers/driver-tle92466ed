/**
 * @file gpio_set_returns_void.cpp
 * @brief Must NOT compile: an optional hook with the wrong signature is rejected, not ignored.
 *
 * EXPECT: GpioSet hook must be
 */
#include "mock_tle92466ed.hpp"
#include "tle92466ed.hpp"

class VoidGpioPort : public tle92466ed_test::MockPortBase<VoidGpioPort> {
public:
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
  void GpioSet(tle92466ed::CtrlPin pin, tle92466ed::GpioSignal signal) noexcept {
    (void)pin;
    (void)signal;
  }
};

int main() {
  VoidGpioPort port;
  tle92466ed::TLE92466ED<VoidGpioPort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
