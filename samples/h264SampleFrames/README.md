GStreamer Pipeline to reproduce sample frames

```
gst-launch-1.0 videotestsrc pattern=ball num-buffers=1500 ! timeoverlay ! videoconvert ! video/x-raw,format=I420,width=1280,height=720,framerate=25/1 ! queue ! x264enc bframes=0 speed-preset=veryfast bitrate=512 byte-stream=TRUE tune=zerolatency key-int-max=25 ! video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline ! multifilesink location="frame-%04d.h264" index=1
```

Note: since the fps is 25, we need 1500 frames to create a 60 seconds video

## Multi-track sample frames

For the 4-video-track master sample, generate per-track frame sets with distinct
patterns and a "Track #N" overlay using the script in the samples directory:

```
cd samples && bash generate_multi_track_frames.sh
```

This creates `h264SampleFrames_track0/` through `h264SampleFrames_track3/` with
patterns: ball, smpte, snow, circular. Copy them to your build directory alongside
the existing frame folders. The master sample auto-detects these directories at
runtime and falls back to the single `h264SampleFrames/` directory if they are
not present.
