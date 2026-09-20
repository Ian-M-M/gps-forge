<!--
SPDX-FileCopyrightText: 2026 GPS Forge contributors
SPDX-License-Identifier: Apache-2.0
-->

# GPS Forge v1

## Purpose

GPS Forge v1 is a hardware emulation and application development platform for the ESP32-S3 CrowPanel target. It lets an application developer build and exercise a display application under QEMU before physical hardware is available.

GPS is the first reference peripheral. The GPS simulator and virtual controls simulator demonstrate how host-side tools connect to the emulated device; they are examples of the platform boundary rather than the platform's long-term scope.

## Included in v1

- ESP32-S3 firmware booting in Espressif QEMU
- 480x480 display abstraction with QEMU and CrowPanel backends
- Shared encoder, button, and touch event API
- GPS service with streaming NMEA parsing and synchronized fix state
- Host GPS simulator with fixed, moving, random-course, and GPX modes
- Host virtual controls simulator
- LVGL GPS diagnostic screen as a replaceable reference application
- Installable `gps-sim` and `controls-sim` host commands
- Host tests for GPS parsing, route motion, and virtual controls
- Documented build, run, and application replacement workflow

## v1 acceptance criteria

1. A developer can build the firmware for ESP32-S3 with the default QEMU profile.
2. QEMU can receive simulated NMEA data and virtual input events.
3. The reference application displays a simulated fix and input activity.
4. A developer can replace the reference UI while keeping the GPS and controls APIs.
5. Host simulators can be installed as console commands from the project root.
6. The physical CrowPanel profile compiles without requiring hardware.
7. Hardware validation is explicitly deferred until the board and GPS receiver are available.

## Deferred after v1

- Additional sensor and peripheral simulators
- Dynamic plugin discovery or simulator loading
- A generalized sensor schema
- Complete electrical emulation of every CrowPanel component
- Production application UI
