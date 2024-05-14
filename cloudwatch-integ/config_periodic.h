#ifndef KVS_SDK_SAMPLE_CONFIG_H
#define KVS_SDK_SAMPLE_CONFIG_H

#define USE_TRICKLE_ICE             TRUE
#define FORCE_TURN_ONLY             FALSE
#define CHANNEL_NAME                (PCHAR) "test"
#define USE_TURN                    TRUE
#define ENABLE_TTFF_VIA_DC          FALSE
#define IOT_CORE_ENABLE_CREDENTIALS FALSE
#define ENABLE_STORAGE              FALSE
#define ENABLE_METRICS              TRUE
#define SAMPLE_PRE_GENERATE_CERT    TRUE
#define SAMPLE_RUN_TIME             (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#endif // KVS_SDK_SAMPLE_CONFIG_H
