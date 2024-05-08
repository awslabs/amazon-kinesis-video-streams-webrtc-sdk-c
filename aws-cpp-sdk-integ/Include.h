#pragma once

#define DEFAULT_CLOUDWATCH_NAMESPACE "KinesisVideoSDKWebRTC"
#define DEFAULT_FPS_VALUE            25
// TODO: This value shouldn't matter. But, since we don't allow NULL value, we have to set to a value
#define DEFAULT_VIEWER_PEER_ID           "ConsumerViewer"
#define DEFAULT_FILE_LOGGING_BUFFER_SIZE (200 * 1024)

#define MAX_CLOUDWATCH_LOG_COUNT       128
#define MAX_CONCURRENT_CONNECTIONS     10
#define MAX_TURN_SERVERS               1
#define MAX_STATUS_CODE_LENGTH         16
#define MAX_CONFIG_JSON_TOKENS         128
#define MAX_CONFIG_JSON_VALUE_SIZE     256
#define MAX_CONFIG_JSON_FILE_SIZE      1024
#define MAX_CONTROL_PLANE_URI_CHAR_LEN 256
#define MAX_UINT64_DIGIT_COUNT         20

#define NUMBER_OF_H264_FRAME_FILES  1500
#define NUMBER_OF_OPUS_FRAME_FILES  618
#define SAMPLE_VIDEO_FRAME_DURATION (HUNDREDS_OF_NANOS_IN_A_SECOND / DEFAULT_FPS_VALUE)
#define SAMPLE_AUDIO_FRAME_DURATION (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

#define ASYNC_ICE_CONFIG_INFO_WAIT_TIMEOUT (3 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define ICE_CONFIG_INFO_POLL_PERIOD        (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

#define CA_CERT_PEM_FILE_EXTENSION                               ".pem"
#define SIGNALING_CANARY_MASTER_CLIENT_ID                        "CANARY_MASTER"
#define SIGNALING_CANARY_VIEWER_CLIENT_ID                        "CANARY_VIEWER"
#define SIGNALING_CANARY_START_DELAY                             (100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define SIGNALING_CANARY_MIN_SESSION_PERIOD                      (20 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define SIGNALING_CANARY_OFFER                                   "Signaling canary offer"
#define SIGNALING_CANARY_ANSWER                                  "Signaling canary answer"
#define SIGNALING_CANARY_ROUNDTRIP_TIMEOUT                       (10 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define SIGNALING_CANARY_CHANNEL_NAME                            (PCHAR) "ScaryTestChannel_"
#define SIGNALING_CANARY_MAX_CONSECUTIVE_ITERATION_FAILURE_COUNT 5

#define CANARY_METADATA_SIZE (SIZEOF(UINT64) + SIZEOF(UINT32) + SIZEOF(UINT32))
#define ANNEX_B_NALU_SIZE    4

#define CANARY_DEFAULT_FRAMERATE 30
#define CANARY_DEFAULT_BITRATE   (250 * 1024)

#define CANARY_DEFAULT_ITERATION_DURATION_IN_SECONDS 30

#define CANARY_DEFAULT_VIEWER_INIT_DELAY (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define CANARY_MIN_DURATION           (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define CANARY_MIN_ITERATION_DURATION (15 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define CANARY_ENDPOINT_ENV_VAR                          "CANARY_ENDPOINT"
#define CANARY_LABEL_ENV_VAR                             "CANARY_LABEL"
#define CANARY_CHANNEL_NAME_ENV_VAR                      "CANARY_CHANNEL_NAME"
#define CANARY_CLIENT_ID_ENV_VAR                         "CANARY_CLIENT_ID"
#define CANARY_TRICKLE_ICE_ENV_VAR                       "CANARY_TRICKLE_ICE"
#define CANARY_IS_MASTER_ENV_VAR                         "CANARY_IS_MASTER"
#define CANARY_USE_TURN_ENV_VAR                          "CANARY_USE_TURN"
#define CANARY_LOG_GROUP_NAME_ENV_VAR                    "CANARY_LOG_GROUP_NAME"
#define CANARY_LOG_STREAM_NAME_ENV_VAR                   "CANARY_LOG_STREAM_NAME"
#define CANARY_CERT_PATH_ENV_VAR                         "CANARY_CERT_PATH"
#define CANARY_DURATION_IN_SECONDS_ENV_VAR               "CANARY_DURATION_IN_SECONDS"
#define CANARY_VIDEO_CODEC_ENV_VAR                       "CANARY_VIDEO_CODEC"
#define CANARY_ITERATION_IN_SECONDS_ENV_VAR              "CANARY_ITERATION_IN_SECONDS"
#define CANARY_FORCE_TURN_ENV_VAR                        "CANARY_FORCE_TURN"
#define CANARY_BIT_RATE_ENV_VAR                          "CANARY_DATARATE_IN_BITS_PER_SECOND"
#define CANARY_FRAME_RATE_ENV_VAR                        "CANARY_FRAME_RATE"
#define CANARY_RUN_BOTH_PEERS_ENV_VAR                    "CANARY_RUN_BOTH_PEERS"
#define CANARY_USE_IOT_CREDENTIALS_ENV_VAR               "CANARY_USE_IOT_PROVIDER"
#define CANARY_RUN_IN_PROFILING_MODE_ENV_VAR             "CANARY_IS_PROFILING_MODE"
#define IOT_CORE_CREDENTIAL_ENDPOINT_ENV_VAR             "AWS_IOT_CORE_CREDENTIAL_ENDPOINT"
#define IOT_CORE_CERT_ENV_VAR                            "AWS_IOT_CORE_CERT"
#define IOT_CORE_PRIVATE_KEY_ENV_VAR                     "AWS_IOT_CORE_PRIVATE_KEY"
#define IOT_CORE_ROLE_ALIAS_ENV_VAR                      "AWS_IOT_CORE_ROLE_ALIAS"
#define IOT_CORE_THING_NAME_ENV_VAR                      "AWS_IOT_CORE_THING_NAME"
#define STORAGE_CANARY_FIRST_FRAME_TS_FILE_ENV_VAR       "STORAGE_CANARY_FIRST_FRAME_TS_FILE"

#define CANARY_DEFAULT_LABEL                       "ScaryTestLabel"
#define CANARY_DEFAULT_CHANNEL_NAME                "ScaryTestStream"
#define CANARY_VIDEO_CODEC_H264                    "h264"
#define CANARY_VIDEO_CODEC_H265                    "h265"
#define CANARY_DEFAULT_CLIENT_ID                   "DefaultClientId"
#define CANARY_DEFAULT_LOG_GROUP_NAME              "DefaultLogGroupName"
#define CANARY_DEFAULT_LOG_GROUP_NAME              "DefaultLogGroupName"
#define FIRST_FRAME_TS_FILE_PATH                   "../"
#define STORAGE_CANARY_DEFAULT_FIRST_FRAME_TS_FILE "DefaultFirstFrameSentTSFileName.txt"

#define INDIVIDUAL_STORAGE_CW_DIMENSION "StorageWebRTCSDKCanaryChannelName"
#define INDIVIDUAL_CW_DIMENSION         "WebRTCSDKCanaryChannelName"
#define AGGREGATE_STORAGE_CW_DIMENSION  "StorageWebRTCSDKCanaryLabel"
#define AGGREGATE_CW_DIMENSION          "WebRTCSDKCanaryLabel"

// Signaling Canary error definitions
#define STATUS_SIGNALING_CANARY_BASE                    0x73000000
#define STATUS_SIGNALING_CANARY_UNEXPECTED_MESSAGE      STATUS_SIGNALING_CANARY_BASE + 0x00000001
#define STATUS_SIGNALING_CANARY_ANSWER_CID_MISMATCH     STATUS_SIGNALING_CANARY_BASE + 0x00000002
#define STATUS_SIGNALING_CANARY_OFFER_CID_MISMATCH      STATUS_SIGNALING_CANARY_BASE + 0x00000003
#define STATUS_SIGNALING_CANARY_ANSWER_PAYLOAD_MISMATCH STATUS_SIGNALING_CANARY_BASE + 0x00000004
#define STATUS_SIGNALING_CANARY_OFFER_PAYLOAD_MISMATCH  STATUS_SIGNALING_CANARY_BASE + 0x00000005

#define STATUS_WEBRTC_CANARY_BASE                       0x74000000
#define STATUS_WEBRTC_EMPTY_IOT_CRED_FILE               STATUS_WEBRTC_CANARY_BASE + 0x00000001
#define STATUS_WAITING_ON_FIRST_FRAME                   STATUS_WEBRTC_CANARY_BASE + 0x00000002

#define CANARY_VIDEO_FRAMES_PATH (PCHAR) "./assets/h264SampleFrames/frame-%04d.h264"
#define CANARY_AUDIO_FRAMES_PATH (PCHAR) "./assets/opusSampleFrames/sample-%03d.opus"

#define METRICS_INVOCATION_PERIOD            (60 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define END_TO_END_METRICS_INVOCATION_PERIOD (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define KVS_METRICS_INVOCATION_PERIOD        (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define CANARY_METADATA_SIZE                 (SIZEOF(UINT64) + SIZEOF(UINT32) + SIZEOF(UINT32))

#define MAX_CALL_RETRY_COUNT                 10

#include <numeric>

#include <aws/core/Aws.h>
#include <aws/monitoring/CloudWatchClient.h>
#include <aws/monitoring/model/PutMetricDataRequest.h>
#include <aws/logs/CloudWatchLogsClient.h>
#include <aws/logs/model/CreateLogGroupRequest.h>
#include <aws/logs/model/CreateLogStreamRequest.h>
#include <aws/logs/model/PutLogEventsRequest.h>
#include <aws/logs/model/DeleteLogStreamRequest.h>
#include <aws/logs/model/DescribeLogStreamsRequest.h>

#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

using namespace Aws::Client;
using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Model;
using namespace Aws::CloudWatch::Model;
using namespace Aws::CloudWatch;
using namespace std;
