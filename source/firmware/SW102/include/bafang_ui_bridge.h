#ifndef _BAFANG_UI_BRIDGE_H_
#define _BAFANG_UI_BRIDGE_H_

#include "bafang_display.h"

/*
 * Read-only accessor(s) for values the existing ui_vars_t/screen_main.c
 * framework has no field for. Deliberately NOT added as new fields on
 * ui_vars_t itself - that struct is shared with the 850C/860C targets and
 * touched by eeprom.c, so keeping this separate avoids any risk to that
 * unrelated code path. The main telemetry (speed/brake/battery/power) is
 * bridged directly into existing ui_vars_t fields instead (see main.c),
 * since those already exist and screen_main.c already reads them.
 */

/* Calibrated current in amps (bafang_calibration_factor() already
 * applied) - not something ui_vars_t has a field for, unlike power. */
float bafang_get_current_amps(void);

/* main.c's g_bafang_display, otherwise static/private to that file - only
 * the "Bafang Assist Level Programming" menu (screen_bas_programming.c)
 * needs direct access, to issue on-demand BAS block read/write requests
 * (bafang_display_request_bas_read/write()) alongside the normal telemetry
 * polling those functions already drive. */
bafang_display_t* bafang_ui_bridge_get_display(void);

#endif
