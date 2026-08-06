# Troubleshooting H.265 Video Frames

This guide covers diagnosing problems with on-disk H.265 (HEVC) frames streamed
over WebRTC, whether they ship with a sample, come from your own encoder.
Most of it applies to H.264 as well; codec-specific differences are called out
where they matter.

## Symptom: one frame plays, then video freezes

**What you see:** On a healthy connection (low loss, plenty of bandwidth) the
video received by the WebRTC viewer plays briefly and then freezes at the
**same spot every time**. If there is a separate audio track, the
**audio keeps playing**. Reloading reproduces the identical freeze. That
the freeze lands at the same spot on every run (especially with a preset, fixed set
of frames rather than a live encoder) is a strong sign the problem is in the frame
data itself, not the network.

**What it means:** Confirm the freeze occurs on a keyframe. The spot where it
stalls should line up with a keyframe boundary (see
[Find every keyframe in a track](#find-every-keyframe-in-a-track)). It means that
keyframe can't be decoded: the decoder played up to the previous keyframe's group
of pictures but cannot decode, or recover to, the next one, so playback stops
there. Because the network is fine, the issue is almost always the bitstream: that
keyframe never carries what the decoder needs to start a fresh access unit. Audio
is an independent track, so it is unaffected.

### Most common root cause: keyframes missing parameter sets

An H.265 keyframe must be a **self-contained access unit**. In Annex-B byte-stream
form that means each keyframe should be preceded by the parameter sets
`VPS` (32), `SPS` (33), and `PPS` (34) before the coded slice (`IDR` 19/20 or
`CRA` 21).

If only the *first* frame carries `VPS/SPS/PPS` and later keyframes are bare
slices, the stream decodes locally in `ffmpeg` (which caches the parameter sets
from frame 1) but breaks over WebRTC: the receiver has no persistent parameter-set
state to fall back on, and on any packet loss or keyframe request (PLI/FIR) it
cannot re-establish decoding, so the picture freezes on the first frame.

> This is codec-specific. `x264enc` repeats SPS/PPS on keyframes by default, so
> H.264 frames tend to work even without an explicit parser. `x265enc` does **not**
> repeat parameter sets, so H.265 needs a parser to insert them.

**The fix** is to run the encoder output through a parser with
`config-interval=-1`, which re-inserts parameter sets before every keyframe:

```
... ! x265enc ... ! h265parse config-interval=-1 ! video/x-h265,stream-format=byte-stream,alignment=au ! ...
```

If you generate frames with the helper script (see
[Generating frames with GStreamer](#generating-frames-with-gstreamer)) this is
already handled (`h265parse config-interval=-1` for H.265, `h264parse
config-interval=-1` for H.264). If you write your own GStreamer pipeline, do the
same.

## Inspecting the frames

The commands below assume one access unit per file (`frame-0001.h265`,
`frame-0002.h265`, …) laid out in a directory. Adjust the paths to wherever your
frames live. The Python snippets were tested with Python 3.13.

### List the NAL units in a frame

Use this small Python script to print the NAL unit types in any frame file. It
walks Annex-B start codes (`00 00 01` / `00 00 00 01`) and reads the 6-bit HEVC NAL
type from each header.

> **Why not ffmpeg's `trace_headers` filter?** It looks like the obvious pre-built
> tool for this, but it lifts the parameter sets into container extradata and then
> feeds *both* the extradata copy and the in-band copy through the filter, so every
> keyframe appears to carry `VPS/SPS/PPS` even when the bitstream doesn't. It also
> can't parse a lone bare-slice file at all. The raw scanner below reads the bytes
> directly and has no such blind spot.

```bash
python3 - "$@" <<'PY'
import sys
names = {32:'VPS', 33:'SPS', 34:'PPS', 19:'IDR_W_RADL', 20:'IDR_N_LP',
         21:'CRA', 1:'TRAIL_R', 0:'TRAIL_N', 39:'SEI_PREFIX', 40:'SEI_SUFFIX'}
for path in sys.argv[1:]:
    data = open(path, 'rb').read()
    i, n, nals = 0, len(data), []
    while i < n - 4:
        if data[i] == 0 and data[i+1] == 0 and data[i+2] == 1:
            start = i + 3
        elif data[i] == 0 and data[i+1] == 0 and data[i+2] == 0 and data[i+3] == 1:
            start = i + 4
        else:
            i += 1; continue
        nals.append((data[start] >> 1) & 0x3F)
        i = start
    print(f"{path}: {[names.get(t, t) for t in nals]}")
PY
```

Save it as `nals.py` and run it against the first frame and a couple of later
keyframes:

```bash
python3 nals.py frames/frame-0001.h265 \
                frames/frame-0026.h265 \
                frames/frame-0051.h265
```

**Healthy output** - parameter sets accompany every keyframe:

```
frames/frame-0001.h265: ['VPS', 'SPS', 'PPS', 'SEI_PREFIX', 'IDR_N_LP']
frames/frame-0026.h265: ['VPS', 'SPS', 'PPS', 'CRA']
frames/frame-0051.h265: ['VPS', 'SPS', 'PPS', 'CRA']
```

**Broken output** - later keyframes are bare slices (this freezes in Chrome):

```
frames/frame-0001.h265: ['VPS', 'SPS', 'PPS', 'SEI_PREFIX', 'IDR_N_LP']
frames/frame-0026.h265: ['CRA']
frames/frame-0051.h265: ['CRA']
```

### Find every keyframe in a track

The sample stores each frame (access unit) in its own file, but `ffprobe` expects a
single continuous video stream. First, join the per-frame files back into one
file, in order, by appending their bytes end to end. This is called
*concatenating*, and because each file is already a complete access unit, simply
`cat`-ing them together reproduces the original elementary stream. `ffprobe`
can then walk that stream and flag which frames are keyframes.

```bash
# Concatenate the track into one raw stream (adjust the count to your frame set)
rm /tmp/gen.h265
for k in $(seq -f "%04g" 1 1500); do cat frames/frame-$k.h265 >> /tmp/gen.h265; done

# List the 1-based frame index of every keyframe
ffprobe -v error -select_streams v -show_entries frame=key_frame -of csv=p=0 /tmp/gen.h265 \
  | nl -ba | awk -F'\t' '$2 ~ /^1/ { print "keyframe at frame " $1 }'
```

With `key-int-max=25` you should see a keyframe roughly every 25 frames.

> **Note:** `ffprobe` finds *where* the keyframes are but does not tell you whether
> each one carries its parameter sets. A bare keyframe (missing `VPS/SPS/PPS`) is
> still reported as a keyframe here. To confirm the parameter sets are present, feed
> the keyframe indexes back into the NAL scanner above.

### Decode with ffmpeg (sanity check, but note the caveat)

```bash
# Concatenate the first 60 access units into one raw stream
rm -f /tmp/gen.h265
for k in $(seq -f "%04g" 1 60); do cat frames/frame-$k.h265 >> /tmp/gen.h265; done

# Decode; look for errors
ffmpeg -v error -i /tmp/gen.h265 -f null -

# Count decoded frames
ffprobe -v error -count_frames -select_streams v \
        -show_entries stream=nb_read_frames -of csv=p=0 /tmp/gen.h265
```

> **Caveat:** `ffmpeg` caches parameter sets from the first frame, so a stream with
> missing per-keyframe parameter sets will still decode cleanly here. A clean
> `ffmpeg` decode does **not** prove the stream is valid for WebRTC. Use the NAL
> inspection above to confirm keyframes carry `VPS/SPS/PPS`.

### Compare against a known-good stream

If you have a stream that is known to play correctly over WebRTC, diff its SPS
bytes against your frames to confirm resolution/profile match. A healthy reference
keyframe looks like `['VPS', 'SPS', 'PPS', 'SEI_PREFIX', 'CRA']`.

```bash
python3 - <<'PY'
def sps(path):
    data = open(path, 'rb').read(); i, n = 0, len(data)
    while i < n - 4:
        if data[i] == 0 and data[i+1] == 0 and data[i+2] == 1:
            s = i + 3
        elif data[i] == 0 and data[i+1] == 0 and data[i+2] == 0 and data[i+3] == 1:
            s = i + 4
        else:
            i += 1; continue
        if ((data[s] >> 1) & 0x3F) == 33:  # SPS
            j = s
            while j < n - 3 and not (data[j] == 0 and data[j+1] == 0 and
                    (data[j+2] == 1 or data[j+2] == 0)):
                j += 1
            return data[s:j]
        i = s
    return b''
print("reference SPS:", sps("reference/frame-0001.h265").hex())
print("your      SPS:", sps("frames/frame-0001.h265").hex())
PY
```

## Debugging from the receiver (Chrome)

The frame-inspection tools above look at the bitstream you send. It is also worth
confirming what the *receiver* sees. This separates a bad bitstream from a
transport or negotiation problem, and lets you watch decode counters stall in real
time.

### `chrome://webrtc-internals`

Open `chrome://webrtc-internals` in a new tab **before** you start the session, then
join. It dumps live `getStats()` for every active peer connection. Run your stream,
let it freeze at that exact spot, and immediately read the graphs under the active
inbound video track (`inbound-rtp` … `kind=video`):

- **`framesReceived` / `framesDecoded`**: if `framesReceived` keeps climbing but
  `framesDecoded` stops completely (or sticks at 1), the browser is downloading the
  packets but refusing to parse them. That points at the bitstream (missing
  parameter sets, bad keyframes), not the network.
- **`keyFramesDecoded`**: stays at 1 when later keyframes can't be decoded.
- **`pliCount` / `firCount`**: watch whether these spike or climb continuously right
  when the freeze happens. If they do, the browser is repeatedly begging for a fresh
  keyframe because the frame at that spot broke the decoder chain and it never
  recovers, the classic missing-per-keyframe-parameter-set signature (`nackCount`
  and near-zero `packetsLost` confirm the network is fine).
- **`jitterBufferDelay`**: if this spikes to an abnormally high ceiling at the
  freeze point, suspect a timestamp or packet-ordering problem breaking the playback
  clock rather than a decode problem (see [Freezes only when frames loop or
  repeat](#freezes-only-when-frames-loop-or-repeat)).
- **`freezeCount` / `totalFreezesDuration`**: confirms and quantifies the freeze.

You can also use the "Create Dump" button at the top of the page to save the full
stats log and share it when asking for help.

### Freezes only when frames loop or repeat

If the freeze appears only after a fixed set of frames loops, or when you re-send
the same buffers, the bitstream itself may be fine and the problem is how frames are
fed to the sender:

- **Start each loop on a keyframe.** When the frame set loops back to the beginning,
  the first frame of the loop must be a real keyframe (IDR/CRA for H.265, IDR for
  H.264). If the loop restarts on a P-frame, the decoder has no reference and stalls.
- **Keep RTP timestamps monotonically increasing.** Recalculate the timestamp for
  every frame sent, even when re-sending the same underlying buffer. Never reuse the
  original file timestamps when looping or injecting frames, a timestamp that goes
  backward or repeats breaks the playback clock (and shows up as a `jitterBufferDelay`
  spike above).
- **Inspect the frame at the freeze point.** Use the NAL scanner or `ffprobe` above
  on the specific frame where playback stalls to rule out an undecodable frame type
  or a mid-stream resolution/profile change.

### Verbose decoder logs

For decoder-level errors that `webrtc-internals` doesn't surface:

- **Chrome debug logs**: enable verbose logging to capture RTP depacketizer and
  decoder messages. Google documents how to turn on debug logging at
  <https://support.google.com/chrome/a/answer/6271282?hl=en>; in short, launch
  Chrome with `--enable-logging=stderr --vmodule="*/webrtc/*=2,*video*=2"` to get
  WebRTC and video-decode messages.
- **Codec support**: HEVC/H.265 over WebRTC is only decodable in Chrome where the
  platform provides a hardware HEVC decoder. Confirm the receiver actually supports
  it (H.264 is a good control: if H.264 frames play and H.265 don't, suspect codec
  support before the bitstream).

## Other things to check

- **Wrong working directory.** Samples typically open frames with relative paths
  (e.g. `./h265SampleFrames/frame-0001.h265`), so run the binary from the directory
  those paths resolve against. A `readFile(): operation returned status code:
  0x00000009` (`STATUS_OPEN_FILE_FAILED`) usually means the frames aren't where the
  binary is looking.
- **Codec mismatch.** The codec the frames were encoded as must match the codec
  advertised for the video track. If H.265 frames are sent but the track was
  negotiated as H.264 (or vice versa), the payload won't decode. The negotiated
  codec lives in the transceiver's `fmtp` line in the SDP, so inspect the SDP to
  confirm it matches your frames. In `chrome://webrtc-internals`, expand the peer
  connection and look at the offer/answer SDP (or the codec shown on the
  `inbound-rtp` stats) — the `a=rtpmap`/`a=fmtp` entries for the payload type name
  the codec (`H264` vs `H265`) and its parameters. An H.265 stream should map to an
  `H265` payload type; if it maps to `H264`, the transceiver is mislabeled.
- **Frame count vs. files on disk.** Samples loop over a fixed frame count (e.g.
  `NUMBER_OF_H265_FRAME_FILES` in `samples/common/Samples.h`). Make sure that many
  numbered files actually exist: `ls frames/frame-*.h265 | wc -l`.
- **B-frames.** Keep B-frames off for the simple one-file-per-access-unit playback
  these samples assume, since frame reordering complicates timestamp handling. In
  GStreamer that's `option-string=bframes=0` for `x265enc` and `bframes=0` for
  `x264enc`.

## Generating frames with GStreamer

You can produce test frames with a GStreamer pipeline. The key requirements for
WebRTC-playable H.265 are:

- **Byte-stream, AU-aligned output** (`stream-format=byte-stream,alignment=au`) so
  each file is one complete access unit.
- **`config-interval=-1` on the parser** so `VPS/SPS/PPS` are repeated before every
  keyframe.
- **B-frames disabled** (`option-string=bframes=0`).

A minimal single-track pipeline writing one frame (access unit) per file:

```bash
gst-launch-1.0 -e \
    videotestsrc pattern=ball num-buffers=1500 ! \
    videoconvert ! \
    video/x-raw,format=I420,width=1280,height=720,framerate=25/1 ! \
    x265enc speed-preset=veryfast tune=zerolatency key-int-max=25 option-string=bframes=0 ! \
    h265parse config-interval=-1 ! \
    video/x-h265,stream-format=byte-stream,alignment=au ! \
    multifilesink location="frames/frame-%04d.h265" index=1
```

For H.264, swap the encoder/parser/caps:

```
    x264enc bframes=0 speed-preset=veryfast tune=zerolatency key-int-max=25 byte-stream=TRUE ! \
    h264parse config-interval=-1 ! \
    video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline ! \
    multifilesink location="frames/frame-%04d.h264" index=1
```

### Using the helper script

[`scripts/generate_multi_track_frames.sh`](../scripts/generate_multi_track_frames.sh)
wraps the above to generate four tracks, each with a different `videotestsrc`
pattern and a "Track #N" overlay. Run it from the directory your sample resolves
frame paths against (e.g. `build/samples/`):

```bash
../../scripts/generate_multi_track_frames.sh h265   # or h264
```

This writes `h265SampleFrames_track0..3/` (one pattern per track). After
regenerating, spot-check a keyframe with the NAL inspector above to confirm it
carries `VPS/SPS/PPS`.
