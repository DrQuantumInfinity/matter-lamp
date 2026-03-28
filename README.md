# Matter Lamp

ESP32 Matter lighting device with tunable white + RGB.

## Hardware

- ESP-WROOM-32
- 3 white LEDs on GPIOs 17, 18, 19 (color temperatures: 2600K, 3000K, 5000K)
- 1 WS2812 RGB LED on GPIO 5 (via RMT)

## How It Works

The `LEDCluster` class manages both the white and RGB LEDs:

- **Color temperature mode**: Blends the 3 white LEDs using `multiLerp` interpolation across their color temperatures. Setting a target temperature (e.g. 3500K) proportionally mixes the nearest LEDs.
- **Color mode**: Switches to the WS2812 RGB LED for HSV color control.
- **Rainbow mode**: Setting hue to 21 triggers a cycling rainbow effect on the RGB LED.

Matter clusters supported: OnOff, LevelControl, ColorControl (hue/saturation + color temperature).

## Building

This project is built as part of the [connectedhomeip](https://github.com/project-chip/connectedhomeip) SDK (tested with ESP-IDF 5.1).

1. Symlink `main/` into the SDK's lighting-app example:
   ```sh
   cd connectedhomeip/examples/lighting-app/esp32
   mv main main.orig
   ln -s /path/to/matter-lamp/main main
   ```

2. Build:
   ```sh
   source ~/esp/esp-idf/export.sh
   source third_party/connectedhomeip/scripts/activate.sh
   idf.py set-target esp32
   idf.py build
   ```

**Note**: If you get `repository_info` errors from managed components, remove `espressif/esp_insights` and related dependencies from `config/esp32/components/chip/idf_component.yml` — they're not needed for this project and the newer component versions are incompatible with ESP-IDF 5.1.

## Flashing

Erase flash first (clears old WiFi/commissioning state):
```sh
python3 -m esptool -p /dev/ttyUSB0 erase_flash
```

Then flash:
```sh
python3 -m esptool -p /dev/ttyUSB0 -b 460800 --chip esp32 write-flash \
  0x1000 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x1d000 build/ota_data_initial.bin \
  0x20000 build/chip-lighting-app.bin
```

## Pairing

QR code for commissioning (default credentials):
https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3AY.K9042C00KA0648G00

Manual pairing code: `34970112332`

## Serial Monitor

```sh
python3 -m serial.tools.miniterm /dev/ttyUSB0 115200
```

Exit with `Ctrl+]`.
