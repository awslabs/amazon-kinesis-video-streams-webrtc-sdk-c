# KVS WebRTC Camera Input Sample

このサンプルは、V4L2対応のUSBカメラ（UVC H.264対応）からの映像入力を使用してAmazon Kinesis Video Streams WebRTCで配信するためのものです。

## 前提条件

- Linux環境（V4L2サポート）
- UVC H.264対応のUSBカメラ
- Amazon Kinesis Video Streamsのセットアップ済み

## ビルド方法

```bash
# プロジェクトルートディレクトリで
mkdir build && cd build
cmake .. -DBUILD_SAMPLE=ON
make

# または特定のターゲットのみビルド
make kvsWebrtcClientMasterCamera
```

## 使用方法

### 基本的な使用方法

```bash
# デフォルト設定で実行（/dev/video0、640x480）
./kvsWebrtcClientMasterCamera [channel_name]

# カメラデバイスを指定
./kvsWebrtcClientMasterCamera [channel_name] /dev/video1

# 解像度も指定
./kvsWebrtcClientMasterCamera [channel_name] /dev/video0 1280 720
```

### パラメータ

1. `channel_name`: Kinesis Video Streamsのチャネル名（省略時は環境変数またはデフォルト値を使用）
2. `camera_device`: カメラデバイスパス（デフォルト: `/dev/video0`）
3. `width`: 映像の幅（デフォルト: 640）
4. `height`: 映像の高さ（デフォルト: 480）

## カメラの確認方法

### 利用可能なカメラデバイスの確認

```bash
# V4L2デバイスの一覧
ls /dev/video*

# カメラの詳細情報
v4l2-ctl --list-devices

# サポートされているフォーマットの確認
v4l2-ctl --device=/dev/video0 --list-formats-ext
```

### H.264対応の確認

```bash
# H.264フォーマットがサポートされているかチェック
v4l2-ctl --device=/dev/video0 --list-formats | grep H264
```

## トラブルシューティング

### よくある問題

1. **カメラデバイスが見つからない**
   ```
   Error: Failed to open camera device: /dev/video0
   ```
   - カメラが接続されているか確認
   - デバイスパスが正しいか確認（`ls /dev/video*`）
   - ユーザーがvideoグループに属しているか確認

2. **H.264フォーマットがサポートされていない**
   ```
   Error: Camera does not support H.264 format
   ```
   - UVC H.264対応カメラを使用してください
   - `v4l2-ctl --list-formats`でサポートフォーマットを確認

3. **権限エラー**
   ```bash
   # ユーザーをvideoグループに追加
   sudo usermod -a -G video $USER
   # ログアウト・ログインが必要
   ```

### デバッグ情報の有効化

```bash
# より詳細なログを出力
export KVS_LOG_LEVEL=2
./kvsWebrtcClientMasterCamera
```

## 対応カメラの例

- Logitech C920/C922/C930e（UVC H.264対応モデル）
- その他のUVC H.264対応Webカメラ

## 制限事項

- Linux環境でのみ動作（V4L2依存）
- H.264ハードウェアエンコード対応カメラが必要
- 音声は既存のサンプルファイルを使用（カメラからの音声入力は未対応）

## ファイル構成

- `CameraInput.h/c`: V4L2カメラ入力の実装
- `kvsWebRTCClientMasterCamera.c`: カメラ対応マスタークライアント
- `Samples.h`: 関数宣言の追加

## 参考情報

- [Amazon Kinesis Video Streams WebRTC SDK](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c)
- [V4L2 API Documentation](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html)
- [UVC Driver Documentation](https://www.kernel.org/doc/html/latest/media/v4l-drivers/uvcvideo.html)
