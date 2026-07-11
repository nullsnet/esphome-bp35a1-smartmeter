# AGENTS.md

## Build

```bash
docker run --rm \
  -v $(pwd):/config \
  -v $(pwd)/.cache/platformio:/root/.platformio \
  -v $(pwd)/.cache/esphome:/config/.esphome \
  ghcr.io/esphome/esphome:latest compile /config/esphome/smart_meter.yaml
```

Mount `.cache/platformio` and `.cache/esphome` to avoid rebuilding toolchain each time. First build takes ~60s, subsequent builds ~14s.

## Structure

- `esphome/smart_meter.yaml` — main config (wifi, ota, api, uart, sensors, web_server)
- `esphome/secrets.yaml` — credentials template (excluded from git via `.gitignore`)
- `custom_components/bp35a1_smartmeter/` — ESPHome custom component (4 files only)

External libraries are pulled via `lib_deps` in YAML:
- `nullsnet/Arduino_BP35A1_B_route` — BP35A1 Wi-SUN state machine + UART protocol
- `nullsnet/Arduino_EchonetLite` — EchonetLite protocol parser

Do NOT copy library files into `custom_components/`. PlatformIO handles them automatically.

## Hardware

- M5StickC (ESP32)
- BP35A1 Wi-SUN module: TX→GPIO0, RX→GPIO26, 115200 baud
- GPIO0 is a strapping pin — ESPHome warns about this, ignore for UART TX

## Sensors Exposed

| Sensor | Unit | Description |
|--------|------|-------------|
| `power` | W | Instantaneous power |
| `current_r` | A | R-phase current |
| `current_t` | A | T-phase current |
| `energy` | kWh | Cumulative energy (positive) |
| `connection` | binary | B-route network status |

## Custom Component API

The C++ class inherits `PollingComponent` + `uart::UARTDevice`. Key methods:
- `setup()` — creates `UARTDeviceAdapter` and `BP35A1` instance, registers callbacks
- `update()` (60s poll) — calls `communicationLoop()` with lambda callback to read meter data
- `loop()` — runs `initializeLoop()` until ready; restarts ESP after 180s timeout

Callbacks use `std::function` with `[this]` capture. Library headers are included via PlatformIO LDF (`lib_ldf_mode: deep`).

## Secrets Required

Create `esphome/secrets.yaml` with: `wifi_ssid`, `wifi_password`, `ota_password`, `api_encryption_key`, `b_route_id`, `b_route_password`
