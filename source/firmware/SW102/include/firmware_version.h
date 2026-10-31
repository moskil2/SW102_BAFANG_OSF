#ifndef _FIRMWARE_VERSION_H_
#define _FIRMWARE_VERSION_H_

/*
 * Single source of truth for the firmware version string, shown on the
 * boot screen and in the menu's Info submenu. Naming: SW102_BAF_X.Y.Z.
 * Bump on every build that gets flashed to real hardware - starts at
 * 0.0.1, each number 0-9 then carries into the next (0.0.9 -> 0.1.0,
 * 0.9.9 -> 1.0.0). Every version that gets flashed also gets saved as its
 * own file (not overwritten).
 */
#define FIRMWARE_VERSION_STRING "SW102_BAF_0.1.5"

/* Same info, split for the two-line boot screen display - keep these two
 * in sync with FIRMWARE_VERSION_STRING above on every bump. */
#define FIRMWARE_NAME_STRING "SW102_BAF"
#define FIRMWARE_VERSION_NUMBER_STRING "V0.1.5"

#endif
