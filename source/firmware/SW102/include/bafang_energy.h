#ifndef _BAFANG_ENERGY_H_
#define _BAFANG_ENERGY_H_

#include <stdint.h>

/*
 * Range/consumption estimator - direct C port of EggSPEED's
 * energy analyzer, which is itself plain
 * Kotlin with no Android dependencies, so the port is close to 1:1.
 *
 *
 * Energy-per-distance model, not a physics/GPS simulation: range =
 * remaining_Wh / recent_Wh_per_km, where the Wh/km rate is three
 * exponentially-decaying-by-distance averages (short/medium/long-term)
 * blended together. All state is a handful of scalars - no history
 * buffers, no arrays - trivial on a 21KB-RAM target.
 */

typedef struct {
	/* Long-term learned average, persists across trips (capped
	 * "confidence" accumulation up to 300km - see bafang_energy.c). */
	float historical_avg_wh_per_km;
	float historical_km;

	/* Current trip since the last full charge. */
	float trip_distance_km;
	float trip_energy_wh;

	/* Fast (0.3km half-life) and medium (3km half-life) exponential
	 * moving averages of Wh/km, by distance not time. */
	float short_term_avg_wh_per_km;
	float medium_term_avg_wh_per_km;

	/* In-flight accumulator, committed into the averages above once it
	 * reaches BAFANG_ENERGY_MIN_CHUNK_KM - batches samples so the decay
	 * math runs a few times a second, not every 100ms tick. */
	float pending_distance_km;
	float pending_energy_wh;

	uint8_t last_battery_pct;
} bafang_energy_state_t;

extern bafang_energy_state_t g_bafang_energy;

typedef struct {
	float predicted_range_km;
	float trip_avg_wh_per_km;    /* whole-trip average - "AV" row */
	float short_term_wh_per_km;  /* fast-decaying "right now" rate - "AC" row */
} bafang_energy_estimate_t;

/* Updated by every bafang_energy_tick() call - screen_main.c reads this
 * directly instead of calling tick() itself. */
extern bafang_energy_estimate_t g_bafang_energy_estimate;

void bafang_energy_init_defaults(void);

/* Resets only the current trip (distance/energy/pending chunk) - the
 * learned short/medium/historical averages survive. Same logic already
 * ran automatically on a detected full recharge; now also reachable from
 * the menu's "Trip reset". */
void bafang_energy_reset_trip(void);

/* Call every 100ms (same cadence/call site as bafang_trip_tick()) with
 * live telemetry and the user's configured pack capacity (see
 * bafang_settings.h's battery_capacity_wh). */
void bafang_energy_tick(float speed_kmh, uint8_t battery_pct,
	float voltage_v, float current_a, float capacity_wh);

/* FDS-backed persistence - implemented in bafang_storage.c (real
 * hardware only; stubbed as no-ops in emu-rs/csrc/bafang_storage_stub.c),
 * same split as bafang_settings_t/bafang_storage_load()/save(). */
void bafang_energy_load(void);
void bafang_energy_save(void);

#endif
