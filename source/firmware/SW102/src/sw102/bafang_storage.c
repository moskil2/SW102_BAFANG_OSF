#include <string.h>
#include "bafang_storage.h"
#include "bafang_settings.h"
#include "bafang_energy.h"
#include "bafang_trip.h"
#include "bafang_street_race.h"
#include "common.h"
#include "fds.h"
#include "nrf_delay.h"
#include "nrf_soc.h"

/* Own FDS file/record, distinct from eeprom_hw.c's FILE_ID=0x1001/
 * REC_KEY=0x2002 (used for the shared 850C/860C-style m_eeprom_data) - FDS
 * is one flash-wide key/value system, multiple independent records living
 * side by side is the normal way to use it (see FDS_MAX_USERS=8 in
 * sdk_config.h, plenty of room for a second registered handler). */
#define FILE_ID  0x1002
#define REC_KEY  0x2003
#define STORAGE_MAGIC 0xBAFA5517u /* bumped: cockpit_layout_big added since 0x...16 - force old records back to defaults instead of loading garbage/padding as a real setting */

typedef struct {
	uint32_t magic;
	uint8_t use_mph;
	uint8_t wheel_diameter_index;
	uint16_t speed_limit_kph;
	uint16_t calibration_steps;
	uint16_t battery_capacity_wh;
	uint8_t brightness_auto;
	uint8_t brightness_manual_pct;
	uint8_t show_separators;
	uint8_t show_pas_bar;
	uint16_t voltage_calibration_steps;
	uint8_t battery_percent_precise;
	uint16_t battery_max_voltage_dv;
	uint16_t battery_min_voltage_dv;
	uint8_t pas_type_b;
	uint8_t pas_negative;
	uint8_t cockpit_layout;
	uint8_t screen_rotated; /* sits in what used to be the struct's tail padding byte: old records have 0 there (data below is a zero-initialised static, fields are assigned one by one), i.e. "not rotated" - no STORAGE_MAGIC bump needed, no settings lost */
} bafang_storage_data_t;
_Static_assert(sizeof(bafang_storage_data_t) == 28, "record size changed - old flash records would no longer load; bump STORAGE_MAGIC and handle migration deliberately");

static volatile bool write_done;

static void fds_evt_handler(fds_evt_t const *const evt)
{
	switch (evt->id) {
	case FDS_EVT_UPDATE:
	case FDS_EVT_WRITE:
		write_done = true;
		break;
	default:
		break;
	}
}

void bafang_storage_load(void)
{
	APP_ERROR_CHECK(fds_register(fds_evt_handler));

	bafang_storage_data_t data;
	fds_flash_record_t flash_record;
	fds_record_desc_t record_desc;
	fds_find_token_t ftok;

	memset(&record_desc, 0x00, sizeof(record_desc));
	memset(&ftok, 0x00, sizeof(ftok));

	if (fds_record_find(FILE_ID, REC_KEY, &record_desc, &ftok) != FDS_SUCCESS)
		return; /* no record yet - keep whatever bafang_settings_init_defaults() set */

	if (fds_record_open(&record_desc, &flash_record) != FDS_SUCCESS)
		return;

	memcpy(&data, flash_record.p_data, sizeof(data));
	fds_record_close(&record_desc);

	if (data.magic != STORAGE_MAGIC)
		return; /* blank/corrupt flash - keep defaults */

	g_bafang_settings.use_mph = data.use_mph;
	g_bafang_settings.wheel_diameter_index = data.wheel_diameter_index;
	g_bafang_settings.speed_limit_kph = data.speed_limit_kph;
	g_bafang_settings.calibration_steps = data.calibration_steps;
	g_bafang_settings.battery_capacity_wh = data.battery_capacity_wh;
	g_bafang_settings.brightness_auto = data.brightness_auto;
	g_bafang_settings.brightness_manual_pct = data.brightness_manual_pct;
	g_bafang_settings.show_separators = data.show_separators;
	g_bafang_settings.show_pas_bar = data.show_pas_bar;
	g_bafang_settings.voltage_calibration_steps = data.voltage_calibration_steps;
	g_bafang_settings.battery_percent_precise = data.battery_percent_precise;
	g_bafang_settings.battery_max_voltage_dv = data.battery_max_voltage_dv;
	g_bafang_settings.battery_min_voltage_dv = data.battery_min_voltage_dv;
	g_bafang_settings.pas_type_b = data.pas_type_b;
	g_bafang_settings.pas_negative = data.pas_negative;
	g_bafang_settings.cockpit_layout = data.cockpit_layout < BAFANG_COCKPIT_LAYOUT_COUNT ? data.cockpit_layout : 0;
	g_bafang_settings.screen_rotated = data.screen_rotated == 1;
}

void bafang_storage_save(void)
{
	if (!useSoftDevice)
		return; /* matches eeprom_hw.c: silently skip when running without the soft device */

	static bafang_storage_data_t data; /* static: must outlive the async fds write */
	data.magic = STORAGE_MAGIC;
	data.use_mph = g_bafang_settings.use_mph;
	data.wheel_diameter_index = g_bafang_settings.wheel_diameter_index;
	data.speed_limit_kph = g_bafang_settings.speed_limit_kph;
	data.calibration_steps = g_bafang_settings.calibration_steps;
	data.battery_capacity_wh = g_bafang_settings.battery_capacity_wh;
	data.brightness_auto = g_bafang_settings.brightness_auto;
	data.brightness_manual_pct = g_bafang_settings.brightness_manual_pct;
	data.show_separators = g_bafang_settings.show_separators;
	data.show_pas_bar = g_bafang_settings.show_pas_bar;
	data.voltage_calibration_steps = g_bafang_settings.voltage_calibration_steps;
	data.battery_percent_precise = g_bafang_settings.battery_percent_precise;
	data.battery_max_voltage_dv = g_bafang_settings.battery_max_voltage_dv;
	data.battery_min_voltage_dv = g_bafang_settings.battery_min_voltage_dv;
	data.pas_type_b = g_bafang_settings.pas_type_b;
	data.pas_negative = g_bafang_settings.pas_negative;
	data.cockpit_layout = g_bafang_settings.cockpit_layout;
	data.screen_rotated = g_bafang_settings.screen_rotated;

	fds_record_desc_t record_desc;
	fds_find_token_t ftok;
	memset(&record_desc, 0x00, sizeof(record_desc));
	memset(&ftok, 0x00, sizeof(ftok));
	bool has_old = fds_record_find(FILE_ID, REC_KEY, &record_desc, &ftok) == FDS_SUCCESS;

	fds_record_chunk_t record_chunk;
	record_chunk.p_data = &data;
	record_chunk.length_words = sizeof(data) / sizeof(uint32_t);

	fds_record_t record;
	record.file_id = FILE_ID;
	record.key = REC_KEY;
	record.data.p_chunks = &record_chunk;
	record.data.num_chunks = 1;

	write_done = false;
	if (has_old)
		fds_record_update(&record_desc, &record);
	else
		fds_record_write(&record_desc, &record);

	for (volatile int count = 0; count < 1000 && !write_done; count++) {
		sd_app_evt_wait();
		nrf_delay_ms(1);
	}
}

/* Same FILE_ID as above (own file, distinct REC_KEY) - the learned
 * consumption averages for bafang_energy.c's range estimator. Separate
 * record from bafang_storage_data_t above because this is accumulated
 * ride data (like trip/odo), not a user-chosen setting, even though both
 * happen to live in this file for now. */
#define ENERGY_REC_KEY 0x2004
#define ENERGY_STORAGE_MAGIC 0xBAFA5522u

typedef struct {
	uint32_t magic;
	float historical_avg_wh_per_km;
	float historical_km;
	float trip_distance_km;
	float trip_energy_wh;
	float short_term_avg_wh_per_km;
	float medium_term_avg_wh_per_km;
} bafang_energy_storage_data_t; /* 4 + 6*4 = 28 bytes = 7 words */

void bafang_energy_load(void)
{
	bafang_energy_storage_data_t data;
	fds_flash_record_t flash_record;
	fds_record_desc_t record_desc;
	fds_find_token_t ftok;

	memset(&record_desc, 0x00, sizeof(record_desc));
	memset(&ftok, 0x00, sizeof(ftok));

	if (fds_record_find(FILE_ID, ENERGY_REC_KEY, &record_desc, &ftok) != FDS_SUCCESS)
		return; /* no record yet - keep whatever bafang_energy_init_defaults() set */

	if (fds_record_open(&record_desc, &flash_record) != FDS_SUCCESS)
		return;

	memcpy(&data, flash_record.p_data, sizeof(data));
	fds_record_close(&record_desc);

	if (data.magic != ENERGY_STORAGE_MAGIC)
		return; /* blank/corrupt flash - keep defaults */

	g_bafang_energy.historical_avg_wh_per_km = data.historical_avg_wh_per_km;
	g_bafang_energy.historical_km = data.historical_km;
	g_bafang_energy.trip_distance_km = data.trip_distance_km;
	g_bafang_energy.trip_energy_wh = data.trip_energy_wh;
	g_bafang_energy.short_term_avg_wh_per_km = data.short_term_avg_wh_per_km;
	g_bafang_energy.medium_term_avg_wh_per_km = data.medium_term_avg_wh_per_km;
}

void bafang_energy_save(void)
{
	if (!useSoftDevice)
		return;

	static bafang_energy_storage_data_t data;
	data.magic = ENERGY_STORAGE_MAGIC;
	data.historical_avg_wh_per_km = g_bafang_energy.historical_avg_wh_per_km;
	data.historical_km = g_bafang_energy.historical_km;
	data.trip_distance_km = g_bafang_energy.trip_distance_km;
	data.trip_energy_wh = g_bafang_energy.trip_energy_wh;
	data.short_term_avg_wh_per_km = g_bafang_energy.short_term_avg_wh_per_km;
	data.medium_term_avg_wh_per_km = g_bafang_energy.medium_term_avg_wh_per_km;

	fds_record_desc_t record_desc;
	fds_find_token_t ftok;
	memset(&record_desc, 0x00, sizeof(record_desc));
	memset(&ftok, 0x00, sizeof(ftok));
	bool has_old = fds_record_find(FILE_ID, ENERGY_REC_KEY, &record_desc, &ftok) == FDS_SUCCESS;

	fds_record_chunk_t record_chunk;
	record_chunk.p_data = &data;
	record_chunk.length_words = sizeof(data) / sizeof(uint32_t);

	fds_record_t record;
	record.file_id = FILE_ID;
	record.key = ENERGY_REC_KEY;
	record.data.p_chunks = &record_chunk;
	record.data.num_chunks = 1;

	write_done = false;
	if (has_old)
		fds_record_update(&record_desc, &record);
	else
		fds_record_write(&record_desc, &record);

	for (volatile int count = 0; count < 1000 && !write_done; count++) {
		sd_app_evt_wait();
		nrf_delay_ms(1);
	}
}


/* Same FILE_ID again (own REC_KEY): the data screen's extra trip statistics
 * (bafang_trip.c's g_bafang_trip_extra). Own record, own magic - so neither
 * the settings record above nor the energy record is touched (no lost
 * settings, no lost learned consumption). Saved at power-off, like TRIP. */
#define TRIP_EXTRA_REC_KEY 0x2005
#define TRIP_EXTRA_STORAGE_MAGIC 0xBAFA5523u

typedef struct {
	uint32_t magic;
	uint32_t total_time_s;
	uint16_t max_current_x10;
	uint16_t max_power_w;
	float used_wh;
} bafang_trip_extra_storage_data_t; /* 4 + 4 + 2 + 2 + 4 = 16 bytes = 4 words */

bool bafang_trip_extra_load(void)
{
	bafang_trip_extra_storage_data_t data;
	fds_flash_record_t flash_record;
	fds_record_desc_t record_desc;
	fds_find_token_t ftok;

	memset(&record_desc, 0x00, sizeof(record_desc));
	memset(&ftok, 0x00, sizeof(ftok));

	if (fds_record_find(FILE_ID, TRIP_EXTRA_REC_KEY, &record_desc, &ftok) != FDS_SUCCESS)
		return false;

	if (fds_record_open(&record_desc, &flash_record) != FDS_SUCCESS)
		return false;

	memcpy(&data, flash_record.p_data, sizeof(data));
	fds_record_close(&record_desc);

	if (data.magic != TRIP_EXTRA_STORAGE_MAGIC)
		return false;

	g_bafang_trip_extra.total_time_s = data.total_time_s;
	g_bafang_trip_extra.max_current_x10 = data.max_current_x10;
	g_bafang_trip_extra.max_power_w = data.max_power_w;
	g_bafang_trip_extra.used_wh = data.used_wh;
	return true;
}

void bafang_trip_extra_save(void)
{
	if (!useSoftDevice)
		return;

	static bafang_trip_extra_storage_data_t data; /* static: must outlive the async fds write */
	data.magic = TRIP_EXTRA_STORAGE_MAGIC;
	data.total_time_s = g_bafang_trip_extra.total_time_s;
	data.max_current_x10 = g_bafang_trip_extra.max_current_x10;
	data.max_power_w = g_bafang_trip_extra.max_power_w;
	data.used_wh = g_bafang_trip_extra.used_wh;

	fds_record_desc_t record_desc;
	fds_find_token_t ftok;
	memset(&record_desc, 0x00, sizeof(record_desc));
	memset(&ftok, 0x00, sizeof(ftok));
	bool has_old = fds_record_find(FILE_ID, TRIP_EXTRA_REC_KEY, &record_desc, &ftok) == FDS_SUCCESS;

	fds_record_chunk_t record_chunk;
	record_chunk.p_data = &data;
	record_chunk.length_words = sizeof(data) / sizeof(uint32_t);

	fds_record_t record;
	record.file_id = FILE_ID;
	record.key = TRIP_EXTRA_REC_KEY;
	record.data.p_chunks = &record_chunk;
	record.data.num_chunks = 1;

	write_done = false;
	if (has_old)
		fds_record_update(&record_desc, &record);
	else
		fds_record_write(&record_desc, &record);

	for (volatile int count = 0; count < 1000 && !write_done; count++) {
		sd_app_evt_wait();
		nrf_delay_ms(1);
	}
}


/* Same FILE_ID again (own REC_KEY): STREET/RACE (bafang_street_race.c's
 * g_street_race). Own record, own magic - none of the records above is
 * touched, so updating to a firmware with this feature loses nothing.
 * Unlike the others it is saved at once on every change (see
 * bafang_street_race.h), not only at power-off. */
#define STREET_RACE_REC_KEY 0x2006
#define STREET_RACE_STORAGE_MAGIC 0xBAFA5524u

typedef struct {
	uint32_t magic;
	uint8_t enabled;
	uint8_t active;
	uint8_t race_valid;
	uint8_t street_valid;
	uint8_t race_current_pct[BAFANG_PAS_LEVELS];
	uint8_t race_speed_pct[BAFANG_PAS_LEVELS];
	uint8_t street_current_pct[BAFANG_PAS_LEVELS];
	uint8_t street_speed_pct[BAFANG_PAS_LEVELS];
} bafang_street_race_storage_data_t; /* 4 + 4 + 4*10 = 48 bytes = 12 words */
_Static_assert(sizeof(bafang_street_race_storage_data_t) == 48, "record size changed - bump STREET_RACE_STORAGE_MAGIC");

void bafang_street_race_load(void)
{
	bafang_street_race_storage_data_t data;
	fds_flash_record_t flash_record;
	fds_record_desc_t record_desc;
	fds_find_token_t ftok;

	memset(&record_desc, 0x00, sizeof(record_desc));
	memset(&ftok, 0x00, sizeof(ftok));

	if (fds_record_find(FILE_ID, STREET_RACE_REC_KEY, &record_desc, &ftok) != FDS_SUCCESS)
		return; /* no record yet - keep bafang_street_race_init_defaults() */

	if (fds_record_open(&record_desc, &flash_record) != FDS_SUCCESS)
		return;

	memcpy(&data, flash_record.p_data, sizeof(data));
	fds_record_close(&record_desc);

	if (data.magic != STREET_RACE_STORAGE_MAGIC)
		return;

	g_street_race.enabled = data.enabled == 1;
	g_street_race.active = data.active == 1 ? BAFANG_PROFILE_RACE : BAFANG_PROFILE_STREET;
	g_street_race.race_valid = data.race_valid == 1;
	g_street_race.street_valid = data.street_valid == 1;
	memcpy(g_street_race.race_current_pct, data.race_current_pct, BAFANG_PAS_LEVELS);
	memcpy(g_street_race.race_speed_pct, data.race_speed_pct, BAFANG_PAS_LEVELS);
	memcpy(g_street_race.street_current_pct, data.street_current_pct, BAFANG_PAS_LEVELS);
	memcpy(g_street_race.street_speed_pct, data.street_speed_pct, BAFANG_PAS_LEVELS);
}

void bafang_street_race_save(void)
{
	if (!useSoftDevice)
		return;

	static bafang_street_race_storage_data_t data; /* static: must outlive the async fds write */
	data.magic = STREET_RACE_STORAGE_MAGIC;
	data.enabled = g_street_race.enabled;
	data.active = g_street_race.active;
	data.race_valid = g_street_race.race_valid;
	data.street_valid = g_street_race.street_valid;
	memcpy(data.race_current_pct, g_street_race.race_current_pct, BAFANG_PAS_LEVELS);
	memcpy(data.race_speed_pct, g_street_race.race_speed_pct, BAFANG_PAS_LEVELS);
	memcpy(data.street_current_pct, g_street_race.street_current_pct, BAFANG_PAS_LEVELS);
	memcpy(data.street_speed_pct, g_street_race.street_speed_pct, BAFANG_PAS_LEVELS);

	fds_record_desc_t record_desc;
	fds_find_token_t ftok;
	memset(&record_desc, 0x00, sizeof(record_desc));
	memset(&ftok, 0x00, sizeof(ftok));
	bool has_old = fds_record_find(FILE_ID, STREET_RACE_REC_KEY, &record_desc, &ftok) == FDS_SUCCESS;

	fds_record_chunk_t record_chunk;
	record_chunk.p_data = &data;
	record_chunk.length_words = sizeof(data) / sizeof(uint32_t);

	fds_record_t record;
	record.file_id = FILE_ID;
	record.key = STREET_RACE_REC_KEY;
	record.data.p_chunks = &record_chunk;
	record.data.num_chunks = 1;

	write_done = false;
	if (has_old)
		fds_record_update(&record_desc, &record);
	else
		fds_record_write(&record_desc, &record);

	for (volatile int count = 0; count < 1000 && !write_done; count++) {
		sd_app_evt_wait();
		nrf_delay_ms(1);
	}
}
