#include "bafang_calibration.h"

float bafang_calibration_factor(uint16_t steps)
{
	return steps / 100.0f;
}

uint16_t bafang_calibration_adjust(uint16_t steps, int16_t delta_steps)
{
	int32_t result = (int32_t)steps + delta_steps;

	if (result < BAFANG_CALIBRATION_STEPS_MIN)
	{
		result = BAFANG_CALIBRATION_STEPS_MIN;
	}
	else if (result > BAFANG_CALIBRATION_STEPS_MAX)
	{
		result = BAFANG_CALIBRATION_STEPS_MAX;
	}

	return (uint16_t)result;
}
