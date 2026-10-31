#ifndef _BAFANG_PROTOCOL_H_
#define _BAFANG_PROTOCOL_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * Bafang display <-> controller UART protocol.
 *
 * Independently reimplemented from the documented wire format (opcodes,
 * checksum, frame layout) - not copied from any GPL controller firmware.
 * Framing/checksum facts were cross-checked against a real controller-side
 * implementation (the "extcom" module of a well-known open source BBSHD/
 * BBS02 firmware). Which registers actually WORK on genuine, unmodified
 * Bafang firmware (as opposed to only on that open source controller
 * firmware) and the exact safe polling order were confirmed separately by
 * our own field-tested EggSPEED app - see comments below for what is
 * OEM-confirmed vs. bbs-fw-only vs. still experimental.
 */

#define BAFANG_UART_BAUD_RATE 1200

/* Request type byte (first byte of every frame sent by the display) */
#define BAFANG_REQUEST_READ  0x11
#define BAFANG_REQUEST_WRITE 0x16

/* Read opcodes (display -> controller -> response) */
#define BAFANG_OPCODE_READ_STATUS   0x08
#define BAFANG_OPCODE_READ_CURRENT  0x0a
#define BAFANG_OPCODE_READ_BATTERY  0x11
#define BAFANG_OPCODE_READ_SPEED    0x20
#define BAFANG_OPCODE_READ_UNKNOWN1 0x21
#define BAFANG_OPCODE_READ_RANGE    0x22
#define BAFANG_OPCODE_READ_CALORIES 0x24 /* controller actually returns battery voltage x10 here */
#define BAFANG_OPCODE_READ_UNKNOWN3 0x25
#define BAFANG_OPCODE_READ_MOVING   0x31

/* Write opcodes (display -> controller, no response expected) */
#define BAFANG_OPCODE_WRITE_PAS        0x0b
#define BAFANG_OPCODE_WRITE_MODE       0x0c
#define BAFANG_OPCODE_WRITE_LIGHTS     0x1a
#define BAFANG_OPCODE_WRITE_SPEED_LIM  0x1f

/* "BAS" (Basic Settings) config block, address 0x52 - readable AND writable,
 * unlike everything else in this file. Holds the per-assist-level current/
 * speed limits (the actual controller-side programming, as opposed to
 * BAFANG_OPCODE_WRITE_PAS above which only SELECTS which of these 10
 * pre-programmed levels is currently active) plus a handful of other
 * fields that share the same block on the wire. This is NOT part of the
 * minimal genuine-OEM-display polling loop (bafang_display.c) - it's a
 * separate, on-demand request/response pair issued only when the user
 * opens the "Bafang Assist Level Programming" menu.
 *
 * Byte format cross-checked against a real controller-side "programming"
 * protocol implementation (the EggSPEED app's own writer/
 * commands/config frame parser - field-tested on genuine OEM
 * hardware via its STREET/RACE feature and Basic config screen, same
 * project as the rest of this file's OEM-confirmed facts). Request-type
 * bytes are the SAME family as everything else here (0x11 read / 0x16
 * write) - this is not a separate protocol, just a different address than
 * the single-value opcodes above.
 *
 * Both WRITE and READ RESPONSE encoding below are now byte-exact
 * confirmed - including the READ response envelope ([block_address,
 * length, ...data, LRC], address echoed back - initially guessed wrong as
 * "no echo" before the reference parser was checked) and the wheel/
 * speed-meter decode functions (confirmed by disassembling
 * the reference decoder, since
 * its source was not at hand at the time,
 * resolved 2026-09-17). Not yet verified end-to-end
 * against a REAL controller (only against the confirmed reference
 * implementation) - a first real-hardware read is still the right next
 * step before trusting this in practice, but nothing here is a guess any
 * more. */
#define BAFANG_OPCODE_BAS 0x52
#define BAFANG_BAS_DATA_LEN 24 /* LBP + LC + 10x current% + 10x speed% + wheel + speedmeter */

typedef struct
{
	uint8_t low_battery_protection_v; /* LBP, volts */
	uint8_t current_limit_a;          /* LC, amps */
	uint8_t assist_current_pct[10];   /* per-level current limit, 0-100%, index = assist level 0-9 */
	uint8_t assist_speed_pct[10];     /* per-level speed limit, 0-100%, index = assist level 0-9 */
	uint8_t wheel_diameter_code;      /* same 0-15 coding as g_bafang_settings.wheel_diameter_index / BAFANG_WHEEL_CIRCUMFERENCE_M - encoded/decoded to the wire's own quirky format internally */
	uint8_t speed_meter_model;        /* 0=external, 1=internal, 2=motor phase */
	uint8_t speed_meter_signals;      /* sensor magnet/signal count */
} bafang_bas_block_t;

/* Some read requests are 2 bytes (type+opcode), others 3 (type+opcode+pad).
 * Which is which was reverse engineered from the controller side and must
 * match exactly or the controller will not respond. */
#define BAFANG_READ_REQUEST_LEN_STATUS   2
#define BAFANG_READ_REQUEST_LEN_CURRENT  2
#define BAFANG_READ_REQUEST_LEN_BATTERY  2
#define BAFANG_READ_REQUEST_LEN_SPEED    2
#define BAFANG_READ_REQUEST_LEN_UNKNOWN1 3
#define BAFANG_READ_REQUEST_LEN_RANGE    3
#define BAFANG_READ_REQUEST_LEN_CALORIES 3
#define BAFANG_READ_REQUEST_LEN_UNKNOWN3 3
#define BAFANG_READ_REQUEST_LEN_MOVING   2

typedef enum
{
	BAFANG_ASSIST_0 = 0,
	BAFANG_ASSIST_1,
	BAFANG_ASSIST_2,
	BAFANG_ASSIST_3,
	BAFANG_ASSIST_4,
	BAFANG_ASSIST_5,
	BAFANG_ASSIST_6,
	BAFANG_ASSIST_7,
	BAFANG_ASSIST_8,
	BAFANG_ASSIST_9,
	BAFANG_ASSIST_PUSH /* walk assist */
} bafang_assist_level_t;

typedef enum
{
	BAFANG_MODE_DEFAULT = 0,
	BAFANG_MODE_SPORT
} bafang_operation_mode_t;

/* Holds per-device settings that affect how raw wire values are turned
 * into displayed values. This is where the v1 "kalibracja prądu" feature
 * hooks in: current_calibration_factor is applied once, right after
 * decoding the raw current from the wire, so power (computed elsewhere as
 * voltage * current) is automatically correct too. */
typedef struct
{
	float current_calibration_factor; /* 1.0 = no correction, adjustable via on-device menu */
} bafang_protocol_state_t;

void bafang_protocol_state_init(bafang_protocol_state_t* state);

/* ---- Building outgoing requests ----
 * Each function writes a complete frame into buf (caller-provided, must be
 * at least 5 bytes) and returns the number of bytes written. */

uint8_t bafang_build_read_request(uint8_t opcode, uint8_t* buf);
uint8_t bafang_build_write_pas_request(bafang_assist_level_t level, uint8_t* buf);
uint8_t bafang_build_write_mode_request(bafang_operation_mode_t mode, uint8_t* buf);
uint8_t bafang_build_write_lights_request(bool on, uint8_t* buf);
/* speed_limit_kph: 99 means "unlimited" per OEM manual convention */
uint8_t bafang_build_write_speed_limit_request(uint16_t speed_limit_kph, uint8_t* buf);

/* Writes the whole BAS block in one request (28 bytes: 0x16, 0x52, 24,
 * ...24 data bytes, checksum). To read it, reuse bafang_build_read_request
 * (BAFANG_OPCODE_BAS, buf) directly - no dedicated builder needed, it's
 * just another 2-byte opcode-only read request like the others above. */
uint8_t bafang_build_write_bas_request(const bafang_bas_block_t* block, uint8_t* buf);

/* A BAS write is NOT fire-and-forget - real hardware confirmed (2026-09-18,
 * first real-hardware test of this feature) to reply with a 3-byte ACK:
 * [block_address, status_code, block_address+status_code]. status_code ==
 * BAFANG_BAS_DATA_LEN (24) means the write was accepted as-is; any other
 * value is a specific per-field validation error (source: the EggSPEED
 * app's write response parser - status codes 0=LBP, 1=current limit,
 * 2-21=per-level current/speed pct, 22=wheel, 23=speed signal count, all
 * out of range). Our first implementation didn't know this ACK existed and
 * sent a plain READ_BAS request right after the write, expecting a 27-byte
 * reply - the 3 ACK bytes arrived first and shifted/misaligned everything
 * after them, so bafang_parse_read_bas() always failed the checksum check
 * even though the write itself had genuinely succeeded (confirmed by power-
 * cycling and reading back). */
#define BAFANG_BAS_WRITE_ACK_LEN 3
bool bafang_parse_write_bas_ack(const uint8_t* data, uint8_t len, uint8_t* out_status);

/* ---- Parsing incoming responses ----
 * data/len is the response payload as received after the read request
 * (i.e. NOT including the request_type/opcode bytes we sent - those are
 * not echoed back by the controller). Returns false on checksum mismatch
 * or wrong length. */

bool bafang_parse_read_current(const uint8_t* data, uint8_t len,
	const bafang_protocol_state_t* state, float* out_amps);

bool bafang_parse_read_battery(const uint8_t* data, uint8_t len, uint8_t* out_raw_level);

bool bafang_parse_read_moving(const uint8_t* data, uint8_t len, bool* out_moving);

/* Status/brake register (opcode 0x08). OEM-confirmed: bit 0x02 of the raw
 * status byte is brake-active. Other bits are of unknown meaning on
 * genuine firmware (an independent open source controller implementation
 * treats the whole byte as a generic fault/status code). */
#define BAFANG_STATUS_BRAKE_BIT 0x02

bool bafang_parse_read_status(const uint8_t* data, uint8_t len, uint8_t* out_status_raw);

/* See the BAS block comment above re: the read-response envelope being
 * inferred, not yet hardware-confirmed. */
bool bafang_parse_read_bas(const uint8_t* data, uint8_t len, bafang_bas_block_t* out);

static inline bool bafang_status_brake_active(uint8_t status_raw)
{
	return (status_raw & BAFANG_STATUS_BRAKE_BIT) != 0;
}

/* out_raw_speed is the raw wire value: wheel RPM, NOT km/h. Confirmed
 * conversion (field-tested against a real display, accurate within our
 * measurement error on a live bike): kmh = rpm * wheel_circumference_m * 60 / 1000. */
bool bafang_parse_read_speed(const uint8_t* data, uint8_t len, uint16_t* out_raw_speed);

float bafang_speed_rpm_to_kmh(uint16_t raw_rpm, float wheel_circumference_m);

/* Wheel circumference in meters, indexed by the same wheel-diameter code
 * used in the Basic config block (0=16" .. 12=700C, 13=28", 14=29", 15=30"). */
extern const float BAFANG_WHEEL_CIRCUMFERENCE_M[16];

/*
 * IMPORTANT - CONFIRMED ON REAL HARDWARE (field testing, not a theoretical
 * concern): on some genuine OEM Bafang controllers, polling the "extended"
 * registers below (mode write, controller/motor temperature, status as a
 * separate poll, unknown3, moving) makes the controller STOP RESPONDING
 * to everything, including speed/current - symptom observed was speed,
 * current and distance permanently stuck at zero. These registers are
 * confirmed to only be safe on bbs-fw-flashed controllers.
 *
 * For genuine OEM Bafang firmware, the display polling loop MUST use only
 * this order: INIT_DISPLAY -> set assist -> GET_STATUS(brake) ->
 * GET_BATTERY -> GET_SPEED -> GET_CURRENT -> set lights -> (loop).
 * Do not add GET_TEMP_*, separate GET_STATUS-as-diagnostics, GET_UNKNOWN3,
 * GET_MOVING or set-mode into this loop unless explicitly targeting bbs-fw.
 */
uint8_t bafang_build_init_display_request(uint8_t* buf); /* same bytes as lights-off */

/* Controller repurposes the "calories" opcode to actually return battery
 * voltage x10 (e.g. 365 = 36.5V) - but this is CONFIRMED DEAD (always
 * returns 0) on genuine, unmodified Bafang firmware. Only meaningful when
 * the controller is running bbs-fw. Never use this as the primary voltage
 * source for OEM hardware - use the display's own local ADC measurement
 * on the P+ line instead, falling
 * back to bafang_estimate_voltage() below if that is not yet available. */
bool bafang_parse_read_calories_as_voltage_x10(const uint8_t* data, uint8_t len, uint16_t* out_voltage_x10);

/* Voltage estimated from battery percentage - the fallback OEM firmware
 * itself effectively uses, since it has no real voltage register. Field
 * validated: predicted 58.8V vs 58.6V actual on a live 14S pack. Only use
 * this if a real measurement (local ADC, or bbs-fw's voltage register) is
 * unavailable. */
float bafang_estimate_voltage(uint8_t battery_percent, float low_battery_protection_v, uint8_t cell_count);

/* Inverse of the above, for the menu's "Precise" battery-percentage mode
 * (see bafang_settings.h) - straight linear interpolation between a
 * user-entered min (LVC) and max (full charge) voltage, NOT the LBP/
 * cell_count model bafang_estimate_voltage() uses. Clamped to 0.0-100.0,
 * NOT rounded to an integer - the caller (main.c) runs this through a
 * smoothing filter before display, and rounding here would throw away
 * the sub-percent precision that filter needs to move gradually. Caller
 * must guard max_v > min_v before calling - this does not check
 * (division by zero/negative range is the caller's contract to avoid,
 * same as every other bafang_* math helper in this file). */
float bafang_estimate_percent_from_voltage(float voltage_v, float min_v, float max_v);

#endif
