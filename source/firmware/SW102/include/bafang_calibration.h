#ifndef _BAFANG_CALIBRATION_H_
#define _BAFANG_CALIBRATION_H_

#include <stdint.h>

/*
 * Current calibration (v1 scope feature) - a simple multiplier applied to
 * the raw current value decoded from the wire, exactly like EggSPEED's
 * currentCalibrationFactor: base value 1.00 means "no correction" (raw
 * reading used as-is, multiplied by one). Adjustable on-device from 0.01
 * to 3.00 in steps of 0.01.
 *
 * Stored internally as integer hundredths (1..300) so it round-trips
 * exactly through EEPROM/flash and repeated UP/DOWN adjustments without
 * float rounding drift.
 */

#define BAFANG_CALIBRATION_STEPS_MIN     1   /* 0.01 */
#define BAFANG_CALIBRATION_STEPS_MAX     300 /* 3.00 */
#define BAFANG_CALIBRATION_STEPS_DEFAULT 100 /* 1.00 = no correction */

/* Converts stored steps (1..300) to the float multiplier (0.01..3.00) used
 * by bafang_protocol_state_t.current_calibration_factor. */
float bafang_calibration_factor(uint16_t steps);

/* Adjusts by delta_steps (e.g. +1/-1 for a single UP/DOWN press on the
 * on-device menu), clamped to [BAFANG_CALIBRATION_STEPS_MIN,
 * BAFANG_CALIBRATION_STEPS_MAX]. Returns the new steps value to store. */
uint16_t bafang_calibration_adjust(uint16_t steps, int16_t delta_steps);

#endif
