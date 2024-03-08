#include "Samples.h"

extern PSampleConfiguration gSampleConfiguration;

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;
    TimerTaskConfiguration pregencertconfig, statsconfig;
    CHK_STATUS(initializeConfiguration(&pSampleConfiguration, SIGNALING_CHANNEL_ROLE_TYPE_MASTER, NULL));
    pregencertconfig.startTime = 0;
    pregencertconfig.iterationTime = SAMPLE_PRE_GENERATE_CERT_PERIOD;
    pregencertconfig.timerCallbackFunc = pregenerateCertTimerCallback;
    pregencertconfig.customData = (UINT64) pSampleConfiguration;
    CHK_STATUS(addTaskToTimerQueue(pSampleConfiguration, &pregencertconfig));
    CHK_STATUS(initializeMediaSenders(pSampleConfiguration, sendAudioPackets, sendVideoPackets));
    CHK_STATUS(initializeMediaReceivers(pSampleConfiguration, sampleReceiveAudioVideoFrame));

    if (argc > 2 && STRNCMP(argv[2], "1", 2) == 0) {
        pSampleConfiguration->channelInfo.useMediaStorage = TRUE;
    }

    CHK_STATUS(initSignaling(pSampleConfiguration, SAMPLE_MASTER_CLIENT_ID));

    statsconfig.startTime = SAMPLE_STATS_DURATION;
    statsconfig.iterationTime = SAMPLE_STATS_DURATION;
    statsconfig.timerCallbackFunc = getIceCandidatePairStatsCallback;
    statsconfig.customData = (UINT64) pSampleConfiguration;
    CHK_STATUS(addTaskToTimerQueue(pSampleConfiguration, &statsconfig));
    // Checking for termination
    CHK_STATUS(sessionCleanupWait(pSampleConfiguration));
    DLOGI("[KVS Master] Streaming session terminated");

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        DLOGE("[KVS Master] Terminated with status code 0x%08x", retStatus);
    }

    DLOGI("[KVS Master] Cleaning up....");
    CHK_LOG_ERR(freeSampleConfiguration(&pSampleConfiguration));
    DLOGI("[KVS Master] Cleanup done");
    CHK_LOG_ERR(retStatus);

    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}