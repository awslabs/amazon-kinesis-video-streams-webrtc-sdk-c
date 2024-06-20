prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
includedir=${prefix}/include
libdir=${exec_prefix}/@CMAKE_INSTALL_LIBDIR@

Name: KVS-libkvsWebRtcSignalingClient
Description: KVS C WebRTC Library
Version: @KINESIS_VIDEO_WEBRTC_C_VERSION@
Cflags: -I${includedir}
Libs: -L${libdir} -lkvsWebrtcSignalingClient
