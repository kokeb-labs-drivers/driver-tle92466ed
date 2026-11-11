/**
 * @file test_rig.hpp
 * @brief A port, a driver and a configuration with two loaded current-control channels: what most
 * host tests start from.
 */
#ifndef TLE92466ED_TEST_RIG_HPP
#define TLE92466ED_TEST_RIG_HPP

#include <cstdint>

#include "mock_tle92466ed.hpp"
#include "tle92466ed.hpp"

namespace tle92466ed_test {

/// A value of `E` that is not one of its enumerators, as a corrupted or careless caller could pass:
/// the driver must refuse it. The one place the tests make such a value.
template <typename E>
constexpr E NotAnEnumerator(uint8_t raw) noexcept {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): out of range on purpose
  return static_cast<E>(raw);
}

/// CH0 and CH1 loaded, current control with feedback; the rest at their reset values.
inline tle92466ed::Config TwoChannels() noexcept {
  tle92466ed::Config c{};
  c.channels[0] = tle92466ed::CurrentControlChannel();
  c.channels[1] = tle92466ed::CurrentControlChannel();
  return c;
}

/// A port and a driver on it. The port is declared first, so it outlives the driver.
template <typename Port>
struct Rig {
  Port port;
  tle92466ed::TLE92466ED<Port> drv;

  explicit Rig(const tle92466ed::Config& config = TwoChannels()) noexcept : drv(port, config) {}

  /// Start the driver; true on success.
  bool Start() noexcept {
    return drv.EnsureInitialized().ok();
  }
  /// Start and enter Mission Mode; true on success.
  bool StartMission() noexcept {
    return Start() && drv.EnterMissionMode().ok();
  }
};

} // namespace tle92466ed_test

#endif // TLE92466ED_TEST_RIG_HPP
