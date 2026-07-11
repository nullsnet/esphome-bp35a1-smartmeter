# ESPHome BP35A1 Smart Meter

ESP32（M5StickC）と BP35A1 Wi-SUN モジュールを使って、スマートメーターの電力データを Home Assistant に送信するプロジェクトです。

## 概要

- **ボード**: M5StickC (ESP32)
- **通信モジュール**: BP35A1 (Wi-SUN B-route)
- **プロトコル**: EchonetLite over UART
- **連携**: ESPHome → Home Assistant（MQTT なし）

## 取得できるデータ

### 数値センサー

| センサー | 単位 | 説明 |
|----------|------|------|
| 瞬時電力 | W | 現在の消費電力 |
| R 相電流 | A | R 相の電流値 |
| T 相電流 | A | T 相の電流値 |
| 累積エネルギー | kWh | 正方向の累計使用量 |

### バイナリセンサー

| センサー | 説明 |
|----------|------|
| 接続ステータス | B-route 通信状態 |

### テキストセンサー（初期化時に一度だけ発行）

| センサー | 説明 |
|----------|------|
| IPv6 Address | BP35A1 のローカル IPv6 アドレス |
| Dest IPv6 Address | スマートメーターの IPv6 アドレス |
| MAC Address | BP35A1 の 64 ビット MAC アドレス |
| MAC Address 16 | 16 ビットアドレス（FFE = 未割当） |
| Channel | Wi-SUN チャンネル |
| PAN ID | Wi-SUN PAN ID |
| LQI | リンク品質インジケータ |
| Pair ID | ペアリング ID |
| Scan Mode | スキャンモード |

## セットアップ

### 1. 仮想環境作成

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install esphome
```

### 2. シークレットファイル作成

```bash
cp secrets.yaml.example secrets.yaml
# secrets.yaml を編集して Wi-Fi 情報などを設定
```

必須項目: `wifi_ssid`, `wifi_password`, `ota_password`, `api_encryption_key`, `b_route_id`, `b_route_password`

### 3. コンパイル

```bash
source .venv/bin/activate
esphome compile smart_meter.yaml
```

初回ビルドは ~60 秒、それ以降は ~14 秒（キャッシュ有効時）。

### 4. フラッシュ

```bash
esphome upload smart_meter.yaml --device /dev/ttyUSB0
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
