/**
 * @file hook_narrowed_parameter.cpp
 * @brief Must NOT compile: a Transfer hook with a narrower length would truncate every transfer, so
 * the port concept rejects it (handbook KB-CPP20-006).
 *
 * EXPECT: SpiPort
 */
#include <cstddef>
#include <cstdint>

#include "tle92466ed.hpp"

class NarrowPort : public tle92466ed::SpiInterface<NarrowPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, uint16_t len) noexcept {
    (void)tx;
    (void)rx;
    return len > 0U;
  }
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
};

int main() {
  NarrowPort port;
  tle92466ed::TLE92466ED<NarrowPort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
