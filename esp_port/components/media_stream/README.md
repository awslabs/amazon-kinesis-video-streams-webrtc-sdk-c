# Media Stream

This component provides a media stream interface for the KVS SDK for ESP.

## Components

- `OpusAudioPlayer`: Plays a single frame of OPUS. Internally, the frame is decoded and inserted into a ring buffer. The special i2s task is used to read the ring buffer and play the audio.
- `H264FrameGrabber`: Grabs one h264 encoded frame. Camera frames are captured, encoded using H264 encoder and continuously inserted into a frame_queue.
- `OpusFrameGrabber`: Grabs one frame of OPUS from the frame queue. The i2s data is recorded by the module in a ring buffer, encoded using the Opus encoder and continuously inserted into a frame_queue.

## Usage

- Please refer to the header files in media_stream/include for the API descriptions.
