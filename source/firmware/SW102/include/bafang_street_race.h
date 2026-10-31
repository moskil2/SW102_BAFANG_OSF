#ifndef _BAFANG_STREET_RACE_H_
#define _BAFANG_STREET_RACE_H_

#include <stdint.h>
#include <stdbool.h>
#include "bafang_display.h"

/*
 * STREET/RACE assist profiles (OEM Bafang only) - same model and naming as
 * the EggSPEED app, on purpose: the plan is to integrate SW102
 * with EggSPEED over Bluetooth later, where switching on one side means
 * switching on the other, so both keep the same two-table model.
 *
 * The controller only ever holds ONE per-level table (the BAS block's
 * current%/speed% for PAS 0-9). RACE is a second, independent table kept on
 * the display and loaded onto the controller only by a switch. STREET's
 * table is captured from the controller at the moment of switching to RACE,
 * so it can be written back when switching to STREET again.
 *
 * Decisions (user, 2026-10-29):
 * - "USE RACE" YES/NO lives at the bottom of the Assist Level Programming
 *   screen; the first enable copies STREET into RACE.
 * - While RACE is active, BOTH tables are view-only (switch to STREET first).
 * - USE RACE cannot be turned off while RACE is active.
 * - RACE edits are committed by their own SAVE RACE row (display flash
 *   only, never the controller).
 * - Switching on the cockpit: UP+DOWN held together for 3s.
 */

#define BAFANG_PAS_LEVELS 10

typedef enum
{
	BAFANG_PROFILE_STREET = 0,
	BAFANG_PROFILE_RACE = 1
} bafang_profile_t;

typedef struct
{
	bool enabled;                /* menu "USE RACE" */
	bafang_profile_t active;     /* profile currently loaded on the controller */
	bool race_valid;             /* race_* tables hold real data (set by the first enable) */
	bool street_valid;           /* street_* tables hold real data (captured at the last switch to RACE) */
	uint8_t race_current_pct[BAFANG_PAS_LEVELS];
	uint8_t race_speed_pct[BAFANG_PAS_LEVELS];
	uint8_t street_current_pct[BAFANG_PAS_LEVELS];
	uint8_t street_speed_pct[BAFANG_PAS_LEVELS];
} bafang_street_race_t;

extern bafang_street_race_t g_street_race;

typedef enum
{
	BAFANG_SWITCH_RESULT_NONE = 0,
	BAFANG_SWITCH_RESULT_OK,
	BAFANG_SWITCH_RESULT_FAIL
} bafang_switch_result_t;

void bafang_street_race_init_defaults(void);

/* Persistence lives in bafang_storage.c (own FDS record). Saved at once on
 * every change, not at power-off like the other settings: if the battery is
 * pulled while RACE is active, the display must still know the controller
 * holds RACE after the next boot. */
void bafang_street_race_load(void);
void bafang_street_race_save(void);

/* Starts a switch to the other profile (STREET <-> RACE). Ignored if the
 * feature is off or a switch is already running. */
void bafang_street_race_request_switch(bafang_display_t* d);

/* True for the whole switch - the caller holds the assist level at 0 while
 * this is set (see bafang_ui_bridge_update()). */
bool bafang_street_race_busy(void);

/* Call every 100ms, right after bafang_display_tick(). */
void bafang_street_race_tick(void);

/* Result of the last finished switch, returned once (then NONE again) - for
 * the cockpit's "RACE MOD ACTIVE"/"Write error" notice. */
bafang_switch_result_t bafang_street_race_take_result(void);

#endif
