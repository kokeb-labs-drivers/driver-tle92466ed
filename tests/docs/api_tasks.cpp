/**
 * @file api_tasks.cpp
 * @brief The API guide's examples, compiled by the host build (never run).
 */
#include <cstdio>

#include "my_port.hpp"
#include "tle92466ed.hpp"

using Driver = tle92466ed::TLE92466ED<MyPort>;
using tle92466ed::Channel;

// --8<-- [start:lifecycle]
tle92466ed::Error BringUp(Driver& driver) {
  // The one entry point: starts the chip on the first call, then returns Ok with no I/O. After a
  // failure the next call starts again from the beginning. It ends in Config Mode.
  if (const auto r = driver.EnsureInitialized(); !r) {
    std::printf("TLE92466ED init: %s\n", tle92466ed::ToString(r.error));
    return r.error;
  }
  return driver.EnterMissionMode().error; // MissionBlocked: a supply, clock or watchdog fault
}
// --8<-- [end:lifecycle]

// --8<-- [start:drive]
tle92466ed::Error Energise(Driver& driver, uint32_t microamps) {
  const auto set = driver.SetCurrent(Channel::Ch0, microamps); // checked, never clamped
  if (!set) {
    return set.error; // InvalidArgument above 1.5 A, or with dither above 1.8 A
  }
  std::printf("programmed %lu uA\n", static_cast<unsigned long>(set.value)); // TARGET rounding
  return driver.EnableChannel(Channel::Ch0, tle92466ed::Verify::ReadBack).error;
}
// --8<-- [end:drive]

// --8<-- [start:feedback]
void SampleCurrent(Driver& driver) {
  const auto fb = driver.ReadChannelFeedback(Channel::Ch0); // freeze, read, release (4.10.5)
  if (fb.error == tle92466ed::Error::NotReady) {
    return; // no new measurement period yet: never a 0 mA reading
  }
  if (fb) {
    std::printf("%ld uA, duty %lu/10000\n", static_cast<long>(fb.value.average_current_ua),
                static_cast<unsigned long>(fb.value.duty_permyriad));
  }
}
// --8<-- [end:feedback]

// --8<-- [start:faults]
void ServiceFaults(Driver& driver) {
  const auto f = driver.ReadFaults(); // every diagnosis register, ORed into the latch
  if (!f || !f.value.Any()) {
    return;
  }
  const tle92466ed::ChannelFaults ch0 = f.value.Of(Channel::Ch0);
  if (ch0.open_load || ch0.short_to_ground || ch0.olsg) {
    std::printf("CH0 load fault\n");
  }
  // The chip keeps a faulted channel off until its flags are cleared and EN_CH is set again.
  if (driver.ClearChannelFaults(Channel::Ch0)) {
    (void)driver.EnableChannel(Channel::Ch0);
  }
}
// --8<-- [end:faults]

// --8<-- [start:watchdog]
void ControlTick(Driver& driver) {
  // With Config::spi_watchdog, a missed reload ends Mission Mode and turns every channel off.
  if (const auto r = driver.ReloadWatchdog(); !r) {
    std::printf("watchdog reload: %s\n", tle92466ed::ToString(r.error));
  }
}
// --8<-- [end:watchdog]

// --8<-- [start:errors]
const char* Describe(tle92466ed::Error e) {
  switch (e) {
  case tle92466ed::Error::Bus:
  case tle92466ed::Error::NoDevice:
    return "check SPI wiring, VIO and the chip select";
  case tle92466ed::Error::CriticalFault:
    return "internal supply or clock fault: reset the chip";
  case tle92466ed::Error::Crc:
  case tle92466ed::Error::ReplyMismatch:
    return "signal integrity: lower SCK, check CSN high time";
  default:
    return tle92466ed::ToString(e);
  }
}
// --8<-- [end:errors]

// --8<-- [start:deinit]
void PowerDown(Driver& driver) {
  if (auto r = driver.Deinitialize(); !r) { // Config Mode, every channel off, EN low
    std::printf("TLE92466ED shutdown write failed: %s\n", tle92466ed::ToString(r.error));
  }
}
// --8<-- [end:deinit]
