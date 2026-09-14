# SW102 Firmware

Custom firmware for the Bafang SW102 display, targeting full OEM feature parity plus adjustable current/power display calibration.

Built against `anszom/SW102_LCD` (branch `sw102-new`). Changes to fork files are tracked as a patch (`patches/sw102_lcd.patch`); modules written from scratch are committed in full under `firmware/`.

## Contents

- **`firmware/`** - modules written from scratch: Bafang UART protocol (`bafang_protocol.*`), telemetry state machine (`bafang_display.*`), current/voltage calibration (`bafang_calibration.*`), settings (`bafang_settings.*`), persistent flash storage (`bafang_storage.*`), trip/odo (`bafang_trip.*`), range/energy-use estimator - a port of EggSPEED's `EnergyAnalyzer.kt` (`bafang_energy.*`), firmware version string (`firmware_version.h`), integration bridges (`*_bridge.h`), plus the full family of hand-drawn cockpit fonts, the menu marker icon, and the boot screen logo (`font_*.xbm`, `icon_*.xbm`, `logo_eggspeed.xbm`)
- **`emu-rs/`** - terminal firmware emulator (Rust/ratatui) - compiles and runs the REAL firmware C code (not an approximation), renders the framebuffer as Braille in the terminal, simulates a fake Bafang controller or bridges to a real serial port (`--serial COM3`). Requires MinGW-w64 GCC on PATH (target `x86_64-pc-windows-gnu` - MSVC doesn't support the GCC syntax used in the firmware) - `cargo build --target x86_64-pc-windows-gnu`
- **`patches/sw102_lcd.patch`** - exact diff against the base fork (`git diff --binary`, covers both modifications to fork files and new files/`emu-rs/`)
- **`font_speed_work/`** - scripts for generating cockpit fonts (pixel-by-pixel extraction from hand-drawn grid templates) and pixel-accurate Python simulations (`simulate_cockpit.py`, `simulate_menu.py`) used to iterate on layout before touching C code
- **`research.md`** - full technical documentation of the project (protocol, hardware, architecture decisions, history)

## Building

1. Clone the base fork: `git clone https://github.com/anszom/SW102_LCD.git` (branch `sw102-new`)
2. Apply the patch: `git apply /path/to/patches/sw102_lcd.patch` inside the fork directory
3. Copy `firmware/*` from this repo into the fork's corresponding `firmware/SW102/include/` and `firmware/SW102/src/sw102/` directories (if the patch doesn't already add these as new files in your git version - `git apply` with new-file support should do this automatically if the patch was generated from `git diff` after `git add -A`, which also covers new files)
4. Build per the instructions in `research.md` (arm-none-eabi-gcc, make, OpenOCD)

## Preview (simulation)

<img src="font_speed_work/cockpit_simulation_natural.png" alt="SW102 cockpit simulation - speed, power, assist level, trip/odo/range" width="260">

Pixel-accurate cockpit layout simulation (`font_speed_work/simulate_cockpit.py`) using the
hand-drawn fonts, used to iterate on the UI before touching C code.

## Status

Firmware tested on real hardware (flashed via SWD/OpenOCD and an ST-Link V2, current version `SW102_BAF_0.0.3`). Working on hardware: Bafang UART protocol (telemetry, lights, assist), current and voltage calibration, trip/odo with reset, km/h<->mph unit switch, menu with a marker icon instead of highlight, boot screen with version number. Versioning convention: `SW102_BAF_X.Y.Z`, each version flashed to hardware is saved as its own `.hex` file (never overwritten). Builds cleanly on both toolchains (ARM and emulator). Details and history in `research.md` and `CHANGELOG.md`.

## License and provenance

The base fork (`anszom/SW102_LCD`, itself a fork of `OpenSourceEBike/Color_LCD`) is GPL-3.0 licensed. This project is currently private and undistributed. Before any public distribution, the code in `patches/` will be replaced with a fully independent implementation.
