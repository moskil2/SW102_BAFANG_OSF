#include <math.h>
#include "bafang_energy.h"

bafang_energy_state_t g_bafang_energy;
bafang_energy_estimate_t g_bafang_energy_estimate;

/* Same cadence as bafang_trip.c's bafang_trip_tick() - both are called
 * once per 100ms from bafang_ui_bridge_update(). */
#define TICK_SECONDS 0.1f

#define SHORT_TERM_HALF_LIFE_KM      0.3f
#define MEDIUM_TERM_HALF_LIFE_KM     3.0f
#define MAX_HISTORY_KM               300.0f
#define MIN_CHUNK_DISTANCE_KM        0.05f
#define RANGE_BLEND_TRANSITION_KM    4.0f
#define MAX_LEARN_WEIGHT_PER_CHUNK   0.3f
#define DEFAULT_HISTORICAL_WH_PER_KM 12.0f
#define MIN_WH_PER_KM_FOR_RANGE      0.5f

void bafang_energy_init_defaults(void)
{
	g_bafang_energy.historical_avg_wh_per_km = DEFAULT_HISTORICAL_WH_PER_KM;
	g_bafang_energy.historical_km = 0.0f;
	g_bafang_energy.trip_distance_km = 0.0f;
	g_bafang_energy.trip_energy_wh = 0.0f;
	g_bafang_energy.short_term_avg_wh_per_km = DEFAULT_HISTORICAL_WH_PER_KM;
	g_bafang_energy.medium_term_avg_wh_per_km = DEFAULT_HISTORICAL_WH_PER_KM;
	g_bafang_energy.pending_distance_km = 0.0f;
	g_bafang_energy.pending_energy_wh = 0.0f;
	g_bafang_energy.last_battery_pct = 0;
}

/* Exponential decay by DISTANCE travelled, not time - alpha = 1-0.5^(step/half_life). */
static float decay(float current, float sample, float step_km, float half_life_km)
{
	float alpha = 1.0f - expf(-0.6931472f * step_km / half_life_km); /* ln(2) */
	return current * (1.0f - alpha) + sample * alpha;
}

void bafang_energy_reset_trip(void)
{
	/* Only the CURRENT trip resets (on a full charge, or now also from
	 * the menu's "Trip reset") - the learned short/medium/historical
	 * averages intentionally survive, same as the EggSPEED app's
	 * reset(). */
	g_bafang_energy.trip_distance_km = 0.0f;
	g_bafang_energy.trip_energy_wh = 0.0f;
	g_bafang_energy.pending_distance_km = 0.0f;
	g_bafang_energy.pending_energy_wh = 0.0f;
}

static void commit_chunk(void)
{
	if (g_bafang_energy.pending_distance_km <= 0.0f)
		return;

	float chunk_rate = g_bafang_energy.pending_energy_wh / g_bafang_energy.pending_distance_km;

	g_bafang_energy.short_term_avg_wh_per_km = decay(g_bafang_energy.short_term_avg_wh_per_km,
		chunk_rate, g_bafang_energy.pending_distance_km, SHORT_TERM_HALF_LIFE_KM);
	g_bafang_energy.medium_term_avg_wh_per_km = decay(g_bafang_energy.medium_term_avg_wh_per_km,
		chunk_rate, g_bafang_energy.pending_distance_km, MEDIUM_TERM_HALF_LIFE_KM);

	float capped_history_km = g_bafang_energy.historical_km < MAX_HISTORY_KM
		? g_bafang_energy.historical_km : MAX_HISTORY_KM;
	float learn_weight = g_bafang_energy.pending_distance_km
		/ (capped_history_km + g_bafang_energy.pending_distance_km);
	if (learn_weight > MAX_LEARN_WEIGHT_PER_CHUNK)
		learn_weight = MAX_LEARN_WEIGHT_PER_CHUNK;

	g_bafang_energy.historical_avg_wh_per_km = g_bafang_energy.historical_avg_wh_per_km * (1.0f - learn_weight)
		+ chunk_rate * learn_weight;
	g_bafang_energy.historical_km += g_bafang_energy.pending_distance_km;
	if (g_bafang_energy.historical_km > MAX_HISTORY_KM)
		g_bafang_energy.historical_km = MAX_HISTORY_KM;

	g_bafang_energy.pending_distance_km = 0.0f;
	g_bafang_energy.pending_energy_wh = 0.0f;
}

static void compute_estimate(float capacity_wh)
{
	float blend_weight = g_bafang_energy.trip_distance_km
		/ (g_bafang_energy.trip_distance_km + RANGE_BLEND_TRANSITION_KM);
	float blended_wh_per_km = g_bafang_energy.historical_avg_wh_per_km * (1.0f - blend_weight)
		+ g_bafang_energy.medium_term_avg_wh_per_km * blend_weight;
	if (blended_wh_per_km < MIN_WH_PER_KM_FOR_RANGE)
		blended_wh_per_km = MIN_WH_PER_KM_FOR_RANGE;

	float pct = g_bafang_energy.last_battery_pct;
	if (pct > 100.0f)
		pct = 100.0f;
	float remaining_wh = capacity_wh * pct / 100.0f;

	float predicted_range_km = remaining_wh / blended_wh_per_km;
	if (predicted_range_km < 0.0f)
		predicted_range_km = 0.0f;

	g_bafang_energy_estimate.predicted_range_km = predicted_range_km;
	g_bafang_energy_estimate.trip_avg_wh_per_km = g_bafang_energy.trip_distance_km >= MIN_CHUNK_DISTANCE_KM
		? g_bafang_energy.trip_energy_wh / g_bafang_energy.trip_distance_km
		: g_bafang_energy.historical_avg_wh_per_km;
	g_bafang_energy_estimate.short_term_wh_per_km = g_bafang_energy.short_term_avg_wh_per_km;
}

void bafang_energy_tick(float speed_kmh, uint8_t battery_pct, float voltage_v, float current_a, float capacity_wh)
{
	if (battery_pct >= 100 && g_bafang_energy.last_battery_pct < 100)
		bafang_energy_reset_trip();
	g_bafang_energy.last_battery_pct = battery_pct;

	float delta_km = speed_kmh * (TICK_SECONDS / 3600.0f);
	float energy_delta_wh = current_a * voltage_v * (TICK_SECONDS / 3600.0f);

	g_bafang_energy.trip_distance_km += delta_km;
	g_bafang_energy.trip_energy_wh += energy_delta_wh;
	g_bafang_energy.pending_distance_km += delta_km;
	g_bafang_energy.pending_energy_wh += energy_delta_wh;

	if (g_bafang_energy.pending_distance_km >= MIN_CHUNK_DISTANCE_KM)
		commit_chunk();

	compute_estimate(capacity_wh);
}
