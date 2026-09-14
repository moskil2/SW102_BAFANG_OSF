# Installing the firmware (ST-Link)

This is the simple, end-user version of the flashing procedure - tested on real hardware. No build tools needed if you're using a pre-built `.hex` release file.

## What you need

- An ST-Link V2 programmer (a cheap clone is fine, ~$5)
- 4 jumper wires (or pogo pins for a solderless connection)
- A PC with [OpenOCD](https://openocd.org/) and the ST-Link driver installed
- The firmware `.hex` file (e.g. `SW102_BAF_X.Y.Z.hex`)

## 1. Install the ST-Link driver

Plugging the ST-Link into USB alone is not enough - Windows needs the driver, or OpenOCD will fail with `Error: open failed`.

1. Download **STSW-LINK009** from ST's website.
2. Unzip it and run `stlink_winusb_install.bat` as Administrator.
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

Open a terminal where OpenOCD is installed and run these two commands, one after the other (they must be separate - see Troubleshooting below).

**Erase:**
```
openocd -f interface/stlink.cfg -f target/nrf51.cfg -c "init; reset init; nrf51 mass_erase; shutdown"
```

**Write and verify** (replace the filename with your actual `.hex` file):
```
openocd -f interface/stlink.cfg -f target/nrf51.cfg -c "init; reset init; flash write_image SW102_BAF_X.Y.Z.hex; verify_image SW102_BAF_X.Y.Z.hex; reset halt; resume; shutdown"
```

If `verify_image` reports no errors, the flash succeeded.

## 4. Check it worked

Disconnect the ST-Link, connect normal power (battery or the controller cable), and press the power button. You should see the boot screen with the firmware name and version.

## Troubleshooting

- **`Error: open failed`** - the ST-Link driver isn't installed, or the cable/USB connection is loose. Recheck step 1, unplug/replug.
- **`Error: init mode failed` / no target detected** - almost always a bad physical connection on the CLK/DIO pads. Double-check the wires are making solid contact.
