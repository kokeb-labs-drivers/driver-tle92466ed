/**
 * @file main.cpp
 * @brief The TLE92466ED driver on your PC: start a simulated chip, drive one valve channel at
 * 400 mA and read its current feedback.
 *
 * No hardware and no SDK: the host port (host_tle92466ed_port.hpp) connects the real driver to the
 * chip model the host tests use. The same calls run on an MCU with that MCU's port
 * (examples/esp32/ for ESP-IDF). Exit code 0 when every call succeeded.
 */
#include <cstdint>
#include <cstdio>

#include "host_tle92466ed_port.hpp"
#include "tle92466ed.hpp"

namespace {

int Fail(const char* what, tle92466ed::Error e) {
  std::printf("%s failed: %s\n", what, tle92466ed::ToString(e));
  return 1;
}

} // namespace

int main() {
  using tle92466ed::Channel;
  HostTle92466edPort port;
  tle92466ed::Config config{};
  config.channels[0] = tle92466ed::CurrentControlChannel();        // a valve on LOAD0
  tle92466ed::TLE92466ED<HostTle92466edPort> driver(port, config); // no I/O yet

  std::printf("TLE92466ED driver %s on the host\n", tle92466ed::GetDriverVersion());
  if (const auto r = driver.EnsureInitialized(); !r) {
    return Fail("EnsureInitialized", r.error);
  }
  std::printf("ICVID version 0x%02X, reset watch %s\n", driver.GetIcVersion(),
              driver.IsResetWatchArmed() ? "armed" : "off");
  if (const auto r = driver.EnterMissionMode(); !r) {
    return Fail("EnterMissionMode", r.error);
  }
  const auto set = driver.SetCurrent(Channel::Ch0, 400'000);
  if (!set) {
    return Fail("SetCurrent", set.error);
  }
  if (const auto r = driver.EnableChannel(Channel::Ch0, tle92466ed::Verify::ReadBack); !r) {
    return Fail("EnableChannel", r.error);
  }
  std::printf("CH0 setpoint %lu uA\n", static_cast<unsigned long>(set.value));

  for (int i = 0; i < 3; ++i) {
    port.DelayUs(1500); // one measurement period of the 1 ms dither clock, and some
    const auto fb = driver.ReadChannelFeedback(Channel::Ch0);
    if (!fb) {
      return Fail("ReadChannelFeedback", fb.error);
    }
    std::printf("CH0 %ld uA, duty %lu.%02lu %%, VBAT %lu mV\n",
                static_cast<long>(fb.value.average_current_ua),
                static_cast<unsigned long>(fb.value.duty_permyriad / 100U),
                static_cast<unsigned long>(fb.value.duty_permyriad % 100U),
                static_cast<unsigned long>(fb.value.vbat_mv));
  }
  const auto supply = driver.ReadSupply();
  if (!supply) {
    return Fail("ReadSupply", supply.error);
  }
  std::printf("VIO %lu mV, VDD %lu mV, %ld mdegC\n",
              static_cast<unsigned long>(supply.value.vio_mv),
              static_cast<unsigned long>(supply.value.vdd_mv),
              static_cast<long>(supply.value.temperature_mc));
  return driver.Deinitialize().ok() ? 0 : Fail("Deinitialize", tle92466ed::Error::Bus);
}
