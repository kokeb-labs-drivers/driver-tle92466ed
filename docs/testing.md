# Testing and CI

How the driver is checked: host tests against a model of the chip, a fuzzer that corrupts what the chip sends
back, compiled documentation, an install test, ESP32 and Cortex-M builds, static analysis and measured quality
gates, on every pull request.

## Host tests

The tests build the real driver against a protocol and register model of the TLE92466ED: 32-bit frames with the
CRC it checks while CRC_EN is set, the reply one frame later in the 16-bit, 22-bit or critical fault format, the
reset values, Config and Mission Mode with their rules, clear-on-write flags with the two clears OC and OTP_ECC_ERR
need, a channel kept off while it has a fault, parallel pairs, INIT_DONE, the SPI watchdog, the feedback freeze
with a measurement period from the dither clock, and the self-test. The mock bus adds the faults a real line shows:
a failed transfer, MISO stuck low or high, MOSI looped back, a bit flipped on MISO or MOSI, a frame the chip ignores
or never sees, a reply replaced by any word. They build with the firmware's flags, `-fno-exceptions -fno-rtti`, and
warnings as errors, on GCC and Clang.

```bash
cmake -S . -B build -DTLE92466ED_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

| File | Covers |
| --- | --- |
| [`tests/test_tle92466ed.cpp`](../tests/test_tle92466ed.cpp) | Start-up (each step and each failure), the calls before the start, operation states, the SPI watchdog, configuration, reset detection |
| [`tests/test_channels.cpp`](../tests/test_channels.cpp) | Enables, setpoints and their limits, parallel pairs, dither, the OFF-state overcurrent check, the DRV pins |
| [`tests/test_diagnostics.cpp`](../tests/test_diagnostics.cpp) | Status, faults and the latch, clearing, feedback, supplies, pins, self-test, chip ID, the raw read |
| [`tests/test_protocol.cpp`](../tests/test_protocol.cpp) | The frame layout and CRC, the bus requirements, stuck and looped-back lines, retries, never-retried writes, ignored and lost frames, the anchor, the status field, critical fault frames, resynchronisation |
| [`tests/test_state_safety.cpp`](../tests/test_state_safety.cpp) | The driver's copy never overrides the chip: faulted channels stay off, Mission Mode only as Figure 4 allows, flags read before they are cleared, critical faults and resets turn EN off, the start-up order, the frame counts |
| [`tests/test_ambiguous_writes.cpp`](../tests/test_ambiguous_writes.cpp) | A write whose reply failed or whose frame was lost: every guard that depends on what it may have changed refuses with `WrongState` and no frame, until a read or a successful write (setpoint, EN_CH, OP_MODE, pairs, channel, dither and global registers) |
| [`tests/test_bus_faults.cpp`](../tests/test_bus_faults.cpp) | Every transfer of every operation failing in turn is `Error::Bus` |
| [`tests/test_types.cpp`](../tests/test_types.cpp) | `Validate()` against every documented range, the conversions and decoders at run time |
| [`tests/fuzz/`](../tests/fuzz/fuzz_tle92466ed.cpp) | The fuzz target; CTest replays its corpus (`tle92466ed_fuzz_tle92466ed_corpus`), see below |
| [`tests/test_errors.cpp`](../tests/test_errors.cpp) | The base `Error` values are 0 to 9; every chip-specific value (16 and up) is returned by some call |
| [`tests/test_version.cpp`](../tests/test_version.cpp) | The version getters follow the generated header (also against a 9.8.7 probe) |
| [`tests/test_kconfig.cpp`](../tests/test_kconfig.cpp) | `KconfigDefaults()` with and without `CONFIG_` values |
| [`tests/compile_fail/`](../tests/compile_fail/port_without_delay.cpp) | Code that must not compile, each for its `EXPECT:` diagnostic: a port without `DelayUs()`, a `void GpioSet()`, a `GpioRead()` by value, a `DelayUs(uint16_t)`, a hook with a narrowed parameter or without `noexcept`, an invalid `CheckedConfig`, a class that is not a port |
| [`tests/mock_tle92466ed.hpp`](../tests/mock_tle92466ed.hpp) | The chip model, with its own register map; the line faults are in `mock_tle92466ed_bus.hpp` |
| [`tests/test_model.cpp`](../tests/test_model.cpp) | The model's register map, reset values, fields, timing and CRC against the driver's, with `static_assert`; the reply widths of every address |
| [`tests/check_version_copy.cmake`](../tests/check_version_copy.cmake) | `_config/Doxyfile` carries the CMake version |
| [`examples/host/`](../examples/host/README.md) | The host example, built with the test flags and run (`tle92466ed_host_example`) |
| [`tests/check_install.cmake`](../tests/check_install.cmake) | Installs into a scratch prefix and builds [`tests/install_consumer/`](../tests/install_consumer/CMakeLists.txt) with `find_package(tle92466ed)` |
| [`tests/check_doxygen.cmake`](../tests/check_doxygen.cmake) | Doxygen with `_config/Doxyfile` and warnings as errors, when `doxygen` is installed |

### The model stands alone

[`tests/mock_tle92466ed.hpp`](../tests/mock_tle92466ed.hpp) is written from the datasheet, not from the driver: it does
not include `tle92466ed_registers.hpp` or `tle92466ed_types.hpp`. Its register map, field masks, reset values, timing and
CRC-8 are its own (namespace `tle92466ed_test::chip`, each with the datasheet section), so a wrong constant in the
driver cannot hide behind the same wrong constant in the model (handbook KB-TST-030).
[`tests/test_model.cpp`](../tests/test_model.cpp) compares the two at compile time:

```cpp
--8<-- "tests/test_model.cpp:cross-check"
```

When the build stops on one of these, one of the two maps is wrong: check it against the section the model names,
and fix that one. A new register goes into both, with its check. Every behaviour found on hardware becomes a fault
mode of the model and a test that fails on the code before the fix; a chip with timing gets a virtual clock in the
model (here `Elapse()`, driven by the mock port's `DelayUs()`).

### The model's limits

The model is written from the datasheet, so it shares the driver's reading of the datasheet wherever the datasheet
leaves room. These shared assumptions are not proven by the tests; each has a bench step in
[Status and limitations](limitations.md#bench-steps):

- A 1 clears an rwh flag and a 0 leaves it; an rw flag takes the value written; OC and OTP_ECC_ERR need two clears.
- CH_CTRL, MODE, CH_CONFIG and SETPOINT read back what was written; a slave reads MODE and SETPOINT as 0.
- The reply to a write carries the register after the write. Section 5.2.1 allows the contents before it too, so
  the knob `write_reply_before_write` gives that reading and `tle92466ed_host_tests_reply_before_write` runs every
  host test under it (the build defines `TLE92466ED_MODEL_REPLY_BEFORE_WRITE`); the driver must pass both.
- CHIPID0..2 and every register from FB_STAT on answer with 22-bit replies.
- A frame the chip ignores leaves the previous reply to be sent again; the first reply after a reset is all zeros.
- The don't-care bits of a read frame are ignored; replies always carry a CRC.
- INIT_DONE after a reset or a clock change takes `init_delay_us` (50 µs), the self-test 500 µs: the datasheet gives
  neither.
- The feedback is synthetic (the setpoint, or `load_ua`, over one dither period); the frame time is 32 SCK periods
  plus 1 µs at 4 MHz.
- An OC in a pair disables both channels; COTERR switches every stage off and keeps Mission Mode; VPRE_OV switches
  them off; COTWARN and VBAT faults are reported only; a channel OTW or OTE blocks Mission Mode entry.

## The hardware lessons, as tests

Each rule the handbook took from testing drivers on real hardware has a code path and a model test:

| Rule | Code path | Test |
| --- | --- | --- |
| A reply is used only when it answers the request (KB-ARCH-042) | `detail::DecodeReply()`, the anchor check | `test_protocol.cpp`: corrupted, reserved-mode, wrong-echo, status and anchor replies |
| A pipelined exchange completes in one call, matched by content (KB-ARCH-043) | `detail::Transport::Access()`: request and anchor of the other width | "A request the chip ignored repeats the previous reply and is refused" |
| Retries only for idempotent frames, bounded and counted (KB-ARCH-044) | `Transport::Read()`; writes have no retry | "Retries stop at Config::max_retries", "A write is never retried" |
| A stuck or floating line is never data (KB-CPP-116) | All zeros, all ones, the echo, the repeated word | "A stuck or looped-back MISO is NoDevice" |
| Resynchronise after a failure (KB-CPP-118) | `Transport::resync()` | "After any failure the next access starts with one anchor frame alone" |
| An identity a dead bus cannot pass (KB-CPP-117) | ICVID with a valid CRC, GLOBAL_CONFIG read back | "another manufacturer or an unknown version", "a GLOBAL_CONFIG that does not read back" |
| Wait for the chip's own ready flag (KB-CPP-124) | INIT_DONE polled after t_RESN or t_POR | "INIT_DONE late is waited for", "INIT_DONE that never comes" |
| Start-up reports and clears old faults (KB-CPP-058) | `takeStartupFaults()` | "faults latched before the start are reported and cleared" |
| Load-dependent diagnosis only on loaded channels (KB-CPP-131) | `ChConfigValue()`, `FaultMask0Value()` | "Start-up: ...", "A fault on a channel not declared loaded never reaches FAULTN" |
| One latch for clear-on-read and clear-on-write flags (KB-CPP-119) | `latch()` | "OLSG_WARN_CHK_NOK is cleared by a read; the latch keeps it" |
| A setter updates its copy after the chip took it, changes only what it names (KB-CPP-120, -121, -122) | `writeChCtrl()`, CH_CTRL from the copy | "Enables: one channel at a time, the others kept", "A channel kept off by a latched fault" |
| Values checked, never clamped (KB-CPP-126) | `checkCurrent()` | "SetCurrent: outside the operating range is InvalidArgument" |
| Not a measurement is an error, never a number (KB-CPP-127) | `DecodeFeedback()` | "Feedback with no measurement period (TP_MANT = 0) is NotReady" |
| Conversions use the chip's actual setting (KB-CPP-128) | `SystemClockHz(config_)` in every time conversion | "an external clock programs CLK_DIV from Table 10" |
| Raw access and the last raw reply (KB-CPP-129) | `ReadRegister()`, `GetLastReply()` | "ReadRegister: one access, no retry" |
| Bus requirements as `constexpr` (KB-CPP-123) | `kSpiMode` to `kFrameBytes` | "Bus requirements" and the `static_assert`s of every port |
| Control outputs at their idle level first (KB-HW-018) | The ports; EN low before RESN in `resetChip()` | "Start-up: EN low, RESN pulsed" |
| Buffers in the driver object (KB-HW-019) | `Transport::tx_`, `rx_` | (structure) |

## Fuzz tests

Everything the driver reads comes from the chip over the bus, so the data read is what a noisy cable, a wrong part
or a broken chip controls. [`tests/fuzz/fuzz_tle92466ed.cpp`](../tests/fuzz/fuzz_tle92466ed.cpp) lets libFuzzer choose a
configuration, then up to 1024 driver calls, and on every bus transfer what the bus does: pass it to the model,
fail it, flip bits of the reply or replace it. Between calls it can change the chip: another version, an
unprogrammed OTP, INIT_DONE that never comes, events that cannot be cleared, supply, clock, channel and data faults,
the Critical Fault state, a refused Mission Mode, time passing, a brown-out. After every call it checks the driver's
promises and aborts, naming the line, when one breaks:

- a failed call returns an `Error` the enum defines and `T{}`;
- before `EnsureInitialized()` no call makes a transfer; `InvalidArgument` and `WrongState` mean nothing was sent;
- `UnexpectedReset` leaves the driver uninitialised;
- a programmed setpoint is inside the operating range and is what `GetCurrentSetpoint()` reports;
- feedback is a duty of at most 100 % over a measurement period;
- every transfer is one 32-bit frame with both buffers, and every log line is a non-empty plain-text message.

```bash
# libFuzzer needs Clang (on macOS, Homebrew's LLVM)
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DTLE92466ED_BUILD_TESTS=ON -DTLE92466ED_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_tle92466ed
./build-fuzz/tests/fuzz/fuzz_tle92466ed -max_total_time=120 tests/fuzz/corpus/fuzz_tle92466ed
```

The same file also builds without libFuzzer as `fuzz_replay_fuzz_tle92466ed`, and CTest replays every input in
[`tests/fuzz/corpus/fuzz_tle92466ed/`](../tests/fuzz/corpus/fuzz_tle92466ed) on GCC and Clang
(`tle92466ed_fuzz_tle92466ed_corpus`): the seeds, and every input that ever broke a promise (name it `regress-*`), so a
fixed bug stays fixed. CI fuzzes for 120 s on every pull request, from the seeds plus a corpus it keeps between runs
(job `Host quality / fuzz`).

## Quality gates

The `Host quality` job measures the library code, `inc/` and `src/` only (tests and examples are left out), and
fails when a value is over the limits of this repository's assurance level, `high` (set in
[`.kokeb/handbook.yml`](../.kokeb/handbook.yml), defined by the handbook's ADR-0020):

| Gate | Tool and how it is measured | Limit at `high` |
| --- | --- | --- |
| Complexity | lizard 1.24.1, modified cyclomatic complexity (`-m`: a `switch` counts once) per function | CCN at most 15 |
| Function length | lizard, lines of code per function | at most 60 |
| Parameters | lizard, per function | at most 6 |
| Line coverage | GCC 14 `--coverage -O0`, every CTest, gcovr 8.7 `--merge-lines` (a template's lines count once across its instances), `--exclude-unreachable-branches --exclude-throw-branches` | at least 85 % |
| Branch coverage | same run | at least 75 % |
| MC/DC | GCC `-fcondition-coverage`, same run | measured, not gated at `high` (100 % at `safety`) |
| Sanitizers | Clang, `-fsanitize=address,undefined -fno-sanitize-recover=all`, every CTest | no report |
| Fuzzing | libFuzzer with ASan and UBSan, 120 s per target | no broken promise |

The reports are in the run's `host-quality-reports` artifact and its step summary. To run the gates locally:

```bash
# Coverage (GCC 14 or later), then the complexity limits
cmake -S . -B build-cov -DCMAKE_CXX_COMPILER=g++-14 -DTLE92466ED_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="--coverage -O0 -fcondition-coverage"
cmake --build build-cov && ctest --test-dir build-cov
gcovr --root . --filter inc/ --filter src/ --merge-lines --exclude-unreachable-branches --txt build-cov
lizard -m -C 15 -L 60 -a 6 inc src

# Sanitizers (Clang)
cmake -S . -B build-san -DCMAKE_CXX_COMPILER=clang++ -DTLE92466ED_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g"
cmake --build build-san && ctest --test-dir build-san --output-on-failure
```

The fuzz gate runs locally as shown in [Fuzz tests](#fuzz-tests).

clang-tidy 20 ([`_config/.clang-tidy`](../_config/.clang-tidy): bugprone, CERT, clang-analyzer,
cppcoreguidelines, modernize, performance, portability and readability, each disabled check with its reason) and
cppcheck (strict) run in the `C++ lint` and `Repository checks` jobs and fail on any finding.

The `MISRA` job ([`ci-misra.yml`](../.github/workflows/ci-misra.yml)) runs the shared `ru-misra` workflow of
ci-tools at level `high`: it traces a host build and the Cortex-M build (`tle92466ed_optimised_build`) of every
translation unit, checks them against MISRA C++:2023 and the handbook's C++20 profile (the CodeQL coding-standards
and profile packs, plus the validated clang-tidy and cppcheck checks), and reports each finding against the
handbook's compliance matrix and deviation records. A finding is fixed in the code; a deliberate exception names
its deviation on the line before it (`// codeql::misra_deviation_next_line(MD-CPP-009)` above the `.ipp` include
of `inc/tle92466ed.hpp`). Until the repository is public the job runs with `open_source_prerelease: true`, the
declaration that the code is released under the licence in `LICENSE` and is to be published.

## Documentation snippets

Every C++ block on this site is included from [`tests/docs/`](../tests/docs/my_port.hpp), which the host build
compiles (target `tle92466ed_doc_snippets`) with the driver instantiated over the documented port. A change to
the API that breaks a page breaks the build first. The README's example is a generated copy of the same file;
`kokeb-docs check` fails when it drifts.

## CI workflows

Everything runs from [`ci.yml`](../.github/workflows/ci.yml), which calls one workflow file per check and ends in
the `CI gate` job, the single required check (`CI gate / gate`).

| Job | What it checks |
| --- | --- |
| Host tests | GCC, then Clang (one job), build and run every test above, then build and run `examples/host` on its own |
| Host quality | The gates above: complexity, coverage, sanitizers, fuzzing |
| Cross-compile | `arm-none-eabi-g++` for Cortex-M0+, M4F and M7F with the strict flags, warnings as errors (nothing linked) |
| C++ lint | clang-format and clang-tidy over the sources |
| MISRA | MISRA C++:2023 and the C++20 profile on traced host and Cortex-M builds, at level `high` (see [Quality gates](#quality-gates)) |
| Docs | Strict site build (`_config/mkdocs.yml`), figure and generated-region drift, Doxygen (`_config/Doxyfile`), offline link check, Lighthouse |
| Repository checks | One job: DCO on pull requests, the org standard, Markdown and YAML lint, the [Kokeb Labs handbook](https://github.com/kokeb-labs/handbook) rules for `.kokeb/handbook.yml` (by rule ID), template drift (warns only) and cppcheck over the driver sources; a second job runs the handbook's source checks (include guards, bus requirements, raw access) |
| CI gate | Passes only if every job above passed: the one required check |

The **ESP32 examples** job builds every app in `examples/esp32/app_config.yml`, for each listed ESP-IDF version and
build type, with the repository root as the driver's component, and lints the example sources. The gate needs it
too.

The workflows call the shared, versioned workflows of
[kokeb-labs/ci-tools](https://github.com/kokeb-labs/ci-tools) at `@v1`.

## On hardware

The ESP32 example ([Examples](examples.md)) is the bench test: it drives LOAD0 at 300 mA and logs its feedback and
any fault twice a second. The bench steps, and what each confirms, are in
[Status and limitations](limitations.md#bench-steps). Record the board and the result in the pull request's
bench-status section.
