# Figures

<!-- Written by kokeb-docs figures from docs/diagrams/figures/*.py. Do not edit by hand. -->

Every figure on this site is drawn by a Python module from facts extracted from the code, so it is rebuilt,
not redrawn, when the code changes. CI runs `kokeb-docs figures --check` and fails when a
committed figure no longer matches the code. Each figure below lists the files it is drawn from.

## Inside the TLE92466ED

<!-- figure: chip-blocks -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-blocks.dark.svg">
  <img src="chip-blocks.svg"
    alt="Block diagram of the TLE92466ED: its function blocks, how they connect and which package pins reach
    each block.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tle92466ed_spi_interface.hpp`](../../inc/tle92466ed_spi_interface.hpp)

## What the TLE92466ED does

<!-- figure: chip-capabilities -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-capabilities.dark.svg">
  <img src="chip-capabilities.svg"
    alt="Capability map of the TLE92466ED: Integrating current controller (supported): Holds each coil at a
    current setpoint from 0 to 1.5 A, with the PWM frequency regulated to a target; Dither (supported):
    Adds a triangular or trapezoidal ripple to the setpoint to keep a valve spool moving freely;
    Parallel channels (supported): Joins channels 0/3, 1/2 or 4/5 into one output for a larger load
    current; Direct drive (supported): Switches a channel without current control: a fixed on-time set
    over SPI, or following DRV0 or DRV1; Current feedback (supported): Reports each channel's average
    current, minimum and maximum, duty cycle and VBAT; Load diagnosis (supported): Detects open load,
    short to ground and overcurrent per channel, in the on and the off state; Supply and temperature
    monitoring (supported): Measures VBAT, VDD, VIO and the die temperature and flags under- and
    overvoltage and overtemperature; Operation states and watchdogs (supported): Config and Mission
    Mode, the SPI and clock watchdogs, CRC-checked frames and the built-in self-test; Monitor test modes
    (not covered): Test bits that fake a supply or overtemperature fault; two of them reset the chip, so
    they stay 0.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tle92466ed_spi_interface.hpp`](../../inc/tle92466ed_spi_interface.hpp)

## TLE92466ED: Simplified application circuit

<!-- figure: chip-circuit-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-typical.dark.svg">
  <img src="chip-circuit-typical.svg"
    alt="Schematic of the TLE92466ED simplified application circuit: the chip with U2 MCU, C1 polarized-
    capacitor 100 µF, C2 capacitor 100 nF, C3 capacitor 100 nF, C4 capacitor 100 nF, C5 capacitor 100
    nF, R1 resistor 10 kΩ, L0 solenoid, L1 solenoid, L2 solenoid, L3 solenoid, L4 solenoid, L5 solenoid,
    D0 diode, D1 diode, D2 diode, D3 diode, D4 diode, D5 diode, C10 capacitor 4.7 nF to 10 nF, C11
    capacitor 4.7 nF to 10 nF, C12 capacitor 4.7 nF to 10 nF, C13 capacitor 4.7 nF to 10 nF, C14
    capacitor 4.7 nF to 10 nF, C15 capacitor 4.7 nF to 10 nF, C20 capacitor 4.7 nF to 10 nF, C21
    capacitor 4.7 nF to 10 nF, C22 capacitor 4.7 nF to 10 nF, C23 capacitor 4.7 nF to 10 nF, C24
    capacitor 4.7 nF to 10 nF, C25 capacitor 4.7 nF to 10 nF; the MCU wires carry the port hooks
    GpioRead(CtrlPin::Fault), GpioSet(CtrlPin::Drv0), GpioSet(CtrlPin::Drv1), GpioSet(CtrlPin::Enable),
    GpioSet(CtrlPin::Reset), Transfer().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tle92466ed_spi_interface.hpp`](../../inc/tle92466ed_spi_interface.hpp)

## TLE92466ED: SPI frame

<!-- figure: chip-frame-spi -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-frame-spi.dark.svg">
  <img src="chip-frame-spi.svg"
    alt="SPI frame of the TLE92466ED, 32 bits, most significant bit first. Write: CRC (bits 31 to 24),
    address (bits 23 to 17), R/W (bit 16), data (bits 15 to 0); Read: CRC (bits 31 to 24), don't care
    (bits 23 to 17), R/W (bit 16), address (bits 15 to 0); Reply, 16 bit: CRC (bits 31 to 24), reply
    mode (bits 23 to 22), status (bits 21 to 17), R/W (bit 16), data (bits 15 to 0); Reply, 22 bit: CRC
    (bits 31 to 24), reply mode (bits 23 to 22), feedback data (bits 21 to 0); Critical fault: don't
    care (bits 31 to 24), reply mode (bits 23 to 22), don't care (bits 21 to 8), fault flags (bits 7 to
    0). The fault flags field of the Critical fault row: bit 7 1V5 ok, bit 6 2V5 ok, bit 5 BG ok, bit 4
    CLK_TOO_SLOW, bit 3 CLK_TOO_FAST, bit 2 DIG_CLK_TOO_SLOW, bit 1 DIG_CLK_TOO_FAST, bit 0 WD_REF_CLK.
    Below, the SPI mode 1 waveform of one transfer in the same bit grid.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tle92466ed_spi_interface.hpp`](../../inc/tle92466ed_spi_interface.hpp)

## TLE92466ED

<!-- figure: chip-hero -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-hero.dark.svg">
  <img src="chip-hero.svg"
    alt="Overview of the TLE92466ED, a six-channel low-side driver for solenoids and valves: each channel
    switches its load through an integrated MOSFET and regulates the coil current itself, with an
    optional dither on top. A MCU talks to it over SPI; it drives Solenoids or valves; it reads back
    Coil current, VBAT, VDD, VIO. Headline figures: 6 x 1.5 A regulated current setpoint per channel; 15
    bit current setpoint resolution; < 1 % current control accuracy; 6 to 18 V load supply VBAT,
    functional range; 8 MHz SPI clock, 32-bit frames with CRC-8.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tle92466ed_spi_interface.hpp`](../../inc/tle92466ed_spi_interface.hpp)

## TLE92466ED pinout, PG-DSO-36

<!-- figure: chip-pinout -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-pinout.dark.svg">
  <img src="chip-pinout.svg"
    alt="Pinout of the TLE92466ED in PG-DSO-36, top view: 36 pins and an exposed pad, each with its name, its
    electrical type as a colour and the driver hook that uses it.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tle92466ed_spi_interface.hpp`](../../inc/tle92466ed_spi_interface.hpp)

## Wiring: Simplified application circuit

<!-- figure: chip-wiring-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-typical.dark.svg">
  <img src="chip-wiring-typical.svg"
    alt="Wiring of the TLE92466ED for the Simplified application circuit: MCU SPI SCK to SCK (Transfer());
    MCU SPI MOSI to SI (Transfer()); MCU SPI MISO to SO (Transfer()); MCU SPI CS to CSN (Transfer());
    MCU GPIO to RESN (GpioSet(CtrlPin::Reset)); MCU GPIO to EN (GpioSet(CtrlPin::Enable)); MCU GPIO
    (IRQ) to FAULTN (GpioRead(CtrlPin::Fault)); MCU GPIO to DRV0 (GpioSet(CtrlPin::Drv0)); MCU GPIO to
    DRV1 (GpioSet(CtrlPin::Drv1)); MCU clock out to CLK. Supplies, grounds and the support parts are
    listed under the drawing.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tle92466ed_spi_interface.hpp`](../../inc/tle92466ed_spi_interface.hpp)

## The layers, one of them yours

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="driver-layers.dark.svg">
  <img src="driver-layers.svg"
    alt="Layer diagram: your application calls the TLE92466ED core, which sends register reads and writes
    through the SPI port contract to the port class you write, which drives the chip; the core and the
    contract are the same on every MCU.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/driver-layers.py`](../../docs/diagrams/figures/driver-layers.py)

Drawn from:

- `public calls` in [`inc/tle92466ed.hpp`](../../inc/tle92466ed.hpp)
- `SpiInterface` in [`inc/tle92466ed_spi_interface.hpp`](../../inc/tle92466ed_spi_interface.hpp)

## One call through the layers

<!-- figure: one-call-through-the-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="one-call-through-the-layers.dark.svg">
  <img src="one-call-through-the-layers.svg"
    alt="One SetCurrent call: the core range-checks and converts the target, the transport writes the set-
    point frame and checks the reply with an anchor read of the status register, and the core then
    updates its copy.">
</picture>
<!-- /figure -->

Hardware description: [`docs/model/one-call-through-the-layers.seq.yml`](../model/one-call-through-the-layers.seq.yml)

Drawn from:

- [`docs/model/one-call-through-the-layers.seq.yml`](../model/one-call-through-the-layers.seq.yml)

## SPI bus timing

<!-- figure: spi-timing -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="spi-timing.dark.svg">
  <img src="spi-timing.svg"
    alt="SPI timing of the TLE92466ED in mode 1: CSN low frames a 32-bit frame, SO changes on each rising SCK
    edge and SI is read on each falling edge, with the lead, lag, setup, hold and output times the
    datasheet gives.">
</picture>
<!-- /figure -->

Hardware description: [`docs/model/spi-timing.timing.yml`](../model/spi-timing.timing.yml)

Drawn from:

- [`docs/model/spi-timing.timing.yml`](../model/spi-timing.timing.yml)

## Two SPI frames per read

<!-- figure: spi-two-frame-read -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="spi-two-frame-read.dark.svg">
  <img src="spi-two-frame-read.svg"
    alt="A register read on the TLE92466ED SPI bus takes two frames: the first sends the read and returns the
    reply to the previous frame, the second is an anchor read that returns the requested value as a
    22-bit reply.">
</picture>
<!-- /figure -->

Hardware description: [`docs/model/spi-two-frame-read.seq.yml`](../model/spi-two-frame-read.seq.yml)

Drawn from:

- [`docs/model/spi-two-frame-read.seq.yml`](../model/spi-two-frame-read.seq.yml)
