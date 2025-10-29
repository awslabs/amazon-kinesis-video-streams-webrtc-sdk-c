#!/bin/bash

echo "🚀 デュアルカメラ分岐システム開始"
echo "📋 構成:"
echo "  Camera1 (/dev/video0) → WebRTC + /dev/video4"
echo "  Camera2 (/dev/video2) → WebRTC + /dev/video5"
echo ""

# v4l2loopback設定
echo "⚙️ v4l2loopback設定中..."
sudo modprobe v4l2loopback devices=2 video_nr=4,5 \
  card_label="Camera1_Loopback,Camera2_Loopback"

if [ $? -ne 0 ]; then
    echo "❌ v4l2loopback設定に失敗しました"
    exit 1
fi

echo "✅ v4l2loopback設定完了"
ls -la /dev/video{4,5}

# 両カメラ設定
echo ""
echo "📷 カメラ設定中..."
echo "  Camera1 (/dev/video0) 設定中..."
python3 scripts/configure_h264_camera.py -d /dev/video0 -w 1280 --ht 720 -f 30 -b 2000000

if [ $? -ne 0 ]; then
    echo "⚠️ Camera1設定に失敗しました（続行します）"
fi

echo "  Camera2 (/dev/video2) 設定中..."
python3 scripts/configure_h264_camera.py -d /dev/video2 -w 1280 --ht 720 -f 30 -b 2000000

if [ $? -ne 0 ]; then
    echo "⚠️ Camera2設定に失敗しました（続行します）"
fi

echo "✅ カメラ設定完了"

# AWS認証情報確認
echo ""
echo "🔐 AWS認証情報確認中..."
if [ -z "$AWS_ACCESS_KEY_ID" ] || [ -z "$AWS_SECRET_ACCESS_KEY" ]; then
    echo "⚠️ AWS認証情報が設定されていません"
    echo "以下のコマンドで設定してください:"
    echo "export AWS_ACCESS_KEY_ID=\"your-access-key\""
    echo "export AWS_SECRET_ACCESS_KEY=\"your-secret-key\""
    echo "export AWS_DEFAULT_REGION=\"ap-northeast-1\""
    echo ""
    echo "続行しますか？ (y/N)"
    read -r response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        echo "終了します"
        exit 1
    fi
else
    echo "✅ AWS認証情報確認完了"
fi

# GStreamer分岐開始
echo ""
echo "🔀 GStreamer分岐開始..."
chmod +x gstreamer_camera1_split.sh gstreamer_camera2_split.sh

./gstreamer_camera1_split.sh &
GSTREAMER1_PID=$!
echo "  Camera1分岐開始 (PID: $GSTREAMER1_PID)"

./gstreamer_camera2_split.sh &
GSTREAMER2_PID=$!
echo "  Camera2分岐開始 (PID: $GSTREAMER2_PID)"

# 少し待機（GStreamerの初期化待ち）
echo "  GStreamer初期化待機中..."
sleep 5

# v4l2loopback出力確認
echo ""
echo "🔍 v4l2loopback出力確認中..."
if v4l2-ctl --device=/dev/video4 --all >/dev/null 2>&1; then
    echo "✅ /dev/video4 出力確認"
else
    echo "⚠️ /dev/video4 出力に問題があります"
fi

if v4l2-ctl --device=/dev/video5 --all >/dev/null 2>&1; then
    echo "✅ /dev/video5 出力確認"
else
    echo "⚠️ /dev/video5 出力に問題があります"
fi

# WebRTCアプリ並列実行
echo ""
echo "🌐 WebRTCアプリ開始..."
cd build/samples

if [ ! -f "./kvsWebrtcClientMasterCamera" ]; then
    echo "❌ kvsWebrtcClientMasterCamera が見つかりません"
    echo "ビルドを実行してください: cd build && make"
    exit 1
fi

if [ ! -f "./kvsWebrtcClientMasterCamera2" ]; then
    echo "❌ kvsWebrtcClientMasterCamera2 が見つかりません"
    echo "ビルドを実行してください: cd build && make"
    exit 1
fi

# Camera1 WebRTC (直接 /dev/video0 から取得)
echo "  Camera1 WebRTC開始..."
./kvsWebrtcClientMasterCamera /dev/video0 &
WEBRTC1_PID=$!

# Camera2 WebRTC (直接 /dev/video2 から取得)  
echo "  Camera2 WebRTC開始..."
./kvsWebrtcClientMasterCamera2 /dev/video2 &
WEBRTC2_PID=$!

echo ""
echo "✅ システム開始完了"
echo ""
echo "📺 配信チャンネル:"
echo "  - MainStream        (/dev/video0 → WebRTC)"
echo "  - Camera2_MainStream (/dev/video2 → WebRTC)"
echo ""
echo "🔄 v4l2loopback出力:"
echo "  - /dev/video4 ← Camera1 (/dev/video0)"
echo "  - /dev/video5 ← Camera2 (/dev/video2)"
echo ""
echo "🔍 動作確認方法:"
echo "  # v4l2loopback出力確認"
echo "  ffplay /dev/video4 &"
echo "  ffplay /dev/video5 &"
echo ""
echo "  # AWS KVS WebRTCコンソールで配信確認"
echo "  - MainStream チャンネル"
echo "  - Camera2_MainStream チャンネル"
echo ""
echo "🛑 終了するには Ctrl+C を押してください"

# シグナルハンドラー設定
cleanup() {
    echo ""
    echo "🛑 システム終了中..."
    echo "  WebRTCアプリ終了中..."
    kill $WEBRTC1_PID $WEBRTC2_PID 2>/dev/null
    echo "  GStreamer終了中..."
    kill $GSTREAMER1_PID $GSTREAMER2_PID 2>/dev/null
    echo "✅ システム終了完了"
    exit 0
}

trap cleanup INT TERM

# 終了待機
wait $WEBRTC1_PID $WEBRTC2_PID $GSTREAMER1_PID $GSTREAMER2_PID