/**
 * @file my_port.hpp
 * @brief The SPI port shown on the Quickstart and Porting pages, compiled by the host build.
 *
 * Every platform call goes through the `platform_*` functions declared below; a real port replaces
 * them with its SDK (ESP-IDF, STM32 HAL, Zephyr, Linux spidev).
 */
#ifndef TLE92466ED_MY_PORT_HPP
#define TLE92466ED_MY_PORT_HPP

#include <cstddef>
#include <cstdint>

#include "tle92466ed_spi_interface.hpp"

// Your SDK's calls. Declared only: the docs build checks that the port compiles.
bool platform_spi_init() noexcept; // SPI mode 1, MSB first, <= 8 MHz, CSN high >= 600 ns
bool platform_spi_transfer(const uint8_t* tx, uint8_t* rx,
                           std::size_t len) noexcept; // CSN low for all 4 bytes
void platform_delay_us(uint32_t us) noexcept;         // busy-wait or sleep

// --8<-- [start:port]
class MyPort : public tle92466ed::SpiInterface<MyPort> {
public:
  // Bring the bus up once; later calls return true at once.
  bool EnsureInitialized() noexcept {
    if (!ready_) {
      ready_ = platform_spi_init();
    }
    return ready_;
  }

  // One 32-bit frame: CSN low for the 4 bytes, both buffers given, CSN high after.
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return platform_spi_transfer(tx, rx, len);
  }

  // Wait at least `us` microseconds: t_POR and the INIT_DONE polls depend on it.
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

private:
  bool ready_ = false;
};
// --8<-- [end:port]

// The bus requirements, checked at compile time (handbook KB-CPP-123).
static_assert(tle92466ed::kSpiMode == 1U, "the port is set up for SPI mode 1");
static_assert(tle92466ed::kMaxClockHz >= 4'000'000U, "the port runs SCK at 4 MHz");

#endif // TLE92466ED_MY_PORT_HPP
