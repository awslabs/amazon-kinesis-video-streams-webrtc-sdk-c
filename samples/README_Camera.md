# KVS WebRTC Camera Input Sample

このサンプルは、V4L2対応のUSBカメラ（UVC H.264対応）からの映像入力を使用してAmazon Kinesis Video Streams WebRTCで配信するためのものです。

## 前提条件

- Linux環境（V4L2サポート）
- UVC H.264対応のUSBカメラ
- Python 3.6以上
- Amazon Kinesis Video Streamsのセットアップ済み

## 完全セットアップガイド

### 1. 開発環境のセットアップ

```bash
# システムの更新
sudo apt update && sudo apt upgrade -y

# 基本的な開発ツール
sudo apt install -y cmake build-essential git pkg-config python3

# SSL/TLS通信用
sudo apt install -y libssl-dev libcurl4-openssl-dev

# V4L2カメラアクセス用
sudo apt install -y libv4l-dev v4l-utils

# ログ出力用（オプション）
sudo apt install -y liblog4cplus-dev

# カメラアクセス権限の確認（必要に応じて）
# 現在のユーザーがvideoグループに属しているか確認
groups $USER | grep video
# もしvideoが含まれていない場合のみ以下を実行
# sudo usermod -a -G video $USER
# ログアウト・ログインが必要
```

### 2. プロジェクトのビルド

```bash
# プロジェクトルートディレクトリで
mkdir build && cd build
cmake .. -DBUILD_SAMPLE=ON
make -j$(nproc)

# または特定のターゲットのみビルド
make kvsWebrtcClientMasterCamera
```

### 3. カメラの確認と設定

#### 3.1 利用可能なカメラデバイスの確認

```bash
# V4L2デバイスの一覧
ls /dev/video*

# カメラの詳細情報
v4l2-ctl --list-devices

# サポートされているフォーマットの確認
v4l2-ctl --device=/dev/video0 --list-formats-ext
```

#### 3.2 H.264カメラの設定（重要）

UVC H.264カメラを使用する前に、必ず設定スクリプトを実行してください：

```bash
# デフォルト設定（640x480、30fps、300kbps）
python3 scripts/configure_h264_camera.py

# カスタム設定の例
python3 scripts/configure_h264_camera.py -d /dev/video0 -w 1280 --ht 720 -f 25 -b 1000000

# 設定オプション
python3 scripts/configure_h264_camera.py --help
```

**設定パラメータ：**
- `-d, --device`: カメラデバイスパス（デフォルト: `/dev/video0`）
- `-w, --width`: 映像の幅（デフォルト: 640）
- `--ht, --height`: 映像の高さ（デフォルト: 480）
- `-f, --fps`: フレームレート（デフォルト: 30）
- `-b, --bitrate`: ビットレート（デフォルト: 300000）
- `-v, --verbose`: 詳細ログ出力

### 4. KVS WebRTC配信の実行

#### 4.1 AWS認証情報の設定

```bash
# 環境変数で設定
export AWS_ACCESS_KEY_ID="your-access-key"
export AWS_SECRET_ACCESS_KEY="your-secret-key"
export AWS_DEFAULT_REGION="ap-northeast-1"

# またはAWS CLIで設定
aws configure
```

#### 4.2 基本的な使用方法

```bash
# デフォルト設定で実行（/dev/video0、640x480）
./samples/kvsWebrtcClientMasterCamera [channel_name]

# カメラデバイスを指定
./samples/kvsWebrtcClientMasterCamera [channel_name] /dev/video1

# 解像度も指定
./samples/kvsWebrtcClientMasterCamera [channel_name] /dev/video0 1280 720
```

#### 4.3 動的FPS・ビットレート設定（新機能）

```bash
# カスタムFPSとビットレートで実行
./samples/kvsWebrtcClientMasterCamera --fps 25 --bitrate 1000000 [channel_name]

# 低ビットレート設定
./samples/kvsWebrtcClientMasterCamera --fps 15 --bitrate 500000 [channel_name]

# ヘルプ表示
./samples/kvsWebrtcClientMasterCamera --help
```

**新しいコマンドライン引数：**
- `--fps`: フレームレート（5-60fps、デフォルト: 25）
- `--bitrate`: ビットレート（100000-10000000bps、デフォルト: 512000）
- `--help`: 使用方法の表示

### 5. 完全なワークフロー例

```bash
# 1. カメラを1280x720、25fps、1Mbpsで設定
python3 scripts/configure_h264_camera.py -d /dev/video0 -w 1280 --ht 720 -f 25 -b 1000000

# 2. 設定が完了したら、同じ設定でKVS配信を開始
./samples/kvsWebrtcClientMasterCamera --fps 25 --bitrate 1000000 my-camera-channel /dev/video0 1280 720

# 3. 別のターミナルでビューアーを起動してテスト
./samples/kvsWebrtcClientViewer my-camera-channel
```

## パラメータ

### kvsWebrtcClientMasterCamera の引数

1. `--fps`: フレームレート（オプション、デフォルト: 25）
2. `--bitrate`: ビットレート（オプション、デフォルト: 512000）
3. `--help`: ヘルプ表示（オプション）
4. `channel_name`: Kinesis Video Streamsのチャネル名（省略時は環境変数またはデフォルト値を使用）
5. `camera_device`: カメラデバイスパス（デフォルト: `/dev/video0`）
6. `width`: 映像の幅（デフォルト: 640）
7. `height`: 映像の高さ（デフォルト: 480）

