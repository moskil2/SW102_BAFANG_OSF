#include "bafang_settings.h"
#include "bafang_calibration.h"

bafang_settings_t g_bafang_settings;

int g_brightness_preview_pct = -1;

void bafang_settings_init_defaults(void)
{
	g_bafang_settings.use_mph = false;
	g_bafang_settings.wheel_diameter_index = 10; /* 26" */
	g_bafang_settings.speed_limit_kph = 25;
	g_bafang_settings.calibration_steps = BAFANG_CALIBRATION_STEPS_DEFAULT;
	g_bafang_settings.battery_capacity_wh = 500;
	g_bafang_settings.brightness_auto = true;
	g_bafang_settings.brightness_manual_pct = 100;
	g_bafang_settings.show_separators = true;
	g_bafang_settings.show_pas_bar = true;
	g_bafang_settings.pas_type_b = false;
	g_bafang_settings.pas_negative = false;
	g_bafang_settings.cockpit_layout = 0;
	g_bafang_settings.screen_rotated = false;
	g_bafang_settings.voltage_calibration_steps = BAFANG_CALIBRATION_STEPS_DEFAULT;
	g_bafang_settings.battery_percent_precise = false;
	g_bafang_settings.battery_max_voltage_dv = 588; /* 58.8V, a common 14S full charge - user must set their own pack's real values before switching to Precise */
	g_bafang_settings.battery_min_voltage_dv = 410; /* 41.0V */
}
