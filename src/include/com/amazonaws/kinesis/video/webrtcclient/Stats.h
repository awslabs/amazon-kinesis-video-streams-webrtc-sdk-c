/*
 * Profiler public include file
 */
#ifndef __KINESIS_VIDEO_WEBRTCCLIENT_STATS_INCLUDE__
#define __KINESIS_VIDEO_WEBRTCCLIENT_STATS_INCLUDE__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////
// Public headers
////////////////////////////////////////////////////

#define MAX_CANDIDATE_ID_LENGTH                   8U
#define MAX_STATS_ADDRESS_LENGTH                  16U
#define MAX_RELAY_PROTOCOL_LENGTH                 8U
#define MAX_TLS_VERSION_LENGTH                    8U
#define MAX_DTLS_CIPHER_LENGTH                    64U
#define MAX_SRTP_CIPHER_LENGTH                    64U
#define MAX_TLS_GROUP_LENGHTH                     32U
#define MAX_PROTOCOL_LENGTH                       8U

#define MAX_STATS_STRING_LENGTH                   255U

/**
 * @brief Type of Stats object requested by the application
 * Reference https://www.w3.org/TR/webrtc-stats/#rtcstatstype-str*
 */
typedef enum {
    RTC_STATS_TYPE_CANDIDATE_PAIR, //!< ICE candidate pair statistics related to the RTCIceTransport objects
    RTC_STATS_TYPE_CERTIFICATE, //!< Information about a certificate used by an RTCIceTransport.
    RTC_STATS_TYPE_CODEC, //!< Stats for a codec that is currently being used by RTP streams being sent or received by this RTCPeerConnection object.
    RTC_STATS_TYPE_ICE_SERVER, //!< Information about the connection to an ICE server (e.g. STUN or TURN)
    RTC_STATS_TYPE_CSRC, //!< Stats for a contributing source (CSRC) that contributed to an inbound RTP stream.
    RTC_STATS_TYPE_DATA_CHANNEL, //!< Stats related to each RTCDataChannel id
    RTC_STATS_TYPE_INBOUND_RTP, //!< Statistics for an inbound RTP stream that is currently received with this RTCPeerConnection object
    RTC_STATS_TYPE_LOCAL_CANDIDATE, //!< ICE local candidate statistics related to the RTCIceTransport objects
    RTC_STATS_TYPE_OUTBOUND_RTP, //!< Statistics for an outbound RTP stream that is currently received with this RTCPeerConnection object
    RTC_STATS_TYPE_PEER_CONNECTION, //!< Statistics related to the RTCPeerConnection object.
    RTC_STATS_TYPE_RECEIVER, //!< Statistics related to a specific receiver and the corresponding media-level metrics
    RTC_STATS_TYPE_REMOTE_CANDIDATE, //!< ICE remote candidate statistics related to the RTCIceTransport objects
    RTC_STATS_TYPE_REMOTE_INBOUND_RTP, //!< Statistics for the remote endpoint's inbound RTP stream corresponding to an outbound stream that
                        //!< is currently sent with this RTCPeerConnection object
    RTC_STATS_TYPE_REMOTE_OUTBOUND_RTP, //!< Statistics for the remote endpoint's outbound RTP stream corresponding to an inbound stream that
                         //!< is currently sent with this RTCPeerConnection object
    RTC_STATS_TYPE_SENDER, //!< Statistics related to a specific RTCRtpSender and the corresponding media-level metrics
    RTC_STATS_TYPE_TRACK, //!< Statistics related to a specific MediaStreamTrack's attachment to an RTCRtpSender
    RTC_STATS_TYPE_TRANSPORT, //!< Transport statistics related to the RTCPeerConnection object.
    RTC_STATS_TYPE_SCTP_TRANSPORT, //!< SCTP transport statistics related to an RTCSctpTransport object
    RTC_STATS_TYPE_TRANSCEIVER, //!< Statistics related to a specific RTCRtpTransceiver
    RTC_STATS_TYPE_RTC_ALL //!< Report all supported stats
} RTC_STATS_TYPE;

/**
 * @brief Indicates computing states in the checklist
 * Reference: https://www.w3.org/TR/webrtc-stats/#rtcstatsicecandidatepairstate-enum
 */
typedef enum {
    ICE_CANDIDATE_PAIR_STATE_FROZEN         = 0,
    ICE_CANDIDATE_PAIR_STATE_WAITING        = 1,
    ICE_CANDIDATE_PAIR_STATE_IN_PROGRESS    = 2,
    ICE_CANDIDATE_PAIR_STATE_SUCCEEDED      = 3,
    ICE_CANDIDATE_PAIR_STATE_FAILED         = 4,
} ICE_CANDIDATE_PAIR_STATE;

/**
 * @brief Set details of the IceAgent based on STUN_ATTRIBUTE_TYPE_USE_CANDIDATE flag
 * Reference: https://www.w3.org/TR/webrtc/#rtcicerole
 */
typedef enum {
    RTC_ICE_ROLE_UNKNOWN,    //!< An agent whose role is undetermined. Initial state
    RTC_ICE_ROLE_CONTROLLING,//!< A controlling agent. The ICE agent that is responsible for selecting the final choice of candidate pairs
    RTC_ICE_ROLE_CONTROLLED  //!< A controlled agent. The iCE agent waits for the controlling agent to select the final choice of candidate pairs
} RTC_ICE_ROLE;

/**
 * @brief DTLS Transport State
 * Reference: https://www.w3.org/TR/webrtc/#rtcdtlstransport-interface
 */
typedef enum {
    RTC_DTLS_TRANSPORT_STATE_STATS_NEW,       //!< DTLS has not started negotiating yet
    RTC_DTLS_TRANSPORT_STATE_STATS_CONNECTING,//!< DTLS is in the process of negotiating a secure connection
    RTC_DTLS_TRANSPORT_STATE_STATS_CONNECTED, //!< DTLS has completed negotiation of a secure connection
    RTC_DTLS_TRANSPORT_STATE_STATS_CLOSED,    //!< The transport has been closed intentionally
    RTC_DTLS_TRANSPORT_STATE_STATS_FAILED     //!< The transport has failed as the result of an error
} RTC_DTLS_TRANSPORT_STATE_STATS;

/**
 * @brief Defines reasons for quality limitation for sending streams
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#rtcqualitylimitationreason-enum
 */
typedef enum {
    RTC_QUALITY_LIMITATION_REASON_NONE, //!< Resolution as expected. Default value
    RTC_QUALITY_LIMITATION_REASON_BANDWIDTH, //!< Reason for limitation is congestion cues during bandwidth estimation
    RTC_QUALITY_LIMITATION_REASON_CPU,//!< The resolution and/or framerate is primarily limited due to CPU load.
    RTC_QUALITY_LIMITATION_REASON_OTHER, //!< Limitation due to reasons other than above
} RTC_QUALITY_LIMITATION_REASON;

/**
 * @brief Record of duration and quality reason state
 */
typedef struct {
    UINT64 durationInSeconds; //!< Time (seconds) spent in each state
    RTC_QUALITY_LIMITATION_REASON qualityLimitationReason; //!< Quality limitation reason
} QualityLimitationDurationsRecord, PQualityLimitationDurationsRecord;

/**
 * @brief Record of total number of packets sent per DSCP. Used by RTCOutboundRtpStreamStats
 * object
 */
typedef struct {
    CHAR dscp[MAX_STATS_STRING_LENGTH + 1]; //!< DSCP String
    UINT64 totalNumberOfPacketsSent; //!< Number of packets sent
} DscpPacketsSentRecord, *PDscpPacketsSentRecord;

/**
 * @brief RtcIceCandidatePairStats Stats related to the local-remote ICE candidate pair
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#candidatepair-dict*
 */

typedef struct {
    CHAR transportId[MAX_STATS_STRING_LENGTH + 1]; //!< ID of object that was inspected for RTCTransportStats
    CHAR localCandidateId[MAX_CANDIDATE_ID_LENGTH + 1]; //!< Local candidate that is inspected in RTCIceCandidateStats
    CHAR remoteCandidateId[MAX_CANDIDATE_ID_LENGTH + 1]; //!< Remote candidate that is inspected in RTCIceCandidateStats
    ICE_CANDIDATE_PAIR_STATE state; //!< State of checklist for the local-remote candidate pair
    BOOL nominated; //!< Flag is TRUE if the agent is a controlling agent and FALSE otherwise. The agent role is based on the
                    //!< STUN_ATTRIBUTE_TYPE_USE_CANDIDATE flag
    NullableUint32 circuitBreakerTriggerCount; //!< Represents number of times circuit breaker is triggered during media transmission
                                       //!< It is undefined if the user agent does not use this
    UINT32 packetsDiscardedOnSend; //!< Total number of packets discarded for candidate pair due to socket errors,
    UINT64 packetsSent; //!< Total number of packets sent on this candidate pair;
    UINT64 packetsReceived; //!< Total number of packets received on this candidate pair
    UINT64 bytesSent; //!< Total number of bytes (minus header and padding) sent on this candidate pair
    UINT64 bytesReceived; //!< Total number of bytes (minus header and padding) received on this candidate pair
    UINT64 lastPacketSentTimestamp; //!< Represents the timestamp at which the last packet was sent on this particular
                                    //!< candidate pair, excluding STUN packets.
    UINT64 lastPacketReceivedTimestamp; //!< Represents the timestamp at which the last packet was sent on this particular
                                        //!< candidate pair, excluding STUN packets.
    UINT64 firstRequestTimestamp; //!< Represents the timestamp at which the first STUN request was sent on this particular candidate pair.
    UINT64 lastRequestTimestamp; //!< Represents the timestamp at which the last STUN request was sent on this particular candidate pair.
                                 //!< The average interval between two consecutive connectivity checks sent can be calculated:
                                 //! (lastRequestTimestamp - firstRequestTimestamp) / requestsSent.
    UINT64 lastResponseTimestamp; //!< Represents the timestamp at which the last STUN response was received on this particular candidate pair.
    DOUBLE totalRoundTripTime; //!< The sum of all round trip time (seconds) since the beginning of the session, based
                               //!< on STUN connectivity check responses (responsesReceived), including those that reply to requests
                               //!< that are sent in order to verify consent. The average round trip time can be computed from
                               //!< totalRoundTripTime by dividing it by responsesReceived.
    DOUBLE currentRoundTripTime; //!< Latest round trip time (seconds)
    DOUBLE availableOutgoingBitrate; //!< Total available bit rate for all the outgoing RTP streams on this candidate pair. Calculated by underlying
                                     //!< congestion control
    DOUBLE availableIncomingBitrate; //!< Total available bit rate for all the outgoing RTP streams on this candidate pair. Calculated by underlying
                                     //!< congestion control
    UINT64 requestsReceived; //!< Total number of connectivity check requests received (including retransmission)
    UINT64 requestsSent; //!< The total number of connectivity check requests sent (without retransmissions).
    UINT64 responsesReceived; //!< The total number of connectivity check responses received.
    UINT64 responsesSent; //!< The total number of connectivity check responses sent.
    UINT64 retransmissionsReceived; //!< The total number of connectivity check request retransmissions received
    UINT64 retransmissionsSent; //!< The total number of connectivity check request retransmissions sent.
    UINT64 consentRequestsSent; //!< The total number of consent requests sent.
    UINT64 consentExpiredTimestamp; //!< The timestamp at which the latest valid STUN binding response expired
    UINT64 bytesDiscardedOnSend; //!< Total number of bytes for this candidate pair discarded due to socket errors
} RtcIceCandidatePairStats, *PRtcIceCandidatePairStats;

/**
 * @brief: RtcIceServerStats Stats related to the ICE Server
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#ice-server-dict*
 */
typedef struct {
    CHAR url[MAX_STATS_STRING_LENGTH + 1]; //!< STUN/TURN server URL
    CHAR protocol[MAX_STATS_STRING_LENGTH + 1]; //!< Valid values: UDP, TCP
    INT32 port; //!< Port number used by client
    UINT64 totalRequestsSent; //!< Total amount of requests that have been sent to the server
    UINT64 totalResponsesReceived; //!< Total number of responses received from the server
    UINT64 totalRoundTripTime; //!< Sum of RTTs of all the requests for which response has been received
} RtcIceServerStats, *PRtcIceServerStats;

/**
 * @brief: RtcIceCandidateStats Stats related to a specific candidate in a pair
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#icecandidate-dict*
 */

typedef struct {
    CHAR url[MAX_STATS_STRING_LENGTH + 1]; //!< For local candidates this is the URL of the ICE server from which the candidate was obtained
    CHAR transportId[MAX_STATS_STRING_LENGTH + 1]; //!< ID of object that was inspected for RTCTransportStats
    BYTE address[MAX_STATS_ADDRESS_LENGTH]; //!< IPv4 or IPv6 address of the candidate
    CHAR protocol[MAX_PROTOCOL_LENGTH + 1]; //!< Valid values: UDP, TCP
    CHAR relayProtocol[MAX_RELAY_PROTOCOL_LENGTH + 1]; //!< Protocol used by endpoint to communicate with TURN server.
                                                       //!< Valid values: UDP, TCP, TLS
    INT32 priority; //!< Computed using the formula in https://tools.ietf.org/html/rfc5245#section-15.1
    INT32 port; //!< Port number of the candidate
} RtcIceCandidateStats, *PRtcIceCandidateStats;

/**
 * @brief RtcTransportStats Represents the stats corresponding to an RTCDtlsTransport and
 * its underlying RTCIceTransport
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#transportstats-dict*
 */
typedef struct {
    CHAR rtcpTransportStatsId[MAX_STATS_STRING_LENGTH + 1]; //!< ID of the transport that gives stats for the RTCP component
    CHAR selectedCandidatePairId[MAX_STATS_STRING_LENGTH + 1]; //!< ID of the object inspected to produce RtcIceCandidatePairStats
    CHAR localCertificateId[MAX_STATS_STRING_LENGTH + 1]; //!< For components where DTLS is negotiated, give local certificate
    CHAR remoteCertificateId[MAX_STATS_STRING_LENGTH + 1]; //!< For components where DTLS is negotiated, give remote certificate
    CHAR tlsVersion[MAX_TLS_VERSION_LENGTH + 1]; //!< For components where DTLS is negotiated, the TLS version agreed
    CHAR dtlsCipher[MAX_DTLS_CIPHER_LENGTH + 1]; //!< Descriptive name of the cipher suite used for the DTLS transport.
                                                 //!< Acceptable values: https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml#tls-parameters-4
    CHAR srtpCipher[MAX_SRTP_CIPHER_LENGTH + 1]; //!< Descriptive name of the protection profile used for the SRTP transport
                                                 //!< Acceptable values: https://www.iana.org/assignments/srtp-protection/srtp-protection.xhtml
    CHAR tlsGroup[MAX_TLS_GROUP_LENGHTH + 1]; //!< Descriptive name of the group used for the encryption
                                              //!< Acceptable values: https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml#tls-parameters-8
    RTC_ICE_ROLE iceRole; //!< Set to the current value of the "role" attribute of the underlying RTCDtlsTransport's "transport"
    RTC_DTLS_TRANSPORT_STATE_STATS dtlsState; //!< Set to the current value of the "state" attribute of the underlying RTCDtlsTransport
    UINT64 packetsSent; //!< Total number of packets sent over the transport
    UINT64 packetsReceived; //!< Total number of packets received over the transport
    UINT64 bytesSent; //!< The total number of payload bytes sent on this PeerConnection (excluding header and padding)
    UINT64 bytesReceived; //!< The total number of payload bytes received on this PeerConnection (excluding header and padding)
    UINT32 selectedCandidatePairChanges; //!< The number of times that the selected candidate pair of this transport has changed
} RtcTransportStats, *PRtcTransportStats;

/**
 * @brief RtcOutboundRtpStreamStats
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#outboundrtpstats-dict*
 */
typedef struct {
    BOOL voiceActivityFlag; //!< Only valid for audio. Whether the last RTP packet sent contained voice activity or not based on the presence
                            //!< of the V bit in the extension header
    CHAR trackId[MAX_STATS_STRING_LENGTH + 1]; //!< ID representing current track attached to the sender of the stream
    CHAR mediaSourceId[MAX_STATS_STRING_LENGTH + 1]; //!< ID representing the current media source
    CHAR senderId[MAX_STATS_STRING_LENGTH + 1]; //!< The stats ID used to look up the RTCAudioSenderStats or RTCVideoSenderStats object sending this stream
    CHAR remoteId[MAX_STATS_STRING_LENGTH + 1]; //!< ID to look up the remote RTCRemoteInboundRtpStreamStats object for the same SSRC
    CHAR rid[MAX_STATS_STRING_LENGTH + 1]; //!< Exposes the rid encoding parameter of this RTP stream if it has been set, otherwise it is undefined
    CHAR encoderImplementation[MAX_STATS_STRING_LENGTH + 1]; //!< Identifies the encoder implementation used.
    UINT32 packetsDiscardedOnSend; //!< Total number of RTP packets for this SSRC that have been discarded due to socket errors
    UINT32 framesSent; //!< Only valid for video. Represents the total number of frames sent on this RTP stream
    UINT32 hugeFramesSent; //!< Only valid for video. Represents the total number of huge frames sent by this RTP stream
                           //!< Huge frames have an encoded size at least 2.5 times the average size of the frames
    UINT32 framesEncoded; //!< Only valid for video. It represents the total number of frames successfully encoded for this RTP media stream
    UINT32 keyFramesEncoded; //!< Only valid for video. It represents the total number of key frames encoded successfully in the RTP Stream
    UINT32 framesDiscardedOnSend; //!< Total number of video frames that have been discarded for this SSRC due to socket errors
    UINT32 frameWidth; //!< Only valid for video. Represents the width of the last encoded frame
    UINT32 frameHeight; //!< Only valid for video. Represents the height of the last encoded frame
    UINT32 frameBitDepth; //!< Only valid for video. Represents the bit depth per pixel of the last encoded frame. Typical values: 24, 30, 36
    UINT32 nackCount; //!< Count the total number of Negative ACKnowledgement (NACK) packets received by this sender.
    UINT32 firCount; //!< Only valid for video. Count the total number of Full Intra Request (FIR) packets received by this sender
    UINT32 pliCount; //!< Only valid for video. Count the total number of Picture Loss Indication (PLI) packets received by this sender
    UINT32 sliCount; //!< Only valid for video. Count the total number of Slice Loss Indication (SLI) packets received by this sender
    UINT32 qualityLimitationResolutionChanges; //!< Only valid for video. The number of times that the resolution has changed because we are quality limited
    INT32 fecPacketsSent; //!< Total number of RTP FEC packets sent for this SSRC. Can also be incremented while sending FEC packets in band
    UINT64 lastPacketSentTimestamp; //!< The timestamp at which the last packet was sent for this SSRC
    UINT64 headerBytesSent; //!< Total number of RTP header and padding bytes sent for this SSRC
    UINT64 bytesDiscardedOnSend; //!< Total number of bytes for this SSRC that have been discarded due to socket errors
    UINT64 retransmittedPacketsSent; //!< The total number of packets that were retransmitted for this SSRC
    UINT64 retransmittedBytesSent; //!< The total number of PAYLOAD bytes retransmitted for this SSRC
    UINT64 targetBitrate; //!< Current target TIAS bitrate configured for this particular SSRC
    UINT64 totalEncodedBytesTarget; //!< Increased by the target frame size in bytes every time a frame has been encoded
    UINT64 framesPerSecond; //!< Only valid for video. The number of encoded frames during the last second
    UINT64 qpSum; //!< Only valid for video. The sum of the QP values of frames encoded by this sender. QP value depends on the codec
    UINT64 totalSamplesSent; //!< Only valid for audio. The total number of samples that have been sent over this RTP stream
    UINT64 samplesEncodedWithSilk; //!< Only valid for audio and when the audio codec is Opus. Represnets only SILK portion of codec
    UINT64 samplesEncodedWithCelt; //!< Only valid for audio and when the audio codec is Opus. Represnets only CELT portion of codec
    UINT64 totalEncodeTime; //!< Total number of seconds that has been spent encoding the framesEncoded frames of the stream
    UINT64 totalPacketSendDelay; //!< Total time (seconds) packets have spent buffered locally before being transmitted onto the network
    UINT64 averageRtcpInterval; //!< The average RTCP interval between two consecutive compound RTCP packets
    QualityLimitationDurationsRecord qualityLimitationDurations; //!< Total time (seconds) spent in each reason state
    DscpPacketsSentRecord perDscpPacketsSent; //!< Total number of packets sent for this SSRC, per DSCP
    RTC_QUALITY_LIMITATION_REASON qualityLimitationReason; //!< Only valid for video.
} RtcOutboundRtpStreamStats, *PRtcOutboundRtpStreamStats;

/**
 * @brief RTCRemoteInboundRtpStreamStats Represents the remote endpoint's measurement metrics for a particular incoming RTP stream
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#remoteinboundrtpstats-dict*
 */
typedef struct {
    CHAR localId[MAX_STATS_STRING_LENGTH + 1]; //!< Used to look up RTCOutboundRtpStreamStats for the SSRC
    UINT64 roundTripTime; //!< Estimated round trip time (seconds) for this SSRC based on the RTCP timestamps
    UINT64 totalRoundTripTime; //!< The cumulative sum of all round trip time measurements in seconds since the beginning of the session
    UINT64 fractionLost; //!< The fraction packet loss reported for this SSRC
    UINT64 reportsReceived; //!< Total number of RTCP RR blocks received for this SSRC
    UINT64 roundTripTimeMeasurements; //!< Total number of RTCP RR blocks received for this SSRC that contain a valid round trip time
} RtcRemoteInboundRtpStreamStats, *PRtcRemoteInboundRtpStreamStats;

/**
 * @brief SignalingClientMetrics Represent the stats related to the KVS WebRTC SDK signaling client
 */
typedef struct {
    UINT64 apiCallLatency; //!< Latency (milliseconds) incurred per backend API call
    UINT64 signalingClientUptime; //!< Client uptime (milliseconds). Timestamp will be recorded at every SIGNALING_CLIENT_STATE_CONNECTED
    UINT64 connectionDuration; //!< Duration of connection (milliseconds)
    UINT64 numberOfMessagesSent; //!< Number of messages sent by the signaling client
    UINT64 numberOfMessagesReceived; //!< Number of messages received by the signaling client
    UINT64 iceRefreshCount; //!< Number of times the ICE is refreshed
    UINT64 numberOfRecoverableErrors; //!< Number of recoverable errors. Will be a static count returned when requested
    UINT64 numberOfReconnects; //!< Number of reconnects in the session
} SignalingClientStats, PSignalingClientStats;

/**
 * @brief RTCStatsObject Represents an object passed in by the application developer which will
 * be populated internally
 */
typedef struct {
    RtcIceCandidatePairStats iceCandidatePairStats; //!< ICE Candidate Pair  stats object
    RtcIceCandidateStats localRtcIceCandidateStats; //!< Local candidate stats. Reference in Stats.h
    RtcIceCandidateStats remoteRtcIceCandidateStats; //!< Remote candidate stats. Reference in Stats.h
    RtcIceServerStats iceServerStats; //!< ICE Server Pair stats object
    RtcTransportStats transportStats; //!< Transport stats object
    RtcOutboundRtpStreamStats remoteOutboundRtpStreamStats; //!< Outbound RTP Stream stats object
    RtcRemoteInboundRtpStreamStats remoteInboundRtpStreamStats; //!< Inbound RTP Stream stats object
} RtcStatsObject, *PRtcStatsObject;

#ifdef  __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTCCLIENT_STATS_INCLUDE__ */
