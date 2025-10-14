COMPONENT_ADD_INCLUDEDIRS := ../../../../src/include \
                            ../../../../open-source/include \
                            ../../../../src/source/Ice \
                            ../../../../src/source/PeerConnection \
                            ../../../../src/source/Signaling \
                            ../../../../src/source/Srtp \
                            ../../../../src/source/Sctp

CFLAGS += -DKVS_BUILD_WITH_MBEDTLS \
          -DKVS_USE_POOL_ALLOCATOR \
          -DESP_PLATFORM \
          -DAWS_IOT_SDK \
          -DLOG_CLASS=\"WebRTC\"