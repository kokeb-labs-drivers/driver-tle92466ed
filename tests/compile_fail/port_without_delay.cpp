/**
 * @file port_without_delay.cpp
 * @brief Must NOT compile: the chip's reset and conversion waits need the port's DelayUs().
 *
 * EXPECT: DelayUs
 */
#include "mock_tle92466ed.hpp"
#include "tle92466ed.hpp"

class NoDelayPort : public tle92466ed_test::MockPortBase<NoDelayPort> {};

int main() {
  NoDelayPort port;
  tle92466ed::TLE92466ED<NoDelayPort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
