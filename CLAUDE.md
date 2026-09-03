# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A single-sketch PlatformIO/Arduino project for ESP32: button-triggered HTTPS POST client with a
WiFiManager config portal and HTTP OTA update support. See [README.md](README.md) for the pin
mapping and the four button-triggered behaviors. All logic lives in
[src/main.cpp](src/main.cpp).

## Commands

```
pio run                        # build
pio run -t upload              # flash to a connected ESP32
pio device monitor -b 115200   # view serial output
```

No test suite or linter is configured (`test/` is the empty PlatformIO scaffold directory).
`platformio.ini` does not declare `lib_deps` — required libraries (WiFiManager, ArduinoJson v6,
SimpleTimer, LiquidCrystal_I2C) must already be present in `lib/` for a build to succeed.

## Notes for changes

- Runtime configuration (WiFi credentials, HTTP host/path/body, auth token) is read from and
  written to `/config.json` on SPIFFS via `ReadConfigJson()`/`UpdateJsonString()` — don't hardcode
  new endpoint or credential values into `main.cpp`; add them as WiFiManager custom parameters
  instead, the way `custom_HTTP_host`/`custom_Auth_token`/etc. already work.
- `root_ca` is a hardcoded root CA for whatever HTTPS host this was last pointed at — it must be
  swapped if the target host's certificate authority changes, or `WiFiClientSecure` TLS
  verification will fail.
- The config portal only applies changed HTTP fields when `Security_key` matches the hardcoded
  default — that's intentional (prevents someone hitting the open config AP from silently
  redirecting API calls), so don't remove that check when touching `HandleWifimanager()`.
