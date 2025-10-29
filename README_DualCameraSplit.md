# デュアルカメラ分岐配信システム

GStreamerを使用したカメラ分岐とWebRTC独立配信システム

## 📊 システム構成

```
┌─────────────────────┐    ┌─────────────────────┐
│   Camera1           │    │   Camera2           │
│  /dev/video0        │    │  /dev/video2        │
│  MJPEG + H.264      │    │  MJPEG + H.264      │
└─────────┬───────────┘    └─────────┬───────────┘
          │                          │
          ▼                          ▼
┌─────────────────────┐    ┌─────────────────────┐
│  GStreamer tee1     │    │  GStreamer tee2     │
│  (Camera1分岐)      │    │  (Camera2分岐)      │
└─────┬───────┬───────┘    └─────┬───────┬───────┘
      │       │                  │       │
      ▼       ▼                  ▼       ▼
┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐
│WebRTC App1│ │/dev/video4│ │WebRTC App2│ │/dev/video5│
│(直接取得) │ │v4l2sink   │ │(直接取得) │ │v4l2sink   │
│Camera1    │ │loopback   │ │Camera2    │ │loopback   │
└───────────┘ └───────────┘ └───────────┘ └───────────┘
```

## 🎯 特徴

### **分岐方式**
- **GStreamer tee**: 各カメラから2方向に分岐
- **WebRTC**: 物理デバイスから直接取得
- **v4l2loopback**: GStreamerから出力

### **独立プロセス**
- **Camera1**: `kvsWebrtcClientMasterCamera` (MainStream)
- **Camera2**: `kvsWebrtcClientMasterCamera2` (Camera2_MainStream)
- **プロセス分離**: 障害分離、個別制御可能

## 🚀 使用方法

### **1. 準備**
```bash
# 2台のカメラを接続
# /dev/video0 と /dev/video2 に割り当てられることを確認
v4l2-ctl --list-devices
```

### **2. ビルド**
```bash
cd build
make kvsWebrtcClientMasterCamera2
```

### **3. AWS認証設定**
```bash
export AWS_ACCESS_KEY_ID="your-access-key"
export AWS_SECRET_ACCESS_KEY="your-secret-key"
export AWS_DEFAULT_REGION="ap-northeast-1"
```

### **4. 統合実行**
```bash
# 実行権限設定
chmod +x gstreamer_camera1_split.sh gstreamer_camera2_split.sh run_dual_camera_split_system.sh

# システム実行
./run_dual_camera_split_system.sh
```

## 📋 個別実行方法

### **手動実行（段階的）**
```bash
# 1. v4l2loopback設定
sudo modprobe v4l2loopback devices=2 video_nr=4,5 card_label="Camera1_Loopback,Camera2_Loopback"

# 2. カメラ設定
python3 scripts/configure_h264_camera.py -d /dev/video0 -w 1280 --ht 720 -f 30 -b 2000000
python3 scripts/configure_h264_camera.py -d /dev/video2 -w 1280 --ht 720 -f 30 -b 2000000

# 3. GStreamer分岐開始
./gstreamer_camera1_split.sh &
./gstreamer_camera2_split.sh &

# 4. WebRTCアプリ実行
cd build/samples
./kvsWebrtcClientMasterCamera /dev/video0 &
./kvsWebrtcClientMasterCamera2 /dev/video2 &
```

## 🔍 動作確認

### **v4l2loopback出力確認**
```bash
# Camera1出力
ffplay /dev/video4

# Camera2出力
ffplay /dev/video5

# デバイス情報確認
v4l2-ctl --device=/dev/video4 --all
v4l2-ctl --device=/dev/video5 --all
```

### **WebRTC配信確認**
- AWS KVS WebRTCコンソールで確認
- **チャンネル**:
  - `MainStream` (Camera1)
  - `Camera2_MainStream` (Camera2)

## 📊 配信構成

| カメラ | 物理デバイス | WebRTCチャンネル | v4l2loopback出力 |
|--------|--------------|------------------|------------------|
| Camera1 | /dev/video0 | MainStream | /dev/video4 |
| Camera2 | /dev/video2 | Camera2_MainStream | /dev/video5 |

## 🔧 トラブルシューティング

### **カメラが見つからない場合**
```bash
# デバイス確認
v4l2-ctl --list-devices
lsusb | grep -i camera

# USB再接続
sudo rmmod uvcvideo
sudo modprobe uvcvideo
```

### **GStreamer分岐失敗**
```bash
# GStreamer確認
gst-launch-1.0 --version

# 単純テスト
gst-launch-1.0 v4l2src device=/dev/video0 ! fakesink
```

### **v4l2loopback問題**
```bash
# モジュール再ロード
sudo rmmod v4l2loopback
sudo modprobe v4l2loopback devices=2 video_nr=4,5

# 権限確認
ls -la /dev/video{4,5}
```

### **WebRTC接続失敗**
```bash
# AWS認証確認
aws sts get-caller-identity

# ネットワーク確認
ping kinesisvideo.ap-northeast-1.amazonaws.com
```

## 📝 ファイル構成

```
├── samples/
│   ├── kvsWebRTCClientMasterCamera.c      # Camera1 WebRTCアプリ
│   ├── kvsWebRTCClientMasterCamera2.c     # Camera2 WebRTCアプリ
│   └── CMakeLists.txt                     # ビルド設定
├── gstreamer_camera1_split.sh             # Camera1分岐スクリプト
├── gstreamer_camera2_split.sh             # Camera2分岐スクリプト
├── run_dual_camera_split_system.sh        # 統合実行スクリプト
└── README_DualCameraSplit.md              # このファイル
```

## ⚠️ 注意事項

### **リソース要件**
- **CPU**: GStreamer + 2つのWebRTCプロセス
- **メモリ**: 各プロセスが独立してメモリ使用
- **帯域幅**: 2つのWebRTCストリーム同時配信

### **デバイス要件**
- **2台のUSBカメラ**: H.264対応推奨
- **v4l2loopback**: カーネルモジュール
- **GStreamer**: 1.0以上

### **同期**
- **フレーム同期**: 物理デバイスレベルで自然に同期
- **プロセス管理**: 適切な開始・終了順序が重要

## 🎯 利点

### **安定性**
- **プロセス分離**: 1つのWebRTCプロセスがクラッシュしても他に影響なし
- **リソース分散**: CPU・メモリ負荷の分散
- **独立デバッグ**: 各チャンネルを個別にデバッグ可能

### **柔軟性**
- **個別制御**: チャンネルごとに停止・再開可能
- **設定調整**: 各カメラで異なる解像度・ビットレート設定可能
- **スケーラビリティ**: 3台目、4台目のカメラ追加が容易

### **効率性**
- **GStreamer最適化**: ハードウェア最適化された分岐処理
- **メモリ効率**: 不要なフレームコピーを削減
- **CPU効率**: 各プロセスが独立して最適化