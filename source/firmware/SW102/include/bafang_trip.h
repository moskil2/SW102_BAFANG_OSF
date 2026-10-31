#ifndef _BAFANG_TRIP_H_
#define _BAFANG_TRIP_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * Odometer/trip distance accumulation, ported from EggSPEED's
 * trip integration: every tick, integrate
 * distance = speed * dt, but only while speed is above a small moving
 * threshold (0.5 km/h in the original) so noise at a standstill doesn't
 * slowly accumulate fake distance.
 *
 * Writes directly into the pre-existing rt_vars fields (ui32_odometer_x10,
 * ui32_trip_a_distance_x1000/_time/_avg_speed_x10/_max_speed_x10) - those
 * already have full EEPROM persistence (common/src/eeprom.c) and existing
 * (currently unused from main_idle()) display code in screen_main.c. The
 * only thing missing was a real source of distance: rt_calc_odometer()/
 * rt_calc_trips() in common/src/state.c both key off
 * rt_vars.ui32_wheel_speed_sensor_tick_counter, which only the old TSDZ2
 * UART frame parser ever writes to - dead with the Bafang protocol, so
 * those two functions still run every 100ms but always compute zero
 * distance. They're left in place (harmless) rather than removed.
 */
void bafang_trip_tick(float speed_kmh, float current_amps, float power_w);

/* Extra trip statistics for the third cockpit layout (data screen) - not
 * part of rt_vars, so persisted separately (own FDS record, see
 * bafang_storage.c) and, like every rt_vars trip field, saved at power-off
 * and cleared only by "Trip reset". current_amps and power_w passed to
 * bafang_trip_tick() are the already-calibrated values (same ones the rest
 * of the UI uses). */
typedef struct
{
	uint32_t total_time_s;      /* since last reset, INCLUDING standstill (unlike ui32_trip_a_time, which is moving time only) */
	uint16_t max_current_x10;   /* amps * 10 */
	uint16_t max_power_w;
	float used_wh;              /* energy used since last reset - the data screen's own counter, independent of bafang_energy.c (which also resets itself on a full recharge and feeds the AV/AC rows and RANGE) */
} bafang_trip_extra_t;

extern bafang_trip_extra_t g_bafang_trip_extra;

/* FDS-backed (bafang_storage.c, real hardware only - stubbed in the
 * emulator). load returns false if there is no valid saved record yet. */
bool bafang_trip_extra_load(void);
void bafang_trip_extra_save(void);

/* Call once at boot after eeprom_init() and bafang_energy_load(). Loads the
 * saved extras; if there is none yet (first boot after the update that
 * introduced them), starts total time from the already-persisted moving
 * time so TOTAL is never smaller than MOVE. */
void bafang_trip_extra_restore(void);

/* Zeroes the four trip fields (distance/time/avg speed/max speed) plus
 * g_bafang_trip_extra (all of it, including used_wh) - odometer is untouched (that's a separate, non-trip
 * counter). Reachable from the menu's "Trip reset". */
void bafang_trip_reset(void);

#endif
