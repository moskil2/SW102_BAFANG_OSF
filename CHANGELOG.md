# Changelog

Version history for the `SW102_BAF_X.Y.Z` firmware, flashed to real hardware via SWD.

## 0.1.4 (2026-09-25)

- Fixed the TRIP distance: it was rounded to a whole meter on every 100 ms step, so anything below 18 km/h added nothing at all and 18-54 km/h always added exactly 1 m per step (a 7 km ride, mostly unassisted at low speed, showed as 2 km). It is now exact at any speed. The data screen's AVG and CONS, which are derived from it, are corrected too. ODO, the AV/AC rows and range were not affected. A TRIP counted with the old firmware stays wrong until the next "Trip reset"
- TRIP on cockpit layouts 1 and 2 now shows one decimal digit (e.g. 12,3 km)
- On the data screen (layout 3), changing the assist level now blanks the screen and shows the new level big in the middle with "PAS" underneath for 3 seconds (a further change swaps the digit at once and restarts the 3 seconds) - the data screen shows no assist level itself, so a change was invisible
- Brightness menu: "Level" is no longer listed in AUTO mode (it only applies in MANUAL); in MANUAL the level now previews live while you scroll through it (cancelling restores the previous one); switching back to AUTO applies the light-dependent brightness at once
- More accurate battery voltage measurement (0.1 V steps instead of about 0.26 V, and no more systematic under-reading of up to 0.26 V) - this also makes the Precise battery percentage follow the real voltage more closely. After updating, compare the shown voltage with a multimeter and adjust "Voltage cal." if needed

## 0.1.3 (2026-09-19)

- Data screen values are now remembered across power-off, like TRIP, and cleared only by "Trip reset": total time, maximum current, maximum power and energy used. Total time can no longer be smaller than moving time (on the first start after the update it begins from the already-saved moving time)
- The data screen's CONS is now the average consumption over the TRIP shown on the same screen (energy used divided by trip distance), so the two always agree. The AV/AC row on the other two cockpit layouts is unchanged
- "Rotate Screen 180deg" is now remembered after power-off
- PAS Type A: the negative frame is 1px wider on the right and the four bars right of the number moved 1px right; the walk-assist figure moved 1px right in both types; "AC" and its value in the AV/AC row moved 1px right

## 0.1.2 (2026-09-19)

- Third cockpit layout: a plain data list (label on the left, value and unit on the right) with trip distance, average speed, top speed, total time, moving time, maximum current, maximum power, average consumption (Wh/km) and energy used (Wh). A short press of M now cycles through all three layouts
- New "Rotate Screen 180deg" option (Cockpit menu): turns the whole display upside down, with UP and DOWN swapped to match. Deliberately not remembered - after a power cycle the display always starts in the normal orientation
- The walking-person animation now also shows in PAS Type B while walk assist is held (previously only Type A did)


## 0.1.1 (2026-09-18)

- Fixed "Save failed" being shown for every save in the Assist Level Programming screen even though the data had been written: the controller confirms a write with its own short acknowledgement message, which the firmware did not expect. The save result is now based on that acknowledgement


## 0.1.0 (2026-09-18)

- New "Bafang Assist Level Programming" screen (main menu, between "Screen test" and "Info"): reads the controller's own per-assist-level (PAS 0-9) current and speed limits in percent, lets you edit them and writes them back to the controller with SAVE. It reads automatically on entry; editing and SAVE stay locked until a read has succeeded, so nothing empty is ever sent. A blinking underline shows a read or save in progress, and the result is shown as a notice
- "Trip reset" now asks for confirmation (opens a "Confirm trip reset" step, like "Factory reset") so it cannot be triggered by accident
- Fixed the menu selection marker sitting 1px too low in every menu
- Boot screen: back to the smaller skull icon
- Fixes from real-hardware testing: the PAS negative frame is 1px wider on the left, the PAS digit in Type A moved 1px, and the AV/AC row uses fixed positions so "AC" no longer drifts with the number of digits in AV


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
