/**
 * @file checked_config_invalid.cpp
 * @brief Must NOT compile: a CheckedConfig built from a configuration that Validate() rejects fails
 * the build, naming the error (handbook KB-CPP20-013).
 *
 * EXPECT: ConfigIsInvalid
 */
#include "tle92466ed_types.hpp"

int main() {
  // WD_TIME 0 would expire the SPI watchdog at once: Validate() rejects it (5.3.2.11).
  constexpr tle92466ed::CheckedConfig kBoard{tle92466ed::Config{.watchdog_reload = 0}};
  return static_cast<int>(kBoard.Get().watchdog_reload);
}
