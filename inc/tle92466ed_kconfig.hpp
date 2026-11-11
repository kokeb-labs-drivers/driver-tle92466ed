/**
 * @file tle92466ed_kconfig.hpp
 * @brief Optional ESP-IDF layer: a `Config` built from the `CONFIG_TLE92466ED_*` Kconfig options.
 * @copyright Copyright (c) 2026 The driver-tle92466ed authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Include this header only where you want menuconfig to choose the defaults; the driver never
 * includes it and no driver signature depends on it. Under ESP-IDF the values come from
 * `sdkconfig.h`. Without Kconfig every option falls back to the datasheet default, so
 * `KconfigDefaults()` equals `Config{}`.
 *
 * Kconfig writes a bool option set to "n" as an absent macro, so every bool option here is named
 * for the change from the reset value (`CONFIG_TLE92466ED_VIO_3V3`,
 * `CONFIG_TLE92466ED_CRC_CHECK_DISABLE`, `CONFIG_TLE92466ED_SPI_WATCHDOG`): absent means the reset
 * value in both cases. Channels are not Kconfig options: declare them in code.
 *
 * One configuration per program: the inline function below depends on the `CONFIG_` macros, so
 * every translation unit of one program must see the same values (ESP-IDF has one `sdkconfig.h`
 * per build; each host test is its own program). No driver or library header includes this one
 * (handbook KB-CPP20-018).
 */
#ifndef TLE92466ED_KCONFIG_HPP
#define TLE92466ED_KCONFIG_HPP

#ifdef __has_include
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#endif

#include "tle92466ed_types.hpp"

#ifndef CONFIG_TLE92466ED_WATCHDOG_TIMEOUT_MS
/** SPI watchdog timeout in milliseconds when Kconfig does not set it. */
#define CONFIG_TLE92466ED_WATCHDOG_TIMEOUT_MS 100
#endif
#ifndef CONFIG_TLE92466ED_EXTERNAL_CLOCK_KHZ
/** External clock in kHz when Kconfig does not set it: 0, the internal oscillator. */
#define CONFIG_TLE92466ED_EXTERNAL_CLOCK_KHZ 0
#endif
#ifndef CONFIG_TLE92466ED_INIT_TIMEOUT_US
/** INIT_DONE timeout in microseconds when Kconfig does not set it: `Config{}.init_timeout_us`. */
#define CONFIG_TLE92466ED_INIT_TIMEOUT_US 10000
#endif
#ifndef CONFIG_TLE92466ED_MAX_RETRIES
/** Read retries when Kconfig does not set it: `Config{}.max_retries`. */
#define CONFIG_TLE92466ED_MAX_RETRIES 2
#endif

namespace tle92466ed {

/**
 * @brief The configuration menuconfig selected, on top of the datasheet defaults.
 * @return `Config{}` with VIO, the CRC check, the SPI watchdog, the clock and the driver's timing
 * taken from Kconfig. A watchdog timeout out of range leaves WD_TIME at 0, which `Validate()`
 * rejects.
 */
constexpr Config KconfigDefaults() noexcept {
  Config c{};
#if defined(CONFIG_TLE92466ED_VIO_3V3) && CONFIG_TLE92466ED_VIO_3V3
  c.vio = VioLevel::V3V3;
#endif
#if defined(CONFIG_TLE92466ED_CRC_CHECK_DISABLE) && CONFIG_TLE92466ED_CRC_CHECK_DISABLE
  c.crc_check = false;
#endif
  constexpr uint32_t kClockKhz = CONFIG_TLE92466ED_EXTERNAL_CLOCK_KHZ;
  if constexpr (kClockKhz != 0U) {
    c.clock = ClockSource::External;
    c.external_clock_khz = kClockKhz;
  }
#if defined(CONFIG_TLE92466ED_SPI_WATCHDOG) && CONFIG_TLE92466ED_SPI_WATCHDOG
  c.spi_watchdog = true;
  c.watchdog_reload =
      WatchdogReloadCode(CONFIG_TLE92466ED_WATCHDOG_TIMEOUT_MS, SystemClockHz(c)).value;
#endif
  c.init_timeout_us = CONFIG_TLE92466ED_INIT_TIMEOUT_US;
  c.max_retries = static_cast<uint8_t>(CONFIG_TLE92466ED_MAX_RETRIES);
  return c;
}

} // namespace tle92466ed

#endif // TLE92466ED_KCONFIG_HPP
