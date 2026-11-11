// clang-format off: the tables below must keep one row per line.
/**
 * @file tle92466ed_spi_interface.hpp
 * @brief The SPI port of the TLE92466ED driver: the CRTP base, its pin enums, the `SpiPort`
 * concept, the bus requirements, and the frame codec and pipeline of section 5.2.
 * @copyright Copyright (c) 2026 The driver-tle92466ed authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The driver never calls an SDK. It calls hooks on a class you derive from
 * `tle92466ed::SpiInterface<Self>`:
 *
 * | Hook | Contract |
 * | --- | --- |
 * | `bool EnsureInitialized() noexcept` | Bring the bus and the control outputs up on the first call, each output at its idle level; `true` at once afterwards |
 * | `bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept` | One full-duplex transfer of `len` = 4 bytes, both buffers given, CSN low for the 32 bits and high after |
 * | `void DelayUs(uint32_t us) noexcept` | Wait at least `us` microseconds |
 *
 * Every frame is 32 bits in one CSN low period (section 5.1.1): a transfer of any other length is
 * ignored by the chip. The port moves raw bytes: it never retries, never interprets the frame and
 * never configures the chip. What the chip needs from the bus is below as `constexpr` values
 * (`kSpiMode` to `kFrameBytes`, Table 18); a port checks its settings against them with
 * `static_assert`.
 *
 * Control pins and their idle levels (handbook KB-HW-018): the port drives each output to its idle
 * level before it enables the pin's output driver and before the first transfer. RESN idles in
 * reset (`GpioSignal::ACTIVE`, low: every channel off until the driver releases it), EN idles low
 * (`GpioSignal::INACTIVE`: every channel off), DRV0 and DRV1 idle low (`INACTIVE`). FAULTN is an
 * open-drain input. A pin that is not wired to the MCU returns `true` from `GpioSet()` without
 * doing anything (RESN pulled up, EN tied high, DRVx tied to GND).
 */
// clang-format on
#ifndef TLE92466ED_SPI_INTERFACE_HPP
#define TLE92466ED_SPI_INTERFACE_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>

#include "tle92466ed_detail.hpp"
#include "tle92466ed_registers.hpp"
#include "tle92466ed_types.hpp"

namespace tle92466ed {

/**
 * @name Bus requirements
 * What the TLE92466ED needs from the SPI bus (sections 5.1.1, 5.1.3, 5.2.1; Table 18). A port and a
 * board configuration check their settings against these with `static_assert` (handbook
 * KB-CPP-123).
 * @{
 */
/// SPI mode 1: SCK idles low, SO changes on the rising edge and SI is sampled on the falling edge
/// (section 5.1.1).
inline constexpr uint8_t kSpiMode = 1;
/// Each frame is sent most significant bit first, on SI and SO alike (section 5.2.1).
inline constexpr bool kMsbFirst = true;
/// Highest SCK frequency, f_SCK max (Table 18).
inline constexpr uint32_t kMaxClockHz = 8'000'000;
/// Shortest CSN high time between two frames, t_CSN_TD (Table 18).
inline constexpr uint32_t kMinCsHighNs = 600;
/// CSN falling edge to the first SCK rising edge, t_CSN_LEAD (Table 18).
inline constexpr uint32_t kCsSetupNs = 250;
/// Last SCK falling edge to CSN rising, t_CSN_LAG (Table 18).
inline constexpr uint32_t kCsHoldNs = 250;
/// `false`: the datasheet sets no limit on idle SCK time inside a frame; the chip counts 32 clocks
/// between the CSN edges (section 5.1.1).
inline constexpr bool kContinuousWord = false;
/// Bytes per transfer: one 32-bit frame per CSN low period (section 5.1.1).
inline constexpr std::size_t kFrameBytes = 4;
/** @} */

static_assert((1'000'000'000ULL / kMaxClockHz) == 125ULL, "f_SCK max 8 MHz");
static_assert(kMinCsHighNs > (kCsSetupNs + kCsHoldNs), "t_CSN_TD is the longest CSN timing");

/** @brief The chip's control pins the port drives (`GpioSet`) or reads (`GpioRead`). */
enum class CtrlPin : uint8_t {
  Reset = 0,  ///< RESN, active low: ACTIVE holds the chip in reset (section 4.2.3).
  Enable = 1, ///< EN, active high: ACTIVE lets the channels switch (section 4.2.3).
  Drv0 = 2,   ///< DRV0, active high: the direct-drive input of channels in that mode.
  Drv1 = 3,   ///< DRV1, active high: as DRV0.
  Fault = 4,  ///< FAULTN, open drain, active low on the pin: read only; ACTIVE while a fault is
              ///< reported (section 4.2.3).
};

/** @brief Logical level of a control pin; the port maps it to the physical polarity. */
enum class GpioSignal : uint8_t {
  INACTIVE = 0, ///< The pin function is deasserted.
  ACTIVE = 1,   ///< The pin function is asserted.
};

/**
 * @brief CRTP base of the platform SPI port.
 *
 * Derive `class MyPort : public tle92466ed::SpiInterface<MyPort>` and implement the hooks in this
 * file's description. The base declares no hook itself (a forwarding hook would turn a missing
 * override into infinite recursion) and no data; the driver checks your class with the `SpiPort`
 * concept, so a missing or misdeclared hook is a compile error that names it.
 *
 * @tparam Derived Your port class.
 */
template <typename Derived>
class SpiInterface {
public:
  /** @brief Ports are not copied: the driver holds a reference. */
  SpiInterface(const SpiInterface&) = delete;
  /** @brief Ports are not copied: the driver holds a reference. */
  SpiInterface& operator=(const SpiInterface&) = delete;

protected:
  /** @brief Only the derived port is constructed. */
  SpiInterface() noexcept = default;
  /** @brief Destroyed through the derived port only. */
  ~SpiInterface() noexcept = default;
};

/**
 * @brief Satisfied by a class that derives from `SpiInterface<Port>` and implements the required
 * hooks with exactly the types above, `noexcept` and returning `bool`.
 *
 * Each hook's address is matched with `detail::ExactHook`, which takes a member function of exactly
 * the signature and converts no value: a hook declared with a narrower length (`uint16_t len`) or
 * without `noexcept` does not satisfy the concept, instead of truncating every transfer silently
 * (KB-CPP20-006).
 *
 * @tparam Port The port class.
 */
template <typename Port>
concept SpiPort = std::derived_from<Port, SpiInterface<Port>> && requires {
  detail::ExactHook<Port, bool() noexcept>::Take(&Port::EnsureInitialized);
  detail::ExactHook<Port, bool(const uint8_t*, uint8_t*, std::size_t) noexcept>::Take(
      &Port::Transfer);
};

namespace detail {

/// The width of the reply a request expects (section 5.2.3).
enum class Width : uint8_t {
  Bits16 = 0, ///< Reply mode 00B: status, R/W echo and 16 data bits.
  Bits22 = 1, ///< Reply mode 01B: 22 data bits.
};

/// A decoded reply: its error, and its data (or the critical fault flags).
struct Reply {
  Error error{Error::Ok}; ///< `Ok`, or why the reply is not data.
  uint32_t data{0};       ///< 16 or 22 data bits; the flags byte of a critical fault frame.
};

/// The don't-care bits 23..17 of a read frame are sent as 1100000B, so a read frame looped back to
/// MISO decodes as the reserved reply mode 11B and can never pass as a reply (section 5.2.3.2).
inline constexpr uint32_t kReadFiller = 0x00C0'0000U;

/// The read frame for `address` with its CRC (sections 5.1.2, 5.2.3.2).
[[nodiscard]] constexpr uint32_t EncodeRead(uint16_t address) noexcept {
  const uint32_t word = kReadFiller | static_cast<uint32_t>(address);
  return (static_cast<uint32_t>(reg::FrameCrc(word)) << 24U) | word;
}

/// The write frame of `data` to the 7-bit `address` with its CRC (sections 5.1.2, 5.2.3.1).
[[nodiscard]] constexpr uint32_t EncodeWrite(uint16_t address, uint16_t data) noexcept {
  const uint32_t word =
      ((static_cast<uint32_t>(address) & reg::kWriteAddressMask) << reg::kWriteAddressShift) |
      reg::kFrameRw | static_cast<uint32_t>(data);
  return (static_cast<uint32_t>(reg::FrameCrc(word)) << 24U) | word;
}

/// The error a 16-bit reply's status field reports (section 5.2.3.3); `Ok` for 00000B.
[[nodiscard]] constexpr Error StatusError(uint32_t status) noexcept {
  if (status == 0U) {
    return Error::Ok;
  }
  if (status == reg::kStatusCrcError) {
    return Error::Crc;
  }
  if ((status == reg::kStatusFrameError) || (status == reg::kStatusWriteReadOnly)) {
    return Error::FrameRejected;
  }
  return Error::DeviceFault; // 100B to 110B: internal bus fault
}

/**
 * @brief Decode the word clocked in during a frame as the reply to the previous frame (handbook
 * KB-ARCH-042, KB-CPP-116). In order: a stuck or looped-back line (`NoDevice`), a critical fault
 * frame (its bits 31..24 are don't care, so no CRC is checked), the CRC, the reserved reply mode,
 * the status of a 16-bit reply, then the width and R/W echo the request expects.
 * @param miso The word read.
 * @param mosi The word sent in the same frame.
 * @param width The width the request expects.
 * @param write Whether the request was a write.
 * @return The reply.
 */
[[nodiscard]] constexpr Reply DecodeReply(uint32_t miso, uint32_t mosi, Width width,
                                          bool write) noexcept {
  if ((miso == 0U) || (miso == 0xFFFF'FFFFU) || (miso == mosi)) {
    return {.error = Error::NoDevice, .data = 0U};
  }
  const uint32_t mode = (miso >> reg::kReplyModeShift) & 0x3U;
  if (mode == reg::kReplyModeCritical) {
    return {.error = Error::CriticalFault, .data = miso & 0xFFU};
  }
  if (static_cast<uint32_t>(reg::FrameCrc(miso)) != (miso >> 24U)) {
    return {.error = Error::Crc, .data = 0U};
  }
  if (mode == reg::kReplyMode16) {
    const Error status = StatusError((miso >> reg::kReplyStatusShift) & reg::kReplyStatusMask);
    if (status != Error::Ok) {
      return {.error = status, .data = 0U};
    }
    const bool rw = (miso & reg::kFrameRw) != 0U;
    if ((width != Width::Bits16) || (rw != write)) {
      return {.error = Error::ReplyMismatch, .data = 0U};
    }
    return {.error = Error::Ok, .data = miso & reg::kData16Mask};
  }
  if ((mode != reg::kReplyMode22) || (width != Width::Bits22) || write) {
    return {.error = Error::ReplyMismatch, .data = 0U};
  }
  return {.error = Error::Ok, .data = miso & reg::kData22Mask};
}

/// A 16-bit reply word with a valid CRC, as the chip builds it (for the proofs below and tests).
[[nodiscard]] constexpr uint32_t Reply16(uint32_t data, bool write, uint32_t status) noexcept {
  const uint32_t word = ((status & reg::kReplyStatusMask) << reg::kReplyStatusShift) |
                        (write ? reg::kFrameRw : 0U) | (data & reg::kData16Mask);
  return (static_cast<uint32_t>(reg::FrameCrc(word)) << 24U) | word;
}

// The codec, proved on the datasheet's frame layout (section 5.2.3).
static_assert(EncodeRead(reg::kIcvid) ==
              (0x00C0'0200U + (uint32_t{reg::FrameCrc(0xC0'0200U)} << 24U)));
static_assert((EncodeWrite(reg::kChCtrl, 0x8000U) & 0x00FF'FFFFU) == 0x01'8000U);
static_assert((EncodeWrite(0x0049U, 0x1234U) & 0x00FE'0000U) == (0x49U << 17U));
static_assert(DecodeReply(Reply16(0xC1FEU, false, 0U), 0U, Width::Bits16, false).data == 0xC1FEU);
static_assert(DecodeReply(Reply16(0xC1FEU, false, 0U), 0U, Width::Bits22, false).error ==
              Error::ReplyMismatch);
static_assert(DecodeReply(Reply16(0U, true, 2U), 0U, Width::Bits16, true).error == Error::Crc);
static_assert(DecodeReply(EncodeRead(reg::kIcvid), EncodeRead(reg::kIcvid), Width::Bits16, false)
                  .error == Error::NoDevice);
static_assert(DecodeReply(0x0080'00E0U, 0U, Width::Bits16, false).error == Error::CriticalFault);
static_assert(DecodeReply(Reply16(1U, false, 0U) ^ 0x0100'0000U, 0U, Width::Bits16, false).error ==
              Error::Crc);

/**
 * @brief The SPI pipeline of section 5.2.1: the chip answers each frame during the next one.
 *
 * Every access is two frames, completed inside one call (handbook KB-ARCH-043): the request, then
 * an anchor read whose reply arrives with the next access. The anchor is chosen to differ in width
 * from the request's reply (FB_STAT, 22 bits, after a 16-bit request or a write; ICVID, 16 bits,
 * after a 22-bit read), so a request the chip never took leaves the previous anchor's reply in its
 * slot, and that reply fails the width check instead of passing as data. Each access also checks
 * the reply to the previous anchor: an ICVID that does not read manufacturer 0xC1, or any reply
 * that fails `DecodeReply()`, means the pipeline is out of step. After any failure the next access
 * first sends one anchor frame alone, whose reply it discards (handbook KB-CPP-118). A request
 * the chip ignores (a frame with the wrong number of clocks, section 5.1.1) leaves the previous
 * reply in both slots: the same word twice is refused too.
 *
 * The transfer buffers are members, so they live as long as the driver object (handbook KB-HW-019).
 *
 * @tparam Port The port.
 */
template <SpiPort Port>
class Transport {
public:
  /** @param port The port; it must outlive the transport. */
  explicit Transport(Port& port) noexcept : port_(port) {}

  /** @return The port. */
  Port& GetPort() & noexcept {
    return port_;
  }

  /**
   * @brief One access: `write` sends `data` to `address`, otherwise reads it. No retry.
   * @return The reply's data, or `Bus`, `NoDevice`, `CriticalFault`, `Crc`, `ReplyMismatch`,
   * `FrameRejected`, `DeviceFault` or `UnexpectedReset` (with the reset watch armed).
   */
  Result<uint32_t> Access(uint16_t address, bool write, uint16_t data) noexcept {
    if (!synced_ && !resync()) {
      return Result<uint32_t>::Failure(Error::Bus);
    }
    const Width width = (!write && reg::Is22Bit(address)) ? Width::Bits22 : Width::Bits16;
    const uint32_t request = write ? EncodeWrite(address, data) : EncodeRead(address);
    const Width anchor = (width == Width::Bits16) ? Width::Bits22 : Width::Bits16;
    const uint32_t anchor_frame = EncodeRead(anchorAddress(anchor));
    const auto first = exchange(request);
    const auto second = first.ok() ? exchange(anchor_frame) : first;
    if (!second.ok()) {
      synced_ = false;
      return Result<uint32_t>::Failure(Error::Bus);
    }
    const Error previous = checkAnchor(first.value, request);
    const bool same_register = !write && (address == anchorAddress(pending_));
    const Reply reply = DecodeReply(second.value, anchor_frame, width, write);
    last_reply_ = second.value;
    pending_ = anchor;
    if (reply.error == Error::CriticalFault) {
      last_critical_ = static_cast<uint8_t>(reply.data);
    }
    Error e = choose(previous, repeated(reply.error, first.value, second.value, same_register));
    if (e == Error::CriticalFault) {
      e = confirmCritical(); // a critical frame has no CRC: one more frame must show it too
    }
    if (e != Error::Ok) {
      synced_ = false;
      diagnostics_.rejected_replies++;
      return Result<uint32_t>::Failure(e);
    }
    return Result<uint32_t>::Success(reply.data);
  }

  /**
   * @brief A read, repeated up to `retries` times when its reply fails a check. A port failure, a
   * confirmed critical fault frame and a detected reset are never retried (handbook KB-ARCH-044).
   * Only for registers a read does not change: a clear-on-read register is read with `Access()`.
   */
  Result<uint32_t> Read(uint16_t address, uint32_t retries) noexcept {
    const uint32_t attempts = ((retries < kMaxRetries) ? retries : kMaxRetries) + 1U;
    Result<uint32_t> r = Result<uint32_t>::Failure(Error::Bus);
    for (uint32_t attempt = 0U; attempt < attempts; ++attempt) {
      r = Access(address, false, 0U);
      if (r.ok() || (r.error == Error::Bus) || (r.error == Error::CriticalFault) ||
          (r.error == Error::UnexpectedReset)) {
        break;
      }
      if ((attempt + 1U) < attempts) {
        diagnostics_.retries++;
      }
    }
    return r;
  }

  /// Arm or disarm the reset watch: an FB_STAT anchor with POR_event or RES_event set then fails
  /// the access with `UnexpectedReset` (section 4.1.1).
  void WatchReset(bool armed) noexcept {
    watch_reset_ = armed;
  }
  /// Check the manufacturer byte of every ICVID anchor reply: on once the identity is known.
  void ExpectIdentity(bool on) noexcept {
    expect_identity_ = on;
  }
  /// Forget the pipeline state: the next access starts with an anchor frame alone.
  void Desync() noexcept {
    synced_ = false;
  }
  /// The last word read in a reply slot (handbook KB-CPP-129).
  [[nodiscard]] uint32_t LastReply() const noexcept {
    return last_reply_;
  }
  /// The flags byte of the last critical fault frame; 0 before the first.
  [[nodiscard]] uint8_t LastCritical() const noexcept {
    return last_critical_;
  }
  /// The last FB_STAT anchor reply, 22 bits; 0 before the first.
  [[nodiscard]] uint32_t LastStatus() const noexcept {
    return last_status_;
  }
  /// The protocol counters.
  [[nodiscard]] const BusDiagnostics& Diagnostics() const& noexcept {
    return diagnostics_;
  }
  /// Not on a temporary.
  void Diagnostics() const&& = delete;

private:
  /// One 32-bit frame, MSB first (section 5.2.1).
  Result<uint32_t> exchange(uint32_t mosi) noexcept {
    for (uint32_t i = 0U; i < kFrameBytes; ++i) {
      tx_[i] = static_cast<uint8_t>((mosi >> (24U - (8U * i))) & 0xFFU);
    }
    diagnostics_.frames++;
    if (!port_.Transfer(tx_.data(), rx_.data(), kFrameBytes)) {
      return Result<uint32_t>::Failure(Error::Bus);
    }
    uint32_t miso = 0U;
    for (const uint8_t byte : rx_) {
      miso = (miso << 8U) | static_cast<uint32_t>(byte);
    }
    return Result<uint32_t>::Success(miso);
  }

  /// The register an anchor of `width` reads: FB_STAT (22 bits) or ICVID (16 bits).
  [[nodiscard]] static constexpr uint16_t anchorAddress(Width width) noexcept {
    return (width == Width::Bits22) ? reg::kFbStat : reg::kIcvid;
  }

  /// The same word in both slots: the chip ignored the request and sent the previous reply again
  /// (KB-CPP-116). Only a read of the anchor's own register may repeat it.
  [[nodiscard]] static constexpr Error repeated(Error reply, uint32_t first, uint32_t second,
                                                bool same_register) noexcept {
    return ((reply == Error::Ok) && (first == second) && !same_register) ? Error::ReplyMismatch
                                                                         : reply;
  }

  /// A critical fault frame carries no CRC (5.2.3.5), so noise can fake one: it counts only when
  /// the next frame is a critical fault frame too (KB-CPP-116). Otherwise the access failed with a
  /// mismatch, which a read retries.
  Error confirmCritical() noexcept {
    const uint32_t frame = EncodeRead(reg::kIcvid);
    const auto again = exchange(frame);
    if (!again.ok()) {
      return Error::Bus;
    }
    const Reply r = DecodeReply(again.value, frame, Width::Bits16, false);
    if (r.error != Error::CriticalFault) {
      return Error::ReplyMismatch;
    }
    last_critical_ = static_cast<uint8_t>(r.data);
    return Error::CriticalFault;
  }

  /// One anchor frame alone, its reply discarded: the pipeline in a known state again.
  bool resync() noexcept {
    diagnostics_.resyncs++;
    if (!exchange(EncodeRead(reg::kIcvid)).ok()) {
      return false;
    }
    pending_ = Width::Bits16;
    synced_ = true;
    return true;
  }

  /// The reply to the previous anchor, clocked in during `request`.
  Error checkAnchor(uint32_t miso, uint32_t request) noexcept {
    const Reply r = DecodeReply(miso, request, pending_, false);
    if (r.error != Error::Ok) {
      if (r.error == Error::CriticalFault) {
        last_critical_ = static_cast<uint8_t>(r.data);
      }
      return r.error;
    }
    if (pending_ == Width::Bits16) {
      return (!expect_identity_ ||
              (reg::IcManufacturer::Extract(static_cast<uint16_t>(r.data)) == kManufacturerId))
                 ? Error::Ok
                 : Error::ReplyMismatch;
    }
    last_status_ = r.data;
    const bool reset = (r.data & (reg::kStatPorEvent | reg::kStatResEvent)) != 0U;
    return (watch_reset_ && reset) ? Error::UnexpectedReset : Error::Ok;
  }

  /// The error of an access: a dead line first, then a critical fault, then the anchor, then the
  /// reply.
  [[nodiscard]] static constexpr Error choose(Error anchor, Error reply) noexcept {
    if (reply == Error::NoDevice) {
      return reply;
    }
    if ((anchor == Error::CriticalFault) || (reply == Error::CriticalFault)) {
      return Error::CriticalFault;
    }
    return (anchor != Error::Ok) ? anchor : reply;
  }

  Port& port_;
  std::array<uint8_t, kFrameBytes> tx_{};
  std::array<uint8_t, kFrameBytes> rx_{};
  BusDiagnostics diagnostics_{};
  uint32_t last_reply_{0};
  uint32_t last_status_{0};
  uint8_t last_critical_{0};
  Width pending_{Width::Bits16};
  bool synced_{false};
  bool watch_reset_{false};
  bool expect_identity_{false};
};

} // namespace detail

} // namespace tle92466ed

#endif // TLE92466ED_SPI_INTERFACE_HPP
