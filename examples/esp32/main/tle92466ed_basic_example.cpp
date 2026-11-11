/**
 * @file tle92466ed_basic_example.cpp
 * @brief Start the TLE92466ED from the menuconfig values, drive a valve on LOAD0 at 300 mA, and log
 * its current feedback and any fault twice a second.
 *
 * Wiring: examples/esp32/README.md. Expected output: one "initialised" line with the design step,
 * then "CH0 ... uA" lines. Every Result is checked and logged by name.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_tle92466ed_port.hpp"
#include "tle92466ed.hpp"
#include "tle92466ed_kconfig.hpp"

namespace {
constexpr const char* kTag = "tle92466ed_example";
constexpr uint32_t kSetpointUa = 300'000;

using Driver = tle92466ed::TLE92466ED<Esp32Tle92466edPort>;

/// Start, Mission Mode, setpoint, enable; logs the step that failed.
bool Start(Driver& driver) {
  if (const auto r = driver.EnsureInitialized(); !r) {
    ESP_LOGE(kTag, "EnsureInitialized failed: %s", tle92466ed::ToString(r.error));
    return false;
  }
  ESP_LOGI(kTag, "initialised, design step 0x%02X", driver.GetIcVersion());
  if (const auto r = driver.EnterMissionMode(); !r) {
    ESP_LOGE(kTag, "EnterMissionMode failed: %s", tle92466ed::ToString(r.error));
    return false;
  }
  const auto set = driver.SetCurrent(tle92466ed::Channel::Ch0, kSetpointUa);
  if (!set) {
    ESP_LOGE(kTag, "SetCurrent failed: %s", tle92466ed::ToString(set.error));
    return false;
  }
  const auto en = driver.EnableChannel(tle92466ed::Channel::Ch0, tle92466ed::Verify::ReadBack);
  if (!en) {
    ESP_LOGE(kTag, "EnableChannel failed: %s", tle92466ed::ToString(en.error));
    return false;
  }
  ESP_LOGI(kTag, "CH0 on, setpoint %lu uA", static_cast<unsigned long>(set.value));
  return true;
}

/// One pass of the loop: feedback, then faults.
void Service(Driver& driver) {
  if (driver.GetConfig().spi_watchdog) {
    (void)driver.ReloadWatchdog();
  }
  const auto fb = driver.ReadChannelFeedback(tle92466ed::Channel::Ch0);
  if (fb) {
    ESP_LOGI(kTag, "CH0 %ld uA, duty %lu/10000, VBAT %lu mV",
             static_cast<long>(fb.value.average_current_ua),
             static_cast<unsigned long>(fb.value.duty_permyriad),
             static_cast<unsigned long>(fb.value.vbat_mv));
  } else if (fb.error != tle92466ed::Error::NotReady) {
    ESP_LOGW(kTag, "feedback: %s", tle92466ed::ToString(fb.error));
  } else {
    // No new measurement period since the last read: nothing to log.
  }
  const auto faults = driver.ReadFaults();
  if (faults && faults.value.Any()) {
    const tle92466ed::ChannelFaults ch0 = faults.value.Of(tle92466ed::Channel::Ch0);
    ESP_LOGW(kTag, "faults: CH0 olsg %d ol %d sg %d oc %d ot %d", ch0.olsg, ch0.open_load,
             ch0.short_to_ground, ch0.overcurrent, ch0.overtemperature);
    (void)driver.ClearFaults();
  }
}
} // namespace

extern "C" void app_main(void) {
  static Esp32Tle92466edPort port; // static: the driver keeps a reference
  static tle92466ed::Config config = tle92466ed::KconfigDefaults();
  config.channels[0] = tle92466ed::CurrentControlChannel(); // a valve on LOAD0
  static Driver driver(port, config);                       // no I/O

  ESP_LOGI(kTag, "TLE92466ED driver %s", tle92466ed::GetDriverVersion());
  if (!Start(driver)) {
    return;
  }
  while (true) {
    if (!driver.IsInitialized()) {
      ESP_LOGW(kTag, "the chip reset: starting it again");
      (void)Start(driver);
    } else {
      Service(driver);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
