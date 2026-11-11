/**
 * @file hook_not_noexcept.cpp
 * @brief Must NOT compile: a Transfer hook that is not noexcept breaks the port contract, so the
 * port concept rejects it (handbook KB-CPP20-006).
 *
 * EXPECT: SpiPort
 */
#include <cstddef>
#include <cstdint>

#include "tle92466ed.hpp"

class ThrowingPort : public tle92466ed::SpiInterface<ThrowingPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) {
    (void)tx;
    (void)rx;
    return len > 0U;
  }
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
};

int main() {
  ThrowingPort port;
  tle92466ed::TLE92466ED<ThrowingPort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
