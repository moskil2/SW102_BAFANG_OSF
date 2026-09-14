# Installing the firmware (ST-Link)

This is the simple, end-user version of the flashing procedure - tested on real hardware. No build tools needed if you're using a pre-built `.hex` release file.

## What you need

- An ST-Link V2 programmer (a cheap clone is fine, ~$5) - e.g. [search on Amazon](https://www.amazon.com/s?k=ST-Link+V2+programmer)
- 4 jumper wires (or pogo pins for a solderless connection)
- A Windows PC
- **OpenOCD** - download the Windows build from the [xPack OpenOCD releases page](https://github.com/xpack-dev-tools/openocd-xpack/releases) (the `...win32-x64.zip` asset) and unzip it anywhere, e.g. `C:\OpenOCD`
- The **ST-Link driver** (see step 1 below)
- The firmware `.hex` file from this repo's Releases (e.g. `SW102_BAF_X.Y.Z.hex`) - download it and note where you saved it, e.g. `C:\SW102\`

This single `.hex` file is a complete, ready-to-flash image - it already contains the bootloader, the Nordic SoftDevice (BLE stack), and the application, merged together at build time. You don't need to download anything else from any other repository.

## 1. Install the ST-Link driver

Plugging the ST-Link into USB alone is not enough - Windows needs the driver, or OpenOCD will fail with `Error: open failed`.

1. Download **[STSW-LINK009](https://www.st.com/en/development-tools/stsw-link009.html)** from ST's website and unzip the downloaded file (e.g. into your Downloads folder).
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

1. Open a Command Prompt (press the Windows key, type `cmd`, press Enter).
2. Go into OpenOCD's `bin` folder - adjust the path to wherever you unzipped it in step "What you need":
   ```
   cd C:\OpenOCD\bin
   ```
3. Run the erase command (paste it exactly, then press Enter):
   ```
   openocd.exe -f interface/stlink.cfg -f target/nordic/nrf51.cfg -c "init; reset init; nrf51 mass_erase; shutdown"
   ```
4. Run the write-and-verify command, using the full path to wherever you saved the `.hex` file:
   ```
   openocd.exe -f interface/stlink.cfg -f target/nordic/nrf51.cfg -c "init; reset init; flash write_image C:\SW102\SW102_BAF_X.Y.Z.hex; verify_image C:\SW102\SW102_BAF_X.Y.Z.hex; reset halt; resume; shutdown"
   ```

These must be two separate commands, run one after the other - see Troubleshooting below for why. If `verify_image` reports no errors, the flash succeeded.

## 4. Check it worked

Disconnect the ST-Link, connect normal power (battery or the controller cable), and press the power button. You should see the boot screen with the firmware name and version.

## Troubleshooting

- **`Error: open failed`** - the ST-Link driver isn't installed, or the cable/USB connection is loose. Recheck step 1, unplug/replug.
- **`Error: init mode failed` / no target detected** - almost always a bad physical connection on the CLK/DIO pads. Double-check the wires are making solid contact.
- **Why two separate commands?** - Running erase and write in the same OpenOCD session fails with `error writing to flash`. The chip needs a reset in between, which is why the erase and the write-and-verify are two separate commands rather than one long one.
