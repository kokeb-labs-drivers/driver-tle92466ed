/**
 * @file main.cpp
 * @brief Builds the driver from the installed package only: headers, version header and .ipp.
 */
#include <cstddef>
#include <cstdint>

#include "tle92466ed.hpp"

namespace {

// A port that answers every read with 0: enough to instantiate the whole driver template.
class NullPort : public tle92466ed::SpiInterface<NullPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    (void)tx;
    for (std::size_t i = 0; rx != nullptr && i < len; ++i) {
      rx[i] = 0;
    }
    return true;
  }
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
};

} // namespace

int main() {
  NullPort port;
  tle92466ed::TLE92466ED<NullPort> driver(port);
  // MISO reads all zeros, so EnsureInitialized() must fail with NoDevice: proof the template
  // compiled and ran.
  const auto r = driver.EnsureInitialized();
  return (r.error == tle92466ed::Error::NoDevice && tle92466ed::GetDriverVersion()[0] != '\0') ? 0
                                                                                               : 1;
}
