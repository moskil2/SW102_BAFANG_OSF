#ifndef _BAFANG_DISPLAY_H_
#define _BAFANG_DISPLAY_H_

#include <stdint.h>
#include <stdbool.h>
#include "bafang_protocol.h"

/*
 * Safe polling loop for a genuine OEM Bafang controller.
 *
 * State order mirrors the "extendedRegistersEnabled = false" path of
 * the EggSPEED app's display state machine exactly, for this reason:
 * on real hardware, sending the extended registers (mode / temperatures /
 * status-as-a-separate-diagnostic-poll / unknown3 / moving) is confirmed
 * to make some OEM controllers stop responding to everything. Do not add
 * those registers to this loop - they belong in a separate, bbs-fw-only
 * diagnostics path if ever added.
 *
 * Loop: init+assist+status -> battery -> speed -> current -> lights+assist+status -> ...
 *
 * Unlike the Kotlin original (which adds small artificial delays between
 * writes to give a USB-serial adapter time to flush), this version sends
 * the next request immediately after parsing the previous response -
 * appropriate for a real UART peripheral with proper interrupt-driven
 * buffering, not a USB-serial bridge. The state ORDER is what matters for
 * correctness on real hardware; the artificial delays were an Android/USB
 * adapter workaround, not a wire-protocol requirement.
 *
 * Call bafang_display_tick() frequently (assumed ~10ms cadence - adjust
 * BAFANG_DISPLAY_TIMEOUT_TICKS if your caller uses a different interval)
 * from the main loop, and bafang_display_on_data_received() from the UART
 * RX path whenever bytes arrive.
 */

#define BAFANG_DISPLAY_TIMEOUT_TICKS 100 /* ~1s at a 10ms tick cadence */
/* 32, not 8: large enough to hold a full BAS config-block read response
 * (1 length byte + 24 data bytes + 1 checksum = 26) - see
 * bafang_display_request_bas_read() below. Every other response this
 * struct ever parses is 3 bytes or fewer. */
#define BAFANG_DISPLAY_RX_BUF_SIZE 32

typedef void (*bafang_write_fn)(const uint8_t* data, uint8_t len);

typedef struct
{
	float speed_kmh;
	uint8_t battery_percent;
	float current_amps;
	bool brake_active;

	/* Voltage: prefer a real local measurement (adc.c's
	 * battery_voltage_10x_get(), fed in via bafang_display_set_measured_voltage())
	 * over the %-battery estimate, which is only a fallback OEM firmware
	 * itself is forced to rely on since it has no working voltage register. */
	float voltage_v;
	bool voltage_is_measured;
} bafang_telemetry_t;

typedef enum
{
	BAFANG_DISPLAY_STATE_INIT = 0,
	BAFANG_DISPLAY_STATE_WAIT_STATUS,
	BAFANG_DISPLAY_STATE_WAIT_BATTERY,
	BAFANG_DISPLAY_STATE_WAIT_SPEED,
	BAFANG_DISPLAY_STATE_WAIT_CURRENT,
	/* On-demand BAS config-block read, requested by the "Bafang Assist
	 * Level Programming" menu (screen_bas_programming.c) - see
	 * bafang_display_request_bas_read() below. Entering this state
	 * abandons whatever normal telemetry request was in flight and
	 * suspends the regular polling loop (it naturally can't advance
	 * while d->state sits here) until the response arrives or times out,
	 * at which point it resumes cleanly from BAFANG_DISPLAY_STATE_INIT. */
	BAFANG_DISPLAY_STATE_BAS_WAIT,
	/* Waiting for the 3-byte write-ACK a real controller sends after a
	 * WRITE_BAS request (confirmed on real hardware 2026-09-18 - see
	 * bafang_parse_write_bas_ack()'s comment). Separate from
	 * BAFANG_DISPLAY_STATE_BAS_WAIT above because the ACK is a different,
	 * shorter frame than a plain read's reply - conflating the two was the
	 * actual bug: expecting a 27-byte read reply right after a write
	 * silently ate the 3 ACK bytes as if they were its first 3 bytes. */
	BAFANG_DISPLAY_STATE_BAS_WRITE_ACK_WAIT
} bafang_display_state_t;

/* What bafang_display_tick() should send, as a single atomic step, at the
 * very start of its next call - see bas_deferred_op's field comment below
 * for why both the plain read and the save's write go through this instead
 * of sending anything synchronously from the request functions themselves. */
typedef enum
{
	BAFANG_BAS_DEFERRED_NONE = 0,
	BAFANG_BAS_DEFERRED_READ,
	/* Write, then wait for the controller's own 3-byte ACK (NOT a followup
	 * read - see BAFANG_DISPLAY_STATE_BAS_WRITE_ACK_WAIT). Renamed from an
	 * earlier BAFANG_BAS_DEFERRED_WRITE_THEN_READ once real hardware
	 * revealed the ACK, 2026-09-18. */
	BAFANG_BAS_DEFERRED_WRITE
} bafang_bas_deferred_op_t;

typedef struct
{
	/* --- fixed configuration, set once via bafang_display_init --- */
	bafang_write_fn write;
	float wheel_circumference_m;
	float low_battery_protection_v;
	uint8_t cell_count;

	/* --- live inputs, updated by the caller as needed --- */
	bafang_assist_level_t assist_level;
	bool lights_on;

	/* --- internal state --- */
	bafang_protocol_state_t protocol;
	bafang_telemetry_t telemetry;
	bafang_display_state_t state;
	uint8_t rx_buf[BAFANG_DISPLAY_RX_BUF_SIZE];
	uint8_t rx_len;
	uint16_t timeout_ticks;

	/* --- on-demand BAS config-block read result, see below --- */
	bool bas_pending;         /* true from bafang_display_request_bas_read()/bafang_display_request_bas_save() until the response/ACK arrives or times out */
	bool bas_result_ok;       /* valid once bas_pending goes back to false: true = bas_block holds a freshly, successfully read OR written-and-ACKed block */
	bafang_bas_block_t bas_block;
	/* Neither a plain read nor a save's write is sent immediately from the
	 * request functions below - both are deferred to the very start of the
	 * next bafang_display_tick() call instead (real bug, found 2026-09-17):
	 * sending a read synchronously, mid-tick, could race a normal telemetry
	 * request that tick() had *just* sent moments earlier in the very same
	 * tick - both requests' bytes would arrive together and get appended to
	 * the same rx_buf, so the BAS response no longer started where
	 * bafang_parse_read_bas() expected, and the read failed despite
	 * everything being sent correctly. A save's write was initially left as
	 * an immediate, synchronous send - found later the same day to be the
	 * same class of asymmetry. Both are now sent only at a clean tick
	 * boundary via a single deferred bas_deferred_op, closing that gap
	 * entirely. */
	bafang_bas_deferred_op_t bas_deferred_op;
	bafang_bas_block_t bas_write_payload; /* valid only while bas_deferred_op == BAFANG_BAS_DEFERRED_WRITE_THEN_READ */
} bafang_display_t;

void bafang_display_init(bafang_display_t* d, bafang_write_fn write,
	float wheel_circumference_m, float low_battery_protection_v, uint8_t cell_count);

/* Feed raw bytes as they arrive from the UART RX interrupt/buffer. */
void bafang_display_on_data_received(bafang_display_t* d, const uint8_t* data, uint8_t len);

/* Drives the state machine forward by at most one step per call. */
void bafang_display_tick(bafang_display_t* d);

/* Feed a real, locally-measured voltage (e.g. from battery_voltage_10x_get()
 * in adc.c, divided by 10) - takes priority over the %-battery estimate. */
void bafang_display_set_measured_voltage(bafang_display_t* d, float voltage_v);

const bafang_telemetry_t* bafang_display_get_telemetry(const bafang_display_t* d);

/* Adjustable at runtime from the on-device calibration menu (v1 scope). */
void bafang_display_set_current_calibration(bafang_display_t* d, float factor);

/* --- On-demand BAS config-block access, for the "Bafang Assist Level
 * Programming" menu only - NOT part of the normal polling loop above.
 * Both functions immediately interrupt/suspend normal telemetry polling
 * (see BAFANG_DISPLAY_STATE_BAS_WAIT); it resumes on its own once the
 * operation completes. Caller must not issue a new request while
 * bafang_display_bas_pending() is still true. */

void bafang_display_request_bas_read(bafang_display_t* d);

/* Writes the whole block, as a deferred operation (sent at the next clean
 * tick boundary - see bas_deferred_op), then waits for the controller's own
 * 3-byte write ACK (see bafang_parse_write_bas_ack()) - NOT a followup read.
 * An earlier version re-read and compared the whole block instead
 * (matching the EggSPEED app's STREET/RACE switch pattern);
 * changed 2026-09-18 once real hardware revealed writes get their own ACK
 * frame, which a plain read request right after the write was colliding
 * with. bafang_display_bas_result_ok() below reflects the ACK's own
 * accepted/rejected status - the caller must still wait for
 * bafang_display_bas_pending() to go back to false first. On success,
 * bafang_display_get_bas_block() is updated to the just-written block
 * (the ACK confirms it was stored exactly as sent, no separate read
 * needed). */
void bafang_display_request_bas_save(bafang_display_t* d, const bafang_bas_block_t* block);

static inline bool bafang_display_bas_pending(const bafang_display_t* d) { return d->bas_pending; }

/* Valid only once bafang_display_bas_pending() has gone back to false -
 * true if bafang_display_get_bas_block() below now holds a freshly,
 * successfully parsed (read) or written-and-ACKed (save) block. */
static inline bool bafang_display_bas_result_ok(const bafang_display_t* d) { return d->bas_result_ok; }

const bafang_bas_block_t* bafang_display_get_bas_block(const bafang_display_t* d);

#endif
