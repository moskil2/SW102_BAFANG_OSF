#include "bafang_display.h"
#include <string.h>

void bafang_display_init(bafang_display_t* d, bafang_write_fn write,
	float wheel_circumference_m, float low_battery_protection_v, uint8_t cell_count)
{
	memset(d, 0, sizeof(*d));
	d->write = write;
	d->wheel_circumference_m = wheel_circumference_m;
	d->low_battery_protection_v = low_battery_protection_v;
	d->cell_count = cell_count;
	d->assist_level = BAFANG_ASSIST_0;
	d->lights_on = false;
	bafang_protocol_state_init(&d->protocol);
	d->state = BAFANG_DISPLAY_STATE_INIT;
}

void bafang_display_on_data_received(bafang_display_t* d, const uint8_t* data, uint8_t len)
{
	for (uint8_t i = 0; i < len && d->rx_len < BAFANG_DISPLAY_RX_BUF_SIZE; ++i)
	{
		d->rx_buf[d->rx_len++] = data[i];
	}
}

void bafang_display_set_measured_voltage(bafang_display_t* d, float voltage_v)
{
	d->telemetry.voltage_v = voltage_v;
	d->telemetry.voltage_is_measured = true;
}

void bafang_display_set_current_calibration(bafang_display_t* d, float factor)
{
	d->protocol.current_calibration_factor = factor;
}

const bafang_telemetry_t* bafang_display_get_telemetry(const bafang_display_t* d)
{
	return &d->telemetry;
}

void bafang_display_request_bas_read(bafang_display_t* d)
{
	/* Deferred to the start of the next bafang_display_tick() call - see
	 * the bas_deferred_op header comment for why this can't be sent
	 * synchronously here. bas_pending is set to true HERE, immediately
	 * though (not just once the deferred send actually happens) - real bug,
	 * found 2026-09-17 by comparing decoded save results against the raw
	 * bytes actually on the wire: a caller (screen_bas_programming.c) sets
	 * its own "operation in flight" flag right after this call returns and
	 * then polls bafang_display_bas_pending() on every subsequent idle call
	 * to know when it's done. If bas_pending stayed false until the deferred
	 * tick actually fired (up to ~100ms later), that poll would see
	 * "pending == false" on the very next idle call - a leftover value from
	 * whatever the *previous* BAS operation left behind - and wrongly
	 * conclude this brand new request had already finished, reading
	 * bas_block before it had been touched at all. For a save, this made the
	 * verify-read compare against stale pre-write data and report failure,
	 * while the real write+read completed correctly moments later in the
	 * background, unobserved (a second SAVE press would then coincidentally
	 * pick up *that* real result and report success). */
	d->bas_pending = true;
	d->bas_deferred_op = BAFANG_BAS_DEFERRED_READ;
}

void bafang_display_request_bas_save(bafang_display_t* d, const bafang_bas_block_t* block)
{
	/* Snapshot now (caller's block may be a local working copy it keeps
	 * editing) - the actual write is sent at the start of the next
	 * bafang_display_tick() call, then the controller's 3-byte ACK is
	 * awaited (see BAFANG_DISPLAY_STATE_BAS_WRITE_ACK_WAIT). bas_pending is
	 * set immediately - see bafang_display_request_bas_read()'s comment
	 * above for why. */
	d->bas_write_payload = *block;
	d->bas_pending = true;
	d->bas_deferred_op = BAFANG_BAS_DEFERRED_WRITE;
}

const bafang_bas_block_t* bafang_display_get_bas_block(const bafang_display_t* d)
{
	return &d->bas_block;
}

/* PAS + read-status, shared by both entry points below. */
static void restart_cycle(bafang_display_t* d)
{
	uint8_t buf[8];
	uint8_t len;

	len = bafang_build_write_pas_request(d->assist_level, buf);
	d->write(buf, len);

	d->rx_len = 0;
	len = bafang_build_read_request(BAFANG_OPCODE_READ_STATUS, buf);
	d->write(buf, len);

	d->state = BAFANG_DISPLAY_STATE_WAIT_STATUS;
	d->timeout_ticks = 0;
}

/* Cold start: INIT_DISPLAY (same bytes as lights-off) + PAS + read-status.
 * Confirmed against the EggSPEED app's display state machine: INIT (which
 * sends INIT_DISPLAY) only runs once at start() or after a real read
 * timeout - the normal successful loop goes
 * SET_LIGHT -> SET_ASSIST directly, never back through INIT. Resending
 * INIT_DISPLAY (= lights-off) every healthy cycle, as this used to do,
 * raced against the real light command sent later in the same cycle and
 * made a real light blink continuously. Call this only from the INIT
 * state and from timeout fallbacks; use restart_cycle() after a normal,
 * successful cycle. */
static void start_cycle(bafang_display_t* d)
{
	uint8_t buf[8];
	uint8_t len;

	len = bafang_build_init_display_request(buf);
	d->write(buf, len);

	restart_cycle(d);
}

void bafang_display_tick(bafang_display_t* d)
{
	uint8_t buf[8];
	uint8_t len;

	/* Checked FIRST, before anything else this tick, so a deferred BAS
	 * operation always wins the race against a new normal-poll request that
	 * would otherwise go out this same tick - see bas_deferred_op's header
	 * comment. Any bytes already sitting in rx_buf from an in-flight normal
	 * request are safely discarded (that request's own response, if it
	 * still arrives, is simply ignored - the normal cycle restarts cleanly
	 * from BAFANG_DISPLAY_STATE_INIT once this BAS operation completes).
	 * The write itself is sent here too - not from
	 * bafang_display_request_bas_save() itself - so it goes out at a clean
	 * tick boundary instead of at an arbitrary point mid-tick. */
	if (d->bas_deferred_op != BAFANG_BAS_DEFERRED_NONE)
	{
		bafang_bas_deferred_op_t op = d->bas_deferred_op;
		d->bas_deferred_op = BAFANG_BAS_DEFERRED_NONE;
		d->rx_len = 0;
		d->bas_pending = true;
		d->timeout_ticks = 0;

		if (op == BAFANG_BAS_DEFERRED_WRITE)
		{
			uint8_t wbuf[32];
			uint8_t wlen = bafang_build_write_bas_request(&d->bas_write_payload, wbuf);
			d->write(wbuf, wlen);
			/* Wait for the controller's own 3-byte ACK, NOT a followup read
			 * - see BAFANG_DISPLAY_STATE_BAS_WRITE_ACK_WAIT's comment. */
			d->state = BAFANG_DISPLAY_STATE_BAS_WRITE_ACK_WAIT;
		}
		else /* BAFANG_BAS_DEFERRED_READ */
		{
			len = bafang_build_read_request(BAFANG_OPCODE_BAS, buf);
			d->write(buf, len);
			d->state = BAFANG_DISPLAY_STATE_BAS_WAIT;
		}
		return;
	}

	switch (d->state)
	{
	case BAFANG_DISPLAY_STATE_INIT:
		start_cycle(d);
		break;

	case BAFANG_DISPLAY_STATE_WAIT_STATUS:
		if (d->rx_len >= 1)
		{
			uint8_t status;
			if (bafang_parse_read_status(d->rx_buf, 1, &status))
			{
				d->telemetry.brake_active = bafang_status_brake_active(status);
			}

			d->rx_len = 0;
			len = bafang_build_read_request(BAFANG_OPCODE_READ_BATTERY, buf);
			d->write(buf, len);
			d->state = BAFANG_DISPLAY_STATE_WAIT_BATTERY;
			d->timeout_ticks = 0;
		}
		else if (++d->timeout_ticks > BAFANG_DISPLAY_TIMEOUT_TICKS)
		{
			d->state = BAFANG_DISPLAY_STATE_INIT;
		}
		break;

	case BAFANG_DISPLAY_STATE_WAIT_BATTERY:
		if (d->rx_len >= 2)
		{
			uint8_t percent;
			if (bafang_parse_read_battery(d->rx_buf, 2, &percent))
			{
				d->telemetry.battery_percent = percent;
				if (!d->telemetry.voltage_is_measured)
				{
					d->telemetry.voltage_v = bafang_estimate_voltage(
						percent, d->low_battery_protection_v, d->cell_count);
				}
			}

			d->rx_len = 0;
			len = bafang_build_read_request(BAFANG_OPCODE_READ_SPEED, buf);
			d->write(buf, len);
			d->state = BAFANG_DISPLAY_STATE_WAIT_SPEED;
			d->timeout_ticks = 0;
		}
		else if (++d->timeout_ticks > BAFANG_DISPLAY_TIMEOUT_TICKS)
		{
			d->state = BAFANG_DISPLAY_STATE_INIT;
		}
		break;

	case BAFANG_DISPLAY_STATE_WAIT_SPEED:
		if (d->rx_len >= 3)
		{
			uint16_t raw_rpm;
			if (bafang_parse_read_speed(d->rx_buf, 3, &raw_rpm))
			{
				d->telemetry.speed_kmh = bafang_speed_rpm_to_kmh(raw_rpm, d->wheel_circumference_m);
			}

			d->rx_len = 0;
			len = bafang_build_read_request(BAFANG_OPCODE_READ_CURRENT, buf);
			d->write(buf, len);
			d->state = BAFANG_DISPLAY_STATE_WAIT_CURRENT;
			d->timeout_ticks = 0;
		}
		else if (++d->timeout_ticks > BAFANG_DISPLAY_TIMEOUT_TICKS)
		{
			d->state = BAFANG_DISPLAY_STATE_INIT;
		}
		break;

	case BAFANG_DISPLAY_STATE_WAIT_CURRENT:
		if (d->rx_len >= 2)
		{
			float amps;
			if (bafang_parse_read_current(d->rx_buf, 2, &d->protocol, &amps))
			{
				d->telemetry.current_amps = amps;
			}

			d->rx_len = 0;
			/* Sent unconditionally, every single poll cycle - confirmed
			 * against the EggSPEED app (proven working on
			 * real hardware), which does the exact same thing and
			 * explicitly documents SET_LIGHT as a "volatile"/keep-alive
			 * broadcast the genuine controller expects continuously, same
			 * as assist level. A prior "only resend on change" attempt
			 * here caused the real light to turn off and stay off. */
			len = bafang_build_write_lights_request(d->lights_on, buf);
			d->write(buf, len);

			restart_cycle(d);
		}
		else if (++d->timeout_ticks > BAFANG_DISPLAY_TIMEOUT_TICKS)
		{
			d->state = BAFANG_DISPLAY_STATE_INIT;
		}
		break;

	case BAFANG_DISPLAY_STATE_BAS_WAIT:
	{
		const uint8_t expected_len = 2 + BAFANG_BAS_DATA_LEN + 1; /* [address, length, ...data, LRC] */
		if (d->rx_len >= expected_len)
		{
			d->bas_result_ok = bafang_parse_read_bas(d->rx_buf, expected_len, &d->bas_block);
			d->bas_pending = false;
			d->rx_len = 0;
			d->state = BAFANG_DISPLAY_STATE_INIT; /* resume normal polling from a clean restart */
		}
		else if (++d->timeout_ticks > BAFANG_DISPLAY_TIMEOUT_TICKS)
		{
			d->bas_result_ok = false;
			d->bas_pending = false;
			d->rx_len = 0;
			d->state = BAFANG_DISPLAY_STATE_INIT;
		}
		break;
	}

	case BAFANG_DISPLAY_STATE_BAS_WRITE_ACK_WAIT:
	{
		if (d->rx_len >= BAFANG_BAS_WRITE_ACK_LEN)
		{
			uint8_t status;
			bool parsed = bafang_parse_write_bas_ack(d->rx_buf, BAFANG_BAS_WRITE_ACK_LEN, &status);
			d->bas_result_ok = parsed && (status == BAFANG_BAS_DATA_LEN);
			if (d->bas_result_ok)
			{
				/* ACK confirms the block was stored exactly as sent - no
				 * separate read needed to know the new state. */
				d->bas_block = d->bas_write_payload;
			}
			d->bas_pending = false;
			d->rx_len = 0;
			d->state = BAFANG_DISPLAY_STATE_INIT;
		}
		else if (++d->timeout_ticks > BAFANG_DISPLAY_TIMEOUT_TICKS)
		{
			d->bas_result_ok = false;
			d->bas_pending = false;
			d->rx_len = 0;
			d->state = BAFANG_DISPLAY_STATE_INIT;
		}
		break;
	}
	}
}
