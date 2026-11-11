/**
 * @file quickstart.cpp
 * @brief The Quickstart page's example, compiled by the host build (never run).
 */
#include "my_port.hpp"

// --8<-- [start:quickstart]
#include "tle92466ed.hpp"

int drive_one_valve(MyPort& port) {
  tle92466ed::Config config{};                              // reset values, no channel loaded
  config.vio = tle92466ed::VioLevel::V3V3;                  // the board's VIO
  config.channels[0] = tle92466ed::CurrentControlChannel(); // CH0: a valve, ICC, 1 kHz PWM
  tle92466ed::TLE92466ED<MyPort> driver(port, config);      // no I/O yet

  if (auto r = driver.EnsureInitialized(); !r) { // 1. reset, INIT_DONE, ICVID, every register
    return static_cast<int>(r.error);            //    Bus, NoDevice, Timeout: check the wiring
  }
  if (auto r = driver.EnterMissionMode(); !r) { // 2. EN high, OP_MODE = 1
    return static_cast<int>(r.error);           //    MissionBlocked: read the faults
  }
  if (auto r = driver.SetCurrent(tle92466ed::Channel::Ch0, 400'000); !r) { // 3. 400 mA
    return static_cast<int>(r.error);
  }
  return static_cast<int>(driver.EnableChannel(tle92466ed::Channel::Ch0).error); // 4. on
}
// --8<-- [end:quickstart]
