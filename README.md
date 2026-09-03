# Smart-API-Button

An ESP32 sketch that turns physical push buttons into a triggerable HTTPS POST client — press a
button, it calls a configured API endpoint over TLS and shows the result on a 16x2 I2C LCD.
Originally built to trigger an ERPNext/Frappe-backed API action for a small business (see
`Client_name`/`HTTP_host` defaults in [src/main.cpp](src/main.cpp)), but the HTTP host, path,
body, and auth token are all reconfigurable at runtime — nothing about the API target is baked in
beyond a placeholder default.

## How it works

- **Two buttons held together** (GPIO22 + GPIO23, both LOW for >1s) trigger `POSTapiCall()`, which
  opens a `WiFiClientSecure` connection to the configured host, sends a POST request with the
  configured body/auth header, and shows success/failure on the LCD.
- **One button** (GPIO18) opens a [WiFiManager](https://github.com/tzapu/WiFiManager) captive
  portal for configuring WiFi credentials plus the HTTP host/URL/body/auth token — gated behind a
  security key so the portal can't silently overwrite the API target.
- **One button** (GPIO19) checks a version file over HTTP and, if newer, pulls a new firmware
  binary via `HTTPUpdate` (HTTP OTA).
- Configuration is persisted to SPIFFS (`/config.json`) so it survives reboots; an indicator LED
  (GPIO21) blinks while there's no internet (checked by pinging google.com every 30s) and stays
  solid once connectivity is confirmed.
- TLS to the configured host uses a hardcoded root CA certificate — this must be replaced if
  pointing at a different HTTPS target than the one it was built for.

## Hardware

| Signal | ESP32 GPIO |
|---|---|
| WiFiManager config-portal trigger | GPIO18 |
| API call button 1 | GPIO22 |
| API call button 2 | GPIO23 |
| HTTP OTA update trigger | GPIO19 |
| Internet-status indicator LED | GPIO21 |
| I2C LCD data (SDA) | GPIO16 |
| I2C LCD clock (SCL) | GPIO17 |

All buttons are read with `INPUT_PULLUP`, so they trigger on a LOW (button-to-ground) press.

## Build and flash

This is a [PlatformIO](https://platformio.org/) project targeting a generic `esp32dev` board.
[platformio.ini](platformio.ini) doesn't declare `lib_deps` — WiFiManager, ArduinoJson (v6),
SimpleTimer, and LiquidCrystal_I2C need to be present in `lib/` or added to `lib_deps` before it
will build; see the library links in the top-of-file comment in `main.cpp` for sources.

```
pio run                        # build
pio run -t upload              # flash
pio device monitor -b 115200   # view serial output
```

## Configuration

On first boot with no saved config, hold the WiFiManager trigger button to open the config portal
(default portal password is in `Config_Portal_Password` in `main.cpp` — change it before deploying)
and set: WiFi SSID/password, the target HTTPS host/path/body, an auth token, and the security key
that gates whether the portal's HTTP fields get applied.

> An IFTTT webhook key and a placeholder API auth token were previously hardcoded in this file's
> history and have been scrubbed from it. If reusing this code, supply your own values via the
> config portal rather than hardcoding new ones into source.
