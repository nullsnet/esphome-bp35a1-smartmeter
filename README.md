# ESPHome BP35A1 Smart Meter

ESP32（M5StickC）と BP35A1 Wi-SUN モジュールを使って、スマートメーターの電力データを Home Assistant に送信するプロジェクトです。

## 概要

- **ボード**: M5StickC (ESP32)
- **通信モジュール**: BP35A1 (Wi-SUN B-route)
- **プロトコル**: EchonetLite over UART
- **連携**: ESPHome → Home Assistant（MQTT なし）

## 取得できるデータ

| センサー | 単位 | 説明 |
|----------|------|------|
| 瞬時電力 | W | 現在の消費電力 |
| R 相電流 | A | R 相の電流値 |
| T 相電流 | A | T 相の電流値 |
| 累積エネルギー | kWh | 正方向の累計使用量 |
| 接続ステータス | binary | B-route 通信状態 |

## セットアップ

### 1. シークレットファイル作成

```bash
cp esphome/secrets.yaml.example esphome/secrets.yaml
# secrets.yaml を編集して Wi-Fi 情報などを設定
```

必須項目: `wifi_ssid`, `wifi_password`, `ota_password`, `api_encryption_key`, `b_route_id`, `b_route_password`

### 2. コンパイル

```bash
docker run --rm \
  -v $(pwd):/config \
  -v $(pwd)/.cache/platformio:/root/.platformio \
  -v $(pwd)/.cache/esphome:/config/.esphome \
  ghcr.io/esphome/esphome:latest compile /config/esphome/smart_meter.yaml
```

初回ビルドは ~60 秒、それ以降は ~14 秒（キャッシュ有効時）。

### 3. フラッシュ

```bash
docker run --rm \
  -v $(pwd):/config \
  -v /dev/ttyUSB0:/dev/ttyUSB0 \
  ghcr.io/esphome/esphome:latest run /config/esphome/smart_meter.yaml
```

## ハードウェア接続

| BP35A1 | M5StickC |
|--------|----------|
| TX | GPIO0 |
| RX | GPIO26 |
| GND | GND |
| VCC | 3.3V |

 baud rate: 115200, 8N1

## ライブラリ

- [Arduino_BP35A1_B_route](https://github.com/nullsnet/Arduino_BP35A1_B_route) — BP35A1 モジュールの初期化・通信プロトコル
- [Arduino_EchonetLite](https://github.com/nullsnet/Arduino_EchonetLite) — EchonetLite プロトコルパーサー
