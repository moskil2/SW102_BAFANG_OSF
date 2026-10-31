#include <stdint.h>
#include <stdbool.h>
#include "bafang_trip.h"
#include "state.h"

#define MOVING_THRESHOLD_KMH 0.5f
#define TICK_SECONDS 0.1f /* bafang_trip_tick() is called every 100ms */

bafang_trip_extra_t g_bafang_trip_extra;

void bafang_trip_tick(float speed_kmh, float current_amps, float power_w)
{
	rt_vars_t *rtv = get_rt_vars();
	static float odo_remainder_km = 0.0f;
	static float trip_remainder_m = 0.0f;
	static uint8_t one_second_counter = 0;
	static uint8_t three_second_counter = 0;

	bool moving = speed_kmh > MOVING_THRESHOLD_KMH;

	if (moving) {
		float delta_km = speed_kmh * (TICK_SECONDS / 3600.0f);

		odo_remainder_km += delta_km;
		while (odo_remainder_km >= 0.1f) {
			rtv->ui32_odometer_x10 += 1;
			odo_remainder_km -= 0.1f;
		}

		/* ui32_trip_a_distance_x1000 is in meters, despite the name - see
		 * screen_main.c's draw_2nd_field() ModeTripDistance, which shows it
		 * directly as "%d m" below 1000. */
		/* Whole meters are moved out of a running float remainder, exactly
		 * like the odometer above. This used to be
		 * (uint32_t)(delta_km * 1000.0f + 0.5f) - rounding EVERY 100ms tick
		 * to a whole meter with no remainder kept, so anything below 18 km/h
		 * (0.5 m per tick) added nothing at all and 18-54 km/h always added
		 * exactly 1 m (real bug found 2026-09-25: a 7 km ride, mostly
		 * unassisted at low speed, showed as 2 km). */
		trip_remainder_m += delta_km * 1000.0f;
		uint32_t whole_m = (uint32_t) trip_remainder_m;
		rtv->ui32_trip_a_distance_x1000 += whole_m;
		trip_remainder_m -= (float) whole_m;

		uint16_t speed_x10 = (uint16_t) (speed_kmh * 10.0f + 0.5f);
		if (speed_x10 > rtv->ui16_trip_a_max_speed_x10)
			rtv->ui16_trip_a_max_speed_x10 = speed_x10;
	}

	uint16_t current_x10 = (uint16_t) (current_amps * 10.0f + 0.5f);
	if (current_amps > 0.0f && current_x10 > g_bafang_trip_extra.max_current_x10)
		g_bafang_trip_extra.max_current_x10 = current_x10;
	uint16_t power_i = (uint16_t) (power_w + 0.5f);
	if (power_w > 0.0f && power_i > g_bafang_trip_extra.max_power_w)
		g_bafang_trip_extra.max_power_w = power_i;
	if (power_w > 0.0f)
		g_bafang_trip_extra.used_wh += power_w * (TICK_SECONDS / 3600.0f);

	/* Trip time only advances while moving - matches the pre-existing
	 * rt_calc_trips() semantics for ui32_trip_a_time (ModeTripTime shows
	 * moving time, not total elapsed session time). Total time (data
	 * screen) counts every second, moving or not. */
	if (++one_second_counter >= 10) {
		one_second_counter = 0;
		g_bafang_trip_extra.total_time_s += 1;
		if (moving)
			rtv->ui32_trip_a_time += 1;
	}

	/* Avg speed refreshed every 3s, same formula as rt_calc_trips():
	 * km/h*10 = (meters/seconds) * 3.6 * 10 = meters*36/seconds. */
	if (++three_second_counter >= 30) {
		three_second_counter = 0;
		rtv->ui16_trip_a_avg_speed_x10 = rtv->ui32_trip_a_time
				? (rtv->ui32_trip_a_distance_x1000 * 36) / rtv->ui32_trip_a_time
				: 0;
	}
}

void bafang_trip_reset(void)
{
	rt_vars_t *rtv = get_rt_vars();
	rtv->ui32_trip_a_distance_x1000 = 0;
	rtv->ui32_trip_a_time = 0;
	rtv->ui16_trip_a_avg_speed_x10 = 0;
	rtv->ui16_trip_a_max_speed_x10 = 0;
	g_bafang_trip_extra.total_time_s = 0;
	g_bafang_trip_extra.max_current_x10 = 0;
	g_bafang_trip_extra.max_power_w = 0;
	g_bafang_trip_extra.used_wh = 0.0f;
}

void bafang_trip_extra_restore(void)
{
	if (!bafang_trip_extra_load())
		g_bafang_trip_extra.total_time_s = get_rt_vars()->ui32_trip_a_time;
}
