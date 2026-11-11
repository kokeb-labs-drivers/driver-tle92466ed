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
