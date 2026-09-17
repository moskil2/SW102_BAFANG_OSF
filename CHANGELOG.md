# Changelog

Version history for the `SW102_BAF_X.Y.Z` firmware, flashed to real hardware via SWD.

## 0.0.9 (2026-09-17)

- New complete "ODO" font (`font_odo`) replacing `font_label`/`font_il` in the TRIP/ODO/RANGE/AV/AC rows - full digit/punctuation/uppercase-letter set plus a new ">" arrow glyph, used for the new PAS arrow indicator below. Fixed a real unit-label bug found during the swap: the AV/AC row's unit was showing "Wkm"/"Wml" (missing the "h") instead of the correct "Whkm"/"Whml"
- New, larger skull-and-crossbones boot screen icon
- New large digit font (`font_bigspeed`, 30x56px) for the new cockpit layout below
- New "PAS Type" setting (Cockpit menu): **Type A** is the existing PAS row (centered digit, side bar-graph, status icons above); **Type B** moves the PAS digit to the left with status icons beside it, and shows the assist level as a row of ">" arrows instead of the bar-graph. The bar-graph itself also changed: no more segment for PAS level 0, left side now shows 5 segments (levels 1-5) and right side 4 (levels 6-9)
- "PAS bar" setting renamed to "PAS Icons" - now also controls the Type B arrow row (previously only the Type A bar-graph)
- New "PAS Negativ" setting (Cockpit menu): draws the PAS digit as a cut-out inside a filled rectangle instead of normal ink, in both Type A and Type B
- New second cockpit layout, toggled by a short press of the M button: merges the speed and power rows into one, showing only the whole-number speed as two large digits (no fraction, no unit, no power value) - the choice persists across power cycles
- Settings menu visual overhaul: entries are now left-justified (previously centered for context rows), the selection marker moved to the screen edge, and a too-long selected entry now scrolls further left before disappearing
- Safety fix: assist level now always resets to 0 on power-on, instead of restoring the last-used level from flash
- Fixed RANGE showing near-zero while the battery percentage display showed a healthy value: the range estimator was using the controller's own raw (often inaccurate) reported percentage instead of whichever percentage is actually shown on screen (Standard or Precise)

## 0.0.8 (2026-09-16)

- Fixed a DFU (BLE OTA) update failure that could leave the device stuck in the bootloader when updating to a build smaller than the one already installed (a full SWD reflash of the identical image booted fine, isolating the fault to the DFU path specifically). The application image is now padded with 0xFF up to a fixed 80KB footprint before packaging for OTA, so every update occupies the same flash region regardless of its actual size - only the OTA package is affected, the SWD image is unchanged

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
