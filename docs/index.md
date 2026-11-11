---
hide:
  - navigation
  - toc
---

<div class="kb-hero" markdown>
<span class="kb-kicker">Driver</span>

# TLE92466ED Driver

<p>A C++20 driver for the Infineon TLE92466ED six-channel low-side solenoid driver. It starts the chip, regulates
each coil current with the chip's own controller and dither, reads the current feedback and the diagnosis, and
checks every SPI reply, on any MCU.</p>

[Quickstart](quickstart.md){ .md-button .md-button--primary }
[API guide](api-guide.md){ .md-button }
[Source on GitHub](https://github.com/kokeb-labs-drivers/driver-tle92466ed){ .md-button }
</div>

The driver has no platform code. You give it a small class that brings the bus up and moves 32-bit frames; it does
the rest: the reset and start-up sequence, the operation states, the frame CRC, the one-frame reply pipeline, unit
conversion, fault latching and bounded waits.

<div class="kb-chip" markdown>
<div class="kb-chip__head">
<span class="kb-chip__kicker">Six-channel low-side solenoid driver</span>
<span class="kb-chip__name">TLE92466ED</span>
<span class="kb-chip__vendor">Infineon</span>
</div>

Function
:   Six low-side channels with integrated MOSFETs and shunts; each regulates its average coil current

Current
:   0 to 1.5 A per channel, 15-bit setpoint, 1 % accuracy; 1.8 A with dither; 2.43 A from a parallel pair (2.7 A range)

Control
:   Integrating current controller with a PWM frequency controller (110 Hz to 4 kHz), dither generator,
    direct drive from SPI or the DRV0/DRV1 pins

Diagnosis
:   Open load, short to ground and overcurrent in ON and OFF state, overtemperature, supply and clock monitors

Interface
:   SPI mode 1, up to 8 MHz; 32-bit frames with CRC-8 SAE J1850; SPI watchdog

Supply
:   VDD 4.5 V to 5.5 V, VIO 3.3 V or 5 V, VBAT 6 V to 18 V
</div>


## What you get

- **One object per chip.** `TLE92466ED<YourPort>` takes a reference to your port and does no I/O until
  `EnsureInitialized()`. See [API guide](api-guide.md).
- **Errors you can act on.** Every call returns `Result<T>` with an `Error` such as `NoDevice`, `MissionBlocked` or
  `CriticalFault`. See [API guide](api-guide.md#handle-errors).
- **Replies proven before they are used.** CRC, reply width, R/W echo, status and an anchor read in every access:
  a stuck, looped-back or out-of-step bus is an error, never data. See [Architecture](architecture.md).
- **Datasheet defaults.** `Config{}` holds the chip's reset values, with units in the field names, plus a
  current-control preset. See [Configuration](configuration.md).
- **Zero-overhead portability.** The port is a CRTP class checked by a concept: the bus hooks and a delay, plus
  optional RESN, EN, DRV, FAULTN and logging hooks found at compile time. See [Porting](porting.md).
- **No heap, no exceptions, no RTTI.** The host tests build with `-fno-exceptions -fno-rtti`. See
  [Contract](contract.md).
- **Tested without hardware, honest about it.** Host tests run the driver against a protocol model of the chip;
  what only a bench can confirm is listed in [Status and limitations](limitations.md).

## Datasheet

<!-- generated: datasheet-card -->
| Document | Revision | Get it |
| --- | --- | --- |
| Datasheet: TLE92466ED (Infineon IFX-ntz1491827137291) | 1.2, 2022-02-01 | [PDF](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tle92466ed-datasheet-en.pdf) · [vendor page](https://www.infineon.com/part/TLE92466ED) |

Written against these revisions; the SHA-256 of each file is in [`docs/datasheets.yml`](datasheets.yml). Where each part
of the driver is in the datasheet: [Datasheet map](datasheet.md).
<!-- /generated -->

## Where to go next

<div class="grid cards" markdown>

-   :material-rocket-launch-outline: **[Quickstart](quickstart.md)**

    ---

    Wire the chip, write the port, regulate a coil current in about 20 lines.

-   :material-chip: **[Hardware](hardware.md)**

    ---

    Pins, bus settings, timing and power.

-   :material-file-document-check-outline: **[Contract](contract.md)**

    ---

    What the driver promises, what it needs, threading and errors.

-   :material-code-braces: **[API guide](api-guide.md)**

    ---

    Start, drive channels, read feedback and faults, handle errors, task by task.

-   :material-swap-horizontal: **[Porting](porting.md)**

    ---

    Implement the port for your MCU.

-   :material-book-open-variant: **[API reference](api-reference.md)**

    ---

    Every class and member, generated from the headers.

</div>
