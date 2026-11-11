/**
 * @file delay_us_narrowed.cpp
 * @brief Must NOT compile: a DelayUs hook with a narrower parameter would truncate long waits, so
 * the driver rejects it instead of using it (handbook KB-CPP20-007).
 *
 * EXPECT: DelayUs hook must be
 */
#include <cstdint>

#include "mock_tle92466ed.hpp"
#include "tle92466ed.hpp"

class ShortDelayPort : public tle92466ed_test::MockPortBase<ShortDelayPort> {
public:
  void DelayUs(uint16_t us) noexcept {
    (void)us;
  }
};

int main() {
  ShortDelayPort port;
  tle92466ed::TLE92466ED<ShortDelayPort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
