/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file kvs_webrtc.c
 * @brief KVS WebRTC peer connection implementation
 *
 * This file implements the pluggable peer connection interface with full
 * KVS SDK integration. It provides a clean interface that app_webrtc.c
 * can use without directly calling KVS SDK functions.
 *
 * This implements the actual KVS peer connection logic including:
 * - Peer connection creation and management
 * - SDP offer/answer processing
 * - ICE candidate handling
 * - Media stream setup with transceivers
 * - Connection state management
 */

#include "kvs_peer_connection.h"
#include "app_webrtc_if.h"
#include "kvs_webrtc_internal.h"
#include "kvs_media.h"
#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include "media_stream.h"
#include "app_webrtc.h"
#include "WebRtcLogging.h"

static const char *TAG = "kvs_webrtc";

// Single global callback registry for all data channel operations
static struct {
    bool callbacks_registered;
    RtcOnOpen onOpen;
    RtcOnMessage onMessage;
    UINT64 customData;
} g_data_channel_callbacks = {0};

// Certificate pre-generation settings - match Common.c timing
#define KVS_PRE_GENERATE_CERT TRUE
#define KVS_PRE_GENERATE_CERT_PERIOD (1000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)  // 1 second (same as Common.c)
#define KVS_TIMER_QUEUE_THREAD_SIZE (8 * 1024)

// Sender bandwidth estimation (TWCC) control
#define KVS_ENABLE_SENDER_BANDWIDTH_ESTIMATION FALSE  // Disable TWCC bandwidth estimation

// ICE candidate pair statistics settings
#define KVS_ICE_STATS_DURATION (20 * HUNDREDS_OF_NANOS_IN_A_SECOND)  // 20 seconds

// Active sessions hash table configuration
#define KVS_ACTIVE_SESSIONS_HASH_TABLE_BUCKET_COUNT   32   // Support up to 32 concurrent sessions efficiently
#define KVS_ACTIVE_SESSIONS_HASH_TABLE_BUCKET_LENGTH  2    // Average 2 sessions per bucket

// Forward declarations for KVS SDK callbacks
static VOID onIceCandidateHandler(UINT64 customData, PCHAR candidateJson);
static STATUS kvs_pregenerateCertTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData);
static STATUS kvs_iceCandidatePairStatsCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData);
static STATUS kvs_metricsCollectionCallback(UINT64 callerData, PHashEntry pHashEntry);
static VOID onConnectionStateChangeHandler(UINT64 customData, RTC_PEER_CONNECTION_STATE newState);
static VOID onDataChannelHandler(UINT64 customData, PRtcDataChannel pDataChannel);

// Bandwidth estimation handlers
static VOID kvs_videoBandwidthEstimationHandler(UINT64 customData, DOUBLE maximumBitrate);
static VOID kvs_audioBandwidthEstimationHandler(UINT64 customData, DOUBLE maximumBitrate);
#if KVS_ENABLE_SENDER_BANDWIDTH_ESTIMATION
static VOID kvs_senderBandwidthEstimationHandler(UINT64 customData, UINT32 txBytes, UINT32 rxBytes,
                                                 UINT32 txPacketsCnt, UINT32 rxPacketsCnt, UINT64 duration);
#endif

// Forward declaration for helper functions
static STATUS kvs_create_and_send_offer(kvs_pc_session_t* session);

// Forward declarations for internal KVS peer connection functions
static STATUS kvs_initializePeerConnection(kvs_pc_client_t* client, PRtcPeerConnection* ppRtcPeerConnection);
static STATUS kvs_setupMediaTracks(kvs_pc_session_t* session);
static STATUS kvs_handleOffer(kvs_pc_session_t* session, webrtc_message_t* message);
static STATUS kvs_handleAnswer(kvs_pc_session_t* session, webrtc_message_t* message);
static STATUS kvs_handleRemoteCandidate(kvs_pc_session_t* session, webrtc_message_t* message);
static BOOL kvs_sampleFilterNetworkInterfaces(UINT64 customData, PCHAR networkInt);
static STATUS kvs_initializeCertificatePregeneration(kvs_pc_client_t* client);
static STATUS kvs_freeCertificatePregeneration(kvs_pc_client_t* client);
static STATUS kvs_getOrCreateCertificate(kvs_pc_client_t* client, PRtcCertificate* ppCertificate);
static STATUS kvs_startIceStats(kvs_pc_client_t* client);
static STATUS kvs_stopIceStats(kvs_pc_client_t* client);

/**
 * @brief Callback for applying new ICE servers to existing sessions
 * This is called for each active session when ICE servers are updated
 * Now uses the new peerConnectionUpdateIceServers API for dynamic updates
 */
static STATUS kvs_applyNewIceServersCallback(UINT64 callerData, PHashEntry pHashEntry)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_client_t* client = (kvs_pc_client_t*)callerData;
    kvs_pc_session_t* session = NULL;
    RtcIceServer* newTurnServers = NULL;
    UINT32 newTurnServerCount = 0;
    UINT32 turnServerIndex = 0;

    CHK(client != NULL && pHashEntry != NULL, STATUS_NULL_ARG);

    session = (kvs_pc_session_t*)pHashEntry->value;
    CHK(session != NULL && !session->terminated && session->peer_connection != NULL, STATUS_NULL_ARG);

    ESP_LOGI(TAG, "Applying new ICE servers to existing session: %s", session->peer_id);

    // Extract only TURN servers for dynamic update (STUN servers are typically already applied)
    if (client->config.ice_servers != NULL && client->config.ice_server_count > 0) {
        // First pass: count TURN servers
        for (UINT32 i = 0; i < client->config.ice_server_count; i++) {
            RtcIceServer *server = &((RtcIceServer*)client->config.ice_servers)[i];
            if (STRNCMPI(server->urls, "turn:", 5) == 0 || STRNCMPI(server->urls, "turns:", 6) == 0) {
                newTurnServerCount++;
            }
        }

        // If we have TURN servers, prepare them for dynamic update
        if (newTurnServerCount > 0) {
            newTurnServers = (RtcIceServer*)MEMCALLOC(newTurnServerCount, SIZEOF(RtcIceServer));
            CHK(newTurnServers != NULL, STATUS_NOT_ENOUGH_MEMORY);

            ESP_LOGI(TAG, "Found %" PRIu32 " new TURN servers for dynamic update", newTurnServerCount);

            // Second pass: copy TURN servers
            for (UINT32 i = 0; i < client->config.ice_server_count; i++) {
                RtcIceServer *server = &((RtcIceServer*)client->config.ice_servers)[i];
                if (STRNCMPI(server->urls, "turn:", 5) == 0 || STRNCMPI(server->urls, "turns:", 6) == 0) {
                    MEMCPY(&newTurnServers[turnServerIndex], server, SIZEOF(RtcIceServer));
                    ESP_LOGI(TAG, "TURN server %" PRIu32 ": %s (user: %s)",
                             turnServerIndex, server->urls,
                             server->username[0] != '\0' ? server->username : "(none)");
                    turnServerIndex++;
                }
            }

            // Now use the new API to dynamically add TURN servers to the existing peer connection
            ESP_LOGI(TAG, "Dynamically adding %" PRIu32 " TURN servers to peer connection: %s",
                     newTurnServerCount, session->peer_id);

            STATUS updateStatus = peerConnectionUpdateIceServers(session->peer_connection,
                                                               newTurnServers,
                                                               newTurnServerCount);

            if (STATUS_SUCCEEDED(updateStatus)) {
                ESP_LOGI(TAG, "Successfully added %" PRIu32 " new TURN servers to existing peer connection: %s",
                         newTurnServerCount, session->peer_id);
                ESP_LOGI(TAG, "Progressive ICE: TURN servers now available for ongoing connection!");
            } else {
                ESP_LOGW(TAG, "Failed to add TURN servers to peer connection %s: 0x%08" PRIx32,
                         session->peer_id, updateStatus);
                ESP_LOGI(TAG, "New TURN servers will be available for ICE restarts or new connections");
            }
        } else {
            ESP_LOGI(TAG, "No new TURN servers found in update - only STUN servers (already applied)");
        }
    } else {
        ESP_LOGI(TAG, "No ICE servers in client configuration");
    }

    ESP_LOGI(TAG, "ICE server update processing completed for session: %s", session->peer_id);

CleanUp:
    if (newTurnServers != NULL) {
        SAFE_MEMFREE(newTurnServers);
    }
    return retStatus;
}

/**
 * @brief Set or update ICE servers for a KVS peer connection client
 *
 * This function allows updating ICE servers separately from initialization.
 * It can be called multiple times to refresh ICE servers during runtime.
 * When called, it updates both the client configuration and applies changes
 * to existing sessions where possible.
 */
static WEBRTC_STATUS kvs_pc_set_ice_servers(void *pPeerConnectionClient, void *ice_servers, uint32_t ice_count)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_client_t *client_data = NULL;
    UINT32 activeSessionCount = 0;

    CHK(pPeerConnectionClient != NULL, STATUS_NULL_ARG);

    client_data = (kvs_pc_client_t *)pPeerConnectionClient;
    CHK(client_data->initialized, STATUS_INVALID_OPERATION);

    ESP_LOGI(TAG, "Updating ICE servers for KVS peer connection client (new count: %" PRIu32 ")", ice_count);

    // Free any existing ICE servers
    if (client_data->config.ice_servers != NULL &&
        client_data->config.ice_servers != ice_servers) {
        SAFE_MEMFREE(client_data->config.ice_servers);
    }

    // Deep-copy ICE servers if supplied so caller's storage can be ephemeral
    if (ice_servers != NULL && ice_count > 0) {
        RtcIceServer *src = (RtcIceServer *) ice_servers;
        RtcIceServer *dst = (RtcIceServer *) MEMCALLOC(ice_count, SIZEOF(RtcIceServer));
        if (dst != NULL) {
            for (UINT32 i = 0; i < ice_count; i++) {
                // Copy full struct safely
                MEMCPY(&dst[i], &src[i], SIZEOF(RtcIceServer));
                ESP_LOGI(TAG, "ICE Server %" PRIu32 ": %s", i, dst[i].urls);
            }
            client_data->config.ice_servers = dst;
            client_data->config.ice_server_count = ice_count;

            ESP_LOGI(TAG, "Updated ICE server configuration: %" PRIu32 " servers", ice_count);
        } else {
            client_data->config.ice_servers = NULL;
            client_data->config.ice_server_count = 0;
            ESP_LOGW(TAG, "Failed to allocate memory for ICE servers");
            CHK(FALSE, STATUS_NOT_ENOUGH_MEMORY);
        }
    } else {
        client_data->config.ice_servers = NULL;
        client_data->config.ice_server_count = 0;
        ESP_LOGI(TAG, "Cleared ICE servers for KVS peer connection client");
    }

    // Apply new ICE servers to existing sessions where possible
    if (client_data->activeSessions != NULL) {
        CHK_STATUS(hashTableGetCount(client_data->activeSessions, &activeSessionCount));
        if (activeSessionCount > 0) {
            ESP_LOGI(TAG, "Progressive ICE: Dynamically applying new ICE servers to %" PRIu32 " existing sessions", activeSessionCount);
            CHK_STATUS(hashTableIterateEntries(client_data->activeSessions, (UINT64)client_data, kvs_applyNewIceServersCallback));
            ESP_LOGI(TAG, "Progressive ICE: Successfully processed all %" PRIu32 " existing sessions for dynamic updates", activeSessionCount);
        } else {
            ESP_LOGI(TAG, "Progressive ICE: No existing sessions to update - new ICE servers will be used for future connections");
        }
    }

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to update ICE servers: 0x%08" PRIx32, (UINT32) retStatus);
    }
    return retStatus;
}

/**
 * @brief Convert app_webrtc audio codec type to KVS codec type
 */
static UINT64 convertAppWebrtcAudioCodecToKvs(app_webrtc_rtc_codec_t appCodec)
{
    switch (appCodec) {
        case APP_WEBRTC_CODEC_OPUS:
            return RTC_CODEC_OPUS;
        case APP_WEBRTC_CODEC_MULAW:
            return RTC_CODEC_MULAW;
        case APP_WEBRTC_CODEC_ALAW:
            return RTC_CODEC_ALAW;
        default:
            ESP_LOGW(TAG, "Unknown app_webrtc audio codec: %d, defaulting to OPUS", appCodec);
            return RTC_CODEC_OPUS;
    }
}

/**
 * @brief Convert app_webrtc video codec type to KVS codec type
 */
static UINT64 convertAppWebrtcVideoCodecToKvs(app_webrtc_rtc_codec_t appCodec)
{
    switch (appCodec) {
        case APP_WEBRTC_CODEC_H264:
            return RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
        case APP_WEBRTC_CODEC_H265:
            return RTC_CODEC_H265;
        case APP_WEBRTC_CODEC_VP8:
            return RTC_CODEC_VP8;
        default:
            ESP_LOGW(TAG, "Unknown app_webrtc video codec: %d, defaulting to H264", appCodec);
            return RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    }
}

/**
 * @brief Initialize KVS peer connection client
 */
static WEBRTC_STATUS kvs_pc_init(void *pc_cfg, void **ppPeerConnectionClient)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_client_t *client_data = NULL;

    CHK(pc_cfg != NULL && ppPeerConnectionClient != NULL, STATUS_NULL_ARG);

    client_data = (kvs_pc_client_t *)MEMCALLOC(1, SIZEOF(kvs_pc_client_t));
    CHK(client_data != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // Extract configuration from generic structure
    webrtc_peer_connection_config_t *generic_cfg = (webrtc_peer_connection_config_t *)pc_cfg;
    kvs_peer_connection_config_t *kvs_cfg = NULL;

    // Initialize KVS config with defaults
    MEMSET(&client_data->config, 0, SIZEOF(kvs_peer_connection_config_t));

    // Use reasonable defaults if not provided
    client_data->config.trickle_ice = TRUE;
    client_data->config.use_turn = TRUE;
    client_data->config.audio_codec = RTC_CODEC_OPUS;  // Default audio codec
    client_data->config.video_codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;  // Default video codec

    // If implementation-specific config is provided, use it
    if (generic_cfg != NULL) {
        client_data->config.video_capture = generic_cfg->video_capture;
        client_data->config.audio_capture = generic_cfg->audio_capture;
        client_data->config.video_player = generic_cfg->video_player;
        client_data->config.audio_player = generic_cfg->audio_player;
        // Enable receive_media if any player interface is provided
        client_data->config.receive_media = (generic_cfg->video_player != NULL || generic_cfg->audio_player != NULL);

        // Convert codec configuration from app_webrtc types to KVS types
        client_data->config.audio_codec = convertAppWebrtcAudioCodecToKvs(generic_cfg->audio_codec);
        client_data->config.video_codec = convertAppWebrtcVideoCodecToKvs(generic_cfg->video_codec);

        if (generic_cfg->peer_connection_cfg != NULL) {
            kvs_cfg = (kvs_peer_connection_config_t *)generic_cfg->peer_connection_cfg;
            // Copy KVS-specific settings
            client_data->config.trickle_ice = kvs_cfg->trickle_ice;
            client_data->config.use_turn = kvs_cfg->use_turn;
            client_data->config.ice_servers = kvs_cfg->ice_servers;
            client_data->config.ice_server_count = kvs_cfg->ice_server_count;
        }
    }

    // Initialize the KVS WebRTC SDK
    STATUS status = initKvsWebRtc();
    if (STATUS_FAILED(status)) {
        ESP_LOGE(TAG, "Failed to initialize KVS WebRTC SDK: 0x%08" PRIx32, (UINT32) status);
        SAFE_MEMFREE(client_data);
        return WEBRTC_STATUS_INTERNAL_ERROR;
    }

    // Initialize global media state
    status = kvs_media_init_shared_state();
    if (STATUS_FAILED(status)) {
        ESP_LOGE(TAG, "Failed to initialize global media state: 0x%08" PRIx32, (UINT32) status);
        SAFE_MEMFREE(client_data);
        return WEBRTC_STATUS_INTERNAL_ERROR;
    }

    // Deep-copy ICE servers if supplied so caller's storage can be ephemeral
    if (client_data->config.ice_servers != NULL && client_data->config.ice_server_count > 0) {
        UINT32 count = client_data->config.ice_server_count;
        RtcIceServer *src = (RtcIceServer *) client_data->config.ice_servers;
        RtcIceServer *dst = (RtcIceServer *) MEMCALLOC(count, SIZEOF(RtcIceServer));
        if (dst != NULL) {
            for (UINT32 i = 0; i < count; i++) {
                // Copy full struct safely
                MEMCPY(&dst[i], &src[i], SIZEOF(RtcIceServer));
            }
            client_data->config.ice_servers = dst;
            client_data->config.ice_server_count = count;
        }
    }
    client_data->initialized = TRUE;
    client_data->enable_metrics = TRUE;
    client_data->global_media_started = FALSE;
    client_data->session_count = 0;
    client_data->session_count_mutex = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(client_data->session_count_mutex), STATUS_INVALID_OPERATION);

    // Initialize timer queue for KVS operations
    CHK_STATUS(timerQueueCreate(&client_data->timer_queue));

        // Set up RTC configuration with defaults
    MEMSET(&client_data->rtc_configuration, 0x00, SIZEOF(RtcConfiguration));

    // Set ICE timeouts and behavior
    client_data->rtc_configuration.kvsRtcConfiguration.iceConnectionCheckTimeout = 12 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    client_data->rtc_configuration.kvsRtcConfiguration.iceLocalCandidateGatheringTimeout = 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    client_data->rtc_configuration.kvsRtcConfiguration.iceCandidateNominationTimeout = 15 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    client_data->rtc_configuration.kvsRtcConfiguration.iceConnectionCheckPollingInterval = 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

    // Set the ICE mode
    client_data->rtc_configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_ALL;

    // Initialize certificate pre-generation
    CHK_STATUS(kvs_initializeCertificatePregeneration(client_data));

    // Initialize ICE stats tracking
    client_data->iceCandidatePairStatsTimerId = MAX_UINT32;
    client_data->rtcIceCandidatePairMetrics.requestedTypeOfStats = RTC_STATS_TYPE_CANDIDATE_PAIR;
    client_data->statsLock = MUTEX_CREATE(TRUE);
    CHK(IS_VALID_MUTEX_VALUE(client_data->statsLock), STATUS_INVALID_OPERATION);

    // Initialize internal session tracking for metrics collection
    CHK_STATUS(hashTableCreateWithParams(KVS_ACTIVE_SESSIONS_HASH_TABLE_BUCKET_COUNT,
                                         KVS_ACTIVE_SESSIONS_HASH_TABLE_BUCKET_LENGTH,
                                         &client_data->activeSessions));

    // Start ICE statistics collection timer if metrics are enabled
    if (client_data->enable_metrics) {
        CHK_STATUS(kvs_startIceStats(client_data));
        ESP_LOGI(TAG, "Started ICE statistics collection for comprehensive metrics tracking");
    }

    *ppPeerConnectionClient = client_data;

    ESP_LOGI(TAG, "KVS peer connection client initialized");

CleanUp:
    if (STATUS_FAILED(retStatus) && client_data != NULL) {
        SAFE_MEMFREE(client_data);
    }

    return retStatus;
}

/**
 * @brief Create a new KVS peer connection session
 * This implements the actual KVS SDK peer connection creation logic
 */
static WEBRTC_STATUS kvs_pc_create_session(void *pPeerConnectionClient,
                                           const char *peer_id,
                                           bool is_initiator,
                                           void **ppSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = NULL;
    kvs_pc_client_t *client_data = NULL;

    CHK(pPeerConnectionClient != NULL && peer_id != NULL && ppSession != NULL, STATUS_NULL_ARG);

    client_data = (kvs_pc_client_t *)pPeerConnectionClient;
    CHK(client_data->initialized, STATUS_INVALID_OPERATION);

    session = (kvs_pc_session_t *)MEMCALLOC(1, SIZEOF(kvs_pc_session_t));
    CHK(session != NULL, STATUS_NOT_ENOUGH_MEMORY);

    session->client = client_data;
    STRNCPY(session->peer_id, peer_id, SIZEOF(session->peer_id));
    session->is_initiator = is_initiator;
    session->terminated = FALSE;
    session->start_time = GETTIME();
    session->media_started = FALSE;
    session->media_threads_started = FALSE;
    session->receive_thread_started = FALSE;
    session->receive_audio_video_tid = INVALID_TID_VALUE;
    session->media_sender_tid = INVALID_TID_VALUE;
    session->media_sender_thread_started = FALSE;
    session->correlation_id_postfix = 0;
    session->needs_offer = FALSE;  // Initialize to FALSE, will be set later if needed

    // Initialize callback fields to NULL - critical for proper callback handling
    session->custom_data = 0;
    session->on_message_received = NULL;
    session->on_peer_state_changed = NULL;

#if KVS_ENABLE_DATA_CHANNEL
        // Initialize data channel fields
    session->data_channel = NULL;
    session->data_channel_ready = FALSE;
    MEMSET(session->data_channel_message, 0, MAX_DATA_CHANNEL_MESSAGE_SIZE);

    // Log global callback state
    if (g_data_channel_callbacks.callbacks_registered) {
        ESP_LOGI(TAG, "Global callbacks are registered and will be used for peer: %s", peer_id);
    } else {
        ESP_LOGI(TAG, "No global callbacks registered yet");
    }
#endif

    // Initialize TWCC metadata
    session->twcc_metadata.update_lock = MUTEX_CREATE(TRUE);
    session->twcc_metadata.average_packet_loss = 0.0;
    session->twcc_metadata.current_video_bitrate = 1000000; // 1 Mbps default
    session->twcc_metadata.current_audio_bitrate = 64000;   // 64 kbps default
    session->twcc_metadata.last_adjustment_time_ms = 0;

    // Initialize metrics
    session->pc_metrics.version = PEER_CONNECTION_METRICS_CURRENT_VERSION;
    session->ice_metrics.version = ICE_AGENT_METRICS_CURRENT_VERSION;

    // Initialize comprehensive RTC stats and metrics history
    MEMSET(&session->rtc_stats, 0, SIZEOF(RtcStats));
    MEMSET(&session->rtc_metrics_history, 0, SIZEOF(RtcMetricsHistory));
    session->rtc_stats.requestedTypeOfStats = RTC_STATS_TYPE_CANDIDATE_PAIR;

    // Initialize metrics history timestamp and session start time
    session->rtc_metrics_history.prevTs = GETTIME();
    session->rtc_metrics_history.sessionStartTime = session->start_time;

        // Create the actual KVS peer connection
    ESP_LOGI(TAG, "Creating KVS peer connection for peer: %s", peer_id);
    CHK_STATUS(kvs_initializePeerConnection(client_data, &session->peer_connection));

    // Set up callbacks
    CHK_STATUS(peerConnectionOnIceCandidate(session->peer_connection, (UINT64)session, onIceCandidateHandler));
    CHK_STATUS(peerConnectionOnConnectionStateChange(session->peer_connection, (UINT64)session, onConnectionStateChangeHandler));
    CHK_STATUS(peerConnectionOnDataChannel(session->peer_connection, (UINT64)session, onDataChannelHandler));

    // Configure codecs and media tracks
    CHK_STATUS(kvs_setupMediaTracks(session));

    // Add session to internal tracking for metrics collection using peer_id hash as key
    if (client_data->activeSessions != NULL) {
        UINT32 peerIdHash = COMPUTE_CRC32((PBYTE)peer_id, (UINT32)STRLEN(peer_id));
        STATUS addStatus = hashTablePut(client_data->activeSessions, (UINT64)peerIdHash, (UINT64)session);
        if (STATUS_FAILED(addStatus)) {
            ESP_LOGW(TAG, "Failed to add session to internal tracking for metrics: 0x%08" PRIx32, (UINT32) addStatus);
        } else {
            ESP_LOGI(TAG, "Added session %s to internal metrics tracking (hash=0x%08" PRIx32 ")", peer_id, peerIdHash);
        }
    }

    // Manage global media threads (KVS official pattern)
    MUTEX_LOCK(client_data->session_count_mutex);
    UINT32 old_count = client_data->session_count;
    client_data->session_count++;
    ESP_LOGI(TAG, "Session creation: Session count %" PRIu32 " -> %" PRIu32 " (peer: %s)", old_count, client_data->session_count, peer_id);

    // Start global media threads when first session is created
    if (client_data->session_count == 1 && !client_data->global_media_started) {
        kvs_media_config_t media_config = {
            .video_capture = client_data->config.video_capture,
            .audio_capture = client_data->config.audio_capture,
            .video_player = client_data->config.video_player,
            .audio_player = client_data->config.audio_player,
            .receive_media = client_data->config.receive_media,
            .enable_sample_fallback = TRUE
        };

        ESP_LOGI(TAG, "Starting global media threads for first session: %s", peer_id);

        STATUS media_status = kvs_media_start_global_transmission(client_data, &media_config);
        if (STATUS_SUCCEEDED(media_status)) {
            client_data->global_media_started = TRUE;
            ESP_LOGI(TAG, "Global media threads started successfully");
        } else {
            ESP_LOGW(TAG, "Failed to start global media threads: 0x%08" PRIx32, (UINT32) media_status);
        }
    }
    MUTEX_UNLOCK(client_data->session_count_mutex);

    *ppSession = session;

    ESP_LOGI(TAG, "Created KVS peer connection session for peer: %s (initiator: %s)",
             peer_id, is_initiator ? "true" : "false");

    // For initiator sessions, we'll create an offer AFTER callbacks are registered
    // This avoids the race condition where the offer is created but can't be sent
    // because callbacks aren't registered yet
    if (is_initiator) {
        ESP_LOGI(TAG, "Initiator session created - offer will be created after callbacks are registered: %s", peer_id);
        // Store a flag to indicate this session needs an offer
        session->needs_offer = TRUE;
    }

CleanUp:
    if (STATUS_FAILED(retStatus) && session != NULL) {
        // Cleanup on failure
        if (session->peer_connection != NULL) {
            freePeerConnection(&session->peer_connection);
        }
        SAFE_MEMFREE(session);
    }

    return retStatus;
}

/**
 * @brief Handle WebRTC messages (SDP offers/answers, ICE candidates)
 * Refactored to use helper functions and eliminate code duplication
 */
static WEBRTC_STATUS kvs_pc_send_message(void *pSession, webrtc_message_t *pMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = NULL;

    CHK(pSession != NULL && pMessage != NULL, STATUS_NULL_ARG);

    session = (kvs_pc_session_t *)pSession;
    CHK(!session->terminated, STATUS_INVALID_OPERATION);

    // Validate peer_id in the message - check for empty string only
    if (pMessage->peer_client_id[0] == '\0') {
        ESP_LOGW(TAG, "Empty peer_client_id in message - using session peer_id: %s", session->peer_id);
        // Use the session's peer_id if the message has an empty one
        STRNCPY(pMessage->peer_client_id, session->peer_id, APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN);
        pMessage->peer_client_id[APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
    }

    ESP_LOGD(TAG, "Handling message type: %d for peer: %s", pMessage->message_type, session->peer_id);

    // Use abstracted helper functions to eliminate code duplication
    switch (pMessage->message_type) {
        case WEBRTC_MESSAGE_TYPE_OFFER:
            CHK_STATUS(kvs_handleOffer(session, pMessage));
            break;

        case WEBRTC_MESSAGE_TYPE_ANSWER:
            CHK_STATUS(kvs_handleAnswer(session, pMessage));
            break;

        case WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE:
            CHK_STATUS(kvs_handleRemoteCandidate(session, pMessage));
            break;

        default:
            ESP_LOGW(TAG, "Unhandled message type: %d", pMessage->message_type);
            retStatus = STATUS_INVALID_ARG;
            break;
    }

CleanUp:
    return retStatus;
}

/**
 * @brief Destroy a KVS peer connection session
 */
static WEBRTC_STATUS kvs_pc_destroy_session(void *pSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = NULL;

    CHK(pSession != NULL, STATUS_NULL_ARG);

    session = (kvs_pc_session_t *)pSession;

    ESP_LOGI(TAG, "Destroying KVS peer connection session for peer: %s", session->peer_id);

    // Manage global media threads (KVS official pattern) - MUST happen before removing from activeSessions
    if (session->client != NULL && IS_VALID_MUTEX_VALUE(session->client->session_count_mutex)) {
        MUTEX_LOCK(session->client->session_count_mutex);

        // Check if this session was already cleaned up (e.g., by connection state change handler)
        UINT32 peerIdHash = COMPUTE_CRC32((PBYTE)session->peer_id, (UINT32)STRLEN(session->peer_id));
        UINT64 existingSession = 0;
        STATUS lookupStatus = hashTableGet(session->client->activeSessions, (UINT64)peerIdHash, &existingSession);

        if (STATUS_FAILED(lookupStatus)) {
            ESP_LOGI(TAG, "Session %s already cleaned up, skipping duplicate cleanup", session->peer_id);
            MUTEX_UNLOCK(session->client->session_count_mutex);
            goto SkipCleanup;
        }

        if (session->client->session_count > 0) {
            UINT32 old_count = session->client->session_count;
            session->client->session_count--;
            ESP_LOGI(TAG, "Explicit destroy: Session count %" PRIu32 " -> %" PRIu32 " (removed peer: %s)", old_count, session->client->session_count, session->peer_id);

        // Stop global media threads when last session is destroyed
        if (session->client->session_count == 0 && session->client->global_media_started) {
            ESP_LOGI(TAG, "Stopping global media threads for last session: %s", session->peer_id);
            STATUS media_status = kvs_media_stop_global_transmission(session->client);
            if (STATUS_SUCCEEDED(media_status)) {
                session->client->global_media_started = FALSE;
                ESP_LOGI(TAG, "Global media threads stopped successfully");
            } else {
                ESP_LOGW(TAG, "Failed to stop global media threads: 0x%08" PRIx32, (UINT32) media_status);
            }

            // CRITICAL FIX: Reset KVS WebRTC SDK global state to prevent ICE agent failures
            // When the last session ends, internal KVS state can become corrupted and prevent
            // new ICE agents from working (error 0x5a000025). Reinitialize to clear this state.
            ESP_LOGI(TAG, "Resetting KVS WebRTC SDK state for clean reconnection");
            deinitKvsWebRtc();

            // Small delay to ensure cleanup completes
            THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);  // 100ms

            STATUS reinit_status = initKvsWebRtc();
            if (STATUS_SUCCEEDED(reinit_status)) {
                ESP_LOGI(TAG, "KVS WebRTC SDK reinitialized successfully - ICE agents should work for new connections");
            } else {
                ESP_LOGW(TAG, "Failed to reinitialize KVS WebRTC SDK: 0x%08" PRIx32, (UINT32) reinit_status);
            }
        }
        }

        // Remove session from activeSessions AFTER stopping global media threads to avoid race condition
        if (session->client->activeSessions != NULL) {
            UINT32 peerIdHash = COMPUTE_CRC32((PBYTE)session->peer_id, (UINT32)STRLEN(session->peer_id));
            STATUS removeStatus = hashTableRemove(session->client->activeSessions, (UINT64)peerIdHash);
            if (STATUS_FAILED(removeStatus)) {
                ESP_LOGW(TAG, "Failed to remove session from internal metrics tracking: 0x%08" PRIx32, (UINT32) removeStatus);
            } else {
                ESP_LOGI(TAG, "Removed session %s from internal metrics tracking (hash=0x%08" PRIx32 ")", session->peer_id, peerIdHash);
            }
        }

        MUTEX_UNLOCK(session->client->session_count_mutex);
    }

SkipCleanup:
    session->terminated = TRUE;

    // Cleanup session-specific media (global media threads handled separately)
    kvs_media_stop_session(session);

#if KVS_ENABLE_DATA_CHANNEL
    // Clean up data channel if it exists
    // Note: The data channel will be automatically freed when the peer connection is closed
    if (session->data_channel != NULL) {
        ESP_LOGI(TAG, "Cleaning up data channel for peer: %s", session->peer_id);
        session->data_channel = NULL;
    }
#endif

    // Cleanup TWCC metadata
    if (IS_VALID_MUTEX_VALUE(session->twcc_metadata.update_lock)) {
        MUTEX_FREE(session->twcc_metadata.update_lock);
    }

    // Cleanup KVS SDK objects
    if (session->peer_connection != NULL) {
        ESP_LOGI(TAG, "Gathering final ICE server stats for peer: %s", session->peer_id);

        // Gather ICE server statistics before cleanup (only if connection is still valid)
        if (session->client->config.ice_server_count > 0 && !session->terminated) {
            STATUS stats_status = gatherIceServerStats(session->peer_connection, session->client->config.ice_server_count);
            if (STATUS_FAILED(stats_status)) {
                ESP_LOGW(TAG, "Failed to gather ICE server stats for peer %s: 0x%08" PRIx32, session->peer_id, (UINT32) stats_status);
            }
        }

        ESP_LOGI(TAG, "Closing and freeing peer connection for peer: %s (terminated: %s)",
                 session->peer_id, session->terminated ? "true" : "false");

        // Use non-failing versions for cleanup to avoid issues with already-failed connections
        STATUS closeStatus = closePeerConnection(session->peer_connection);
        if (STATUS_FAILED(closeStatus)) {
            ESP_LOGW(TAG, "closePeerConnection failed for peer %s: 0x%08" PRIx32 " (continuing with freePeerConnection)",
                     session->peer_id, closeStatus);
        }

        STATUS freeStatus = freePeerConnection(&session->peer_connection);
        if (STATUS_FAILED(freeStatus)) {
            ESP_LOGW(TAG, "freePeerConnection failed for peer %s: 0x%08" PRIx32, session->peer_id, (UINT32) freeStatus);
            // Continue anyway since we still need to free our session
        }

        session->peer_connection = NULL;
    } else {
        ESP_LOGI(TAG, "Peer connection already cleaned up for peer: %s", session->peer_id);
    }

    SAFE_MEMFREE(session);

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to destroy session: 0x%08" PRIx32, (UINT32) retStatus);
    }
    return retStatus;
}

/**
 * @brief Free KVS peer connection client resources
 */
static WEBRTC_STATUS kvs_pc_free(void *pPeerConnectionClient)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_client_t *client_data = NULL;

    CHK(pPeerConnectionClient != NULL, STATUS_NULL_ARG);

    client_data = (kvs_pc_client_t *)pPeerConnectionClient;

    ESP_LOGI(TAG, "Freeing KVS peer connection client");

    // Stop global media threads if still running
    if (client_data->global_media_started) {
        ESP_LOGI(TAG, "Stopping global media threads during client cleanup");
        kvs_media_stop_global_transmission(client_data);
        client_data->global_media_started = FALSE;
    }

    // Cleanup session count mutex
    if (IS_VALID_MUTEX_VALUE(client_data->session_count_mutex)) {
        MUTEX_FREE(client_data->session_count_mutex);
        client_data->session_count_mutex = INVALID_MUTEX_VALUE;
    }

    // Cleanup global media state
    kvs_media_cleanup_shared_state();

    // Deinitialize KVS WebRTC SDK
    if (client_data->initialized) {
        ESP_LOGI(TAG, "Deinitializing KVS WebRTC SDK");
        deinitKvsWebRtc();
    }

    // Cleanup certificate pre-generation
    kvs_freeCertificatePregeneration(client_data);

    // Stop and cleanup ICE stats timer properly
    kvs_stopIceStats(client_data);  // This handles the timer cancellation safely

    // Cleanup stats lock
    if (IS_VALID_MUTEX_VALUE(client_data->statsLock)) {
        MUTEX_FREE(client_data->statsLock);
    }

    // Cleanup timer queue
    if (IS_VALID_TIMER_QUEUE_HANDLE(client_data->timer_queue)) {
        timerQueueFree(&client_data->timer_queue);
    }

    // Cleanup internal session tracking hash table (match Common.c pattern)
    if (client_data->activeSessions != NULL) {
        hashTableClear(client_data->activeSessions);
        hashTableFree(client_data->activeSessions);
        client_data->activeSessions = NULL;
    }

    SAFE_MEMFREE(client_data);

CleanUp:
    return retStatus;
}

/**
 * @brief Set callbacks for KVS peer connection events
 */
/**
 * @brief Register an event handler with the KVS WebRTC client
 *
 * @param pPeerConnectionClient Pointer to the peer connection client
 * @param eventHandler Function pointer to the event handler callback
 * @return WEBRTC_STATUS
 */
static WEBRTC_STATUS kvs_pc_register_event_handler(void *pPeerConnectionClient,
                                                 void (*eventHandler)(app_webrtc_event_t event_id,
                                                                    UINT32 status_code,
                                                                    PCHAR peer_id,
                                                                    PCHAR message))
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_client_t *client_data = NULL;

    CHK(pPeerConnectionClient != NULL, STATUS_NULL_ARG);

    client_data = (kvs_pc_client_t *)pPeerConnectionClient;
    client_data->event_handler = eventHandler;

    ESP_LOGI(TAG, "Event handler registered with KVS WebRTC client");

CleanUp:
    return retStatus;
}

static WEBRTC_STATUS kvs_pc_set_callbacks(void *pSession,
                                          uint64_t custom_data,
                                          WEBRTC_STATUS (*on_message_received)(uint64_t, webrtc_message_t*),
                                          WEBRTC_STATUS (*on_peer_state_changed)(uint64_t, webrtc_peer_state_t))
{
    kvs_pc_session_t *session = (kvs_pc_session_t *)pSession;

    if (session == NULL) {
        ESP_LOGE(TAG, "kvs_pc_set_callbacks: NULL session");
        return WEBRTC_STATUS_NULL_ARG;
    }

    ESP_LOGI(TAG, "kvs_pc_set_callbacks: Setting callbacks for peer %s", session->peer_id);
    ESP_LOGI(TAG, "  - custom_data: 0x%llx", (unsigned long long)custom_data);
    ESP_LOGI(TAG, "  - on_message_received: %p", on_message_received);
    ESP_LOGI(TAG, "  - on_peer_state_changed: %p", on_peer_state_changed);

    session->custom_data = custom_data;
    session->on_message_received = on_message_received;
    session->on_peer_state_changed = on_peer_state_changed;

    // Verify the callbacks were properly set
    ESP_LOGI(TAG, "Set KVS peer connection callbacks for peer: %s (msg_cb=%p)",
             session->peer_id, session->on_message_received);

    // If this session needs an offer, create it now that callbacks are registered
    if (session->needs_offer) {
        ESP_LOGI(TAG, "Creating offer for session that needs one: %s", session->peer_id);
        STATUS offer_status = kvs_create_and_send_offer(session);
        if (STATUS_FAILED(offer_status)) {
            ESP_LOGW(TAG, "Failed to create offer after setting callbacks: 0x%08" PRIx32, (UINT32) offer_status);
        } else {
            ESP_LOGI(TAG, "Successfully created and sent offer after setting callbacks");
        }
        // Clear the flag regardless of success/failure
        session->needs_offer = FALSE;
    }

    return WEBRTC_STATUS_SUCCESS;
}

//
// KVS SDK Callback Handlers
//

static VOID onIceCandidateHandler(UINT64 customData, PCHAR candidateJson)
{
    kvs_pc_session_t *session = (kvs_pc_session_t *)(ULONG_PTR)customData;

    if (session == NULL) {
        ESP_LOGE(TAG, "Invalid ICE candidate callback parameters");
        return;
    }

    // Some stacks indicate end-of-candidates with NULL/empty candidate
    if (candidateJson == NULL || candidateJson[0] == '\0') {
        ESP_LOGD(TAG, "ICE candidate callback received end-of-candidates for peer %s", session->peer_id);
        return;
    }

    ESP_LOGD(TAG, "ICE candidate generated for peer %s: %.*s%s",
             session->peer_id,
             64, candidateJson,
             STRLEN(candidateJson) > 64 ? "..." : "");

    // Send ICE candidate through callback with proper format
    if (session->on_message_received != NULL) {
        webrtc_message_t ice_msg = {0};
        ice_msg.version = SIGNALING_MESSAGE_CURRENT_VERSION;
        ice_msg.message_type = WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE;
        STRCPY(ice_msg.peer_client_id, session->peer_id);
        ice_msg.payload = candidateJson;
        ice_msg.payload_len = (UINT32)STRLEN(candidateJson);
        // Match legacy behavior: ICE candidates use empty correlation id
        ice_msg.correlation_id[0] = '\0';

        session->on_message_received(session->custom_data, &ice_msg);
    }
}

static VOID onConnectionStateChangeHandler(UINT64 customData, RTC_PEER_CONNECTION_STATE newState)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = (kvs_pc_session_t *)(ULONG_PTR)customData;
    webrtc_peer_state_t peer_state;

    ESP_LOGI(TAG, "Peer connection state change: peer=%s, KVS_state=%d",
             session ? session->peer_id : "unknown", newState);

    if (session == NULL) {
        ESP_LOGE(TAG, "CRITICAL: Invalid peer connection state callback - session is NULL!");
        return;
    }

    // Map KVS peer connection states to WebRTC peer states
    switch (newState) {
        case RTC_PEER_CONNECTION_STATE_NEW:
            peer_state = WEBRTC_PEER_STATE_NEW;
            break;
        case RTC_PEER_CONNECTION_STATE_CONNECTING:
            peer_state = WEBRTC_PEER_STATE_CONNECTING;
            break;
        case RTC_PEER_CONNECTION_STATE_CONNECTED:
            peer_state = WEBRTC_PEER_STATE_CONNECTED;
            session->media_started = TRUE;

            // Start media reception for this session if needed (transmission is global)
            if (session->client->config.receive_media && !session->media_threads_started) {
                ESP_LOGI(TAG, "Starting media reception for session: %s", session->peer_id);

                kvs_media_config_t media_config = {
                    .video_capture = session->client->config.video_capture,
                    .audio_capture = session->client->config.audio_capture,
                    .video_player = session->client->config.video_player,
                    .audio_player = session->client->config.audio_player,
                    .receive_media = session->client->config.receive_media,
                    .enable_sample_fallback = TRUE
                };

                CHK_STATUS(kvs_media_start_reception(session, &media_config));
                session->media_threads_started = TRUE;
            }

            // Collect metrics like legacy path
            if (session->client->enable_metrics) {
                CHK_STATUS(peerConnectionGetMetrics(session->peer_connection, &session->pc_metrics));
                CHK_STATUS(iceAgentGetMetrics(session->peer_connection, &session->ice_metrics));
                ESP_LOGI(TAG, "Collected peer connection metrics");
            }
            break;
        case RTC_PEER_CONNECTION_STATE_DISCONNECTED:
        case RTC_PEER_CONNECTION_STATE_FAILED:
        case RTC_PEER_CONNECTION_STATE_CLOSED:
            peer_state = WEBRTC_PEER_STATE_DISCONNECTED;
            session->media_started = FALSE;

            // Clean up failed/disconnected sessions to allow new connections
            ESP_LOGI(TAG, "Connection failed/disconnected for peer: %s - performing cleanup", session->peer_id);

            // Only perform cleanup if this session hasn't already been cleaned up by explicit destroy
            if (session->client != NULL && IS_VALID_MUTEX_VALUE(session->client->session_count_mutex)) {
                MUTEX_LOCK(session->client->session_count_mutex);

                // Check if this session is still in activeSessions (not already cleaned up)
                UINT32 peerIdHash = COMPUTE_CRC32((PBYTE)session->peer_id, (UINT32)STRLEN(session->peer_id));
                UINT64 existingSession = 0;
                STATUS lookupStatus = hashTableGet(session->client->activeSessions, (UINT64)peerIdHash, &existingSession);

                if (STATUS_SUCCEEDED(lookupStatus)) {
                    // Session still exists, perform cleanup
                    if (session->client->session_count > 0) {
                        UINT32 old_count = session->client->session_count;
                        session->client->session_count--;
                        ESP_LOGI(TAG, "Automatic cleanup: Session count %" PRIu32 " -> %" PRIu32 " (failed peer: %s)", old_count, session->client->session_count, session->peer_id);

                        // Stop global media threads ONLY when last session fails
                        if (session->client->session_count == 0 && session->client->global_media_started) {
                            ESP_LOGI(TAG, "Stopping global media threads for last failed session: %s", session->peer_id);
                            STATUS media_status = kvs_media_stop_global_transmission(session->client);
                            if (STATUS_SUCCEEDED(media_status)) {
                                session->client->global_media_started = FALSE;
                                ESP_LOGI(TAG, "Global media threads stopped successfully");
                            } else {
                                ESP_LOGW(TAG, "Failed to stop global media threads: 0x%08" PRIx32, (UINT32) media_status);
                            }

                            ESP_LOGI(TAG, "Resetting KVS WebRTC SDK state for clean reconnection (automatic cleanup)");
                            deinitKvsWebRtc();

                            // Small delay to ensure cleanup completes
                            THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);  // 100ms

                            STATUS reinit_status = initKvsWebRtc();
                            if (STATUS_SUCCEEDED(reinit_status)) {
                                ESP_LOGI(TAG, "KVS WebRTC SDK reinitialized successfully - ICE agents should work for new connections");
                            } else {
                                ESP_LOGW(TAG, "Failed to reinitialize KVS WebRTC SDK: 0x%08" PRIx32 " - new connections may fail", (UINT32) reinit_status);
                            }
                        }
                    }

                    // Remove session from activeSessions
                    STATUS removeStatus = hashTableRemove(session->client->activeSessions, (UINT64)peerIdHash);
                    if (STATUS_FAILED(removeStatus)) {
                        ESP_LOGW(TAG, "Failed to remove failed session from tracking: 0x%08" PRIx32, (UINT32) removeStatus);
                    } else {
                        ESP_LOGI(TAG, "Removed failed session %s from tracking (hash=0x%08" PRIx32 ")", session->peer_id, peerIdHash);
                    }
                } else {
                    ESP_LOGI(TAG, "Session %s already cleaned up by explicit destroy, skipping automatic cleanup", session->peer_id);
                }

                MUTEX_UNLOCK(session->client->session_count_mutex);
            }

            // Mark session as terminated and cleanup session-specific media
            session->terminated = TRUE;
            kvs_media_stop_session(session);

            ESP_LOGI(TAG, "Peer connection cleanup deferred to explicit destroy for failed peer: %s", session->peer_id);

            // Note: The session object and peer connection will be cleaned up when the app
            // explicitly calls kvs_pc_destroy_session()

            break;
        default:
            ESP_LOGW(TAG, "Unknown KVS peer state: %d -> defaulting to CONNECTING", newState);
            peer_state = WEBRTC_PEER_STATE_CONNECTING;
            break;
    }

CleanUp:
    // Notify app_webrtc through peer state callback
    ESP_LOGI(TAG, "Calling peer state callback: KVS_state=%d -> peer_state=%d", newState, peer_state);
    if (session->on_peer_state_changed != NULL) {
        session->on_peer_state_changed(session->custom_data, peer_state);
    } else {
        ESP_LOGW(TAG, "No peer state callback registered!");
    }

    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed in connection state handler: 0x%08" PRIx32, (UINT32) retStatus);
    }
}

#if KVS_ENABLE_DATA_CHANNEL
/**
 * @brief Default data channel message callback
 * This is called when a message is received on the data channel
 */
static VOID kvs_onDataChannelMessage(UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen)
{
    kvs_pc_session_t *session = (kvs_pc_session_t *)(ULONG_PTR)customData;

    if (session == NULL || pDataChannel == NULL || pMessage == NULL) {
        ESP_LOGE(TAG, "Invalid data channel message parameters");
        return;
    }

    ESP_LOGI(TAG, "Data channel message received from peer %s: %.*s (binary: %s)",
             session->peer_id, (int) pMessageLen, pMessage, isBinary ? "yes" : "no");

    // Use the global callback registry
    if (g_data_channel_callbacks.callbacks_registered && g_data_channel_callbacks.onMessage != NULL) {
        ESP_LOGI(TAG, "Calling global message callback: %p, customData=0x%llx",
                 g_data_channel_callbacks.onMessage, (unsigned long long)g_data_channel_callbacks.customData);

        // The app_webrtc interface has a different signature than the KVS SDK
        // We need to adapt the parameters to match the app_webrtc_rtc_on_message_t signature
        app_webrtc_rtc_on_message_t app_callback = (app_webrtc_rtc_on_message_t)g_data_channel_callbacks.onMessage;

        // Call the application callback with the message
        ESP_LOGI(TAG, "Invoking global callback with peer_id=%s, message=%.*s",
                 session->peer_id, (int) pMessageLen, pMessage);
        app_callback(g_data_channel_callbacks.customData, pDataChannel, session->peer_id, isBinary, pMessage, pMessageLen);
        ESP_LOGI(TAG, "Global callback completed");
    } else {
        ESP_LOGW(TAG, "No global message callback registered");
        ESP_LOGW(TAG, "This means app_webrtc_set_data_channel_callbacks was not called or failed");
        ESP_LOGW(TAG, "Message from peer %s will not be processed: %.*s",
                 session->peer_id, (int) pMessageLen, pMessage);

        // ALWAYS echo the message back as a guaranteed fallback
        if (session->data_channel != NULL) {
            // For the first message which is usually "Opened data channel by viewer",
            // send a welcome message
            if (strstr((const char*)pMessage, "Opened data channel") != NULL) {
                const char* welcome_msg = "Welcome! Echo server is active.";
                ESP_LOGI(TAG, "Sending welcome message: %s", welcome_msg);
                STATUS welcome_status = dataChannelSend(session->data_channel, FALSE,
                                                     (PBYTE)welcome_msg, strlen(welcome_msg));
                if (STATUS_FAILED(welcome_status)) {
                    ESP_LOGE(TAG, "Failed to send welcome message: 0x%08" PRIx32, (UINT32) welcome_status);
                }
            }

            // Echo the original message back
            ESP_LOGI(TAG, "GUARANTEED AUTO-ECHO: Echoing message back as fallback behavior");
            STATUS echo_status = dataChannelSend(session->data_channel, isBinary, pMessage, pMessageLen);
            if (STATUS_FAILED(echo_status)) {
                ESP_LOGE(TAG, "Failed to echo message: 0x%08" PRIx32, (UINT32) echo_status);
            } else {
                ESP_LOGI(TAG, "Successfully echoed message: %.*s", (int) pMessageLen, pMessage);
            }
        } else {
            ESP_LOGE(TAG, "CRITICAL ERROR: Data channel is NULL, cannot echo message");
        }
    }
}

/**
 * @brief Default data channel open callback
 * This is called when the data channel is opened
 */
static VOID kvs_onDataChannelOpen(UINT64 customData, PRtcDataChannel pDataChannel)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = (kvs_pc_session_t *)(ULONG_PTR)customData;

    if (session == NULL || pDataChannel == NULL) {
        ESP_LOGE(TAG, "Invalid data channel open parameters");
        return;
    }

    ESP_LOGI(TAG, "Data channel opened for peer: %s", session->peer_id);

    // Mark data channel as ready
    session->data_channel_ready = TRUE;
    session->data_channel = pDataChannel;

    // Always register our message handler that will use global callbacks
    ESP_LOGI(TAG, "Setting up data channel message handler");
    CHK_STATUS(dataChannelOnMessage(pDataChannel, customData, kvs_onDataChannelMessage));

    // Use the global callback registry for open events
    if (g_data_channel_callbacks.callbacks_registered && g_data_channel_callbacks.onOpen != NULL) {
        ESP_LOGI(TAG, "Calling global open callback: %p, customData=0x%llx",
                 g_data_channel_callbacks.onOpen, (unsigned long long)g_data_channel_callbacks.customData);

        // The app_webrtc interface has a different signature than the KVS SDK
        // We need to adapt the parameters to match the app_webrtc_rtc_on_open_t signature
        app_webrtc_rtc_on_open_t app_callback = (app_webrtc_rtc_on_open_t)g_data_channel_callbacks.onOpen;

        // Call the application callback with the peer_id
        ESP_LOGI(TAG, "Invoking global open callback with peer_id=%s", session->peer_id);
        app_callback(g_data_channel_callbacks.customData, pDataChannel, session->peer_id);
        ESP_LOGI(TAG, "Global open callback completed");
    } else {
        ESP_LOGI(TAG, "No global open callback registered");
    }

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Error setting up data channel callbacks: 0x%08" PRIx32, (UINT32) retStatus);
    }
}
#endif

/**
 * @brief Handler for data channel events
 * This is called when a data channel is created by the remote peer
 */
static VOID onDataChannelHandler(UINT64 customData, PRtcDataChannel pRtcDataChannel)
{
#if KVS_ENABLE_DATA_CHANNEL
    kvs_pc_session_t *session = (kvs_pc_session_t *)(ULONG_PTR)customData;

    if (session == NULL) {
        ESP_LOGE(TAG, "Invalid data channel callback parameters");
        return;
    }

    ESP_LOGI(TAG, "Data channel created for peer: %s", session->peer_id);

    // Store the data channel
    session->data_channel = pRtcDataChannel;
    session->data_channel_ready = TRUE;

    DLOGI("New DataChannel has been opened %s \n", pRtcDataChannel->name);
    dataChannelOnMessage(pRtcDataChannel, customData, kvs_onDataChannelMessage);
#else
    ESP_LOGI(TAG, "Data channel support is disabled");
#endif
}

/**
 * @brief Create and send an SDP offer for an initiator session
 * This is called automatically when creating a session as initiator
 */
static WEBRTC_STATUS kvs_pc_trigger_offer(void *pSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = (kvs_pc_session_t *)pSession;

    if (session == NULL) {
        ESP_LOGE(TAG, "kvs_pc_trigger_offer: NULL session");
        return WEBRTC_STATUS_NULL_ARG;
    }

    ESP_LOGI(TAG, "Explicitly triggering offer creation for peer: %s", session->peer_id);

    // Call the internal function to create and send the offer
    retStatus = kvs_create_and_send_offer(session);

    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to create and send offer: 0x%08" PRIx32, (UINT32) retStatus);
        return WEBRTC_STATUS_INTERNAL_ERROR;
    }

    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Internal function to create and send an offer
 * Used both by the public API and internally when needed
 */
static STATUS kvs_create_and_send_offer(kvs_pc_session_t* session)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcSessionDescriptionInit offer_sdp = {0};
    webrtc_message_t offer_msg = {0};

    CHK(session != NULL, STATUS_NULL_ARG);
    CHK(session->is_initiator, STATUS_INVALID_OPERATION);

    ESP_LOGI(TAG, "Creating SDP offer for peer: %s", session->peer_id);

    // Set local description first (this initializes the peer connection)
    CHK_STATUS(setLocalDescription(session->peer_connection, &offer_sdp));

    // Create the offer
    CHK_STATUS(createOffer(session->peer_connection, &offer_sdp));

        // Serialize the SDP offer
        UINT32 offer_len = 0;
    CHK_STATUS(serializeSessionDescriptionInit(&offer_sdp, NULL, &offer_len));

        if (offer_len > 0) {
        PCHAR offer_sdp_str = (PCHAR)MEMALLOC(offer_len + 1);
        CHK(offer_sdp_str != NULL, STATUS_NOT_ENOUGH_MEMORY);

        CHK_STATUS(serializeSessionDescriptionInit(&offer_sdp, offer_sdp_str, &offer_len));
        offer_sdp_str[offer_len] = '\0';

        // Prepare the message for callback
        offer_msg.version = SIGNALING_MESSAGE_CURRENT_VERSION;
        offer_msg.message_type = WEBRTC_MESSAGE_TYPE_OFFER;
        STRNCPY(offer_msg.peer_client_id, session->peer_id, APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN);
        offer_msg.peer_client_id[APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
        offer_msg.payload = offer_sdp_str;
        // Compute length from string to avoid counting trailing NUL
        offer_msg.payload_len = (UINT32) STRLEN(offer_sdp_str);

        // Generate correlation ID using legacy format (timestamp_counter)
        SNPRINTF(offer_msg.correlation_id, APP_WEBRTC_MAX_CORRELATION_ID_LEN, "%llu_%zu",
                 GETTIME(), ATOMIC_INCREMENT(&session->correlation_id_postfix));

        ESP_LOGI(TAG, "Sending SDP offer for peer: %s (len=%" PRIu32 ")", session->peer_id, offer_len);

        // Send through callback if registered
        ESP_LOGI(TAG, "Checking message callback: %p for peer %s", session->on_message_received, session->peer_id);
        if (session->on_message_received != NULL) {
            WEBRTC_STATUS cb_status = session->on_message_received(session->custom_data, &offer_msg);
            if (cb_status != WEBRTC_STATUS_SUCCESS) {
                ESP_LOGW(TAG, "Offer callback failed for peer %s: 0x%08" PRIx32, session->peer_id, (UINT32) cb_status);
            } else {
                ESP_LOGI(TAG, "Offer successfully sent via callback for peer: %s", session->peer_id);
            }
        } else {
            ESP_LOGW(TAG, "No message callback registered - offer not sent for peer: %s", session->peer_id);
            ESP_LOGW(TAG, "Debug: session=%p, custom_data=0x%llx", session, (unsigned long long)session->custom_data);
        }

        SAFE_MEMFREE(offer_sdp_str);
    }

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to create/send offer for peer %s: 0x%08" PRIx32,
                 session ? session->peer_id : "unknown", retStatus);
    }

    return retStatus;
}

/**
 * @brief Get the KVS peer connection interface
 */
#if KVS_ENABLE_DATA_CHANNEL
/**
 * @brief Create a data channel for a session
 *
 * @param pSession Session handle
 * @param channelName Name of the data channel
 * @param pDataChannelInit Optional initialization parameters
 * @param ppDataChannel Pointer to store the created data channel
 * @return WEBRTC_STATUS
 */
static WEBRTC_STATUS kvs_pc_create_data_channel(void *pSession, const char *channelName,
                                               void *pDataChannelInit, void **ppDataChannel)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = NULL;
    PRtcDataChannel pDataChannel = NULL;

    CHK(pSession != NULL && channelName != NULL && ppDataChannel != NULL, STATUS_NULL_ARG);

    session = (kvs_pc_session_t *)pSession;
    CHK(session->peer_connection != NULL, STATUS_INVALID_OPERATION);

    ESP_LOGI(TAG, "Creating data channel '%s' for peer: %s", channelName, session->peer_id);

    // Create the data channel
    CHK_STATUS(createDataChannel(session->peer_connection, (PCHAR)channelName,
                                (PRtcDataChannelInit)pDataChannelInit, &pDataChannel));

    // Store the data channel in the session
    session->data_channel = pDataChannel;

    // Set up default callbacks
    CHK_STATUS(dataChannelOnOpen(pDataChannel, (UINT64)session, kvs_onDataChannelOpen));
    CHK_STATUS(dataChannelOnMessage(pDataChannel, (UINT64)session, kvs_onDataChannelMessage));

    // Return the data channel
    *ppDataChannel = pDataChannel;

    ESP_LOGI(TAG, "Data channel created successfully");

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to create data channel: 0x%08" PRIx32, (UINT32) retStatus);
    }
    return retStatus;
}

/**
 * @brief Set callbacks for data channel events
 *
 * @param pSession Session handle
 * @param onOpen Callback for data channel open event
 * @param onMessage Callback for data channel message event
 * @return WEBRTC_STATUS
 */
static WEBRTC_STATUS kvs_pc_set_data_channel_callbacks(void *pSession,
                                                       app_webrtc_rtc_on_open_t onOpen,
                                                       app_webrtc_rtc_on_message_t onMessage,
                                                       uint64_t customData)
{
    STATUS retStatus = STATUS_SUCCESS;

    // Store the callbacks in the single global registry
    g_data_channel_callbacks.callbacks_registered = TRUE;
    g_data_channel_callbacks.onOpen = (RtcOnOpen)onOpen;
    g_data_channel_callbacks.onMessage = (RtcOnMessage)onMessage;
    g_data_channel_callbacks.customData = customData;

    ESP_LOGI(TAG, "GLOBAL CALLBACK REGISTRATION: Set global data channel callbacks for all sessions");
    ESP_LOGI(TAG, "  - onOpen callback: %p", (void *)onOpen);
    ESP_LOGI(TAG, "  - onMessage callback: %p", (void *)onMessage);
    ESP_LOGI(TAG, "  - customData: 0x%llx", (unsigned long long)customData);

    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to set data channel callbacks: 0x%08" PRIx32, (UINT32) retStatus);
    }
    return retStatus;
}

/**
 * @brief Send a message through the data channel
 *
 * @param pSession Session handle
 * @param isBinary Whether the message is binary data
 * @param pMessage Message buffer
 * @param messageLen Message length
 * @return WEBRTC_STATUS
 */
static WEBRTC_STATUS kvs_pc_send_data_channel_message(void *pSession,
                                                      void *pDataChannel,
                                                      bool isBinary,
                                                      const uint8_t *pMessage,
                                                      uint32_t messageLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_session_t *session = NULL;

    CHK(pSession != NULL && pMessage != NULL && messageLen > 0, STATUS_NULL_ARG);

    session = (kvs_pc_session_t *)pSession;
    CHK(pDataChannel != NULL, STATUS_INVALID_OPERATION);
    CHK(session->data_channel_ready, STATUS_SOCKET_CONNECTION_NOT_READY_TO_SEND);

    ESP_LOGI(TAG, "Sending data channel message to peer: %s (len=%" PRIu32 ", binary=%s)",
             session->peer_id, messageLen, isBinary ? "yes" : "no");

    // Send the message
    CHK_STATUS(dataChannelSend(pDataChannel, isBinary, (PBYTE)pMessage, messageLen));

    ESP_LOGI(TAG, "Data channel message sent successfully");

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to send data channel message: 0x%08" PRIx32, (UINT32) retStatus);
    }
    return retStatus;
}
#else
/**
 * @brief Stub implementation for data channel creation when data channels are disabled
 */
static WEBRTC_STATUS kvs_pc_create_data_channel(void *pSession, const char *channelName,
                                                void *pDataChannelInit, void **ppDataChannel)
{
    ESP_LOGW(TAG, "Data channel support is disabled");
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Stub implementation for setting data channel callbacks when data channels are disabled
 */
static WEBRTC_STATUS kvs_pc_set_data_channel_callbacks(void *pSession,
                                                       app_webrtc_rtc_on_open_t onOpen,
                                                       app_webrtc_rtc_on_message_t onMessage,
                                                       uint64_t customData)
{
    ESP_LOGW(TAG, "Data channel support is disabled");
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Stub implementation for sending data channel messages when data channels are disabled
 */
static WEBRTC_STATUS kvs_pc_send_data_channel_message(void *pSession,
                                                      bool isBinary,
                                                      const uint8_t *pMessage,
                                                      uint32_t messageLen)
{
    ESP_LOGW(TAG, "Data channel support is disabled");
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}
#endif

webrtc_peer_connection_if_t* kvs_peer_connection_if_get(void)
{
    static webrtc_peer_connection_if_t kvs_peer_connection_if = {
        .init = kvs_pc_init,
        .set_ice_servers = kvs_pc_set_ice_servers,
        .create_session = kvs_pc_create_session,
        .send_message = kvs_pc_send_message,
        .destroy_session = kvs_pc_destroy_session,
        .free = kvs_pc_free,
        .set_callbacks = kvs_pc_set_callbacks,
        .trigger_offer = kvs_pc_trigger_offer,  // New function for explicit offer triggering

        // Event handler registration
        .register_event_handler = kvs_pc_register_event_handler,

        // Data channel functions
        .create_data_channel = kvs_pc_create_data_channel,
        .set_data_channel_callbacks = kvs_pc_set_data_channel_callbacks,
        .send_data_channel_message = kvs_pc_send_data_channel_message,
    };

    return &kvs_peer_connection_if;
}

// =============================================================================
// Internal KVS Peer Connection Implementation - equivalent to app_webrtc.c
// =============================================================================

/**
 * @brief Initialize peer connection with proper KVS configuration
 * Equivalent to initializePeerConnection in app_webrtc.c
 */
static STATUS kvs_initializePeerConnection(kvs_pc_client_t* client, PRtcPeerConnection* ppRtcPeerConnection)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    RtcConfiguration configuration;
    PRtcCertificate pRtcCertificate = NULL;

    CHK(client != NULL && ppRtcPeerConnection != NULL, STATUS_NULL_ARG);

    // Initialize the configuration structure to zeros
    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    // Use EXACTLY the same ICE timeouts as legacy path for compatibility
    configuration.kvsRtcConfiguration.iceConnectionCheckTimeout = 12 * HUNDREDS_OF_NANOS_IN_A_SECOND; // 12 seconds
    configuration.kvsRtcConfiguration.iceLocalCandidateGatheringTimeout = 20 * HUNDREDS_OF_NANOS_IN_A_SECOND; // 20 seconds (more generous)
    configuration.kvsRtcConfiguration.iceCandidateNominationTimeout = 30 * HUNDREDS_OF_NANOS_IN_A_SECOND; // 30 seconds (doubled)
    configuration.kvsRtcConfiguration.iceConnectionCheckPollingInterval = 200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND; // 200ms

    // Enable interface filtering to handle IPv6 properly
    configuration.kvsRtcConfiguration.iceSetInterfaceFilterFunc = kvs_sampleFilterNetworkInterfaces;

    // Set the ICE mode - prefer ALL; RELAY-only can be forced by upstream if needed
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_ALL;

    // Configure ICE servers from client config when provided; fallback to public STUN
    if (client->config.ice_servers != NULL && client->config.ice_server_count > 0) {
        RtcIceServer *iceServers = (RtcIceServer *) client->config.ice_servers;

        // Copy up to the capacity of configuration.iceServers
        for (UINT32 i = 0; i < client->config.ice_server_count && i < ARRAY_SIZE(configuration.iceServers); i++) {
            // urls: ensure null-termination and trim whitespace
            configuration.iceServers[i].urls[0] = '\0';
            if (iceServers[i].urls[0] != '\0') {
                // Optionally filter out TURN when not desired
                BOOL isTurn = (STRNCMPI(iceServers[i].urls, "turn:", 5) == 0) || (STRNCMPI(iceServers[i].urls, "turns:", 6) == 0);
                if (isTurn && !client->config.use_turn) {
                    configuration.iceServers[i].urls[0] = '\0';
                } else {
                    STRNCPY(configuration.iceServers[i].urls, iceServers[i].urls, MAX_ICE_CONFIG_URI_LEN - 1);
                    configuration.iceServers[i].urls[MAX_ICE_CONFIG_URI_LEN - 1] = '\0';
                }
            }

            // username
            configuration.iceServers[i].username[0] = '\0';
            if (iceServers[i].username[0] != '\0') {
                STRNCPY(configuration.iceServers[i].username, iceServers[i].username, MAX_ICE_CONFIG_USER_NAME_LEN - 1);
                configuration.iceServers[i].username[MAX_ICE_CONFIG_USER_NAME_LEN - 1] = '\0';
            }

            // credential
            configuration.iceServers[i].credential[0] = '\0';
            if (iceServers[i].credential[0] != '\0') {
                STRNCPY(configuration.iceServers[i].credential, iceServers[i].credential, MAX_ICE_CONFIG_CREDENTIAL_LEN - 1);
                configuration.iceServers[i].credential[MAX_ICE_CONFIG_CREDENTIAL_LEN - 1] = '\0';
            }

            // Skip obviously bad entries
            if (configuration.iceServers[i].urls[0] == '\0') {
                continue;
            }
            ESP_LOGI(TAG, "ICE server %" PRIu32 ": %s", i, configuration.iceServers[i].urls);
        }
    } else {
        // Fallback to hardcoded STUN server
        SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, "stun:stun.l.google.com:19302");
        configuration.iceServers[0].username[0] = '\0';
        configuration.iceServers[0].credential[0] = '\0';
        ESP_LOGI(TAG, "Creating peer connection with STUN server: %s", configuration.iceServers[0].urls);
    }

    // Get or create a certificate for the peer connection
    CHK_STATUS(kvs_getOrCreateCertificate(client, &pRtcCertificate));

    // Add certificate to configuration if available
    if (pRtcCertificate != NULL) {
        configuration.certificates[0] = *pRtcCertificate;
        ESP_LOGI(TAG, "Using certificate for peer connection");
    }

    // Create the peer connection
    CHK_STATUS(createPeerConnection(&configuration, ppRtcPeerConnection));

    ESP_LOGI(TAG, "KVS peer connection created successfully");

CleanUp:
    CHK_LOG_ERR(retStatus);

    freeRtcCertificate(pRtcCertificate);

    LEAVES();
    return retStatus;
}

/**
 * @brief Setup media tracks for the peer connection
 * Equivalent to the media track setup in createSampleStreamingSession
 */
static STATUS kvs_setupMediaTracks(kvs_pc_session_t* session)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcMediaStreamTrack videoTrack, audioTrack;
    RtcRtpTransceiverInit videoRtpTransceiverInit, audioRtpTransceiverInit;

    CHK(session != NULL && session->peer_connection != NULL, STATUS_NULL_ARG);

    MEMSET(&videoTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
    MEMSET(&audioTrack, 0x00, SIZEOF(RtcMediaStreamTrack));

    ESP_LOGI(TAG, "Setting up media tracks for peer: %s", session->peer_id);

    // Add supported codecs from configuration
    CHK_STATUS(addSupportedCodec(session->peer_connection, session->client->config.video_codec));
    CHK_STATUS(addSupportedCodec(session->peer_connection, session->client->config.audio_codec));

    // Add video transceiver (match legacy identifiers for compatibility)
    videoTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    videoTrack.codec = session->client->config.video_codec;
    videoRtpTransceiverInit.direction = session->client->config.receive_media
        ? RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV
        : RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
    STRCPY(videoTrack.streamId, "myKvsVideoStream");
    STRCPY(videoTrack.trackId, "myVideoTrack");
    CHK_STATUS(addTransceiver(session->peer_connection, &videoTrack, &videoRtpTransceiverInit, &session->video_transceiver));

    // Set up bandwidth estimation for video transceiver
    CHK_STATUS(transceiverOnBandwidthEstimation(session->video_transceiver, (UINT64)session, kvs_videoBandwidthEstimationHandler));

    // Add audio transceiver (match legacy identifiers for compatibility)
    audioTrack.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
    audioTrack.codec = session->client->config.audio_codec;
    audioRtpTransceiverInit.direction = session->client->config.receive_media
        ? RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV

        : RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
    STRCPY(audioTrack.streamId, "myKvsVideoStream");
    STRCPY(audioTrack.trackId, "myAudioTrack");
    CHK_STATUS(addTransceiver(session->peer_connection, &audioTrack, &audioRtpTransceiverInit, &session->audio_transceiver));

    // Set up bandwidth estimation for audio transceiver
    CHK_STATUS(transceiverOnBandwidthEstimation(session->audio_transceiver, (UINT64)session, kvs_audioBandwidthEstimationHandler));

    // Set up TWCC bandwidth estimation if enabled and client supports it
#if KVS_ENABLE_SENDER_BANDWIDTH_ESTIMATION
    if (session->client->config.trickle_ice) {  // Use trickle_ice as proxy for TWCC support
        CHK_STATUS(peerConnectionOnSenderBandwidthEstimation(session->peer_connection, (UINT64)session, kvs_senderBandwidthEstimationHandler));
        ESP_LOGI(TAG, "TWCC sender bandwidth estimation enabled for peer: %s", session->peer_id);
    }
#else
    ESP_LOGD(TAG, "TWCC sender bandwidth estimation disabled by macro for peer: %s", session->peer_id);
#endif

    ESP_LOGI(TAG, "Media tracks configured successfully");

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

/**
 * @brief Handle incoming SDP offer
 * Equivalent to handleOffer in app_webrtc.c with full functionality from inline implementation
 */
static STATUS kvs_handleOffer(kvs_pc_session_t* session, webrtc_message_t* message)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcSessionDescriptionInit offerSessionDescriptionInit;
    NullableBool canTrickle;

    CHK(session != NULL && message != NULL, STATUS_NULL_ARG);
    CHK(message->message_type == WEBRTC_MESSAGE_TYPE_OFFER, STATUS_INVALID_ARG);
    CHK(!session->is_initiator, STATUS_INVALID_OPERATION); // Sanity check: only responders process offers

    ESP_LOGD(TAG, "Master processing SDP offer from viewer: peer=%s, payload_len=%" PRIu32 ", offer_corr_id='%s'",
             session->peer_id, message->payload_len, message->correlation_id);
    ESP_LOGD(TAG, "Offer content (first 200 chars): %.200s", message->payload);

    MEMSET(&offerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    // Deserialize the offer
    CHK_STATUS(deserializeSessionDescriptionInit(message->payload, message->payload_len, &offerSessionDescriptionInit));

    // Set remote description
    CHK_STATUS(setRemoteDescription(session->peer_connection, &offerSessionDescriptionInit));

    // Check if remote supports trickle ICE
    canTrickle = canTrickleIceCandidates(session->peer_connection);
    CHECK(!NULLABLE_CHECK_EMPTY(canTrickle));
    session->remote_can_trickle_ice = canTrickle.value;

    // CRITICAL FIX: Follow EXACT legacy sequence - setLocalDescription BEFORE createAnswer!
    MEMSET(&session->answer_session_description, 0x00, SIZEOF(RtcSessionDescriptionInit));
    session->answer_session_description.useTrickleIce = TRUE;
    CHK_STATUS(setLocalDescription(session->peer_connection, &session->answer_session_description));
    CHK_STATUS(createAnswer(session->peer_connection, &session->answer_session_description));

    // CRITICAL: Send answer timing based on trickle ICE support
    if (session->remote_can_trickle_ice && session->on_message_received != NULL) {
        webrtc_message_t answer_msg = {0};
        answer_msg.version = SIGNALING_MESSAGE_CURRENT_VERSION;
        answer_msg.message_type = WEBRTC_MESSAGE_TYPE_ANSWER;
        STRCPY(answer_msg.peer_client_id, session->peer_id);
        // Generate fresh correlation ID like legacy respondWithAnswer
        SNPRINTF(answer_msg.correlation_id, MAX_CORRELATION_ID_LEN, "%llu_%zu",
                 GETTIME(), ATOMIC_INCREMENT(&session->correlation_id_postfix));

        // Serialize the SDP answer with exact-sized heap buffer
        UINT32 answer_len = 0;
        CHK_STATUS(serializeSessionDescriptionInit(&session->answer_session_description, NULL, &answer_len));
        if (answer_len > 0) {
            PCHAR payload = (PCHAR) MEMALLOC(answer_len + 1);
            CHK(payload != NULL, STATUS_NOT_ENOUGH_MEMORY);
            CHK_STATUS(serializeSessionDescriptionInit(&session->answer_session_description, payload, &answer_len));
            payload[answer_len] = '\0';
            answer_msg.payload = payload;
            answer_msg.payload_len = (UINT32) STRLEN(payload);
            ESP_LOGD(TAG, "Master sending SDP answer to viewer: peer=%s, len=%" PRIu32 ", corr_id='%s', trickle=%s",
                     session->peer_id, answer_len, answer_msg.correlation_id,
                     session->remote_can_trickle_ice ? "true" : "false");
            ESP_LOGI(TAG, "INTERFACE_SDP_ANSWER_FULL: %s", payload);

            session->on_message_received(session->custom_data, &answer_msg);
            SAFE_MEMFREE(payload);
        }
    }

    // CRITICAL PARITY FIX: Start media threads exactly like legacy handleOffer
    if (session->on_peer_state_changed != NULL) {
        session->on_peer_state_changed(session->custom_data, WEBRTC_PEER_STATE_MEDIA_STARTING);
    }

    ESP_LOGI(TAG, "SDP offer processed successfully, trickle ICE: %s",
             session->remote_can_trickle_ice ? "enabled" : "disabled");

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

/**
 * @brief Handle incoming SDP answer
 * Equivalent to handleAnswer in app_webrtc.c with full functionality from inline implementation
 */
static STATUS kvs_handleAnswer(kvs_pc_session_t* session, webrtc_message_t* message)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcSessionDescriptionInit answerSessionDescriptionInit;

    CHK(session != NULL && message != NULL, STATUS_NULL_ARG);
    CHK(message->message_type == WEBRTC_MESSAGE_TYPE_ANSWER, STATUS_INVALID_ARG);

    ESP_LOGI(TAG, "Handling SDP answer from peer: %s", session->peer_id);

    MEMSET(&answerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    // Deserialize the answer
    CHK_STATUS(deserializeSessionDescriptionInit(message->payload, message->payload_len, &answerSessionDescriptionInit));

    // Set remote description
    CHK_STATUS(setRemoteDescription(session->peer_connection, &answerSessionDescriptionInit));

    // CRITICAL PARITY FIX: Start media threads exactly like legacy handleAnswer
    if (session->on_peer_state_changed != NULL) {
        session->on_peer_state_changed(session->custom_data, WEBRTC_PEER_STATE_MEDIA_STARTING);
    }

    ESP_LOGI(TAG, "SDP answer processed successfully");

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

/**
 * @brief Handle incoming ICE candidate
 * Equivalent to handleRemoteCandidate in app_webrtc.c with full validation from inline implementation
 */
static STATUS kvs_handleRemoteCandidate(kvs_pc_session_t* session, webrtc_message_t* message)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcIceCandidateInit iceCandidate;

    CHK(session != NULL && message != NULL, STATUS_NULL_ARG);
    CHK(message->message_type == WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE, STATUS_INVALID_ARG);

    ESP_LOGD(TAG, "Handling ICE candidate from peer: %s", session->peer_id);

    MEMSET(&iceCandidate, 0x00, SIZEOF(RtcIceCandidateInit));

    // Deserialize the ICE candidate
    CHK_STATUS(deserializeRtcIceCandidateInit(message->payload, message->payload_len, &iceCandidate));

    if (iceCandidate.candidate[0] == '\0') {
        ESP_LOGD(TAG, "Ignoring empty ICE candidate");
        CHK(FALSE, STATUS_SUCCESS);
    }

    // Basic validation: candidate should contain the attribute prefix
    if (STRSTR(iceCandidate.candidate, "candidate:") == NULL && STRSTR(iceCandidate.candidate, "candidate ") == NULL) {
        ESP_LOGW(TAG, "Skipping non-standard ICE candidate: %.*s%s",
                 64, iceCandidate.candidate, STRLEN(iceCandidate.candidate) > 64 ? "..." : "");
        CHK(FALSE, STATUS_SUCCESS);
    }

    // Skip TCP candidates – ESP flow typically uses UDP only
    if (STRSTR(iceCandidate.candidate, " tcp ") != NULL) {
        ESP_LOGD(TAG, "Skipping TCP ICE candidate");
        CHK(FALSE, STATUS_SUCCESS);
    }

    // Ensure UDP appears as protocol token
    if (STRSTR(iceCandidate.candidate, " udp ") == NULL) {
        ESP_LOGD(TAG, "Skipping non-UDP ICE candidate");
        CHK(FALSE, STATUS_SUCCESS);
    }

    // Add the candidate
    {
        STATUS s = addIceCandidate(session->peer_connection, iceCandidate.candidate);
        if (STATUS_FAILED(s)) {
            ESP_LOGE(TAG, "addIceCandidate failed: 0x%08" PRIx32 " for '%.*s%s'", (UINT32) s,
                     64, iceCandidate.candidate,
                     STRLEN(iceCandidate.candidate) > 64 ? "..." : "");
            retStatus = s;
            goto CleanUp;
        }
    }
    ESP_LOGD(TAG, "Added ICE candidate OK");

CleanUp:
    return retStatus;
}





/**
 * @brief Network interface filter - equivalent to sampleFilterNetworkInterfaces
 */
static BOOL kvs_sampleFilterNetworkInterfaces(UINT64 customData, PCHAR networkInt)
{
    UNUSED_PARAM(customData);

    // Filter out IPv6 interfaces
    if (STRSTR(networkInt, "::") != NULL) {
        ESP_LOGD(TAG, "Filtering out IPv6 interface: %s", networkInt);
        return FALSE;
    }

    BOOL isIpv6 = (STRSTR(networkInt, ":") != NULL && STRSTR(networkInt, ".") == NULL);
    if (isIpv6) {
        ESP_LOGD(TAG, "Filtering out IPv6 interface: %s", networkInt);
        return FALSE;
    }

    ESP_LOGD(TAG, "Using interface: %s", networkInt);
    return TRUE;
}

// =============================================================================
// Bandwidth Estimation Handlers - equivalent to app_webrtc_media.c handlers
// =============================================================================

/**
 * @brief KVS bandwidth estimation handler - equivalent to sampleBandwidthEstimationHandler
 */
static VOID kvs_videoBandwidthEstimationHandler(UINT64 customData, DOUBLE maximumBitrate)
{
    kvs_pc_session_t *session = (kvs_pc_session_t *)(ULONG_PTR)customData;

    if (session == NULL) {
        ESP_LOGE(TAG, "Invalid session in video bandwidth estimation handler");
        return;
    }

    // Convert to kbps and apply some headroom (80% of suggested bitrate)
    uint32_t bitrate_kbps = (uint32_t)((maximumBitrate * 0.8) / 1000);

    // Apply reasonable limits for video
    if (bitrate_kbps < 100) bitrate_kbps = 100;  // Minimum 100 kbps
    if (bitrate_kbps > 5000) bitrate_kbps = 5000; // Maximum 5 Mbps

    ESP_LOGI(TAG, "Received VIDEO bitrate suggestion for peer %s: %.0f bps (adjusted to %" PRIu32 " kbps)",
             session->peer_id, maximumBitrate, bitrate_kbps);

    // Update bitrate via video capture interface if available
    if (session->client->config.video_capture != NULL && session->video_handle != NULL) {
        media_stream_video_capture_t *video_capture =
            (media_stream_video_capture_t*)session->client->config.video_capture;

        if (video_capture->set_bitrate != NULL) {
            ESP_LOGI(TAG, "Adjusting video bitrate to %" PRIu32 " kbps for peer %s",
                     bitrate_kbps, session->peer_id);
            video_capture->set_bitrate(session->video_handle, bitrate_kbps);
        } else {
            ESP_LOGW(TAG, "Video capture interface does not support dynamic bitrate adjustment");
        }
    }
}

static VOID kvs_audioBandwidthEstimationHandler(UINT64 customData, DOUBLE maximumBitrate)
{
    kvs_pc_session_t *session = (kvs_pc_session_t *)(ULONG_PTR)customData;

    if (session == NULL) {
        ESP_LOGE(TAG, "Invalid session in audio bandwidth estimation handler");
        return;
    }

    // Convert to kbps
    uint32_t bitrate_kbps = (uint32_t)(maximumBitrate / 1000);

    // Apply reasonable limits for audio
    if (bitrate_kbps < 8) bitrate_kbps = 8;      // Minimum 8 kbps
    if (bitrate_kbps > 128) bitrate_kbps = 128;  // Maximum 128 kbps

    ESP_LOGI(TAG, "Received AUDIO bitrate suggestion for peer %s: %.0f bps (adjusted to %" PRIu32 " kbps)",
             session->peer_id, maximumBitrate, bitrate_kbps);

    // Currently we don't have a mechanism to adjust audio bitrate dynamically
    // This could be implemented in the future if needed
    ESP_LOGD(TAG, "Audio bitrate adjustment not implemented");
}

#if KVS_ENABLE_SENDER_BANDWIDTH_ESTIMATION
/**
 * @brief KVS TWCC bandwidth estimation handler - equivalent to sampleSenderBandwidthEstimationHandler
 */
static VOID kvs_senderBandwidthEstimationHandler(UINT64 customData, UINT32 txBytes, UINT32 rxBytes,
                                                 UINT32 txPacketsCnt, UINT32 rxPacketsCnt, UINT64 duration)
{
    UNUSED_PARAM(duration);
    kvs_pc_session_t *session = (kvs_pc_session_t *)(ULONG_PTR)customData;
    UINT64 videoBitrate, audioBitrate;
    UINT64 currentTimeMs, timeDiff;
    UINT32 lostPacketsCnt = txPacketsCnt - rxPacketsCnt;
    DOUBLE percentLost = (DOUBLE) ((txPacketsCnt > 0) ? (lostPacketsCnt * 100 / txPacketsCnt) : 0.0);

    if (session == NULL) {
        ESP_LOGW(TAG, "Invalid session in TWCC bandwidth estimation handler");
        return;
    }

    // Calculate packet loss with exponential moving average
    #define EMA_ACCUMULATOR_GET_NEXT(avg, sample) ((avg) * 0.9 + (sample) * 0.1)
    session->twcc_metadata.average_packet_loss =
        EMA_ACCUMULATOR_GET_NEXT(session->twcc_metadata.average_packet_loss, percentLost);

    currentTimeMs = GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    timeDiff = currentTimeMs - session->twcc_metadata.last_adjustment_time_ms;

    #define TWCC_BITRATE_ADJUSTMENT_INTERVAL_MS 1000  // 1 second
    if (timeDiff < TWCC_BITRATE_ADJUSTMENT_INTERVAL_MS) {
        return; // Too soon for another adjustment
    }

    if (!IS_VALID_MUTEX_VALUE(session->twcc_metadata.update_lock)) {
        return;
    }

    MUTEX_LOCK(session->twcc_metadata.update_lock);
    videoBitrate = session->twcc_metadata.current_video_bitrate;
    audioBitrate = session->twcc_metadata.current_audio_bitrate;

    // Adjust bitrate based on packet loss
    if (session->twcc_metadata.average_packet_loss <= 5.0) {
        // Low packet loss - increase bitrate by 5%
        videoBitrate = (UINT64)(videoBitrate * 1.05);
        audioBitrate = (UINT64)(audioBitrate * 1.05);
    } else {
        // High packet loss - reduce bitrate by percentage lost
        DOUBLE reductionFactor = 1.0 - (session->twcc_metadata.average_packet_loss / 100.0);
        videoBitrate = (UINT64)(videoBitrate * reductionFactor);
        audioBitrate = (UINT64)(audioBitrate * reductionFactor);
    }

    // Apply limits
    #define MIN_VIDEO_BITRATE 100000   // 100 kbps
    #define MAX_VIDEO_BITRATE 5000000  // 5 Mbps
    #define MIN_AUDIO_BITRATE 32000    // 32 kbps
    #define MAX_AUDIO_BITRATE 128000   // 128 kbps

    videoBitrate = MAX(MIN_VIDEO_BITRATE, MIN(MAX_VIDEO_BITRATE, videoBitrate));
    audioBitrate = MAX(MIN_AUDIO_BITRATE, MIN(MAX_AUDIO_BITRATE, audioBitrate));

    session->twcc_metadata.current_video_bitrate = videoBitrate;
    session->twcc_metadata.current_audio_bitrate = audioBitrate;
    session->twcc_metadata.last_adjustment_time_ms = currentTimeMs;

    MUTEX_UNLOCK(session->twcc_metadata.update_lock);

    ESP_LOGD(TAG, "TWCC adjustment for peer %s: loss=%.2f%%, video=%lluKbps, audio=%lluKbps",
             session->peer_id, session->twcc_metadata.average_packet_loss,
             videoBitrate / 1000, audioBitrate / 1000);
}
#endif // KVS_ENABLE_SENDER_BANDWIDTH_ESTIMATION

// =============================================================================
// Certificate Pre-generation Implementation
// =============================================================================

/**
 * @brief Timer callback for certificate pre-generation
 */
static STATUS kvs_pregenerateCertTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    UNUSED_PARAM(timerId);
    UNUSED_PARAM(currentTime);
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_client_t *client = (kvs_pc_client_t *) customData;
    BOOL locked = FALSE;
    UINT32 certCount;
    PRtcCertificate pRtcCertificate = NULL;

    CHK_WARN(client != NULL, STATUS_NULL_ARG, "[KVS] kvs_pregenerateCertTimerCallback(): Passed argument is NULL");

    // Use MUTEX_TRYLOCK to avoid possible dead lock when canceling timerQueue
    if (!MUTEX_TRYLOCK(client->certificateLock)) {
        return retStatus;
    } else {
        locked = TRUE;
    }

    // Quick check if there is anything that needs to be done.
    CHK_STATUS(stackQueueGetCount(client->pregeneratedCertificates, &certCount));

    // If we've reached the maximum, nothing to do (same logic as Common.c)
    CHK(certCount != MAX_RTCCONFIGURATION_CERTIFICATES, retStatus);

    // Generate the certificate with the keypair
    CHK_STATUS(createRtcCertificate(&pRtcCertificate));

    // Add to the stack queue
    CHK_STATUS(stackQueueEnqueue(client->pregeneratedCertificates, (UINT64) pRtcCertificate));

    ESP_LOGV(TAG, "New certificate has been pre-generated and added to the queue");

    // Reset it so it won't be freed on exit
    pRtcCertificate = NULL;

    MUTEX_UNLOCK(client->certificateLock);
    locked = FALSE;

CleanUp:

    if (pRtcCertificate != NULL) {
        freeRtcCertificate(pRtcCertificate);
    }

    if (locked) {
        MUTEX_UNLOCK(client->certificateLock);
    }

    return retStatus;
}

/**
 * @brief Initialize certificate pre-generation for a KVS client
 */
static STATUS kvs_initializeCertificatePregeneration(kvs_pc_client_t* client)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(client != NULL, STATUS_NULL_ARG);

    ESP_LOGI(TAG, "Initializing certificate pre-generation");

    // Create certificate lock
    client->certificateLock = MUTEX_CREATE(TRUE);
    CHK(IS_VALID_MUTEX_VALUE(client->certificateLock), STATUS_INVALID_OPERATION);

    // Create certificate queue
    CHK_STATUS(stackQueueCreate(&client->pregeneratedCertificates));

    // Create timer queue if not already created
    if (!IS_VALID_TIMER_QUEUE_HANDLE(client->timer_queue)) {
        CHK_STATUS(timerQueueCreateEx(&client->timer_queue, "kvsCertTmr", KVS_TIMER_QUEUE_THREAD_SIZE));
    }

    // Start the certificate pre-generation timer
    if (KVS_PRE_GENERATE_CERT) {
        CHK_STATUS(timerQueueAddTimer(client->timer_queue, 0, KVS_PRE_GENERATE_CERT_PERIOD,
                                     kvs_pregenerateCertTimerCallback, (UINT64) client,
                                     &client->pregenerateCertTimerId));
        ESP_LOGI(TAG, "Certificate pre-generation timer started");
    } else {
        client->pregenerateCertTimerId = MAX_UINT32;
        ESP_LOGI(TAG, "Certificate pre-generation disabled");
    }

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to initialize certificate pre-generation: 0x%08" PRIx32, (UINT32) retStatus);
        if (client != NULL) {
            kvs_freeCertificatePregeneration(client);
        }
    }

    return retStatus;
}

/**
 * @brief Free certificate pre-generation resources
 */
static STATUS kvs_freeCertificatePregeneration(kvs_pc_client_t* client)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 data;
    StackQueueIterator iterator;

    CHK(client != NULL, STATUS_NULL_ARG);

    ESP_LOGI(TAG, "Freeing certificate pre-generation resources");

    // Cancel timer if active
    if (IS_VALID_TIMER_QUEUE_HANDLE(client->timer_queue) && client->pregenerateCertTimerId != MAX_UINT32) {
        retStatus = timerQueueCancelTimer(client->timer_queue, client->pregenerateCertTimerId, (UINT64) client);
        if (STATUS_FAILED(retStatus)) {
            ESP_LOGE(TAG, "Failed to cancel certificate pre-generation timer: 0x%08" PRIx32, (UINT32) retStatus);
        }
        client->pregenerateCertTimerId = MAX_UINT32;
    }

    // Free all pre-generated certificates
    if (client->pregeneratedCertificates != NULL) {
        stackQueueGetIterator(client->pregeneratedCertificates, &iterator);
        while (IS_VALID_ITERATOR(iterator)) {
            stackQueueIteratorGetItem(iterator, &data);
            stackQueueIteratorNext(&iterator);
            freeRtcCertificate((PRtcCertificate) data);
        }

        stackQueueClear(client->pregeneratedCertificates, FALSE);
        stackQueueFree(client->pregeneratedCertificates);
        client->pregeneratedCertificates = NULL;
    }

    // Free certificate lock
    if (IS_VALID_MUTEX_VALUE(client->certificateLock)) {
        MUTEX_FREE(client->certificateLock);
        client->certificateLock = INVALID_MUTEX_VALUE;
    }

CleanUp:
    return retStatus;
}

/**
 * @brief Get a pre-generated certificate or create one on demand
 * Following Common.c pattern for proper certificate ownership and error handling
 */
static STATUS kvs_getOrCreateCertificate(kvs_pc_client_t* client, PRtcCertificate* ppCertificate)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 data;
    UINT32 certCount;
    PRtcCertificate pCertificate = NULL;

    CHK(client != NULL && ppCertificate != NULL, STATUS_NULL_ARG);

    *ppCertificate = NULL;

    // Try to get a pre-generated certificate (following Common.c pattern)
    if (client->pregeneratedCertificates != NULL && IS_VALID_MUTEX_VALUE(client->certificateLock)) {
        MUTEX_LOCK(client->certificateLock);

        // Attempt to dequeue a certificate
        retStatus = stackQueueDequeue(client->pregeneratedCertificates, &data);

        // Handle both success and empty queue cases (like Common.c)
        if (retStatus == STATUS_SUCCESS) {
            pCertificate = (PRtcCertificate) data;
            CHK_STATUS(stackQueueGetCount(client->pregeneratedCertificates, &certCount));
            ESP_LOGI(TAG, "Using pre-generated certificate (remaining in pool: %" PRIu32 ")", certCount);
        } else if (retStatus == STATUS_NOT_FOUND) {
            // Empty queue is not an error - we'll create on demand
            retStatus = STATUS_SUCCESS;
            ESP_LOGD(TAG, "Certificate queue is empty, will create on demand");
        } else {
            // Actual error occurred
            CHK_STATUS(retStatus);
        }

        MUTEX_UNLOCK(client->certificateLock);
    }

    // If no pre-generated certificate available, create one on demand
    if (pCertificate == NULL) {
        ESP_LOGI(TAG, "No pre-generated certificate available, creating on demand");
        CHK_STATUS(createRtcCertificate(&pCertificate));
        ESP_LOGI(TAG, "Certificate created on demand");
    }

    *ppCertificate = pCertificate;

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to get or create certificate: 0x%08" PRIx32, (UINT32) retStatus);
        if (pCertificate != NULL) {
            freeRtcCertificate(pCertificate);
        }
    }

    return retStatus;
}

// =============================================================================
// ICE Statistics Implementation
// =============================================================================

/**
 * @brief Timer callback for ICE candidate pair statistics collection
 * Moved from app_webrtc.c to kvs_webrtc.c where it belongs with the KVS implementation
 */
static STATUS kvs_iceCandidatePairStatsCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    UNUSED_PARAM(timerId);
    UNUSED_PARAM(currentTime);
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_client_t* client = (kvs_pc_client_t*)customData;
    BOOL locked = FALSE;

    CHK(client != NULL, STATUS_NULL_ARG);

    // Use MUTEX_TRYLOCK to avoid possible deadlock when canceling timer
    if (!MUTEX_TRYLOCK(client->statsLock)) {
        return STATUS_SUCCESS;
    }
    locked = TRUE;

    // Iterate through internally tracked sessions and collect comprehensive metrics
    PHashTable pActiveSessionsTable = client->activeSessions;
    UINT32 activeSessionCount = 0;

    ESP_LOGV(TAG, "ICE candidate pair statistics collection timer triggered");

    if (pActiveSessionsTable != NULL) {
        // Get session count
        CHK_STATUS(hashTableGetCount(pActiveSessionsTable, &activeSessionCount));
        ESP_LOGV(TAG, "Collecting metrics for %" PRIu32 " internally tracked sessions", activeSessionCount);

        if (activeSessionCount > 0) {
            // Iterate through all active sessions using callback approach
            CHK_STATUS(hashTableIterateEntries(pActiveSessionsTable, (UINT64)client, kvs_metricsCollectionCallback));
        } else {
            ESP_LOGV(TAG, "No active sessions to collect metrics for");
        }
    } else {
        ESP_LOGW(TAG, "Internal session tracking table is NULL - metrics collection disabled");
    }

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(client->statsLock);
    }
    return retStatus;
}

/**
 * @brief Start ICE statistics collection timer
 */
static STATUS kvs_startIceStats(kvs_pc_client_t* client)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(client != NULL, STATUS_NULL_ARG);
    CHK(IS_VALID_TIMER_QUEUE_HANDLE(client->timer_queue), STATUS_INVALID_OPERATION);

    if (client->iceCandidatePairStatsTimerId == MAX_UINT32) {
        ESP_LOGI(TAG, "Starting ICE candidate pair statistics collection");
        CHK_STATUS(timerQueueAddTimer(client->timer_queue, KVS_ICE_STATS_DURATION, KVS_ICE_STATS_DURATION,
                                      kvs_iceCandidatePairStatsCallback, (UINT64)client,
                                      &client->iceCandidatePairStatsTimerId));
        ESP_LOGI(TAG, "ICE stats timer started successfully");
    }

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to start ICE stats timer: 0x%08" PRIx32, (UINT32) retStatus);
    }
    return retStatus;
}

/**
 * @brief Stop ICE statistics collection timer
 */
static STATUS kvs_stopIceStats(kvs_pc_client_t* client)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(client != NULL, STATUS_NULL_ARG);

    if (client->iceCandidatePairStatsTimerId != MAX_UINT32 &&
        IS_VALID_TIMER_QUEUE_HANDLE(client->timer_queue)) {
        ESP_LOGI(TAG, "Stopping ICE candidate pair statistics collection");
        CHK_STATUS(timerQueueCancelTimer(client->timer_queue, client->iceCandidatePairStatsTimerId, (UINT64)client));
        client->iceCandidatePairStatsTimerId = MAX_UINT32;
        ESP_LOGI(TAG, "ICE stats timer stopped successfully");
    }

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to stop ICE stats timer: 0x%08" PRIx32, (UINT32) retStatus);
    }
    return retStatus;
}

/**
 * @brief Callback function for hash table iteration to collect metrics for each session
 *
 * This function is called by hashTableIterateEntries() for each session in the activeSessions table.
 *
 * @param callerData The kvs_pc_client_t* passed as UINT64
 * @param pHashEntry The hash entry containing session pointer as value
 * @return STATUS Success or failure status
 */
static STATUS kvs_metricsCollectionCallback(UINT64 callerData, PHashEntry pHashEntry)
{
    STATUS retStatus = STATUS_SUCCESS;
    kvs_pc_client_t* client = (kvs_pc_client_t*)callerData;
    kvs_pc_session_t* pSession = NULL;

    CHK(client != NULL && pHashEntry != NULL, STATUS_NULL_ARG);

    // Extract session from hash entry value
    pSession = (kvs_pc_session_t*)pHashEntry->value;
    CHK(pSession != NULL, STATUS_NULL_ARG);

    // Skip invalid or terminated sessions
    if (pSession->terminated || pSession->peer_connection == NULL) {
        ESP_LOGV(TAG, "Skipping invalid or terminated session");
        CHK(FALSE, retStatus);  // Continue to next session
    }

    ESP_LOGV(TAG, "Collecting metrics for session: %s", pSession->peer_id);

    // Collect comprehensive ICE candidate pair statistics using the KVS logging function
    STATUS statsStatus = logIceCandidatePairStats(
        pSession->peer_connection,
        &pSession->rtc_stats,
        &pSession->rtc_metrics_history
    );

    if (STATUS_FAILED(statsStatus)) {
        ESP_LOGW(TAG, "Failed to collect ICE stats for session %s: 0x%08" PRIx32,
                pSession->peer_id, (UINT32) statsStatus);
        // Continue with other metrics even if ICE stats fail
    } else {
        ESP_LOGV(TAG, "Successfully collected ICE stats for session: %s", pSession->peer_id);
    }

    // Also collect general peer connection and ICE agent metrics if enabled
    if (client->enable_metrics) {
        STATUS pcStatsStatus = peerConnectionGetMetrics(pSession->peer_connection, &pSession->pc_metrics);
        STATUS iceStatsStatus = iceAgentGetMetrics(pSession->peer_connection, &pSession->ice_metrics);

        if (STATUS_FAILED(pcStatsStatus)) {
            ESP_LOGV(TAG, "Failed to get peer connection metrics for %s: 0x%08" PRIx32,
                    pSession->peer_id, (UINT32) pcStatsStatus);
        }
        if (STATUS_FAILED(iceStatsStatus)) {
            ESP_LOGV(TAG, "Failed to get ICE agent metrics for %s: 0x%08" PRIx32,
                    pSession->peer_id, (UINT32) iceStatsStatus);
        }
    }

    // Update metrics history timestamp
    pSession->rtc_metrics_history.prevTs = GETTIME();

CleanUp:
    return retStatus;
}
