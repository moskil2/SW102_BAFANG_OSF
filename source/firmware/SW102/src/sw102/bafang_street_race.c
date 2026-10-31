#include <string.h>
#include "bafang_street_race.h"

bafang_street_race_t g_street_race;

/* Switch procedure, 1:1 with the EggSPEED app's STREET/RACE switch:
 * assist -> 0, wait 1s so the controller really sees 0, fresh BAS read (the
 * shared fields - LBP, LC, wheel, speed meter - go back unchanged), swap in
 * only the target's two tables, write, confirm, one retry on failure.
 * Telemetry polling pauses on its own while a BAS read/write is in flight
 * (bafang_display.c's BAS_WAIT states). The confirmation is the
 * controller's own write ACK, same as the SAVE row of the Assist Level
 * Programming screen - real hardware (2026-09-18) showed a write is
 * acknowledged, so no extra verify read is needed. */
#define SWITCH_ZERO_WAIT_TICKS 10 /* 1s at the 100ms tick */
#define SWITCH_ATTEMPTS 2         /* first try + one retry */

typedef enum
{
	SWITCH_IDLE = 0,
	SWITCH_WAIT_ZERO,
	SWITCH_READ,
	SWITCH_WRITE
} switch_state_t;

static bafang_display_t* sw_display;
static switch_state_t sw_state;
static bafang_profile_t sw_target;
static uint8_t sw_ticks;
static uint8_t sw_attempt;
static bafang_bas_block_t sw_fresh;
static bafang_switch_result_t sw_result;

void bafang_street_race_init_defaults(void)
{
	memset(&g_street_race, 0, sizeof(g_street_race));
	g_street_race.active = BAFANG_PROFILE_STREET;
}

void bafang_street_race_request_switch(bafang_display_t* d)
{
	if (!g_street_race.enabled || sw_state != SWITCH_IDLE)
		return;

	sw_display = d;
	sw_target = g_street_race.active == BAFANG_PROFILE_STREET ? BAFANG_PROFILE_RACE : BAFANG_PROFILE_STREET;
	sw_attempt = 0;
	sw_ticks = 0;
	sw_state = SWITCH_WAIT_ZERO;
}

bool bafang_street_race_busy(void)
{
	return sw_state != SWITCH_IDLE;
}

bafang_switch_result_t bafang_street_race_take_result(void)
{
	bafang_switch_result_t r = sw_result;
	sw_result = BAFANG_SWITCH_RESULT_NONE;
	return r;
}

static void finish(bool ok)
{
	sw_state = SWITCH_IDLE;
	sw_result = ok ? BAFANG_SWITCH_RESULT_OK : BAFANG_SWITCH_RESULT_FAIL;
}

static void start_attempt(void)
{
	bafang_display_request_bas_read(sw_display);
	sw_state = SWITCH_READ;
}

static void attempt_failed(void)
{
	if (++sw_attempt < SWITCH_ATTEMPTS)
		start_attempt();
	else
		finish(false);
}

void bafang_street_race_tick(void)
{
	switch (sw_state)
	{
	case SWITCH_IDLE:
		break;

	case SWITCH_WAIT_ZERO:
		if (++sw_ticks >= SWITCH_ZERO_WAIT_TICKS)
		{
			/* STREET's table only exists once a switch to RACE captured it -
			 * without it there is nothing safe to write back. */
			if (sw_target == BAFANG_PROFILE_STREET && !g_street_race.street_valid)
				finish(false);
			else
				start_attempt();
		}
		break;

	case SWITCH_READ:
		if (bafang_display_bas_pending(sw_display))
			break;
		if (!bafang_display_bas_result_ok(sw_display))
		{
			attempt_failed();
			break;
		}
		sw_fresh = *bafang_display_get_bas_block(sw_display);
		{
			bafang_bas_block_t target = sw_fresh;
			if (sw_target == BAFANG_PROFILE_RACE)
			{
				memcpy(target.assist_current_pct, g_street_race.race_current_pct, BAFANG_PAS_LEVELS);
				memcpy(target.assist_speed_pct, g_street_race.race_speed_pct, BAFANG_PAS_LEVELS);
			}
			else
			{
				memcpy(target.assist_current_pct, g_street_race.street_current_pct, BAFANG_PAS_LEVELS);
				memcpy(target.assist_speed_pct, g_street_race.street_speed_pct, BAFANG_PAS_LEVELS);
			}
			bafang_display_request_bas_save(sw_display, &target);
		}
		sw_state = SWITCH_WRITE;
		break;

	case SWITCH_WRITE:
		if (bafang_display_bas_pending(sw_display))
			break;
		if (!bafang_display_bas_result_ok(sw_display))
		{
			attempt_failed();
			break;
		}
		if (sw_target == BAFANG_PROFILE_RACE)
		{
			/* What the controller held right before RACE went on is STREET -
			 * kept so the next switch can put it back. */
			memcpy(g_street_race.street_current_pct, sw_fresh.assist_current_pct, BAFANG_PAS_LEVELS);
			memcpy(g_street_race.street_speed_pct, sw_fresh.assist_speed_pct, BAFANG_PAS_LEVELS);
			g_street_race.street_valid = true;
		}
		g_street_race.active = sw_target;
		bafang_street_race_save();
		finish(true);
		break;
	}
}
