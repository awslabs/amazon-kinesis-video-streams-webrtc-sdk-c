/*
 * Profiler public include file
 */
#ifndef __KINESIS_VIDEO_WEBRTCCLIENT_STATS_INCLUDE__
#define __KINESIS_VIDEO_WEBRTCCLIENT_STATS_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////
/// Stats related string lengths
/////////////////////////////////////////////////////

/*! \addtogroup StatsNameLengths
 * Lengths of some string members of different structures
 *  @{
 */

/**
 * Maximum allowed candidate ID length
 */
#define MAX_CANDIDATE_ID_LENGTH 9U

/**
 * Maximum allowed relay protocol length
 */
#define MAX_RELAY_PROTOCOL_LENGTH 8U

/**
 * Maximum allowed TLS version length
 */
#define MAX_TLS_VERSION_LENGTH 8U

/**
 * Maximum allowed DTLS cipher length
 */
#define MAX_DTLS_CIPHER_LENGTH 64U

/**
 * Maximum allowed SRTP cipher length
 */
#define MAX_SRTP_CIPHER_LENGTH 64U

/**
 * Maximum allowed TLS group length
 */
#define MAX_TLS_GROUP_LENGHTH 32U

/**
 * Maximum allowed maximum protocol length (allowed values: tcp, udp)
 */
#define MAX_PROTOCOL_LENGTH 8U

/**
 * Maximum allowed length of IP address string
 */
#define IP_ADDR_STR_LENGTH 45U

/**
 * Maximum allowed generic length used in DOMString
 */
#define MAX_STATS_STRING_LENGTH 255U
/*!@} */

/**
 * @brief DOMString type is used to store strings of size 256 bytes (inclusive of '\0' character
 *
 * Reference: https://heycam.github.io/webidl/#idl-DOMString
 */
typedef CHAR DOMString[MAX_STATS_STRING_LENGTH + 1];

/**
 * @brief The DOMHighResTimeStamp type is used to store a time value in milliseconds, measured relative from the time origin, global monotonic clock,
 * or a time value that represents a duration between two DOMHighResTimeStamps.
 *
 * Reference: https://www.w3.org/TR/hr-time-2/#sec-domhighrestimestamp
 */
typedef UINT64 DOMHighResTimeStamp;

/**
 * Type of ICE Candidate
 */
typedef enum {
    ICE_CANDIDATE_TYPE_HOST = 0,             //!< ICE_CANDIDATE_TYPE_HOST
    ICE_CANDIDATE_TYPE_PEER_REFLEXIVE = 1,   //!< ICE_CANDIDATE_TYPE_PEER_REFLEXIVE
    ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE = 2, //!< ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE
    ICE_CANDIDATE_TYPE_RELAYED = 3,          //!< ICE_CANDIDATE_TYPE_RELAYED
} ICE_CANDIDATE_TYPE;

/**
 * @brief Type of Stats object requested by the application
 * Reference https://www.w3.org/TR/webrtc-stats/#rtcstatstype-str*
 */
typedef enum {
    RTC_STATS_TYPE_CANDIDATE_PAIR, //!< ICE candidate pair statistics related to the RTCIceTransport objects
    RTC_STATS_TYPE_CERTIFICATE,    //!< Information about a certificate used by an RTCIceTransport.
    RTC_STATS_TYPE_CODEC, //!< Stats for a codec that is currently being used by RTP streams being sent or received by this RTCPeerConnection object.
    RTC_STATS_TYPE_ICE_SERVER,          //!< Information about the connection to an ICE server (e.g. STUN or TURN)
    RTC_STATS_TYPE_CSRC,                //!< Stats for a contributing source (CSRC) that contributed to an inbound RTP stream.
    RTC_STATS_TYPE_DATA_CHANNEL,        //!< Stats related to each RTCDataChannel id
    RTC_STATS_TYPE_INBOUND_RTP,         //!< Statistics for an inbound RTP stream that is currently received with this RTCPeerConnection object
    RTC_STATS_TYPE_LOCAL_CANDIDATE,     //!< ICE local candidate statistics related to the RTCIceTransport objects
    RTC_STATS_TYPE_OUTBOUND_RTP,        //!< Statistics for an outbound RTP stream that is currently received with this RTCPeerConnection object
    RTC_STATS_TYPE_PEER_CONNECTION,     //!< Statistics related to the RTCPeerConnection object.
    RTC_STATS_TYPE_RECEIVER,            //!< Statistics related to a specific receiver and the corresponding media-level metrics
    RTC_STATS_TYPE_REMOTE_CANDIDATE,    //!< ICE remote candidate statistics related to the RTCIceTransport objects
    RTC_STATS_TYPE_REMOTE_INBOUND_RTP,  //!< Statistics for the remote endpoint's inbound RTP stream corresponding to an outbound stream that
                                        //!< is currently sent with this RTCPeerConnection object
    RTC_STATS_TYPE_REMOTE_OUTBOUND_RTP, //!< Statistics for the remote endpoint's outbound RTP stream corresponding to an inbound stream that
                                        //!< is currently sent with this RTCPeerConnection object
    RTC_STATS_TYPE_SENDER,              //!< Statistics related to a specific RTCRtpSender and the corresponding media-level metrics
    RTC_STATS_TYPE_TRACK,               //!< Statistics related to a specific MediaStreamTrack's attachment to an RTCRtpSender
    RTC_STATS_TYPE_TRANSPORT,           //!< Transport statistics related to the RTCPeerConnection object.
    RTC_STATS_TYPE_SCTP_TRANSPORT,      //!< SCTP transport statistics related to an RTCSctpTransport object
    RTC_STATS_TYPE_TRANSCEIVER,         //!< Statistics related to a specific RTCRtpTransceiver
    RTC_STATS_TYPE_RTC_ALL              //!< Report all supported stats
} RTC_STATS_TYPE;

/**
 * @brief Indicates computing states in the checklist
 * Reference: https://www.w3.org/TR/webrtc-stats/#rtcstatsicecandidatepairstate-enum
 */
typedef enum {
    ICE_CANDIDATE_PAIR_STATE_FROZEN = 0,
    ICE_CANDIDATE_PAIR_STATE_WAITING = 1,
    ICE_CANDIDATE_PAIR_STATE_IN_PROGRESS = 2,
    ICE_CANDIDATE_PAIR_STATE_SUCCEEDED = 3,
    ICE_CANDIDATE_PAIR_STATE_FAILED = 4,
} ICE_CANDIDATE_PAIR_STATE;

/**
 * @brief Set details of the IceAgent based on STUN_ATTRIBUTE_TYPE_USE_CANDIDATE flag
 * Reference: https://www.w3.org/TR/webrtc/#rtcicerole
 */
typedef enum {
    RTC_ICE_ROLE_UNKNOWN,     //!< An agent whose role is undetermined. Initial state
    RTC_ICE_ROLE_CONTROLLING, //!< A controlling agent. The ICE agent that is responsible for selecting the final choice of candidate pairs
    RTC_ICE_ROLE_CONTROLLED   //!< A controlled agent. The iCE agent waits for the controlling agent to select the final choice of candidate pairs
} RTC_ICE_ROLE;

/**
 * @brief DTLS Transport State
 * Reference: https://www.w3.org/TR/webrtc/#rtcdtlstransport-interface
 */
typedef enum {
    RTC_DTLS_TRANSPORT_STATE_STATS_NEW,        //!< DTLS has not started negotiating yet
    RTC_DTLS_TRANSPORT_STATE_STATS_CONNECTING, //!< DTLS is in the process of negotiating a secure connection
    RTC_DTLS_TRANSPORT_STATE_STATS_CONNECTED,  //!< DTLS has completed negotiation of a secure connection
    RTC_DTLS_TRANSPORT_STATE_STATS_CLOSED,     //!< The transport has been closed intentionally
    RTC_DTLS_TRANSPORT_STATE_STATS_FAILED      //!< The transport has failed as the result of an error
} RTC_DTLS_TRANSPORT_STATE_STATS;

/**
 * @brief Defines reasons for quality limitation for sending streams
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#rtcqualitylimitationreason-enum
 */
typedef enum {
    RTC_QUALITY_LIMITATION_REASON_NONE,      //!< Resolution as expected. Default value
    RTC_QUALITY_LIMITATION_REASON_BANDWIDTH, //!< Reason for limitation is congestion cues during bandwidth estimation
    RTC_QUALITY_LIMITATION_REASON_CPU,       //!< The resolution and/or framerate is primarily limited due to CPU load.
    RTC_QUALITY_LIMITATION_REASON_OTHER,     //!< Limitation due to reasons other than above
} RTC_QUALITY_LIMITATION_REASON;

/*! \addtogroup StatsStructures
 * @brief Record of duration and quality reason state
 * @{
 */
typedef struct {
    UINT64 durationInSeconds;                              //!< Time (seconds) spent in each state
    RTC_QUALITY_LIMITATION_REASON qualityLimitationReason; //!< Quality limitation reason
} QualityLimitationDurationsRecord, PQualityLimitationDurationsRecord;

/**
 * @brief Record of total number of packets sent per DSCP. Used by RTCOutboundRtpStreamStats
 * object
 */
typedef struct {
    DOMString dscp;                  //!< DSCP String
    UINT64 totalNumberOfPacketsSent; //!< Number of packets sent
} DscpPacketsSentRecord, *PDscpPacketsSentRecord;

/**
 * @brief RtcIceCandidatePairStats Stats related to the local-remote ICE candidate pair
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#candidatepair-dict*
 */

typedef struct {
    CHAR localCandidateId[MAX_CANDIDATE_ID_LENGTH + 1];  //!< Local candidate that is inspected in RTCIceCandidateStats
    CHAR remoteCandidateId[MAX_CANDIDATE_ID_LENGTH + 1]; //!< Remote candidate that is inspected in RTCIceCandidateStats
    ICE_CANDIDATE_PAIR_STATE state;                      //!< State of checklist for the local-remote candidate pair
    BOOL nominated; //!< Flag is TRUE if the agent is a controlling agent and FALSE otherwise. The agent role is based on the
                    //!< STUN_ATTRIBUTE_TYPE_USE_CANDIDATE flag
    NullableUint32 circuitBreakerTriggerCount; //!< Represents number of times circuit breaker is triggered during media transmission
                                               //!< It is undefined if the user agent does not use this
    UINT32 packetsDiscardedOnSend;             //!< Total number of packets discarded for candidate pair due to socket errors,
    UINT64 packetsSent;                        //!< Total number of packets sent on this candidate pair;
    UINT64 packetsReceived;                    //!< Total number of packets received on this candidate pair
    UINT64 bytesSent;                          //!< Total number of bytes (minus header and padding) sent on this candidate pair
    UINT64 bytesReceived;                      //!< Total number of bytes (minus header and padding) received on this candidate pair
    UINT64 lastPacketSentTimestamp;            //!< Represents the timestamp at which the last packet was sent on this particular
                                               //!< candidate pair, excluding STUN packets.
    UINT64 lastPacketReceivedTimestamp;        //!< Represents the timestamp at which the last packet was sent on this particular
                                               //!< candidate pair, excluding STUN packets.
    UINT64 firstRequestTimestamp;    //!< Represents the timestamp at which the first STUN request was sent on this particular candidate pair.
    UINT64 lastRequestTimestamp;     //!< Represents the timestamp at which the last STUN request was sent on this particular candidate pair.
                                     //!< The average interval between two consecutive connectivity checks sent can be calculated:
                                     //! (lastRequestTimestamp - firstRequestTimestamp) / requestsSent.
    UINT64 lastResponseTimestamp;    //!< Represents the timestamp at which the last STUN response was received on this particular candidate pair.
    DOUBLE totalRoundTripTime;       //!< The sum of all round trip time (seconds) since the beginning of the session, based
                                     //!< on STUN connectivity check responses (responsesReceived), including those that reply to requests
                                     //!< that are sent in order to verify consent. The average round trip time can be computed from
                                     //!< totalRoundTripTime by dividing it by responsesReceived.
    DOUBLE currentRoundTripTime;     //!< Latest round trip time (seconds)
    DOUBLE availableOutgoingBitrate; //!< TODO: Total available bit rate for all the outgoing RTP streams on this candidate pair. Calculated by
                                     //!< underlying congestion control
    DOUBLE availableIncomingBitrate; //!< TODO: Total available bit rate for all the outgoing RTP streams on this candidate pair. Calculated by
                                     //!< underlying congestion control
    UINT64 requestsReceived;         //!< Total number of connectivity check requests received (including retransmission)
    UINT64 requestsSent;             //!< The total number of connectivity check requests sent (without retransmissions).
    UINT64 responsesReceived;        //!< The total number of connectivity check responses received.
    UINT64 responsesSent;            //!< The total number of connectivity check responses sent.
    UINT64 bytesDiscardedOnSend;     //!< Total number of bytes for this candidate pair discarded due to socket errors
} RtcIceCandidatePairStats, *PRtcIceCandidatePairStats;

/**
 * @brief: RtcIceServerStats Stats related to the ICE Server
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#ice-server-dict*
 */
typedef struct {
    DOMString url;                 //!< STUN/TURN server URL
    DOMString protocol;            //!< Valid values: UDP, TCP
    UINT32 iceServerIndex;         //!< Ice server index to get stats from. Not available in spec! Needs to be
                                   //!< populated by the application to get specific server stats
    INT32 port;                    //!< Port number used by client
    UINT64 totalRequestsSent;      //!< Total amount of requests that have been sent to the server
    UINT64 totalResponsesReceived; //!< Total number of responses received from the server
    UINT64 totalRoundTripTime;     //!< Sum of RTTs of all the requests for which response has been received
} RtcIceServerStats, *PRtcIceServerStats;

/**
 * @brief: RtcIceCandidateStats Stats related to a specific candidate in a pair
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#icecandidate-dict*
 */

typedef struct {
    DOMString url;                        //!< For local candidates this is the URL of the ICE server from which the candidate was obtained
    DOMString transportId;                //!< Not used currently. ID of object that was inspected for RTCTransportStats
    CHAR address[IP_ADDR_STR_LENGTH + 1]; //!< IPv4 or IPv6 address of the candidate
    DOMString protocol;                   //!< Valid values: UDP, TCP
    DOMString relayProtocol;              //!< Protocol used by endpoint to communicate with TURN server. (Only for local candidate)
                                          //!< Valid values: UDP, TCP, TLS
    INT32 priority;                       //!< Computed using the formula in https://tools.ietf.org/html/rfc5245#section-15.1
    INT32 port;                           //!< Port number of the candidate
    DOMString candidateType;              //!< Type of local/remote ICE candidate
} RtcIceCandidateStats, *PRtcIceCandidateStats;

/**
 * @brief RtcTransportStats Represents the stats corresponding to an RTCDtlsTransport and
 * its underlying RTCIceTransport
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#transportstats-dict*
 */
typedef struct {
    DOMString rtcpTransportStatsId;              //!< ID of the transport that gives stats for the RTCP component
    DOMString selectedCandidatePairId;           //!< ID of the object inspected to produce RtcIceCandidatePairStats
    DOMString localCertificateId;                //!< For components where DTLS is negotiated, give local certificate
    DOMString remoteCertificateId;               //!< For components where DTLS is negotiated, give remote certificate
    CHAR tlsVersion[MAX_TLS_VERSION_LENGTH + 1]; //!< For components where DTLS is negotiated, the TLS version agreed
    CHAR dtlsCipher[MAX_DTLS_CIPHER_LENGTH +
                    1]; //!< Descriptive name of the cipher suite used for the DTLS transport.
                        //!< Acceptable values: https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml#tls-parameters-4
    CHAR srtpCipher[MAX_SRTP_CIPHER_LENGTH + 1]; //!< Descriptive name of the protection profile used for the SRTP transport
                                                 //!< Acceptable values: https://www.iana.org/assignments/srtp-protection/srtp-protection.xhtml
    CHAR tlsGroup[MAX_TLS_GROUP_LENGHTH +
                  1];     //!< Descriptive name of the group used for the encryption
                          //!< Acceptable values: https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml#tls-parameters-8
    RTC_ICE_ROLE iceRole; //!< Set to the current value of the "role" attribute of the underlying RTCDtlsTransport's "transport"
    RTC_DTLS_TRANSPORT_STATE_STATS dtlsState; //!< Set to the current value of the "state" attribute of the underlying RTCDtlsTransport
    UINT64 packetsSent;                       //!< Total number of packets sent over the transport
    UINT64 packetsReceived;                   //!< Total number of packets received over the transport
    UINT64 bytesSent;                         //!< The total number of payload bytes sent on this PeerConnection (excluding header and padding)
    UINT64 bytesReceived;                     //!< The total number of payload bytes received on this PeerConnection (excluding header and padding)
    UINT32 selectedCandidatePairChanges;      //!< The number of times that the selected candidate pair of this transport has changed
} RtcTransportStats, *PRtcTransportStats;

/**
 * @brief RTCRtpStreamStats captures stream stats that will be used as part of RTCSentRtpStreamStats report
 * Reference:  https://www.w3.org/TR/webrtc-stats/#dom-rtcrtpstreamstats
 */
typedef struct {
    UINT32 ssrc; //!< The 32-bit unsigned integer value per [RFC3550] used to identify the source of the stream of RTP packets that this stats object
                 //!< concerns.
    DOMString kind; //!< Either "audio" or "video". This MUST match the media type part of the information in the corresponding codecType member of
                    //!< RTCCodecStats, and MUST match the "kind" attribute of the related MediaStreamTrack.
    // TODO: transportId not yet populated
    DOMString transportId; //!< It is a unique identifier that is associated to the object that was inspected to produce the RTCTransportStats
                           //!< associated with this RTP stream.

    // TODO: codecId not yet populated
    DOMString codecId; //!< It is a unique identifier that is associated to the object that was inspected to produce the RTCCodecStats associated with
                       //!< this RTP stream.
} RTCRtpStreamStats, *PRTCRtpStreamStats;

/**
 * @brief RTCSentRtpStreamStats will be used as part of outbound Rtp stats
 * Reference: https://www.w3.org/TR/webrtc-stats/#dom-rtcsentrtpstreamstats
 */
typedef struct {
    RTCRtpStreamStats rtpStream;
    UINT64 packetsSent;
    UINT64 bytesSent; //!< Total number of bytes sent for this SSRC. Calculated as defined in [RFC3550]
                      //!< section 6.4.1.
                      //!< The total number of payload octets (i.e., not including header or padding)
                      //!< transmitted in RTP data packets by the sender since starting transmission
} RTCSentRtpStreamStats, *PRTCSentRtpStreamStats;

/**
 * @brief RtcOutboundRtpStreamStats Gathers stats for media stream from the embedded device
 * Note: RTCOutboundRtpStreamStats extends RTCSentRtpStreamStats as per https://www.w3.org/TR/webrtc-stats/#dom-rtcoutboundrtpstreamstats
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#outboundrtpstats-dict*
 */
typedef struct {
    RTCSentRtpStreamStats sent;      //!< Comprises of information such as packetsSent and bytesSent
    BOOL voiceActivityFlag;          //!< Only valid for audio. Whether the last RTP packet sent contained voice activity or not based on the presence
                                     //!< of the V bit in the extension header
    DOMString trackId;               //!< ID representing current track attached to the sender of the stream
    DOMString mediaSourceId;         //!< TODO ID representing the current media source
    DOMString senderId;              //!< TODO The stats ID used to look up the RTCAudioSenderStats or RTCVideoSenderStats object sending this stream
    DOMString remoteId;              //!< TODO ID to look up the remote RTCRemoteInboundRtpStreamStats object for the same SSRC
    DOMString rid;                   //!< TODO Exposes the rid encoding parameter of this RTP stream if it has been set, otherwise it is undefined
    DOMString encoderImplementation; //!< Identifies the encoder implementation used.
    UINT32 packetsDiscardedOnSend;   //!< Total number of RTP packets for this SSRC that have been discarded due to socket errors
    UINT32 framesSent;               //!< Only valid for video. Represents the total number of frames sent on this RTP stream
    UINT32 hugeFramesSent;           //!< Only valid for video. Represents the total number of huge frames sent by this RTP stream
                                     //!< Huge frames have an encoded size at least 2.5 times the average size of the frames
    UINT32 framesEncoded;         //!< Only valid for video. It represents the total number of frames successfully encoded for this RTP media stream
    UINT32 keyFramesEncoded;      //!< Only valid for video. It represents the total number of key frames encoded successfully in the RTP Stream
    UINT32 framesDiscardedOnSend; //!< Total number of video frames that have been discarded for this SSRC due to socket errors
    UINT32 frameWidth;            //!< Only valid for video. Represents the width of the last encoded frame
    UINT32 frameHeight;           //!< Only valid for video. Represents the height of the last encoded frame
    UINT32 frameBitDepth;         //!< Only valid for video. Represents the bit depth per pixel of the last encoded frame. Typical values: 24, 30, 36
    UINT32 nackCount;             //!< Count the total number of Negative ACKnowledgement (NACK) packets received by this sender.
    UINT32 firCount;              //!< Only valid for video. Count the total number of Full Intra Request (FIR) packets received by this sender
    UINT32 pliCount;              //!< Only valid for video. Count the total number of Picture Loss Indication (PLI) packets received by this sender
    UINT32 sliCount;              //!< Only valid for video. Count the total number of Slice Loss Indication (SLI) packets received by this sender
    UINT32 qualityLimitationResolutionChanges; //!< Only valid for video. The number of times that the resolution has changed because we are quality
                                               //!< limited
    INT32 fecPacketsSent; //!< TODO Total number of RTP FEC packets sent for this SSRC. Can also be incremented while sending FEC packets in band
    UINT64 lastPacketSentTimestamp;  //!< The timestamp in milliseconds at which the last packet was sent for this SSRC
    UINT64 headerBytesSent;          //!< Total number of RTP header and padding bytes sent for this SSRC
    UINT64 bytesDiscardedOnSend;     //!< Total number of bytes for this SSRC that have been discarded due to socket errors
    UINT64 retransmittedPacketsSent; //!< The total number of packets that were retransmitted for this SSRC
    UINT64 retransmittedBytesSent;   //!< The total number of PAYLOAD bytes retransmitted for this SSRC
    UINT64 targetBitrate;            //!< Current target TIAS bitrate configured for this particular SSRC
    UINT64 totalEncodedBytesTarget;  //!< Increased by the target frame size in bytes every time a frame has been encoded
    DOUBLE framesPerSecond;          //!< Only valid for video. The number of encoded frames during the last second
    UINT64 qpSum;            //!< TODO Only valid for video. The sum of the QP values of frames encoded by this sender. QP value depends on the codec
    UINT64 totalSamplesSent; //!< TODO Only valid for audio. The total number of samples that have been sent over this RTP stream
    UINT64 samplesEncodedWithSilk; //!< TODO Only valid for audio and when the audio codec is Opus. Represnets only SILK portion of codec
    UINT64 samplesEncodedWithCelt; //!< TODO Only valid for audio and when the audio codec is Opus. Represnets only CELT portion of codec
    UINT64 totalEncodeTime;        //!< Total number of milliseconds that has been spent encoding the framesEncoded frames of the stream
    UINT64 totalPacketSendDelay;   //!< Total time (seconds) packets have spent buffered locally before being transmitted onto the network
    UINT64 averageRtcpInterval;    //!< The average RTCP interval between two consecutive compound RTCP packets
    QualityLimitationDurationsRecord qualityLimitationDurations; //!< Total time (seconds) spent in each reason state
    DscpPacketsSentRecord perDscpPacketsSent;                    //!< Total number of packets sent for this SSRC, per DSCP
    RTC_QUALITY_LIMITATION_REASON qualityLimitationReason;       //!< Only valid for video.
} RtcOutboundRtpStreamStats, *PRtcOutboundRtpStreamStats;

/**
 * @brief RTCRemoteInboundRtpStreamStats Represents the remote endpoint's measurement metrics for a particular incoming RTP stream
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#remoteinboundrtpstats-dict*
 */
typedef struct {
    DOMString localId;                //!< Used to look up RTCOutboundRtpStreamStats for the SSRC
    UINT64 roundTripTime;             //!< Estimated round trip time (milliseconds) for this SSRC based on the RTCP timestamps
    UINT64 totalRoundTripTime;        //!< The cumulative sum of all round trip time measurements in seconds since the beginning of the session
    DOUBLE fractionLost;              //!< The fraction packet loss reported for this SSRC
    UINT64 reportsReceived;           //!< Total number of RTCP RR blocks received for this SSRC
    UINT64 roundTripTimeMeasurements; //!< Total number of RTCP RR blocks received for this SSRC that contain a valid round trip time
} RtcRemoteInboundRtpStreamStats, *PRtcRemoteInboundRtpStreamStats;

typedef struct {
    RTCRtpStreamStats rtpStream;
    UINT64 packetsReceived; //!< Total number of RTP packets received for this SSRC.
    INT64 packetsLost; //!< TODO Total number of RTP packets lost for this SSRC. Calculated as defined in [RFC3550] section 6.4.1. Note that because
                       //!< of how this is estimated, it can be negative if more packets are received than sent.
    DOUBLE jitter;     //!< Packet Jitter measured in seconds for this SSRC. Calculated as defined in section 6.4.1. of [RFC3550].
    UINT64 packetsDiscarded; //!< The cumulative number of RTP packets discarded by the jitter buffer due to late or early-arrival, i.e., these
                             //!< packets are not played out. RTP packets discarded due to packet duplication are not reported in this metric
                             //!< [XRBLOCK-STATS]. Calculated as defined in [RFC7002] section 3.2 and Appendix A.a.
    UINT64 packetsRepaired; //!< TODO The cumulative number of lost RTP packets repaired after applying an error-resilience mechanism [XRBLOCK-STATS].
    UINT64 burstPacketsLost;      //!< TODO The cumulative number of RTP packets lost during loss bursts, Appendix A (c) of [RFC6958].
    UINT64 burstPacketsDiscarded; //!< TODO The cumulative number of RTP packets discarded during discard bursts, Appendix A (b) of [RFC7003].
    UINT32 burstLossCount; //!< TODO The cumulative number of bursts of lost RTP packets, Appendix A (e) of [RFC6958].     [RFC3611] recommends a Gmin
                           //!< (threshold) value of 16 for classifying a sequence of packet losses or discards as a burst.
    UINT32 burstDiscardCount; //!< TODO The cumulative number of bursts of discarded RTP packets, Appendix A (e) of [RFC8015].
    DOUBLE burstLossRate;     //!< TODO The fraction of RTP packets lost during bursts to the total number of RTP packets expected in the bursts. As
                              //!< defined in Appendix A (a) of [RFC7004], however, the actual value is reported without multiplying by 32768.
    DOUBLE burstDiscardRate;  //!< TODO The fraction of RTP packets discarded during bursts to the total number of RTP packets expected in bursts. As
                              //!< defined in Appendix A (e) of [RFC7004], however, the actual value is reported without multiplying by 32768.
    DOUBLE gapLossRate; //!< TODO The fraction of RTP packets lost during the gap periods. Appendix A (b) of [RFC7004], however, the actual value is
                        //!< reported without multiplying by 32768.
    DOUBLE gapDiscardRate;    //!< TODO The fraction of RTP packets discarded during the gap periods. Appendix A (f) of [RFC7004], however, the actual
                              //!< value is reported without multiplying by 32768.
    UINT32 framesDropped;     //!< Only valid for video. The total number of frames dropped prior to decode or dropped because the frame missed its
                              //!< display deadline for this receiver's track. The measurement begins when the receiver is created and is a cumulative
                              //!< metric as defined in Appendix A (g) of [RFC7004].
    UINT32 partialFramesLost; //!< TODO Only valid for video. The cumulative number of partial frames lost. The measurement begins when the receiver
                              //!< is created and is a cumulative metric as defined in Appendix A (j) of [RFC7004]. This metric is incremented when
                              //!< the frame is sent to the decoder. If the partial frame is received and recovered via retransmission or FEC before
                              //!< decoding, the framesReceived counter is incremented.
    UINT32 fullFramesLost; //!< Only valid for video. The cumulative number of full frames lost. The measurement begins when the receiver is created
                           //!< and is a cumulative metric as defined in Appendix A (i) of [RFC7004].
} RtcReceivedRtpStreamStats, *PRtcReceivedRtpStreamStats;

/**
 * @brief The RTCInboundRtpStreamStats dictionary represents the measurement metrics for the incoming RTP media stream. The timestamp reported in the
 * statistics object is the time at which the data was sampled.
 *
 * Reference: https://www.w3.org/TR/webrtc-stats/#dom-rtcinboundrtpstreamstats
 */
typedef struct {
    RtcReceivedRtpStreamStats received; // dictionary RTCInboundRtpStreamStats : RTCReceivedRtpStreamStats
    DOMString trackId;    //!< TODO The identifier of the stats object representing the receiving track, an RTCReceiverAudioTrackAttachmentStats or
                          //!< RTCReceiverVideoTrackAttachmentStats.
    DOMString receiverId; //!< TODO The stats ID used to look up the RTCAudioReceiverStats or RTCVideoReceiverStats object receiving this stream.
    DOMString remoteId;   //!< TODO The remoteId is used for looking up the remote RTCRemoteOutboundRtpStreamStats object for the same SSRC.
    UINT32 framesDecoded; //!< TODO Only valid for video. It represents the total number of frames correctly decoded for this RTP stream, i.e., frames
                          //!< that would be displayed if no frames are dropped.
    UINT32 keyFramesDecoded; //!< TODO Only valid for video. It represents the total number of key frames, such as key frames in VP8 [RFC6386] or
                             //!< IDR-frames in H.264 [RFC6184], successfully decoded for this RTP media stream. This is a subset of framesDecoded.
                             //!< framesDecoded - keyFramesDecoded gives you the number of delta frames decoded.
    UINT16 frameWidth;       //!< TODO Only valid for video. Represents the width of the last decoded frame. Before the first frame is decoded this
                             //!< attribute is missing.
    UINT16 frameHeight;      //!< TODO Only valid for video. Represents the height of the last decoded frame. Before the first frame is decoded this
                             //!< attribute is missing.
    UINT8 frameBitDepth; //!< TODO Only valid for video. Represents the bit depth per pixel of the last decoded frame. Typical values are 24, 30, or
                         //!< 36 bits. Before the first frame is decoded this attribute is missing.
    DOUBLE framesPerSecond; //!< TODO Only valid for video. The number of decoded frames in the last second.
    UINT64 qpSum;           //!< TODO Only valid for video. The sum of the QP values of frames decoded by this receiver. The count of frames is in
                  //!< framesDecoded. The definition of QP value depends on the codec; for VP8, the QP value is the value carried in the frame header
                  //!< as the syntax element "y_ac_qi", and defined in [RFC6386] section 19.2. Its range is 0..127. Note that the QP value is only an
                  //!< indication of quantizer values used; many formats have ways to vary the quantizer value within the frame.
    DOUBLE totalDecodeTime; //!< TODO Total number of seconds that have been spent decoding the framesDecoded frames of this stream. The average
                            //!< decode time can be calculated by dividing this value with framesDecoded. The time it takes to decode one frame is the
                            //!< time passed between feeding the decoder a frame and the decoder returning decoded data for that frame.
    DOUBLE totalInterFrameDelay; //!< TODO Sum of the interframe delays in seconds between consecutively decoded frames, recorded just after a frame
                                 //!< has been decoded. The interframe delay variance be calculated from totalInterFrameDelay,
                                 //!< totalSquaredInterFrameDelay, and framesDecoded according to the formula: (totalSquaredInterFrameDelay -
                                 //!< totalInterFrameDelay^2/ framesDecoded)/framesDecoded.
    DOUBLE totalSquaredInterFrameDelay; //!< TODO Sum of the squared interframe delays in seconds between consecutively decoded frames, recorded just
                                        //!< after a frame has been decoded. See totalInterFrameDelay for details on how to calculate the interframe
                                        //!< delay variance.

    BOOL voiceActivityFlag; //!< TODO Only valid for audio. Whether the last RTP packet whose frame was delivered to the RTCRtpReceiver's
                            //!< MediaStreamTrack for playout contained voice activity or not based on the presence of the V bit in the extension
                            //!< header, as defined in [RFC6464]. This is the stats-equivalent of RTCRtpSynchronizationSource.voiceActivityFlag in
                            //!< [[WEBRTC].
    DOMHighResTimeStamp
        lastPacketReceivedTimestamp; //!< Represents the timestamp at which the last packet was received for this SSRC. This differs from timestamp,
                                     //!< which represents the time at which the statistics were generated by the local endpoint.
    DOUBLE averageRtcpInterval; //!< TODO The average RTCP interval between two consecutive compound RTCP packets. This is calculated by the sending
                                //!< endpoint when sending compound RTCP reports. Compound packets must contain at least a RTCP RR or SR block and an
                                //!< SDES packet with the CNAME item.
    UINT64 headerBytesReceived; //!< Total number of RTP header and padding bytes received for this SSRC. This does not include the size of transport
                                //!< layer headers such as IP or UDP. headerBytesReceived + bytesReceived equals the number of bytes received as
                                //!< payload over the transport.
    UINT64 fecPacketsReceived;  //!< TODO Total number of RTP FEC packets received for this SSRC. This counter can also be incremented when receiving
                                //!< FEC packets in-band with media packets (e.g., with Opus).
    UINT64
    fecPacketsDiscarded;  //!< TODO Total number of RTP FEC packets received for this SSRC where the error correction payload was discarded by the
                          //!< application. This may happen 1. if all the source packets protected by the FEC packet were received or already
                          //!< recovered by a separate FEC packet, or 2. if the FEC packet arrived late, i.e., outside the recovery window, and
                          //!< the lost RTP packets have already been skipped during playout. This is a subset of fecPacketsReceived.
    UINT64 bytesReceived; //!< Total number of bytes received for this SSRC. Calculated as defined in [RFC3550] section 6.4.1.
    UINT64 packetsFailedDecryption; //!< The cumulative number of RTP packets that failed to be decrypted according to the procedures in [RFC3711].
                                    //!< These packets are not counted by packetsDiscarded.
    UINT64 packetsDuplicated; //!< TODO The cumulative number of packets discarded because they are duplicated. Duplicate packets are not counted in
                              //!< packetsDiscarded. Duplicated packets have the same RTP sequence number and content as a previously received
    //!< packet. If multiple duplicates of a packet are received, all of them are counted. An improved estimate of lost
    //!< packets can be calculated by adding packetsDuplicated to packetsLost; this will always result in a positive number,
    //!< but not the same number as RFC 3550 would calculate.

    UINT32 nackCount; //!< TODO Count the total number of Negative ACKnowledgement (NACK) packets sent by this receiver.
    UINT32 firCount;  //!< TODO Only valid for video. Count the total number of Full Intra Request (FIR) packets sent by this receiver.
    UINT32 pliCount;  //!< TODO Only valid for video. Count the total number of Picture Loss Indication (PLI) packets sent by this receiver.
    UINT32 sliCount;  //!< TODO Only valid for video. Count the total number of Slice Loss Indication (SLI) packets sent by this receiver.
    DOMHighResTimeStamp estimatedPlayoutTimestamp; //!< TODO This is the estimated playout time of this receiver's track.
    DOUBLE jitterBufferDelay; //!< TODO It is the sum of the time, in seconds, each audio sample or video frame takes from the time it is received and
                              //!< to the time it exits the jitter buffer.
    UINT64 jitterBufferEmittedCount; //!< TODO The total number of audio samples or video frames that have come out of the jitter buffer (increasing
                                     //!< jitterBufferDelay).
    UINT64 totalSamplesReceived; //!< TODO Only valid for audio. The total number of samples that have been received on this RTP stream. This includes
                                 //!< concealedSamples.
    UINT64 samplesDecodedWithSilk; //!< TODO Only valid for audio and when the audio codec is Opus. The total number of samples decoded by the SILK
                                   //!< portion of the Opus codec.
    UINT64 samplesDecodedWithCelt; //!< TODO Only valid for audio and when the audio codec is Opus. The total number of samples decoded by the CELT
                                   //!< portion of the Opus codec.
    UINT64 concealedSamples; //!< TODO Only valid for audio. The total number of samples that are concealed samples. A concealed sample is a sample
                             //!< that was replaced with synthesized samples generated locally before being played out.
    UINT64 silentConcealedSamples; //!< TODO Only valid for audio. The total number of concealed samples inserted that are "silent".
    UINT64 concealmentEvents; //!< TODO Only valid for audio. The number of concealment events. This counter increases every time a concealed sample
                              //!< is synthesized after a non-concealed sample.
    UINT64 insertedSamplesForDeceleration; //!< TODO Only valid for audio. When playout is slowed down, this counter is increased by the difference
                                           //!< between the number of samples received and the number of samples played out.
    UINT64 removedSamplesForAcceleration; //!< TODO Only valid for audio. When playout is sped up, this counter is increased by the difference between
                                          //!< the number of samples received and the number of samples played out.
    DOUBLE audioLevel; //!< TODO Only valid for audio. Represents the audio level of the receiving track. For audio levels of tracks attached locally,
                       //!< see RTCAudioSourceStats instead. The value is between 0..1 (linear), where 1.0 represents 0 dBov, 0 represents silence,
                       //!< and 0.5 represents approximately 6 dBSPL change in the sound pressure level from 0 dBov.The audioLevel is averaged over
                       //!< some small interval, using the algortihm described under totalAudioEnergy. The interval used is implementation dependent.
    DOUBLE totalAudioEnergy; //!< TODO Only valid for audio. Represents the audio energy of the receiving track. For audio energy of tracks attached
                             //!< locally, see RTCAudioSourceStats instead.
    DOUBLE totalSamplesDuration; //!< TODO Only valid for audio. Represents the audio duration of the receiving track. For audio durations of tracks
                                 //!< attached locally, see RTCAudioSourceStats instead.
    UINT32 framesReceived;       //!< Only valid for video. Represents the total number of complete frames received on this RTP stream. This metric is
                                 //!< incremented when the complete frame is received.
    DOMString decoderImplementation; //!<  TODO Identifies the decoder implementation used. This is useful for diagnosing interoperability issues.
} RtcInboundRtpStreamStats, *PRtcInboundRtpStreamStats;

/**
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcdatachannelstate
 */
typedef enum {
    RTC_DATA_CHANNEL_STATE_CONNECTING, //!< Set while creating data channel
    RTC_DATA_CHANNEL_STATE_OPEN,       //!< Set on opening data channel on embedded side or receiving onOpen event
    RTC_DATA_CHANNEL_STATE_CLOSING,    //!< TODO: Set the state to closed after adding onClosing handler to data channel
    RTC_DATA_CHANNEL_STATE_CLOSED      //!< TODO: Set the state to closed after adding onClose handler to data channel
} RTC_DATA_CHANNEL_STATE;

/**
 * Reference: https://www.w3.org/TR/webrtc-stats/#dom-rtcdatachannelstats
 */
typedef struct {
    DOMString label;              //!< The "label" value of the RTCDataChannel object.
    DOMString protocol;           //!< The "protocol" value of the RTCDataChannel object.
    INT32 dataChannelIdentifier;  //!< The "id" attribute of the RTCDataChannel object.
    DOMString transportId;        //!< TODO: A stats object reference for the transport used to carry this datachannel.
    RTC_DATA_CHANNEL_STATE state; //!< The "readyState" value of the RTCDataChannel object.
    UINT32 messagesSent;          //!< Represents the total number of API "message" events sent.
    UINT64 bytesSent;        //!< Represents the total number of payload bytes sent on this RTCDatachannel, i.e., not including headers or padding.
    UINT32 messagesReceived; //!< Represents the total number of API "message" events received.
    UINT64 bytesReceived;    //!< Represents the total number of bytes received on this RTCDatachannel, i.e., not including headers or padding.
} RtcDataChannelStats, *PRtcDataChannelStats;

/**
 * @brief SignalingClientMetrics Represent the stats related to the KVS WebRTC SDK signaling client
 */
typedef struct {
    UINT64 cpApiCallLatency;         //!< Latency (in 100 ns) incurred per backend API call for the control plane APIs
    UINT64 dpApiCallLatency;         //!< Latency (in 100 ns) incurred per backend API call for the data plane APIs
    UINT64 signalingClientUptime;    //!< Client uptime (in 100 ns). Timestamp will be recorded at every SIGNALING_CLIENT_STATE_CONNECTED
    UINT64 connectionDuration;       //!< Duration of connection (in 100 ns)
    UINT32 numberOfMessagesSent;     //!< Number of messages sent by the signaling client
    UINT32 numberOfMessagesReceived; //!< Number of messages received by the signaling client
    UINT32 iceRefreshCount;          //!< Number of times the ICE is refreshed
    UINT32 numberOfErrors;           //!< Number of signaling client API call failures.
                                     //!< These errors are the result of non STATUS_SUCCESS returns from all of the public
                                     //!< APIs defined for the signaling client with the exception of
                                     //!< createSignalingClientSync and freeSignalingClient invocation
                                     //!< and errors where the signaling client handle is invalid
    UINT32 numberOfRuntimeErrors;    //!< Number of indirect or runtime errors.
                                     //!< These are errors that are not returned as part of
                                     //!< public API calls but rather when an error occurs on background threads
                                     //!< for example:
                                     //!< * When received message on the background thread is "bad"
                                     //!< * When re-connect logic fails after pre-configured retries/times out
                                     //!< * When refreshing ICE server configuration fails after pre-configured retries
                                     //!< In all of these cases the error callback (if specified) will be called.
    UINT32 numberOfReconnects;       //!< Number of reconnects in the session
} SignalingClientStats, PSignalingClientStats;

/**
 * @brief RTCStatsObject Represents an object passed in by the application developer which will
 * be populated internally
 */
typedef struct {
    RtcIceCandidatePairStats iceCandidatePairStats;             //!< ICE Candidate Pair  stats object
    RtcIceCandidateStats localIceCandidateStats;                //!< local ICE Candidate stats object
    RtcIceCandidateStats remoteIceCandidateStats;               //!< remote ICE Candidate stats object
    RtcIceServerStats iceServerStats;                           //!< ICE Server Pair stats object
    RtcTransportStats transportStats;                           //!< Transport stats object
    RtcOutboundRtpStreamStats outboundRtpStreamStats;           //!< Outbound RTP Stream stats object
    RtcRemoteInboundRtpStreamStats remoteInboundRtpStreamStats; //!< Remote Inbound RTP Stream stats object
    RtcInboundRtpStreamStats inboundRtpStreamStats;             //!< Inbound RTP Stream stats object
    RtcDataChannelStats rtcDataChannelStats;
} RtcStatsObject, *PRtcStatsObject;
/*!@} */

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTCCLIENT_STATS_INCLUDE__ */
