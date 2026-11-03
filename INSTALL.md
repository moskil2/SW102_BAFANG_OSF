# Installing the firmware (ST-Link)

This is the simple, end-user version of the flashing procedure - tested on real hardware. No build tools needed if you're using a pre-built `.hex` release file.

## What you need

- An ST-Link V2 programmer (a cheap clone is fine, ~$5) - e.g. [search on Amazon](https://www.amazon.com/s?k=ST-Link+V2+programmer)
- A Windows PC
- **OpenOCD** - download [`xpack-openocd-0.12.0-7-win32-x64.zip`](https://github.com/xpack-dev-tools/openocd-xpack/releases/download/v0.12.0-7/xpack-openocd-0.12.0-7-win32-x64.zip) and unzip it to `C:\OpenOCD`. The zip contains its own folder, so OpenOCD ends up in `C:\OpenOCD\xpack-openocd-0.12.0-7\bin`
- The **ST-Link driver** (see step 1 below)
- The firmware `.hex` file: [`SW102_BAF_0.1.9.hex`](https://github.com/moskil2/SW102_BAFANG_OSF/releases/download/0.1.9/SW102_BAF_0.1.9.hex) - download it, create the folder `C:\SW102` and move the downloaded `.hex` there

This single `.hex` file is a complete, ready-to-flash image - it already contains the bootloader, the Nordic SoftDevice (BLE stack), and the application, merged together at build time. You don't need to download anything else from any other repository.

## 1. Install the ST-Link driver

Plugging the ST-Link into USB alone is not enough - Windows needs the driver, or OpenOCD will fail with `Error: open failed`.

1. Download **[STSW-LINK009](https://www.st.com/en/development-tools/stsw-link009.html)** from ST's website (ST asks you to log in or enter an e-mail address before downloading) and unzip the downloaded file (e.g. into your Downloads folder).
2. Open the unzipped folder, find `stlink_winusb_install.bat`, right-click it, and choose **"Run as administrator"** from the menu.
3. Unplug and replug the ST-Link.

## 2. Open the case and connect the SWD pins

Open the SW102 display to expose the 4 programming pads: **GND, CLK (SWCLK), DIO (SWDIO), 3V3**.

| SW102 pad | ST-Link V2 |
|---|---|
| GND | GND |
| 3V3 | 3.3V |
| CLK | SWCLK |
| DIO | SWDIO |

You can power the display straight from the ST-Link's 3.3V pin for flashing - no battery or controller cable needed.

## 3. Flash the firmware

1. Open a Command Prompt: press **Win+R**, type `cmd`, press Enter.
2. Go into OpenOCD's `bin` folder (if you unzipped it somewhere else than `C:\OpenOCD`, adjust the path):
   ```
   cd C:\OpenOCD\xpack-openocd-0.12.0-7\bin
   ```
3. Run the erase command (paste it exactly, then press Enter):
   ```
   openocd.exe -f interface/stlink.cfg -f target/nordic/nrf51.cfg -c "init; reset init; nrf51 mass_erase; shutdown"
   ```
4. Run the write-and-verify command (paste it exactly, then press Enter). If you saved the `.hex` somewhere other than `C:\SW102`, change the path, and write it with `/`, not `\` (OpenOCD drops `\`, so `C:\SW102\...` would not be found):
   ```
   openocd.exe -f interface/stlink.cfg -f target/nordic/nrf51.cfg -c "init; reset init; flash write_image C:/SW102/SW102_BAF_0.1.9.hex; verify_image C:/SW102/SW102_BAF_0.1.9.hex; reset halt; resume; shutdown"
   ```

These must be two separate commands, run one after the other - see Troubleshooting below for why. If `verify_image` reports no errors, the flash succeeded.

## 4. Check it worked

Disconnect the ST-Link, connect normal power (battery or the controller cable), and press and hold the power button. You should see the boot screen with the firmware name and version.

## Troubleshooting

- **`Error: open failed`** - the ST-Link driver isn't installed, or the cable/USB connection is loose. Recheck step 1, unplug/replug.
- **`Error: init mode failed` / no target detected** - almost always a bad physical connection on the CLK/DIO pads. Double-check the wires are making solid contact.
- **Why two separate commands?** - Running erase and write in the same OpenOCD session fails with `error writing to flash`. The chip needs a reset in between, which is why the erase and the write-and-verify are two separate commands rather than one long one.

## Updating (Bluetooth)

Once the bootloader is installed (first flash via ST-Link), later firmware updates can be done wirelessly - no need to open the case again.

[![Watch the Bluetooth update video](https://img.youtube.com/vi/GyanRPCOuG8/maxresdefault.jpg)](https://www.youtube.com/watch?v=GyanRPCOuG8)

### What you need

- The **nRF Connect** app by Nordic Semiconductor: [Android (Google Play)](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp), [iOS (App Store)](https://apps.apple.com/app/nrf-connect-for-mobile/id1054362403)
- The firmware update package: [`SW102_BAF_FW_0.1.9.zip`](https://github.com/moskil2/SW102_BAFANG_OSF/releases/download/0.1.9/SW102_BAF_FW_0.1.9.zip) - open the link on your phone and download it there

### Before updating

If you use STREET/RACE, switch to STREET first. A Bluetooth update is meant to keep the saved settings, but a full ST-Link flash erases them. If RACE were active at that point, the controller would keep the RACE table and the STREET table, stored only on the display, would be lost. With STREET active nothing important can be lost: at worst the RACE table, which starts again as a copy of STREET when you turn USE RACE back on.

### Steps

1. With the display powered on, hold **M + PWR together for at least 8 seconds**, until the screen goes dark - this confirms it's now in bootloader DFU mode.
2. Open **nRF Connect** on your phone, scan for Bluetooth devices, and connect to **"SW102_DFU"**.
3. Start a DFU update in the app and select the `.zip` file (don't unzip it first).
4. Wait for the upload to finish.
5. Power-cycle the display and turn it back on normally.

### Troubleshooting

- **Boots back into DFU mode instead of the app** - hold the power button longer (up to 10 seconds) on the first boot after an update; this is a known quirk of the bootloader.
