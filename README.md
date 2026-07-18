# ESPHome BP35A1 Smart Meter

[![ESPHome](https://img.shields.io/badge/ESPHome-2026.6+-green.svg)](https://esphome.io/)
[![License](https://img.shields.io/github/license/nullsnet/esphome-bp35a1-smartmeter)](LICENSE)

BP35A1 Wi-SUN モジュール経由でスマートメーターの電力データを取得し、ESPHome を介して Home Assistant に公開する

## 特徴

- 電力・電流・エネルギーのリアルタイム監視
- Wi-SUN B-route 接続（BP35A1 モジュール）
- 自動初期化・再接続
- ネットワーク品質の診断センサー（LQI, Channel）
- Wi-SUN スキャンチャンネルの指定による初期化高速化

## ハードウェア

| 部品 | 型番 |
|------|------|
| MCU | M5StickC (ESP32) |
| Wi-SUN モジュール | ROHM BP35A1 |

### 配線

| BP35A1 | M5StickC |
|--------|----------|
| TX | GPIO0 |
| RX | GPIO26 |
| GND | GND |
| VCC | 3.3V |

UART: 115200 baud, 8N1

## センサー

### 数値センサー

| センサー | 単位 | 説明 |
|----------|------|------|
| Instantaneous Power | W | 瞬時電力 |
| Instantaneous Current R | A | R 相電流 |
| Instantaneous Current T | A | T 相電流 |
| Cumulative Energy Positive | kWh | 累積エネルギー |
| Wi-SUN Channel | — | チャンネル番号（診断） |
| Wi-SUN LQI | — | リンク品質インジケータ（診断） |

### バイナリセンサー

| センサー | 説明 |
|----------|------|
| B-route Connection | 接続ステータス |

### テキストセンサー

| センサー | 説明 |
|----------|------|
| Wi-SUN IPv6 Address | BP35A1 のローカル IPv6 アドレス |
| Wi-SUN Dest IPv6 Address | スマートメーターの IPv6 アドレス |
| Wi-SUN MAC Address | BP35A1 の 64 ビット MAC アドレス |
| Wi-SUN MAC Address 16 | 16 ビットアドレス（FFE = 未割当） |
| Wi-SUN PAN ID | PAN ID |
| Wi-SUN Pair ID | ペアリング ID |
| Wi-SUN Scan Mode | スキャンモード |

## セットアップ

### 前提条件

- Python 3.8+
- ESPHome 2026.6+

### インストール

```bash
git clone https://github.com/nullsnet/esphome-bp35a1-smartmeter.git
cd esphome-bp35a1-smartmeter

python3 -m venv .venv
source .venv/bin/activate
pip install esphome
```

### 設定

`secrets.yaml` を作成する:

```yaml
wifi_ssid: "SSID"
wifi_password: "パスワード"
ota_password: "OTAパスワード"
api_encryption_key: "APIキー"  # esphome dashboard-api generate-key
b_route_id: "B-route ID"
b_route_password: "B-route パスワード"
```

### コンポーネント設定

`smart_meter.yaml` でコンポーネントパラメータを設定する:

```yaml
bp35a1_smartmeter:
  b_route_id: !secret b_route_id
  b_route_password: !secret b_route_password
  update_interval: 10s      # データ取得間隔（デフォルト: 60s）
  init_timeout: 180s        # Wi-SUN初期化タイムアウト（デフォルト: 180s）
  loop_interval: 100ms      # 状態機械ループ間隔（デフォルト: 100ms）
  scan_channel_mask: 0x00000400  # Wi-SUNスキャン対象チャンネルマスク（デフォルト: 0xFFFFFFFF）
```

#### `scan_channel_mask` について

Wi-SUN スキャン時に検索するチャンネルを指定するビットマスク。
デフォルト（`0xFFFFFFFF`）では全チャンネルをスキャンするため、初期化に時間がかかる場合がある。

既知のチャンネルに限定することで初期化速度を改善できる:

| 値 | スキャン対象 |
|----|-------------|
| `0xFFFFFFFF` | 全チャンネル（デフォルト） |
| `0x00000400` | チャンネル 10 のみ |
| `0x00000C00` | チャンネル 10, 11 |

チャンネルはデバイスの初期化ログ（`esphome logs`）で確認できる:

```
[I][bp35a1_smartmeter:033]: Starting Wi-SUN scan
```

スキャン完了後にチャンネルがログ出力される。

### ビルド & 書き込み

```bash
source .venv/bin/activate

# コンパイル
esphome compile smart_meter.yaml

# USB 経由で書き込み
esphome upload smart_meter.yaml --device /dev/ttyUSB0

# OTA 経由で書き込み（初回 USB 書き込み後）
esphome upload smart_meter.yaml --device <IPアドレス>
```

### ログ確認

```bash
# USB シリアル
esphome logs smart_meter.yaml --device /dev/ttyUSB0

# OTA
esphome logs smart_meter.yaml --device <IPアドレス>
```

## 依存ライブラリ

| ライブラリ | 説明 |
|------------|------|
| [Arduino_BP35A1_B_route](https://github.com/nullsnet/Arduino_BP35A1_B_route) | BP35A1 Wi-SUN 状態機械 & UART プロトコル |
| [Arduino_EchonetLite](https://github.com/nullsnet/Arduino_EchonetLite) | EchonetLite プロトコルパーサー |

## ライセンス

MIT
