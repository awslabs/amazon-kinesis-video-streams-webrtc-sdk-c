#ifndef KVS_SDK_SAMPLE_CONFIG_H
#define KVS_SDK_SAMPLE_CONFIG_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#define USE_TRICKLE_ICE             TRUE
#define FORCE_TURN_ONLY             FALSE
#define RUNNER_LABEL                (PCHAR) "WebrtcLongRunningStaticOpenSSL-H265"
#define SCENARIO_LABEL              (PCHAR) "WebrtcLongRunning"
#define USE_TURN                    TRUE
#define ENABLE_TTFF_VIA_DC          FALSE
#define USE_IOT                     FALSE
#define ENABLE_STORAGE              FALSE
#define ENABLE_METRICS              TRUE
#define SAMPLE_PRE_GENERATE_CERT    TRUE
#define RUN_TIME                    (12 * HUNDREDS_OF_NANOS_IN_AN_HOUR)
#define LOG_GROUP_NAME              (PCHAR) "WebrtcSDK"
#define CHANNEL_NAME_PREFIX         (PCHAR) "DEFAULT"
#define AUDIO_CODEC                 RTC_CODEC_OPUS
#define VIDEO_CODEC                 RTC_CODEC_H265
#define DEFAULT_BITRATE             (250 * 1024)
#define DEFAULT_FRAMERATE           30
#ifdef __cplusplus
}
#endif

#endif // KVS_SDK_SAMPLE_CONFIG_H
