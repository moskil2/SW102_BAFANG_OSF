#ifndef _BAFANG_SETTINGS_H_
#define _BAFANG_SETTINGS_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * User-adjustable, OEM-menu-equivalent settings. Kept independent of
 * ui_vars_t (shared with the 850C/860C targets, touched by eeprom.c) for
 * the same reason as bafang_ui_bridge.h - avoid any risk to that
 * unrelated code path. "Auto power off" is deliberately NOT duplicated
 * here - ui_vars.ui8_lcd_power_off_time_minutes already exists and is
 * already wired to a working, independent automatic_power_off_management()
 * (called from ui.c, unaffected by MOTOR_INIT_SIMULATING) - the new OEM
 * menu just needs to expose that existing field, not reinvent it.
 */

typedef struct
{
	/* Unit: false=km/h (default, OEM default), true=mph.
	 * NOTE: stored/adjustable here, but not yet wired into the speed/
	 * distance formatting in screen_main.c (draw_main_speed/draw_2nd_field
	 * still always show km/h) - follow-up work, not done in this pass. */
	bool use_mph;

	/* Index into BAFANG_WHEEL_CIRCUMFERENCE_M (bafang_protocol.h), default
	 * 10 = 26" per OEM manual default. */
	uint8_t wheel_diameter_index;

	/* OEM default 25 km/h, 99 = unlimited (per manual convention). Stored
	 * and shown, but NOT currently transmitted to the controller - see
	 * bbs-fw's own reference implementation ignores this
	 * write ("Global speed limit is configured in firmware config tool"),
	 * and whether genuine Bafang firmware honors it from the display is
	 * unconfirmed. Wiring real transmission is follow-up work once that's
	 * verified on hardware. */
	uint16_t speed_limit_kph;

	/* See bafang_calibration.h - steps of 0.01, 100 = 1.00x (no correction). */
	uint16_t calibration_steps;

	/* User-entered nominal pack capacity, Wh - menu range 100-3000. Not yet
	 * consumed anywhere (no range/remaining-Wh estimator exists - see
	 * at the time), just stored for that future use. */
	uint16_t battery_capacity_wh;

	/* Screen brightness mode. true=AUTO: unchanged existing behaviour -
	 * ui_vars.ui8_lcd_backlight_on/off_brightness, switched by the light
	 * toggle (see set_lcd_backlight()/UP_LONG_CLICK). false=MANUAL: always
	 * brightness_manual_pct regardless of light state. */
	bool brightness_auto;
	uint8_t brightness_manual_pct; /* menu steps: 20/40/60/80/100 */

	/* Cockpit menu toggles - purely cosmetic, both default on (unchanged
	 * look). show_separators: the horizontal lines dividing main_idle()'s
	 * rows. show_pas_bar: the 10-segment PAS level bar either side of the
	 * digit (the digit itself always stays). */
	bool show_separators;
	bool show_pas_bar;

	/* PAS row layout (menu "Cockpit" -> "PAS Type"). false=Type A
	 * (default, unchanged): centered PAS digit with side bar-graph and
	 * light/BT/brake icons above it. true=Type B: PAS digit moves to the
	 * left near the top separator, icons to its right on the same line,
	 * and a row of ">" arrow glyphs (font_odo, 2026-09-17) along the
	 * bottom - one per assist level (0 shows none, 9 shows nine). */
	bool pas_type_b;

	/* Menu "Cockpit" -> "PAS Negativ". false=No (default, unchanged): the
	 * PAS digit draws normally in both layouts. true=YES: the digit is
	 * drawn as a cut-out (its own pixels left unlit) inside a filled
	 * rectangle extending 2px past its bounding box on every side - see
	 * draw_pas_digit() in screen_main.c. Applies to both Type A and
	 * Type B. */
	bool pas_negative;

	/* Cockpit layout, cycled by a short M press on the main screen (see
	 * screen_main.c's main_button()/main_idle()). 0=Variant 1 (default,
	 * unchanged): separate speed and power rows. 1=Variant 2: those two
	 * rows merge into one, showing only the whole-number speed as two large
	 * digits (font_bigspeed) - see draw_main_speed_big(). 2=Variant 3: data
	 * screen, a list of trip statistics - see draw_data_screen(). Was a bool
	 * (cockpit_layout_big) before Variant 3 existed - same storage byte, so
	 * old saved 0/1 values load unchanged. Independent of pas_type_b (not
	 * used by Variant 3). */
	uint8_t cockpit_layout;
#define BAFANG_COCKPIT_LAYOUT_COUNT 3

	/* Screen rotated 180 degrees (menu: Cockpit -> "Rotate Screen 180deg").
	 * Saved to flash like the other settings (it started out as a
	 * deliberately unsaved test feature - made permanent once the rotation
	 * was verified on real hardware, 2026-09-19). While set, UP/DOWN button
	 * events are swapped (ui.c), since the physical buttons are upside down
	 * too. */
	bool screen_rotated;

	/* Field calibration for the locally-measured battery voltage (adc.c's
	 * battery_voltage_10x_get()) - same steps-of-0.01 multiplier as
	 * calibration_steps above, applied via bafang_calibration_factor().
	 * Needed because the P+ voltage divider's real resistor values vary
	 * across SW102 board revisions (first real-hardware test measured
	 * 44.4V displayed vs 58.8V actual on a fully charged 52V/14S pack -
	 * adc.c's assumed 300k divider resistor was corrected to 402k as a
	 * better default, this factor absorbs whatever gap remains on a given
	 * unit). Default 100 = 1.00x (no correction). */
	uint16_t voltage_calibration_steps;

	/* Battery percentage source - see menu "Battery" -> "Battery
	 * percentage". false=Standard (default, unchanged behaviour): the
	 * raw percent byte the controller itself sends over UART, shown
	 * as-is - see bafang_display.c/main.c, this project deliberately
	 * never modifies it. true=Precise: compute our own percent locally
	 * from the display's own accurately-measured battery voltage
	 * (adc.c, same source as the "V" shown top-left) via linear
	 * interpolation between battery_min_voltage_dv and
	 * battery_max_voltage_dv, ignoring the controller's own byte
	 * entirely - see the 2026-09-16 battery-% investigation
	 * for why the controller's own value can be way off (confirmed on
	 * real hardware: 35% from the controller vs ~50% on a genuine
	 * Bafang DP C18 display at the same real voltage) with no
	 * identified fix on our side, only this workaround. */
	bool battery_percent_precise;

	/* Deci-volts (588 = 58.8V), menu range 10.0-99.0V. Max = pack fully
	 * charged, Min = the pack's configured low-voltage cutoff (LVC) -
	 * both user-entered, since neither is available to us over the
	 * Bafang UART protocol. Only consumed when battery_percent_precise
	 * is true; guarded against battery_max_voltage_dv <=
	 * battery_min_voltage_dv at the point of use (falls back to
	 * Standard behaviour rather than dividing by zero or producing a
	 * nonsensical percent). */
	uint16_t battery_max_voltage_dv;
	uint16_t battery_min_voltage_dv;
} bafang_settings_t;

extern bafang_settings_t g_bafang_settings;

/* Live brightness preview while editing "Brightness -> Level" in the menu
 * (-1 = none): the value being scrolled through is applied at once instead
 * of only after confirming. main_idle() clears it, so it can't outlive the
 * menu. Not a setting - never saved. */
extern int g_brightness_preview_pct;

void bafang_settings_init_defaults(void);

#endif
