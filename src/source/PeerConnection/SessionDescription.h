/*******************************************
SessionDescription internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_SESSIONDESCRIPTION__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_SESSIONDESCRIPTION__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

#define SESSION_DESCRIPTION_INIT_LINE_ENDING "\\r\\n"

#define SESSION_DESCRIPTION_INIT_TEMPLATE_HEAD "{\"type\": \"%s\", \"sdp\": \""
#define SESSION_DESCRIPTION_INIT_TEMPLATE_TAIL "\"}"

#define SDP_OFFER_VALUE "offer"
#define SDP_ANSWER_VALUE "answer"

#define MEDIA_SECTION_AUDIO_VALUE "audio"
#define MEDIA_SECTION_VIDEO_VALUE "video"

#define SDP_TYPE_KEY "type"
#define SDP_KEY  "sdp"
#define CANDIDATE_KEY  "candidate"
#define SSRC_KEY  "ssrc"
#define BUNDLE_KEY  "BUNDLE"

#define H264_VALUE "H264/90000"
#define OPUS_VALUE "opus/48000"
#define VP8_VALUE "VP8/90000"
#define MULAW_VALUE "PCMU/8000"
#define ALAW_VALUE "PCMA/8000"
#define RTX_VALUE "rtx/90000"
#define RTX_CODEC_VALUE "apt="

#define DEFAULT_PAYLOAD_MULAW (UINT64) 0
#define DEFAULT_PAYLOAD_ALAW (UINT64) 8
#define DEFAULT_PAYLOAD_OPUS (UINT64) 111
#define DEFAULT_PAYLOAD_VP8 (UINT64) 96
#define DEFAULT_PAYLOAD_H264 (UINT64) 125

#define DEFAULT_PAYLOAD_MULAW_STR (PCHAR) "0"
#define DEFAULT_PAYLOAD_ALAW_STR (PCHAR) "8"

#define DTLS_ROLE_ACTPASS (PCHAR) "actpass"
#define DTLS_ROLE_ACTIVE (PCHAR) "active"

#define VIDEO_CLOCKRATE (UINT64) 90000
#define OPUS_CLOCKRATE (UINT64) 48000
#define PCM_CLOCKRATE (UINT64) 8000

STATUS setPayloadTypesFromOffer(PHashTable, PHashTable, PSessionDescription);
STATUS setPayloadTypesForOffer(PHashTable);

STATUS setTransceiverPayloadTypes(PHashTable, PHashTable, PDoubleList);
STATUS populateSessionDescription(PKvsPeerConnection, PSessionDescription, PSessionDescription);
STATUS reorderTransceiverByRemoteDescription(PKvsPeerConnection, PSessionDescription);
STATUS setReceiversSsrc(PSessionDescription, PDoubleList);

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_SESSIONDESCRIPTION__ */
