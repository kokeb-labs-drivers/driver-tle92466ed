/**
 * @file configuration.cpp
 * @brief The Configuration page's examples, compiled by the host build (never run).
 */
#include "my_port.hpp"
#include "tle92466ed.hpp"
#include "tle92466ed_kconfig.hpp"

// --8<-- [start:custom]
constexpr tle92466ed::Config TwoValves() {
  tle92466ed::Config c{}; // datasheet reset values
  c.vio = tle92466ed::VioLevel::V3V3;
  c.spi_watchdog = true; // reload it from the control loop
  c.watchdog_reload = tle92466ed::WatchdogReloadCode(50U, tle92466ed::kInternalClockHz).value;
  tle92466ed::ChannelConfig valve = tle92466ed::CurrentControlChannel();
  valve.diag.ol_fixed = 6;     // OLSG below 47 mA: open load or short to ground
  valve.dither.steps = 8;      // 8 steps per quarter period ...
  valve.dither.step_size = 40; // ... of 40 TARGET counts: 19.5 mA amplitude
  valve.dither.mant = 109;     // t_ref_clk = 109 * 2^3 / 28 MHz = 31 us: a 1 ms period
  valve.dither.exp = 3;
  c.channels[0] = valve;
  c.channels[4] = valve;
  return c; // tle92466ed::Validate(c) checks it without the chip
}

tle92466ed::Error StartValves(MyPort& port) {
  static tle92466ed::TLE92466ED<MyPort> driver(port, TwoValves()); // stored, no I/O
  return driver.EnsureInitialized().error; // InvalidArgument if the config is out of range
}
// --8<-- [end:custom]

// --8<-- [start:parallel]
constexpr tle92466ed::Config OneBigValve() {
  tle92466ed::Config c{};
  c.parallel[static_cast<std::size_t>(tle92466ed::ParallelPair::Ch1Ch2)] = true; // CH1 + CH2
  c.channels[1] = tle92466ed::CurrentControlChannel(); // configure the master only
  return c; // SetCurrent(Channel::Ch1, ...) up to 2.7 A; CH2 is driven with it
}
// --8<-- [end:parallel]

// --8<-- [start:kconfig]
tle92466ed::Error StartFromMenuconfig(MyPort& port) {
  tle92466ed::Config c = tle92466ed::KconfigDefaults(); // VIO, CRC, watchdog, clock from menuconfig
  c.channels[0] = tle92466ed::CurrentControlChannel();  // channels are declared in code
  static tle92466ed::TLE92466ED<MyPort> driver(port, c);
  return driver.EnsureInitialized().error;
}
// --8<-- [end:kconfig]

static_assert(tle92466ed::Validate(TwoValves()) == tle92466ed::Error::Ok);
static_assert(tle92466ed::Validate(OneBigValve()) == tle92466ed::Error::Ok);
