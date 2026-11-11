/**
 * @file test_kconfig.cpp
 * @brief KconfigDefaults(): the defaults without Kconfig, and menuconfig values when they are set.
 *
 * Built twice: without CONFIG_ macros (KCONFIG_SET undefined) and with the values an sdkconfig.h
 * would define (KCONFIG_SET=1, see tests/CMakeLists.txt).
 */
#include "test_harness.hpp"
#include "tle92466ed_kconfig.hpp"

#ifdef KCONFIG_SET
TEST_CASE("Kconfig values reach the Config") {
  constexpr tle92466ed::Config kC = tle92466ed::KconfigDefaults();
  CHECK_EQ(kC.vio, tle92466ed::VioLevel::V3V3);
  CHECK(!kC.crc_check);
  CHECK(kC.spi_watchdog);
  CHECK_EQ(kC.watchdog_reload, 427U); // 250 ms * 28 MHz / 2^14
  CHECK_EQ(kC.clock, tle92466ed::ClockSource::External);
  CHECK_EQ(kC.external_clock_khz, 4000U);
  CHECK_EQ(kC.init_timeout_us, 20000U);
  CHECK_EQ(kC.max_retries, 1U);
  CHECK(kC.clock_watchdog); // not a Kconfig option: datasheet default
  CHECK_EQ(tle92466ed::Validate(kC), tle92466ed::Error::Ok);
}
#else
TEST_CASE("Without Kconfig, KconfigDefaults() equals Config{}") {
  constexpr tle92466ed::Config kC = tle92466ed::KconfigDefaults();
  constexpr tle92466ed::Config kD{};
  CHECK_EQ(kC.vio, kD.vio);
  CHECK_EQ(kC.crc_check, kD.crc_check);
  CHECK_EQ(kC.spi_watchdog, kD.spi_watchdog);
  CHECK_EQ(kC.watchdog_reload, kD.watchdog_reload);
  CHECK_EQ(kC.clock, kD.clock);
  CHECK_EQ(kC.init_timeout_us, kD.init_timeout_us);
  CHECK_EQ(kC.max_retries, kD.max_retries);
}
#endif
