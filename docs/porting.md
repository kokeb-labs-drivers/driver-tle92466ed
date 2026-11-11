# Porting

Run the driver on a new MCU by writing one class: the port.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Infineon IFX-ntz1491827137291 1.2:

    - Pins, RESN, EN, DRV0/DRV1 and FAULTN: 2 Pin configuration; 4.2.3 TDS_I/O Pins, [p. 7](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=7)
    - SPI frame, mode and CRC-8 SAE J1850: 5.1.1 TDS_General information; 5.1.2 TDS_Cyclic redundancy check (CRC), [p. 47](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=47)
    - SPI timing: 5.1.4 Electrical characteristics SPI interface, Table 18, [p. 48](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf#page=48)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

The driver never calls your SDK. It calls hooks on a class you derive from `tle92466ed::SpiInterface<Self>` (in
[`inc/tle92466ed_spi_interface.hpp`](../inc/tle92466ed_spi_interface.hpp)). The base uses CRTP and declares no hook
itself; the driver template is constrained by the `tle92466ed::SpiPort` concept, so calls are resolved at compile
time (no virtual functions, nothing to allocate) and a missing or misdeclared hook is a compile error at the point
of use.

## What you implement

| Method | Required | Contract |
| --- | --- | --- |
| `bool EnsureInitialized() noexcept` | yes | Bring the bus up on the first call; return `true` at once afterwards. `false` makes the driver's `EnsureInitialized()` return `Bus` |
| `bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept` | yes | One full-duplex 32-bit frame: `len` is always 4, both buffers are given, CSN low for the 4 bytes and high after. `false` on any bus error |
| `void DelayUs(uint32_t us) noexcept` | yes, for every wait | Wait at least `us` microseconds |
| `bool GpioSet(CtrlPin, GpioSignal) noexcept` | no | Drive RESN, EN, DRV0 or DRV1; `false` if the pin write failed; `true` for a pin tied on the board |
| `bool GpioRead(CtrlPin, GpioSignal&) noexcept` | no | Read FAULTN (`CtrlPin::Fault`); `false` if it cannot be read |
| `void Log(LogLevel, const char* message) noexcept` | no | Print one line; `message` is plain text, never a format string |

## The port

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

Rules the transfer must follow:

- **One call, one frame.** Keep CSN low for the 4 bytes; the chip takes the frame only when exactly 32 bits were
  clocked between the CSN edges (section 5.1.1). Raise CSN after it for at least 600 ns.
- **The bus requirements below**: mode 1, MSB first, up to `kMaxClockHz`. Start at 2 MHz on jumper wires.
- **Raw bytes only.** Do not interpret, retry or reorder bytes; the driver owns the framing, the CRC and the
  retries.
- **Return `false` for a bus error.** The driver turns it into `Error::Bus` and never ignores it.
- **Bounded time-outs.** A stuck bus fails the call; it does not block forever.
- **No other frames to this chip during a call.** A call's frames belong together: the reply to each comes in the
  next. Hold your bus lock for the whole driver call when tasks share the driver; other devices on the bus may use
  it between the frames, since this chip ignores SCK while its CSN is high.
- **Buffers the bus can reach.** The driver's frame buffers are members of the driver object; if your SPI uses
  DMA from a restricted memory region, place the driver object there.

## Bus requirements

The driver states what the TLE92466ED needs from the bus as `constexpr` values in
[`inc/tle92466ed_spi_interface.hpp`](../inc/tle92466ed_spi_interface.hpp) (handbook KB-CPP-123). Check your port's
settings against them with `static_assert`, as both example ports do, so a port that clocks too fast or in the
wrong mode does not compile:

| Constant | Value | Meaning (datasheet 5.1, Table 18) |
| --- | --- | --- |
| `kSpiMode` | `1` | CPOL 0, CPHA 1: SO changes on the rising edge, SI is sampled on the falling edge |
| `kMsbFirst` | `true` | Most significant bit first |
| `kMaxClockHz` | `8'000'000` | f_SCK max |
| `kMinCsHighNs` | `600` | CSN high between two frames, t_CSN_TD |
| `kCsSetupNs`, `kCsHoldNs` | `250`, `250` | CSN low to the first SCK edge (t_CSN_LEAD), last SCK edge to CSN high (t_CSN_LAG) |
| `kContinuousWord` | `false` | Idle SCK time inside a frame is allowed; the chip counts 32 clocks |
| `kFrameBytes` | `4` | One 32-bit frame per transfer |

## Control pins

Each control pin has a defined idle level, and the port drives it there before it enables the pin's output and
before the driver's first transfer (handbook KB-HW-018). Configuring a pin as an output first and setting its level
afterwards gives the chip a pulse it did not ask for, for example a reset.

| `CtrlPin` | Package pin | Polarity | Idle level | Before the first transfer |
| --- | --- | --- | --- | --- |
| `Reset` | RESN (26) | active low | low, `ACTIVE`: in reset | Set low, then enable the output |
| `Enable` | EN (27) | active high | low, `INACTIVE` | Set low, then enable the output |
| `Drv0`, `Drv1` | DRV0 (25), DRV1 (28) | active high | low, `INACTIVE` | Set low, then enable the output |
| `Fault` | FAULTN (11) | active low, open drain | input with a pull-up to VIO | Configure as an input |

`GpioSignal::ACTIVE` always means "function asserted"; the port maps it to the electrical level. RESN idles in
reset so the chip, and every output, stays off from the MCU's reset until `EnsureInitialized()` releases it. A pin
tied on the board needs no hook, or a hook that returns `true` for it.

## Optional hooks

The driver finds the optional hooks at compile time with C++20 `requires`, in one place
(`tle92466ed::detail`). A missing hook falls back in a documented, safe way or fails to compile; it never skips a
step silently.

| Hook | Used for | Without it |
| --- | --- | --- |
| `DelayUs()` | t_POR or t_RESN at start-up, the INIT_DONE and self-test polls | Compile error at the first call that waits |
| `GpioSet()` | RESN pulse in `EnsureInitialized()` and `HardwareReset()`; EN in `EnterMissionMode()`, `EnterConfigMode()`, `Deinitialize()`; `SetDrivePin()` | `EnsureInitialized()` waits t_POR; `HardwareReset()` and `SetDrivePin()` return `Error::Unsupported` |
| `GpioRead()` | `ReadFaultPin()` | `ReadFaultPin()` returns `Error::Unsupported` |
| `Log()` | One line per lifecycle event or start-up failure | Nothing is logged |

A port with every hook:

```cpp
--8<-- "tests/docs/ports.cpp:full-port"
```

```cpp
--8<-- "tests/docs/ports.cpp:hardware-reset"
```

A `GpioSet()` that returns `void`, or a `GpioRead()` that takes the level by value, is a compile error: the driver
must see a failed pin write and must get the level back. The [compile-fail tests](testing.md) check these rules.

## Reference implementations

Every port lives with its example, in `examples/<platform>/`; the driver has none. [Platforms](platforms.md) lists
them and how to add one.

- [`examples/host/host_tle92466ed_port.hpp`](../examples/host/host_tle92466ed_port.hpp): any PC, against a simulated
  chip. Start from it for a host build or a test bench; it is also the smallest complete port.
- [`examples/esp32/main/esp32_tle92466ed_port.hpp`](../examples/esp32/main/esp32_tle92466ed_port.hpp): ESP-IDF, with
  every hook. Start from it for any ESP-IDF target.

## Check your port

1. Build it: the ports on this page are compiled by the host CI from [`tests/docs/`](../tests/docs/my_port.hpp),
   with the driver instantiated over them.
2. Call `EnsureInitialized()`: `Ok` proves that frames go both ways with valid CRCs, that ICVID read 0xC1 and that
   GLOBAL_CONFIG read back what was written.
3. Call `ReadSupply()`: VIO and VDD read within a few percent of your meter; `GetBusDiagnostics().retries` stays 0.
4. With a load on LOAD0, `EnterMissionMode()`, `SetCurrent()`, `EnableChannel()`, then `ReadChannelFeedback()`:
   the average current is within 1 % (2.5 mA below 250 mA) of the setpoint.

## Threads

The driver is not thread-safe. Give each driver object one owner task, or guard every call with one mutex. When
several devices share a bus, the port must also serialise the bus. See [Contract](contract.md#threading).

## Editor setup

The clang-format and clang-tidy configuration is in `_config/`, and the repository root has no copy of it. Tools
that take a path use it directly; CI does the same:

| Tool | Setting |
| --- | --- |
| Command line | `clang-format --style=file:_config/.clang-format`, `clang-tidy --config-file=_config/.clang-tidy -p build` |
| VS Code (C/C++ extension) | `"C_Cpp.clang_format_style": "file:${workspaceFolder}/_config/.clang-format"` and `"C_Cpp.codeAnalysis.clangTidy.args": ["--config-file=${workspaceFolder}/_config/.clang-tidy"]` |
| clangd (VS Code clangd, Neovim, Emacs), CLion | These find `.clang-format` and `.clang-tidy` only by name, from the file upwards: copy both to the root once (`cp _config/.clang-format _config/.clang-tidy .`). `.gitignore` lists the copies, so they never reach a commit; copy again after `_config/` changes |

Configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (clangd: `--compile-commands-dir=build`) so the editor sees the
same flags as the build.
