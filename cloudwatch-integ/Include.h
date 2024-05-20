#pragma once

#define DEFAULT_CLOUDWATCH_NAMESPACE "KinesisVideoSDKWebRTC"
#define DEFAULT_FPS_VALUE            25
// TODO: This value shouldn't matter. But, since we don't allow NULL value, we have to set to a value
#define DEFAULT_VIEWER_PEER_ID           "ConsumerViewer"
#define DEFAULT_FILE_LOGGING_BUFFER_SIZE (200 * 1024)

#define MAX_CLOUDWATCH_LOG_COUNT       128
#define MAX_STATUS_CODE_LENGTH         16
#define MAX_CONTROL_PLANE_URI_CHAR_LEN 256

#define NUMBER_OF_H264_FRAME_FILES  1500
#define NUMBER_OF_OPUS_FRAME_FILES  618

#define CANARY_METADATA_SIZE (SIZEOF(UINT64) + SIZEOF(UINT32) + SIZEOF(UINT32))
#define ANNEX_B_NALU_SIZE    4

#define CANARY_DEFAULT_FRAMERATE 30
#define CANARY_DEFAULT_BITRATE   (250 * 1024)

#define CANARY_DEFAULT_ITERATION_DURATION_IN_SECONDS 30

#define CANARY_DEFAULT_VIEWER_INIT_DELAY (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define CANARY_MIN_DURATION           (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define CANARY_MIN_ITERATION_DURATION (15 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define END_TO_END_METRICS_INVOCATION_PERIOD    (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define STORAGE_CANARY_FIRST_FRAME_TS_FILE_ENV_VAR       "STORAGE_CANARY_FIRST_FRAME_TS_FILE"

#define CANARY_DEFAULT_LABEL                       "ScaryTestLabel"
#define CANARY_DEFAULT_CHANNEL_NAME                "ScaryTestStream"
#define CANARY_DEFAULT_CLIENT_ID                   "DefaultClientId"
#define CANARY_DEFAULT_LOG_GROUP_NAME              "DefaultLogGroupName"
#define FIRST_FRAME_TS_FILE_PATH                   "../"
#define STORAGE_CANARY_DEFAULT_FIRST_FRAME_TS_FILE "DefaultFirstFrameSentTSFileName.txt"

#define INDIVIDUAL_STORAGE_CW_DIMENSION "StorageWebRTCSDKCanaryChannelName"
#define INDIVIDUAL_CW_DIMENSION         "WebRTCSDKCanaryChannelName"
#define AGGREGATE_STORAGE_CW_DIMENSION  "StorageWebRTCSDKCanaryLabel"
#define AGGREGATE_CW_DIMENSION          "WebRTCSDKCanaryLabel"

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
