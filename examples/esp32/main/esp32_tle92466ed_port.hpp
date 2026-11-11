/**
 * @file esp32_tle92466ed_port.hpp
 * @brief ESP-IDF port for the TLE92466ED driver: spi_master for the bus, GPIO for RESN, EN, DRV0,
 * DRV1 and FAULTN, esp_rom delays and ESP_LOG for the log hook.
 *
 * Start from this file for any ESP-IDF target. The pins are in Esp32Tle92466edPins; the defaults
 * match the wiring table in examples/esp32/README.md (ESP32-C6 DevKitC-1).
 */
#ifndef TLE92466ED_ESP32_TLE92466ED_PORT_HPP
#define TLE92466ED_ESP32_TLE92466ED_PORT_HPP

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#include "tle92466ed.hpp"

/// The port's default SPI clock: 2 MHz, so half a clock (250 ns) covers t_CSN_LEAD and t_CSN_LAG.
inline constexpr int kEsp32Tle92466edClockHz = 2'000'000;
/// The port's SPI mode: CPOL 0, CPHA 1.
inline constexpr int kEsp32Tle92466edSpiMode = 1;
// The bus requirements of the driver (handbook KB-CPP-123), checked at compile time.
static_assert(kEsp32Tle92466edClockHz <= static_cast<int>(tle92466ed::kMaxClockHz),
              "the default clock is above the TLE92466ED's f_SCK max");
static_assert(kEsp32Tle92466edSpiMode == tle92466ed::kSpiMode,
              "the TLE92466ED shifts out on the rising and samples on the falling SCK edge");
static_assert(tle92466ed::kMsbFirst, "this port sends MSB first (no SPI_DEVICE_BIT_LSBFIRST)");
static_assert((500'000'000 / kEsp32Tle92466edClockHz) >= static_cast<int>(tle92466ed::kCsSetupNs),
              "half a clock must cover t_CSN_LEAD");

/// Pins and bus settings of the ESP-IDF port; -1 for a pin that is not wired to the MCU.
struct Esp32Tle92466edPins {
  spi_host_device_t host = SPI2_HOST;
  int sclk = 6;
  int mosi = 7;
  int miso = 2;
  int cs = 10;
  int resn = 11;                          ///< RESN; -1 when pulled up on the board.
  int en = 15;                            ///< EN; -1 when tied high.
  int drv0 = -1;                          ///< DRV0; -1 when tied to GND.
  int drv1 = -1;                          ///< DRV1; -1 when tied to GND.
  int faultn = 3;                         ///< FAULTN input with a pull-up; -1 when not read.
  int clock_hz = kEsp32Tle92466edClockHz; ///< Up to `tle92466ed::kMaxClockHz`; checked at start.
};

/// The TLE92466ED SPI port on ESP-IDF, with every optional hook.
class Esp32Tle92466edPort : public tle92466ed::SpiInterface<Esp32Tle92466edPort> {
public:
  explicit Esp32Tle92466edPort(const Esp32Tle92466edPins& pins = {}) noexcept : pins_(pins) {}

  bool EnsureInitialized() noexcept {
    if (device_ != nullptr) {
      return true;
    }
    if (pins_.clock_hz <= 0 || pins_.clock_hz > static_cast<int>(tle92466ed::kMaxClockHz)) {
      ESP_LOGE(kTag, "clock outside the TLE92466ED's bus requirements");
      return false;
    }
    // Idle levels before the outputs are enabled (handbook KB-HW-018): RESN low (in reset), EN,
    // DRV0 and DRV1 low, so no channel can switch before the driver has configured the chip.
    if (!outputAt(pins_.resn, 0) || !outputAt(pins_.en, 0) || !outputAt(pins_.drv0, 0) ||
        !outputAt(pins_.drv1, 0) || !inputPullUp(pins_.faultn)) {
      return false;
    }
    spi_bus_config_t bus{};
    bus.mosi_io_num = pins_.mosi;
    bus.miso_io_num = pins_.miso;
    bus.sclk_io_num = pins_.sclk;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 4;
    const esp_err_t bus_err = spi_bus_initialize(pins_.host, &bus, SPI_DMA_DISABLED);
    if (bus_err != ESP_OK && bus_err != ESP_ERR_INVALID_STATE) { // INVALID_STATE: bus already up
      ESP_LOGE(kTag, "spi_bus_initialize: %s", esp_err_to_name(bus_err));
      return false;
    }
    spi_device_interface_config_t dev{};
    dev.clock_speed_hz = pins_.clock_hz;
    dev.mode = static_cast<uint8_t>(kEsp32Tle92466edSpiMode);
    dev.spics_io_num = pins_.cs; // spi_master raises CSN between frames, well above 600 ns
    dev.queue_size = 1;
    if (spi_bus_add_device(pins_.host, &dev, &device_) != ESP_OK) {
      device_ = nullptr;
      return false;
    }
    return true;
  }

  /// One 32-bit frame; on a shared bus spi_master keeps other devices off while it runs.
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    if (device_ == nullptr || len != tle92466ed::kFrameBytes || tx == nullptr || rx == nullptr) {
      return false;
    }
    spi_transaction_t t{};
    t.length = len * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    return spi_device_polling_transmit(device_, &t) == ESP_OK;
  }

  void DelayUs(uint32_t us) noexcept {
    esp_rom_delay_us(us);
  }

  bool GpioSet(tle92466ed::CtrlPin pin, tle92466ed::GpioSignal signal) noexcept {
    const uint32_t active = signal == tle92466ed::GpioSignal::ACTIVE ? 1 : 0;
    switch (pin) {
    case tle92466ed::CtrlPin::Reset:
      return level(pins_.resn, active == 1 ? 0 : 1); // active low: in reset is level 0
    case tle92466ed::CtrlPin::Enable:
      return level(pins_.en, active);
    case tle92466ed::CtrlPin::Drv0:
      return level(pins_.drv0, active);
    case tle92466ed::CtrlPin::Drv1:
      return level(pins_.drv1, active);
    case tle92466ed::CtrlPin::Fault: // an input: never driven
    default:
      break;
    }
    return false;
  }

  bool GpioRead(tle92466ed::CtrlPin pin, tle92466ed::GpioSignal& signal) noexcept {
    if (pin != tle92466ed::CtrlPin::Fault || pins_.faultn < 0) {
      return false;
    }
    // Open drain, active low: a low level is a fault.
    const bool low = gpio_get_level(static_cast<gpio_num_t>(pins_.faultn)) == 0;
    signal = low ? tle92466ed::GpioSignal::ACTIVE : tle92466ed::GpioSignal::INACTIVE;
    return true;
  }

  void Log(tle92466ed::LogLevel level, const char* message) noexcept {
    const esp_log_level_t esp_level = level == tle92466ed::LogLevel::Error     ? ESP_LOG_ERROR
                                      : level == tle92466ed::LogLevel::Warning ? ESP_LOG_WARN
                                      : level == tle92466ed::LogLevel::Info    ? ESP_LOG_INFO
                                                                               : ESP_LOG_DEBUG;
    esp_log_write(esp_level, kTag, "%s\n", message); // plain text, never a format
  }

private:
  /// A pin that is not wired (-1) needs nothing and reports success.
  static bool level(int gpio, uint32_t value) noexcept {
    return gpio < 0 || gpio_set_level(static_cast<gpio_num_t>(gpio), value) == ESP_OK;
  }
  /// Set the level first, then enable the output driver, so the pin never shows another level.
  static bool outputAt(int gpio, uint32_t value) noexcept {
    if (gpio < 0) {
      return true;
    }
    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << static_cast<unsigned>(gpio);
    io.mode = GPIO_MODE_OUTPUT;
    return gpio_set_level(static_cast<gpio_num_t>(gpio), value) == ESP_OK &&
           gpio_config(&io) == ESP_OK;
  }
  static bool inputPullUp(int gpio) noexcept {
    if (gpio < 0) {
      return true;
    }
    gpio_config_t in{};
    in.pin_bit_mask = 1ULL << static_cast<unsigned>(gpio);
    in.mode = GPIO_MODE_INPUT;
    in.pull_up_en = GPIO_PULLUP_ENABLE;
    return gpio_config(&in) == ESP_OK;
  }

  static constexpr const char* kTag = "tle92466ed";
  Esp32Tle92466edPins pins_;
  spi_device_handle_t device_ = nullptr;
};

#endif // TLE92466ED_ESP32_TLE92466ED_PORT_HPP
