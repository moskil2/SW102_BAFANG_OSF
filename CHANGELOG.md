# Changelog

Version history for the `SW102_BAF_X.Y.Z` firmware, flashed to real hardware via SWD.

## 0.0.7 (2026-09-16)

- Added smoothing (exponential moving average, ~8s time constant) to the Precise battery-percentage mode only - throttle-induced voltage sag no longer makes the indicator jump around. Standard mode and the displayed voltage remain fully raw/unfiltered

## 0.0.6 (2026-09-16)

- Fixed ODO (and Trip A) resetting to 0 on every power-off or settings-menu exit: the save path was reading from a legacy field this target never populates, instead of the real value our own code maintains
- New "Battery percentage" menu option (Battery submenu) with two modes: **Standard** shows the controller's own reported percentage exactly as before; **Precise** computes the percentage locally from the display's own accurately-measured voltage and user-entered Max/Min battery voltage - for controllers whose own reported percentage runs inaccurate
- Removed/neutralized inherited leftover code found to actively corrupt live telemetry (a fake motor simulator writing garbage into trip calculations, a legacy UART frame parser with a real buffer-overflow risk) plus further confirmed-dead code from the old screen layout. Code size dropped from 66,656 to 53,244 bytes flash (~13 KB recovered) as a direct result of this cleanup

## 0.0.5 (2026-09-14)

- "Trip reset" moved to the top level of the menu, directly under "Unit"
- "Reset" submenu renamed to "Factory reset" (confirm button renamed to "Confirm factory reset")

## 0.0.4 (2026-09-14)

- Fixed a second light bug: `INIT_DISPLAY` (same bytes as "lights off") was being sent at the start of every poll cycle, not just once at startup - this raced against the real light command sent later in the same cycle and made the light blink continuously. Now sent only once at cold start or after a real communication timeout, matching EggSPEED's `DisplayStateMachine.kt` exactly

## 0.0.3 (2026-09-14)

- Fixed light bug: the light command is now sent unconditionally on every poll cycle (matching the verified behavior in EggSPEED), instead of only on state change - the previous version caused the light to blink once and turn off
- New "Trip reset" menu action (resets trip distance/time/speed and trip average energy use, while preserving long-term learned averages)
- Nudged AV/AC value positions (+1px right) and the km/h label (2px up)

## 0.0.2 (2026-09-14)

- Full km/h<->mph unit conversion: speed, distance (ODO/trip), and energy use (Wh/km<->Wh/mile) are converted at the display-formatting boundary, internal data stays in km/h and Wh/km
- New standalone `font_il` font (only "i"/"l" glyphs) for the "ml"/"Wml" labels
- New menu marker icon (`icon_menu_marker.xbm`) replacing the rectangle-highlight + inverted-text selection indicator - fixes a text-overlap glitch
- Fixed `numeric2string()`: missing leading zero in decimal values (e.g. "1.1" -> "1.01")
- Speed limit and the ODO menu editor remain deliberately km-only (a limitation of the numeric editor framework, no runtime-switchable unit)

## 0.0.1 (2026-09-14)

- First version with a version number and boot screen showing the full firmware name
- Fixed voltage calibration: real P+ voltage divider determined to be ~402 kOhm (vs. the fork's assumed 300 kOhm), verified with a multimeter on hardware (58.8V actual vs. 44.4V displayed before the fix); added a "Voltage cal." menu field
- Fixed a `uint8_t` overflow in the LCD contrast/brightness formula (`lcd_refresh()`) - backlight was rendering as binary instead of a smooth range
- New "Screen test" menu item (fills the screen white)
- Added "Dev Tomasz Pieczara" and "spotrobotics.app" to the Info menu
- Finished the "Cockpit" menu tab (ON/OFF toggles for separators and the PAS bar)
- PAS assist level bar changed to a bar-graph style (fills 0..active level)
