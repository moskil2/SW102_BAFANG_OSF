#include <string.h>
#include <stdio.h>
#include "gfx.h"
#include "lcd.h"
#include "buttons.h"
#include "main.h"
#include "bafang_protocol.h"
#include "bafang_display.h"
#include "bafang_ui_bridge.h"
#include "bafang_street_race.h"
#include "screen_cfg_utils.h"

/*
 * "Bafang Assist Level Programming" menu (Cockpit menu, between "Screen
 * test" and "Info") - reads and writes the controller's own BAS config
 * block (per-level current%/speed% for all 10 assist levels, plus a few
 * fields that share the block - see bafang_protocol.h's BAFANG_OPCODE_BAS
 * comment for the full byte-format background and what is/isn't hardware-
 * confirmed yet).
 *
 * Design (all agreed with the user before writing this, 2026-09-17):
 * - Entering the screen immediately issues a read (auto-READ); the READ
 *   row lets the user retry manually (e.g. after a timeout).
 * - Every edit happens on a LOCAL working copy only - nothing is sent to
 *   the controller until SAVE is pressed. Editing (and SAVE itself) is
 *   blocked until a read has actually succeeded at least once since this
 *   screen was entered (`valid`) - this is the "don't write an empty/
 *   uninitialized frame" safety requirement.
 * - SAVE does a single write, immediately followed by a fresh read to
 *   verify the value actually took (same read-modify-write-verify pattern
 *   as the EggSPEED app's STREET/RACE switch), then
 *   shows a result overlay for a few seconds before automatically exiting
 *   back up to the parent menu - only on a CONFIRMED-successful save; a
 *   failed save shows an error instead and stays in this screen so the
 *   user can retry, with the edited values (and read-back reality) intact.
 * - Leaving and re-entering this screen always forces a fresh read -
 *   `valid` is never carried over between visits.
 */

/* Row map (STREET/RACE added 2026-10-29, see bafang_street_race.h):
 *  0      READ
 *  1      SAVE
 *  2-31   STREET: (header+current+speed) per level 0-9 - the controller's own table
 *  32     STREET/RACE - opens the "USE RACE" YES/NO choice
 * Only while USE RACE = YES:
 *  33     RACE title (double line above it)
 *  34     SAVE RACE - display flash only, never the controller
 *  35-64  RACE: (header+current+speed) per level 0-9 */
#define BAS_ROW_SR_IDX 32
#define BAS_ROW_RACE_TITLE_IDX 33
#define BAS_ROW_RACE_SAVE_IDX 34
#define BAS_ROW_RACE_FIRST 35
#define BAS_ROW_PITCH 15 /* font_full is 12px tall on screen + 3px gap between lines, on request */
#define BAS_ROW_SELECTED_Y 58 /* matches the rest of the menu's own convention */
#define BAS_SAVE_CONFIRM_MS 3000
/* Minimum time the "reading"/"saving" blinking-underline state stays up,
 * even if the actual operation finishes sooner - on request (2026-09-17):
 * a fast reply (common in the emulator) could otherwise clear it after well
 * under one 500ms blink half-period, so the user never saw it blink at all.
 * 1500ms guarantees at least 3 visible on/off transitions regardless of
 * where in the 500ms cadence the operation happened to start. */
#define BAS_MIN_BUSY_MS 1500
#define BAS_NOTICE_MS 3000

enum bas_row_kind { BAS_ROW_READ, BAS_ROW_SAVE, BAS_ROW_HEADER, BAS_ROW_CURRENT, BAS_ROW_SPEED,
	BAS_ROW_SR, BAS_ROW_RACE_TITLE, BAS_ROW_RACE_SAVE };

/* Same selection-marker icon and left-justify column the rest of the menu
 * uses (screen_cfg_utils.c) - reused here instead of the old fill_rect+
 * DrawInvert highlight box style, which was already replaced everywhere
 * else this session for a real rendering glitch it had. img_icon_menu_marker
 * has external linkage by default (a non-static file-scope const in C) -
 * MENU_TEXT_X0/MENU_MARKER_X are just #defines there, not visible across
 * files, so their values (6/0) are duplicated here as constants instead. */
extern const struct image img_icon_menu_marker;
/* Marker is already flush against the left screen edge (max-left, on
 * request) and the icon itself is 5px wide - so text starting at x=6
 * already lands exactly 1px past it (also on request), no further change
 * needed for either. */
#define BAS_TEXT_X0 6
#define BAS_MARKER_X 0

struct ss_bas_programming {
	bafang_bas_block_t working;
	bool valid;      /* a read has succeeded at least once since entering this screen */
	bool reading;    /* a read (auto or manual) is currently in flight */
	bool saving;     /* a write+verify-read is currently in flight */
	uint32_t busy_since_ms; /* when reading/saving was set - see BAS_MIN_BUSY_MS */
	bool save_ok;    /* result of the last completed save, valid while confirm_active */
	bool confirm_active;
	uint32_t confirm_until_ms;
	int cidx;        /* selected row, 0..bas_row_count()-1 */
	bool editing;    /* adjusting the value of the selected CURRENT/SPEED row */

	/* RACE working copy - edited locally, committed by SAVE RACE only. */
	uint8_t race_current_pct[BAFANG_PAS_LEVELS];
	uint8_t race_speed_pct[BAFANG_PAS_LEVELS];

	bool sr_open;    /* the "USE RACE" YES/NO choice is shown */
	bool sr_value;   /* choice being made there, applied on M */

	/* Three-line text notice (blocked action), dismissed by time or any button. */
	const char *notice[3];
	uint32_t notice_until_ms;
};

static int bas_row_count(void)
{
	return g_street_race.enabled ? BAS_ROW_RACE_FIRST + BAFANG_PAS_LEVELS * 3 : BAS_ROW_SR_IDX + 1;
}

static bool bas_row_is_race(int idx)
{
	return idx >= BAS_ROW_RACE_FIRST;
}

static int bas_level_base(int idx)
{
	return bas_row_is_race(idx) ? BAS_ROW_RACE_FIRST : 2;
}

static enum bas_row_kind bas_row_kind(int idx)
{
	if (idx == 0) return BAS_ROW_READ;
	if (idx == 1) return BAS_ROW_SAVE;
	if (idx == BAS_ROW_SR_IDX) return BAS_ROW_SR;
	if (idx == BAS_ROW_RACE_TITLE_IDX) return BAS_ROW_RACE_TITLE;
	if (idx == BAS_ROW_RACE_SAVE_IDX) return BAS_ROW_RACE_SAVE;
	switch ((idx - bas_level_base(idx)) % 3) {
	case 0: return BAS_ROW_HEADER;
	case 1: return BAS_ROW_CURRENT;
	default: return BAS_ROW_SPEED;
	}
}

static int bas_row_level(int idx)
{
	return (idx - bas_level_base(idx)) / 3;
}

/* The CURRENT/SPEED value a row shows and edits - the RACE working copy for
 * the RACE section, the controller's (read) block for STREET. */
static uint8_t *bas_value_ptr(struct ss_bas_programming *scr, int idx)
{
	int level = bas_row_level(idx);
	bool current = bas_row_kind(idx) == BAS_ROW_CURRENT;
	if (bas_row_is_race(idx))
		return current ? &scr->race_current_pct[level] : &scr->race_speed_pct[level];
	return current ? &scr->working.assist_current_pct[level] : &scr->working.assist_speed_pct[level];
}

static void bas_show_notice(struct ss_bas_programming *scr, const char *l1, const char *l2, const char *l3)
{
	scr->notice[0] = l1;
	scr->notice[1] = l2;
	scr->notice[2] = l3;
	scr->notice_until_ms = get_time_base_counter_1ms() + BAS_NOTICE_MS;
}

static bool bas_race_active(void)
{
	return g_street_race.enabled && g_street_race.active == BAFANG_PROFILE_RACE;
}

/* While RACE is on the controller, a plain READ returns RACE's table - show
 * the stored STREET table in the STREET section instead (view-only then). */
static void bas_show_street_from_storage(struct ss_bas_programming *scr)
{
	if (bas_race_active() && g_street_race.street_valid) {
		memcpy(scr->working.assist_current_pct, g_street_race.street_current_pct, BAFANG_PAS_LEVELS);
		memcpy(scr->working.assist_speed_pct, g_street_race.street_speed_pct, BAFANG_PAS_LEVELS);
	}
}

#define BAS_VALUE_X 64 /* CURRENT/SPEED "NN%" right-aligned flush to the screen edge, on request */

/* Splits a row into its label (left- or center-aligned, drawn by the
 * caller) and, for CURRENT/SPEED rows only, a separate right-aligned
 * "NN%" value - on request, so the % always lines up on the right instead
 * of trailing directly after the label. out_value is left empty ("") for
 * row kinds that don't have one. */
static void bas_format_row(struct ss_bas_programming *scr, int idx, char *out_label, char *out_value)
{
	int level = bas_row_level(idx);
	out_value[0] = '\0';
	switch (bas_row_kind(idx)) {
	case BAS_ROW_READ:
		sprintf(out_label, "READ");
		break;
	case BAS_ROW_SAVE:
		sprintf(out_label, "SAVE");
		break;
	case BAS_ROW_HEADER:
		sprintf(out_label, "PAS %d", level);
		break;
	case BAS_ROW_CURRENT:
		sprintf(out_label, "CURRENT");
		sprintf(out_value, "%d%%", *bas_value_ptr(scr, idx));
		break;
	case BAS_ROW_SPEED:
		sprintf(out_label, "SPEED");
		sprintf(out_value, "%d%%", *bas_value_ptr(scr, idx));
		break;
	case BAS_ROW_SR:
		sprintf(out_label, "STREET/RACE");
		break;
	case BAS_ROW_RACE_TITLE:
		sprintf(out_label, "RACE");
		break;
	case BAS_ROW_RACE_SAVE:
		sprintf(out_label, "SAVE RACE");
		break;
	}
}

/* A separator line follows the SAVE row (closing off the READ/SAVE block)
 * and every level's SPEED row (closing off that PAS block) - matching the
 * user's hand-drawn mockup exactly. SAVE RACE closes its block the same
 * way; the double line above the RACE title is drawn separately (see
 * draw_bas_programming()). */
static bool bas_separator_after(int idx)
{
	if (idx == 1 || idx == BAS_ROW_RACE_SAVE_IDX)
		return true;
	return bas_row_kind(idx) == BAS_ROW_SPEED;
}

static bool bas_row_centered(enum bas_row_kind kind)
{
	return kind == BAS_ROW_HEADER || kind == BAS_ROW_RACE_TITLE;
}

/* No clear_all()/lcd_refresh() here - screen_cfg.c's cfg_idle() already
 * does clear_all() before sstack_idle() (which reaches this via bas_idle())
 * and lcd_refresh() after, same as every other screen in this menu
 * framework (see cfg_edit_idle() for the same convention). Doing it again
 * here would just be redundant work - wasted SPI time on real hardware,
 * even though it's a harmless no-op in the emulator. */
static void draw_bas_programming(struct ss_bas_programming *scr)
{
	if (scr->confirm_active) {
		font_text(&font_full, 32, 50, scr->save_ok ? "Saved!" : "Save", AlignCenter);
		if (!scr->save_ok)
			font_text(&font_full, 32, 66, "failed", AlignCenter);
		return;
	}

	if (scr->notice[0]) {
		for (int i = 0; i < 3; i++)
			if (scr->notice[i])
				font_text(&font_full, 32, 42 + i * 16, scr->notice[i], AlignCenter);
		return;
	}

	if (scr->sr_open) {
		font_text(&font_full, 32, 42, "USE RACE", AlignCenter);
		const char *value = scr->sr_value ? "YES" : "NO";
		font_text(&font_full, 32, 66, value, AlignCenter);
		int w = font_length(&font_full, value);
		if ((get_time_base_counter_1ms() / 500) % 2 == 0)
			draw_hline(32 - w / 2, 32 + w / 2, 66 + BAS_ROW_PITCH / 2 + 2);
		return;
	}

	/* Blinking underline (500ms, same cadence as the walk-assist icon
	 * blink in screen_main.c) under whatever is currently "busy" or being
	 * edited - READ while reading, SAVE while saving, the value being
	 * adjusted while editing - instead of a separate status text line. */
	bool blink_on = (get_time_base_counter_1ms() / 500) % 2 == 0;

	char label[16], value[8];
	for (int i = -3; i <= 4; i++) {
		int idx = scr->cidx + i;
		if (idx < 0 || idx >= bas_row_count())
			continue;

		int y = BAS_ROW_SELECTED_Y + i * BAS_ROW_PITCH;
		if (y < 0 || y > 127)
			continue;

		bool selected = (idx == scr->cidx);
		enum bas_row_kind kind = bas_row_kind(idx);
		/* RACE rows come from the display's own flash, not from a read -
		 * never greyed out by a failed read. */
		bool disabled;
		if (kind == BAS_ROW_SR)
			disabled = !scr->valid && !g_street_race.race_valid; /* see the M_CLICK handler */
		else
			disabled = !scr->valid && !bas_row_is_race(idx) && kind != BAS_ROW_READ &&
				kind != BAS_ROW_HEADER && kind != BAS_ROW_RACE_TITLE && kind != BAS_ROW_RACE_SAVE;

		if (selected)
			img_draw(&img_icon_menu_marker, BAS_MARKER_X, y);

		if (!disabled || selected) {
			bas_format_row(scr, idx, label, value);
			/* Marker and text both at plain y - the marker's own +1 (like
			 * the rest of the menu, screen_cfg_utils.c) turned out to be
			 * the actual bug (arrow 1px too low everywhere), not the text
			 * being too high - fixed at the source, no offset needed on
			 * either any more. PAS N headers centered on the screen,
			 * everything else left-aligned like the main menu's own
			 * options - on request. */
			if (bas_row_centered(kind)) {
				font_text(&font_full, 32, y, label, AlignCenter);
			} else {
				int label_end = font_text(&font_full, BAS_TEXT_X0, y, label, AlignLeft);
				int underline_y = y + BAS_ROW_PITCH / 2 + 2; /* kept relative to text after marker/text offset fix */
				if (blink_on && underline_y <= 127 &&
					((idx == BAS_ROW_READ && scr->reading) || (idx == BAS_ROW_SAVE && scr->saving)))
					draw_hline(BAS_TEXT_X0, label_end, underline_y);
			}
			if (value[0]) {
				font_text(&font_full, BAS_VALUE_X, y, value, AlignRight);
				int underline_y = y + BAS_ROW_PITCH / 2 + 2; /* kept relative to text after marker/text offset fix */
				if (blink_on && selected && scr->editing && underline_y <= 127) {
					int value_w = font_length(&font_full, value);
					draw_hline(BAS_VALUE_X - value_w, BAS_VALUE_X, underline_y);
				}
			}
		}

		/* nudged 2px higher on request. draw_hline() does NOT clamp its y
		 * to the framebuffer's valid 0-127 range the way fill_rect()/
		 * img_draw_clip() do - every other call site in the codebase only
		 * ever passes hardcoded, pre-validated constants, so this was
		 * never a problem before. This is the first dynamically-computed
		 * y passed to it: for the last visible row (i==4) it landed at
		 * 129, one row past the framebuffer's end - an out-of-bounds
		 * write that corrupted adjacent memory and crashed the emulator
		 * on the very first render (real bug, found 2026-09-17). Guard
		 * explicitly instead of relying on draw_hline() to protect
		 * itself. */
		if (bas_separator_after(idx)) {
			int sep_y = y + BAS_ROW_PITCH - 4; /* kept relative to text after marker/text offset fix */
			if (sep_y >= 0 && sep_y <= 127)
				draw_hline(0, 64, sep_y);
		}

		/* Double line across the screen above the RACE title (the STREET
		 * section's last separator stays where it is, above STREET/RACE). */
		if (kind == BAS_ROW_RACE_TITLE) {
			int sep_y = y - 4;
			if (sep_y >= 0 && sep_y <= 127)
				draw_hline(0, 64, sep_y);
			if (sep_y - 2 >= 0 && sep_y - 2 <= 127)
				draw_hline(0, 64, sep_y - 2);
		}
	}
}

static void bas_enter(void *_it, bool pop)
{
	struct ss_bas_programming *scr = _it;
	if (pop)
		return;

	memset(scr, 0, sizeof(*scr));
	memcpy(scr->race_current_pct, g_street_race.race_current_pct, BAFANG_PAS_LEVELS);
	memcpy(scr->race_speed_pct, g_street_race.race_speed_pct, BAFANG_PAS_LEVELS);
	bafang_display_request_bas_read(bafang_ui_bridge_get_display());
	scr->reading = true;
	scr->busy_since_ms = get_time_base_counter_1ms();
}

static void bas_idle(void *_it)
{
	struct ss_bas_programming *scr = _it;
	bafang_display_t *d = bafang_ui_bridge_get_display();
	bool min_busy_elapsed = (int32_t)(get_time_base_counter_1ms() - (scr->busy_since_ms + BAS_MIN_BUSY_MS)) >= 0;

	if (scr->reading && !bafang_display_bas_pending(d) && min_busy_elapsed) {
		scr->reading = false;
		scr->valid = bafang_display_bas_result_ok(d);
		if (scr->valid) {
			scr->working = *bafang_display_get_bas_block(d);
			bas_show_street_from_storage(scr);
		}
	}

	if (scr->notice[0] && (int32_t)(get_time_base_counter_1ms() - scr->notice_until_ms) >= 0)
		scr->notice[0] = NULL;

	if (scr->saving && !bafang_display_bas_pending(d) && min_busy_elapsed) {
		scr->saving = false;
		/* bafang_display_bas_result_ok() now directly reflects the
		 * controller's own write-ACK (accepted/rejected) - a real hardware
		 * finding, 2026-09-18: writes aren't fire-and-forget, and there's no
		 * need to re-read and compare, the ACK already confirms the block
		 * was stored exactly as sent. See bafang_display_request_bas_save(). */
		scr->save_ok = bafang_display_bas_result_ok(d);
		scr->confirm_active = true;
		scr->confirm_until_ms = get_time_base_counter_1ms() + BAS_SAVE_CONFIRM_MS;
	}

	if (scr->confirm_active && scr->save_ok &&
		(int32_t)(get_time_base_counter_1ms() - scr->confirm_until_ms) >= 0) {
		sstack_pop();
		return;
	}

	draw_bas_programming(scr);
}

static void bas_button(void *_it, int but, int extra)
{
	(void)extra;
	struct ss_bas_programming *scr = _it;
	bafang_display_t *d = bafang_ui_bridge_get_display();

	if (but & M_LONG_CLICK) {
		sstack_pop();
		return;
	}

	if (scr->reading || scr->saving)
		return; /* ignore other input while an operation is in flight */

	if (scr->notice[0]) {
		if (but & (ONOFF_CLICK | UP_CLICK | DOWN_CLICK | M_CLICK))
			scr->notice[0] = NULL;
		return;
	}

	if (scr->sr_open) {
		if (but & (UP_CLICK | DOWN_CLICK))
			scr->sr_value = !scr->sr_value;
		if (but & ONOFF_CLICK)
			scr->sr_open = false;
		if (but & M_CLICK) {
			scr->sr_open = false;
			if (scr->sr_value == g_street_race.enabled)
				return;
			if (!scr->sr_value && bas_race_active()) {
				/* Turning the feature off would leave RACE's table on the
				 * controller - switching back is the rider's own decision. */
				bas_show_notice(scr, "SWITCH TO", "STREET", "FIRST");
				return;
			}
			if (scr->sr_value && !g_street_race.race_valid) {
				/* First enable: RACE starts as a copy of STREET (never
				 * empty/random values) - needs a successful read. */
				memcpy(g_street_race.race_current_pct, scr->working.assist_current_pct, BAFANG_PAS_LEVELS);
				memcpy(g_street_race.race_speed_pct, scr->working.assist_speed_pct, BAFANG_PAS_LEVELS);
				g_street_race.race_valid = true;
				memcpy(scr->race_current_pct, g_street_race.race_current_pct, BAFANG_PAS_LEVELS);
				memcpy(scr->race_speed_pct, g_street_race.race_speed_pct, BAFANG_PAS_LEVELS);
			}
			g_street_race.enabled = scr->sr_value;
			bafang_street_race_save();
		}
		return;
	}

	if (scr->confirm_active) {
		/* Only a failed save waits here for the user (a successful one
		 * auto-exits from bas_idle() above) - any button dismisses it and
		 * returns to the row list, values and read-back state intact. */
		if (but & (ONOFF_CLICK | UP_CLICK | DOWN_CLICK | M_CLICK))
			scr->confirm_active = false;
		return;
	}

	if (scr->editing) {
		uint8_t *value = bas_value_ptr(scr, scr->cidx);

		if (but & UP_CLICK) {
			if (*value < 100) (*value)++;
		}
		if (but & DOWN_CLICK) {
			if (*value > 0) (*value)--;
		}
		if (but & (M_CLICK | ONOFF_CLICK)) {
			scr->editing = false;
		}
		return;
	}

	if (but & ONOFF_CLICK) {
		sstack_pop();
		return;
	}

	if (but & UP_CLICK) {
		if (scr->cidx > 0) scr->cidx--;
	}
	if (but & DOWN_CLICK) {
		if (scr->cidx < bas_row_count() - 1) scr->cidx++;
	}

	if (but & M_CLICK) {
		enum bas_row_kind kind = bas_row_kind(scr->cidx);
		bool is_edit = kind == BAS_ROW_SAVE || kind == BAS_ROW_RACE_SAVE ||
			kind == BAS_ROW_CURRENT || kind == BAS_ROW_SPEED;
		bool usable = scr->valid || bas_row_is_race(scr->cidx) || kind == BAS_ROW_RACE_SAVE;

		/* While RACE is on the controller both tables are view-only. */
		if (is_edit && usable && bas_race_active()) {
			bas_show_notice(scr, "BEFORE EDIT", "SWITCH TO", "STREET");
			return;
		}

		switch (kind) {
		case BAS_ROW_READ:
			bafang_display_request_bas_read(d);
			scr->reading = true;
			scr->busy_since_ms = get_time_base_counter_1ms();
			break;
		case BAS_ROW_SAVE:
			if (scr->valid) {
				bafang_display_request_bas_save(d, &scr->working);
				scr->saving = true;
				scr->busy_since_ms = get_time_base_counter_1ms();
			}
			break;
		case BAS_ROW_CURRENT:
		case BAS_ROW_SPEED:
			if (usable)
				scr->editing = true;
			break;
		case BAS_ROW_SR:
			/* The first enable copies STREET, so it needs a good read;
			 * after that the choice works without one. */
			if (scr->valid || g_street_race.race_valid) {
				scr->sr_open = true;
				scr->sr_value = g_street_race.enabled;
			}
			break;
		case BAS_ROW_RACE_SAVE:
			memcpy(g_street_race.race_current_pct, scr->race_current_pct, BAFANG_PAS_LEVELS);
			memcpy(g_street_race.race_speed_pct, scr->race_speed_pct, BAFANG_PAS_LEVELS);
			bafang_street_race_save();
			scr->save_ok = true;
			scr->confirm_active = true;
			scr->confirm_until_ms = get_time_base_counter_1ms() + BAS_SAVE_CONFIRM_MS;
			break;
		case BAS_ROW_HEADER:
		case BAS_ROW_RACE_TITLE:
			break;
		}
	}
}

static const struct stack_class bas_programming_class = {
	sizeof(struct ss_bas_programming),
	.enter = bas_enter,
	.idle = bas_idle,
	.button = bas_button,
};

void do_open_bas_programming(const struct configtree_t *ign)
{
	(void)ign;
	sstack_alloc(&bas_programming_class);
	sstack_push();
}
