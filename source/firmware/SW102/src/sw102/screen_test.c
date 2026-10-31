#include "gfx.h"
#include "ui.h"
#include "lcd.h"

/* Diagnostic "Screen test" screen (menu item, see screen_cfg_tree.c) - fills
 * the whole display white to check for dead pixels/bad sectors independent
 * of any specific font/icon rendering. Any button press returns to the menu
 * root (screen_cfg's .enter does a clean sstack_reset()). */

extern const struct screen screen_cfg;

static void test_enter()
{
	fill_rect(0, 0, 64, 128, true);
	lcd_refresh();
}

static void test_button(int but)
{
	showScreen(&screen_cfg);
}

const struct screen screen_test = {
	.enter = test_enter,
	.button = test_button,
};
