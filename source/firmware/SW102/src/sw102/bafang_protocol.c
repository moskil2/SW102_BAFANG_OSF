#include "bafang_protocol.h"

static uint8_t compute_checksum(const uint8_t* buf, uint8_t length)
{
	uint8_t result = 0;

	for (uint8_t i = 0; i < length; ++i)
	{
		result = (uint8_t)(result + buf[i]);
	}

	return result;
}

void bafang_protocol_state_init(bafang_protocol_state_t* state)
{
	state->current_calibration_factor = 1.0f;
}

static uint8_t read_request_length(uint8_t opcode)
{
	switch (opcode)
	{
	case BAFANG_OPCODE_READ_UNKNOWN1:
	case BAFANG_OPCODE_READ_RANGE:
	case BAFANG_OPCODE_READ_CALORIES:
	case BAFANG_OPCODE_READ_UNKNOWN3:
		return 3;
	default:
		return 2;
	}
}

uint8_t bafang_build_read_request(uint8_t opcode, uint8_t* buf)
{
	buf[0] = BAFANG_REQUEST_READ;
	buf[1] = opcode;

	uint8_t len = read_request_length(opcode);
	if (len == 3)
	{
		buf[2] = compute_checksum(buf, 2);
	}

	return len;
}

static uint8_t assist_level_code(bafang_assist_level_t level)
{
	/* Non-sequential byte codes as sent by a genuine OEM display - order
	 * confirmed from controller-side handling, not something we chose. */
	switch (level)
	{
	case BAFANG_ASSIST_0:    return 0x00;
	case BAFANG_ASSIST_1:    return 0x01;
	case BAFANG_ASSIST_2:    return 0x0b;
	case BAFANG_ASSIST_3:    return 0x0c;
	case BAFANG_ASSIST_4:    return 0x0d;
	case BAFANG_ASSIST_5:    return 0x02;
	case BAFANG_ASSIST_6:    return 0x15;
	case BAFANG_ASSIST_7:    return 0x16;
	case BAFANG_ASSIST_8:    return 0x17;
	case BAFANG_ASSIST_9:    return 0x03;
	case BAFANG_ASSIST_PUSH: return 0x06;
	default:                 return 0x00;
	}
}

uint8_t bafang_build_write_pas_request(bafang_assist_level_t level, uint8_t* buf)
{
	buf[0] = BAFANG_REQUEST_WRITE;
	buf[1] = BAFANG_OPCODE_WRITE_PAS;
	buf[2] = assist_level_code(level);
	buf[3] = compute_checksum(buf, 3);

	return 4;
}

uint8_t bafang_build_write_mode_request(bafang_operation_mode_t mode, uint8_t* buf)
{
	buf[0] = BAFANG_REQUEST_WRITE;
	buf[1] = BAFANG_OPCODE_WRITE_MODE;
	buf[2] = (mode == BAFANG_MODE_SPORT) ? 0x04 : 0x02;
	buf[3] = compute_checksum(buf, 3);

	return 4;
}

uint8_t bafang_build_write_lights_request(bool on, uint8_t* buf)
{
	/* No checksum byte for this opcode - confirmed from controller side,
	 * which explicitly skips checksum validation for lights. */
	buf[0] = BAFANG_REQUEST_WRITE;
	buf[1] = BAFANG_OPCODE_WRITE_LIGHTS;
	buf[2] = on ? 0xf1 : 0xf0;

	return 3;
}

uint8_t bafang_build_write_speed_limit_request(uint16_t speed_limit_kph, uint8_t* buf)
{
	buf[0] = BAFANG_REQUEST_WRITE;
	buf[1] = BAFANG_OPCODE_WRITE_SPEED_LIM;
	buf[2] = (uint8_t)(speed_limit_kph >> 8);
	buf[3] = (uint8_t)speed_limit_kph;
	buf[4] = compute_checksum(buf, 4);

	return 5;
}

bool bafang_parse_read_current(const uint8_t* data, uint8_t len,
	const bafang_protocol_state_t* state, float* out_amps)
{
	/* Controller sends the same byte twice - value doubles as its own
	 * checksum (sum of a single byte equals that byte). */
	if (len != 2 || data[0] != data[1])
	{
		return false;
	}

	/* Wire unit is 0.5A per LSB (controller computes amp_x2 = amps * 2). */
	float raw_amps = data[0] * 0.5f;
	*out_amps = raw_amps * state->current_calibration_factor;

	return true;
}

bool bafang_parse_read_battery(const uint8_t* data, uint8_t len, uint8_t* out_raw_level)
{
	if (len != 2 || data[0] != data[1])
	{
		return false;
	}

	*out_raw_level = data[0];

	return true;
}

bool bafang_parse_read_moving(const uint8_t* data, uint8_t len, bool* out_moving)
{
	if (len != 2 || data[0] != data[1])
	{
		return false;
	}

	*out_moving = (data[0] == 0x31);

	return true;
}

bool bafang_parse_read_status(const uint8_t* data, uint8_t len, uint8_t* out_status_raw)
{
	if (len < 1)
	{
		return false;
	}

	*out_status_raw = data[0];

	return true;
}

/* Wheel size uses a different, non-linear byte encoding on the wire than
 * our own 0-15 index (BAFANG_WHEEL_CIRCUMFERENCE_M). encode_wheel_code is
 * byte-exact confirmed from the EggSPEED app's writer
 * - 700C (our index 12) is a fixed sentinel value (55)
 * instead of following the arithmetic pattern. decode_wheel_code is
 * byte-exact confirmed against the reference decoder's actual
 * behaviour
 * - NOT a hand-derived "inverse" of the encoder, which
 * turned out to disagree with the real decoder for odd raw values other
 * than 55 (never produced by the real encoder, but worth getting exactly
 * right rather than "close enough"). */
static uint8_t encode_wheel_code(uint8_t wd)
{
	if (wd == 12)
	{
		return 55;
	}
	if (wd < 12)
	{
		return (uint8_t)((wd + 16) * 2);
	}
	return (uint8_t)((wd + 15) * 2);
}

static uint8_t decode_wheel_code(uint8_t raw)
{
	uint8_t rem = (uint8_t)(raw % 2);
	uint8_t half_ceil = (uint8_t)(rem + raw / 2); /* ceil(raw/2) */

	if (half_ceil <= 27)
	{
		return (uint8_t)(half_ceil - 16);
	}
	if (half_ceil == 28 && rem == 1)
	{
		return 12;
	}
	return (uint8_t)(half_ceil - 15);
}

static uint8_t encode_speed_meter(uint8_t model, uint8_t signals)
{
	return (uint8_t)(model * 64 + signals);
}

/* Byte-exact confirmed (same bytecode-disassembly source as
 * decode_wheel_code above): a raw value whose high byte (raw/64) would
 * decode to model=3 is clamped to model=2 instead - 3 is not a valid
 * speed_meter_model (0=external/1=internal/2=motorphase), so the real
 * decoder defensively folds it into the nearest valid one rather than
 * exposing an out-of-range value. A plain "raw/64" without this clamp
 * would disagree with the real decoder whenever this edge case occurs. */
static void decode_speed_meter(uint8_t raw, uint8_t* out_model, uint8_t* out_signals)
{
	uint8_t div64 = (uint8_t)(raw / 64);
	*out_model = (div64 == 3) ? 2 : div64;
	*out_signals = (uint8_t)(raw % 64);
}

uint8_t bafang_build_write_bas_request(const bafang_bas_block_t* block, uint8_t* buf)
{
	buf[0] = BAFANG_REQUEST_WRITE;
	buf[1] = BAFANG_OPCODE_BAS;
	buf[2] = BAFANG_BAS_DATA_LEN;

	uint8_t i = 3;
	buf[i++] = block->low_battery_protection_v;
	buf[i++] = block->current_limit_a;
	for (uint8_t j = 0; j < 10; ++j)
	{
		buf[i++] = block->assist_current_pct[j];
	}
	for (uint8_t j = 0; j < 10; ++j)
	{
		buf[i++] = block->assist_speed_pct[j];
	}
	buf[i++] = encode_wheel_code(block->wheel_diameter_code);
	buf[i++] = encode_speed_meter(block->speed_meter_model, block->speed_meter_signals);

	/* LRC over everything from the opcode through the last data byte
	 * (buf[1..26], 26 bytes) - NOT including the leading request-type
	 * byte, matching the reference writer exactly. */
	buf[i] = compute_checksum(buf + 1, 2 + BAFANG_BAS_DATA_LEN);
	i++;

	return i; /* 28 */
}

bool bafang_parse_write_bas_ack(const uint8_t* data, uint8_t len, uint8_t* out_status)
{
	/* [block_address, status_code, block_address+status_code] - see the
	 * declaration comment. Checksum here is a plain 2-byte sum, not the
	 * additive-LRC-over-N-bytes formula used everywhere else in this file
	 * (the third byte is a plain sum, not a full LRC). */
	if (len != BAFANG_BAS_WRITE_ACK_LEN)
	{
		return false;
	}
	if (data[0] != BAFANG_OPCODE_BAS)
	{
		return false;
	}
	if ((uint8_t)(data[0] + data[1]) != data[2])
	{
		return false;
	}

	*out_status = data[1];
	return true;
}

bool bafang_parse_read_bas(const uint8_t* data, uint8_t len, bafang_bas_block_t* out)
{
	/* Byte-exact confirmed response envelope (reference
	 * config frame parser): [block_address,
	 * length, ...data(length), LRC] - the address IS echoed back (unlike
	 * every other read response in this file), and the LRC is the same
	 * additive-sum formula as everything else here, computed over
	 * [address, length, ...data]. */
	const uint8_t expected_len = (uint8_t)(2 + BAFANG_BAS_DATA_LEN + 1);
	if (len != expected_len)
	{
		return false;
	}
	if (data[0] != BAFANG_OPCODE_BAS || data[1] != BAFANG_BAS_DATA_LEN)
	{
		return false;
	}
	if (compute_checksum(data, 2 + BAFANG_BAS_DATA_LEN) != data[2 + BAFANG_BAS_DATA_LEN])
	{
		return false;
	}

	const uint8_t* p = data + 2;
	out->low_battery_protection_v = p[0];
	out->current_limit_a = p[1];
	for (uint8_t j = 0; j < 10; ++j)
	{
		out->assist_current_pct[j] = p[2 + j];
	}
	for (uint8_t j = 0; j < 10; ++j)
	{
		out->assist_speed_pct[j] = p[12 + j];
	}
	out->wheel_diameter_code = decode_wheel_code(p[22]);
	decode_speed_meter(p[23], &out->speed_meter_model, &out->speed_meter_signals);

	return true;
}

const float BAFANG_WHEEL_CIRCUMFERENCE_M[16] = {
	1.2f, 1.28f, 1.35f, 1.4f, 1.545f, 1.667f, 1.78f, 1.84f,
	1.9f, 1.95f, 2.058f, 2.145f, 2.09f, 2.268f, 2.288f, 2.31f
};

float bafang_speed_rpm_to_kmh(uint16_t raw_rpm, float wheel_circumference_m)
{
	return raw_rpm * wheel_circumference_m * 60.0f / 1000.0f;
}

uint8_t bafang_build_init_display_request(uint8_t* buf)
{
	return bafang_build_write_lights_request(false, buf);
}

float bafang_estimate_percent_from_voltage(float voltage_v, float min_v, float max_v)
{
	float percent = (voltage_v - min_v) / (max_v - min_v) * 100.0f;

	if (percent < 0.0f)
		percent = 0.0f;
	else if (percent > 100.0f)
		percent = 100.0f;

	return percent;
}

float bafang_estimate_voltage(uint8_t battery_percent, float low_battery_protection_v, uint8_t cell_count)
{
	float max_v = cell_count * 4.2f;

	if (max_v <= low_battery_protection_v)
	{
		return max_v;
	}

	return low_battery_protection_v + (max_v - low_battery_protection_v) * battery_percent / 100.0f;
}

bool bafang_parse_read_speed(const uint8_t* data, uint8_t len, uint16_t* out_raw_speed)
{
	if (len != 3)
	{
		return false;
	}

	/* Checksum here is offset by a constant 0x20 - confirmed quirk in the
	 * controller-side implementation, not a mistake in this code. */
	uint8_t checksum = (uint8_t)(compute_checksum(data, 2) + 0x20);
	if (checksum != data[2])
	{
		return false;
	}

	*out_raw_speed = ((uint16_t)data[0] << 8) | data[1];

	return true;
}

bool bafang_parse_read_calories_as_voltage_x10(const uint8_t* data, uint8_t len, uint16_t* out_voltage_x10)
{
	if (len != 3 || compute_checksum(data, 2) != data[2])
	{
		return false;
	}

	*out_voltage_x10 = ((uint16_t)data[0] << 8) | data[1];

	return true;
}
