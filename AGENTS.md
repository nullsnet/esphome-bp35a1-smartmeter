# AGENTS.md

## Build

```bash
source .venv/bin/activate
esphome compile smart_meter.yaml
esphome upload smart_meter.yaml --device /dev/ttyUSB0
```

First build takes ~60s, subsequent builds ~14s.

## Structure

- `smart_meter.yaml` — main config (wifi, ota, api, uart, sensors, web_server)
- `secrets.yaml` — credentials (excluded from git via `.gitignore`)
- `components/bp35a1_smartmeter/` — ESPHome custom component (3 files: `__init__.py`, `.cpp`, `.h`)

External libraries are pulled via `lib_deps` in YAML:
- `nullsnet/Arduino_BP35A1_B_route` — BP35A1 Wi-SUN state machine + UART protocol
- `nullsnet/Arduino_EchonetLite` — EchonetLite protocol parser

Do NOT copy library files into `components/`. PlatformIO handles them automatically.

## Hardware

- M5StickC (ESP32)
- BP35A1 Wi-SUN module: TX→GPIO0, RX→GPIO26, 115200 baud
- GPIO0 is a strapping pin — ESPHome warns about this, ignore for UART TX

## Sensors Exposed

### Numeric sensors

| Sensor | Unit | Description |
|--------|------|-------------|
| `power` | W | Instantaneous power |
| `current_r` | A | R-phase current |
| `current_t` | A | T-phase current |
| `energy` | kWh | Cumulative energy (positive) |

### Binary sensors

| Sensor | Description |
|--------|-------------|
| `connection` | B-route network status |

### Text sensors (published once at initialization)

| Sensor | Description |
|--------|-------------|
| `ipv6_address` | BP35A1 local IPv6 address |
| `dest_ipv6_address` | Smart meter IPv6 address (from SK CONVERTMAC2IPV6) |
| `mac_address` | BP35A1 64-bit MAC address |
| `mac_address_16` | 16-bit address (FFE = not assigned) |
| `channel` | Wi-SUN channel |
| `pan_id` | Wi-SUN PAN ID |
| `lqi` | Link Quality Indicator |
| `pair_id` | Pairing ID |
| `scan_mode` | Scan mode string |

## Custom Component API

The C++ class inherits `PollingComponent` + `uart::UARTDevice`.

- `setup()` — creates `UARTDeviceAdapter` and `BP35A1` instance, registers init/communication callbacks
- `loop()` — 100ms throttled; runs `initializeLoop()` until readySmartMeter, then runs `communicationLoop()` to continuously receive meter data; restarts ESP after 180s init timeout
- `update()` — called at `update_interval` (default 10s); sends property requests (power, current, energy)
- `publish_info_sensors_()` — publishes text sensors once when initialization completes

Library callbacks use `std::function` with `[this]` capture. Headers included via PlatformIO LDF (`lib_ldf_mode: deep`).

## Secrets Required

Create `secrets.yaml` with: `wifi_ssid`, `wifi_password`, `ota_password`, `api_encryption_key`, `b_route_id`, `b_route_password`
