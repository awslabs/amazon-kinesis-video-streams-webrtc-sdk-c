# KVS WebRTC Camera Input Sample

このサンプルは、V4L2 対応の USB カメラ（UVC H.264 対応）からの映像入力を使用して Amazon Kinesis Video Streams WebRTC で配信するためのものです。

## 前提条件

- Linux 環境（V4L2 サポート）
- UVC H.264 対応の USB カメラ
- Python 3.6 以上
- Amazon Kinesis Video Streams のセットアップ済み

## セットアップガイド

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

#### 3.2 H.264 カメラの設定（重要）

UVC H.264 カメラを使用する前に、必ず設定スクリプトを実行してください：

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

### 4. KVS WebRTC 配信の実行

#### 4.1 AWS 認証情報の設定

```bash
# 環境変数で設定
export AWS_ACCESS_KEY_ID="your-access-key"
export AWS_SECRET_ACCESS_KEY="your-secret-key"
export AWS_DEFAULT_REGION="ap-northeast-1"

# またはAWS CLIで設定
aws configure
```

#### 4.2 基本的な使用方法

**コマンドライン引数の順序：**

```
./samples/kvsWebrtcClientMasterCamera [channel_name] [camera_device] [width] [height] [fps] [bitrate]
```

**使用例：**

```bash
# デフォルト設定で実行（/dev/video0、640x480、30fps、300kbps）
./samples/kvsWebrtcClientMasterCamera my-camera-channel

# カメラデバイスを指定
./samples/kvsWebrtcClientMasterCamera my-camera-channel /dev/video1

# 解像度を指定（1280x720）
./samples/kvsWebrtcClientMasterCamera my-camera-channel /dev/video0 1280 720

# 解像度とFPSを指定（1280x720、30fps）
./samples/kvsWebrtcClientMasterCamera my-camera-channel /dev/video0 1280 720 30

# 全パラメータを指定（1280x720、30fps、300kbps）
./samples/kvsWebrtcClientMasterCamera my-camera-channel /dev/video0 1280 720 30 300000

./samples/kvsWebrtcClientMasterCamera test-redimpulz-takenori /dev/video0 640 480 30 300000


# ヘルプ表示
./samples/kvsWebrtcClientMasterCamera --help
```

**引数の説明：**

- `channel_name`: Kinesis Video Streams のチャネル名（必須）
- `camera_device`: カメラデバイスパス（デフォルト: `/dev/video0`）
- `width`: 映像の幅（デフォルト: 640）
- `height`: 映像の高さ（デフォルト: 480）
- `fps`: フレームレート（デフォルト: 30）
- `bitrate`: ビットレート（デフォルト: 300000）

### 5. 完全なワークフロー例

```bash
# 1. カメラを1280x720、25fps、1Mbpsで設定
python3 scripts/configure_h264_camera.py -d /dev/video0 -w 1280 --ht 720 -f 25 -b 1000000

# 2. 設定が完了したら、同じ設定でKVS配信を開始
./samples/kvsWebrtcClientMasterCamera my-camera-channel /dev/video0 1280 720 25 1000000

# 3. 別のターミナルでビューアーを起動してテスト
./samples/kvsWebrtcClientViewer my-camera-channel
```

## パラメータ

### kvsWebrtcClientMasterCamera の引数

**位置引数（順序重要）：**

1. `channel_name`: Kinesis Video Streams のチャネル名（必須）
2. `camera_device`: カメラデバイスパス（オプション、デフォルト: `/dev/video0`）
3. `width`: 映像の幅（オプション、デフォルト: 640）
4. `height`: 映像の高さ（オプション、デフォルト: 480）
5. `fps`: フレームレート（オプション、デフォルト: 30、範囲: 1-120）
6. `bitrate`: ビットレート（オプション、デフォルト: 300000、範囲: 100000-50000000）

**オプション引数：**

- `--help` または `-h`: 使用方法の表示

**注意：** 現在のバージョンでは `--fps` や `--bitrate` のような名前付きオプションは実装されていません。全て位置引数として指定してください。

## トラブルシューティング

### カメラが認識されない場合

```bash
# カメラデバイスの診断ツールを実行
./samples/kvsWebrtcClientMasterCameraDebug /dev/video0
```

このツールは以下の情報を表示します：

- デバイス能力（ドライバー、カード名など）
- サポートされているフォーマット一覧
- H.264/MJPEG 対応状況
- フォーマット設定テスト結果

### よくあるエラー

1. **"Failed to open camera device"**

   - カメラデバイスパスが正しいか確認
   - ユーザーが video グループに属しているか確認
   - カメラが他のアプリケーションで使用されていないか確認

2. **"Camera does not support H.264 or MJPEG format"**

   - UVC H.264 対応カメラを使用してください
   - 診断ツールでサポートフォーマットを確認

3. **"Invalid FPS value" または "Bitrate too low"**
   - 引数の順序が正しいか確認
   - 数値が適切な範囲内か確認

## 備考

### ファイル説明

MjpegH264Extractor.c: mjpeg に多層化された h264 を抽出する処理
MjpegH264Extractor.h: MjpegH264Extractor.c のヘッダーファイル
CameraInput.c: V4L2 を使用してカメラデバイスからストリーム取得、カメラ情報取得等
CameraInput.h: CameraInput.c のヘッダーファイル
kvsWebRTCClientMasterCamera.c: カメラデバイスから取得したストリームを配信するメインアプリ
kvsWebRTCClientMasterCameraDebug.c: カメラデバイス診断アプリ,現在のフォーマット等診断
Samples.h: サンプルアプリで共通で使用するヘッダーファイル、MjpegH264Extractor.c でも使用
