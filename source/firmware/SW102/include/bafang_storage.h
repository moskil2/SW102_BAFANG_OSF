#ifndef _BAFANG_STORAGE_H_
#define _BAFANG_STORAGE_H_

/*
 * Persists bafang_settings_t (calibration, wheel size, speed limit, unit)
 * across power cycles, in its own FDS record - deliberately NOT folded
 * into common/src/eeprom.c's m_eeprom_data (850C/860C-shared struct), for
 * the same isolation reason bafang_settings.h already gives for staying
 * out of ui_vars_t: zero risk of touching that unrelated, already-working
 * persistence path. Uses the same underlying nRF5 FDS flash API as
 * SW102/src/sw102/eeprom_hw.c, but with its own file/record key so the two
 * never collide.
 */

// Call once at boot, after eeprom_init() (which brings up the shared FDS
// subsystem via eeprom_hw_init() - FDS is a single flash-wide system, safe
// to use from here once that's done). Loads g_bafang_settings from flash
// if a valid record exists, otherwise leaves it untouched (caller should
// have already applied bafang_settings_init_defaults()).
void bafang_storage_load(void);

// Call whenever settings should be committed to flash - currently done
// once, at power-off (lcd_power_off()), matching the existing convention
// for every other persisted setting in this firmware (avoids wearing the
// flash on every menu keypress).
void bafang_storage_save(void);

#endif
