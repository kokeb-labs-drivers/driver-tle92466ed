/**
 * @file not_a_port.cpp
 * @brief Must NOT compile: the driver is constrained by the port concept.
 *
 * EXPECT: Port
 */
#include "tle92466ed.hpp"

struct NotAPort {
  bool EnsureInitialized() noexcept {
    return true;
  }
};

int main() {
  NotAPort port;
  tle92466ed::TLE92466ED<NotAPort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
