/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __KVS_WEBRTC_INTERNAL_H__
#define __KVS_WEBRTC_INTERNAL_H__

/**
 * @file kvs_webrtc_internal.h
 * @brief Internal definitions for KVS WebRTC implementation
 *
 * This file contains internal structures and definitions shared between
 * kvs_webrtc.c and kvs_media.c components. It should not be included
 * by external components.
 */

#include "kvs_peer_connection.h"
#include "media_stream.h"
#include "WebRtcLogging.h"
#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

#ifdef __cplusplus
extern "C" {
#endif

// Check if data channel support is enabled
#if defined(CONFIG_ENABLE_DATA_CHANNEL)
#define KVS_ENABLE_DATA_CHANNEL 1
#else
#define KVS_ENABLE_DATA_CHANNEL 0
#endif

// Constants
#define MAX_DATA_CHANNEL_MESSAGE_SIZE 1024

/**
 * @brief KVS peer connection client data
 * Contains the KVS-specific configuration and state
 */
typedef struct {
    kvs_peer_connection_config_t config;
    bool initialized;

    // KVS SDK objects
    RtcConfiguration rtc_configuration;
    TIMER_QUEUE_HANDLE timer_queue;

    // Certificate pre-generation
    PStackQueue pregeneratedCertificates;   // Max MAX_RTCCONFIGURATION_CERTIFICATES certificates
    UINT32 pregenerateCertTimerId;
    MUTEX certificateLock;

    // ICE candidate pair statistics
    UINT32 iceCandidatePairStatsTimerId;
    RtcStats rtcIceCandidatePairMetrics;
    MUTEX statsLock;

    // Internal session tracking for metrics collection (independent of app_webrtc.c)
    PHashTable activeSessions;

    // Statistics tracking
    bool enable_metrics;

    // Event handler callback
    void (*event_handler)(app_webrtc_event_t event_id, UINT32 status_code, PCHAR peer_id, PCHAR message);
} kvs_pc_client_t;

/**
 * @brief KVS peer connection session data
 * Contains per-session KVS peer connection state
 */
typedef struct kvs_pc_session_s {
    kvs_pc_client_t *client;
    char peer_id[64];
    bool is_initiator;
    bool terminated;
    bool needs_offer;      // Flag to indicate an offer needs to be created after callbacks are set

    // Callbacks from app_webrtc
    uint64_t custom_data;
    WEBRTC_STATUS (*on_message_received)(uint64_t, webrtc_message_t*);
    WEBRTC_STATUS (*on_peer_state_changed)(uint64_t, webrtc_peer_state_t);

    // KVS SDK peer connection objects
    PRtcPeerConnection peer_connection;
    PRtcRtpTransceiver video_transceiver;
    PRtcRtpTransceiver audio_transceiver;

    // Media capture handles for dynamic reconfiguration
    video_capture_handle_t video_handle;
    audio_capture_handle_t audio_handle;

    // Media player handles for reception
    video_player_handle_t video_player_handle;
    audio_player_handle_t audio_player_handle;
    bool media_players_initialized;

    // Sample file fallback buffers and state
    PBYTE video_frame_buffer;
    UINT32 video_buffer_size;
    PBYTE audio_frame_buffer;
    UINT32 audio_buffer_size;
    volatile SIZE_T frame_index;  // For video frame sequencing

#if KVS_ENABLE_DATA_CHANNEL
    // Data channel objects and callbacks
    PRtcDataChannel data_channel;
    RtcOnOpen on_data_channel_open;
    RtcOnMessage on_data_channel_message;
    bool data_channel_ready;
    char data_channel_message[MAX_DATA_CHANNEL_MESSAGE_SIZE];
#endif

    // Session description for answers
    RtcSessionDescriptionInit answer_session_description;

    // ICE state
    bool remote_can_trickle_ice;
    bool candidate_gathering_done;

    // Media streaming state
    bool media_started;
    bool media_threads_started;
    uint64_t audio_timestamp;
    uint64_t video_timestamp;

    // Thread management
    TID receive_audio_video_tid;
    bool receive_thread_started;
    TID media_sender_tid;              // Main media sender thread
    bool media_sender_thread_started;  // Track if media sender thread is running

    // TWCC metadata for bandwidth estimation
    struct {
        MUTEX update_lock;
        DOUBLE average_packet_loss;
        UINT64 current_video_bitrate;
        UINT64 current_audio_bitrate;
        UINT64 last_adjustment_time_ms;
    } twcc_metadata;

    // Metrics
    uint64_t start_time;
    PeerConnectionMetrics pc_metrics;
    KvsIceAgentMetrics ice_metrics;

    // RTC Stats and Metrics History for comprehensive metrics tracking
    RtcStats rtc_stats;
    RtcMetricsHistory rtc_metrics_history;

    // Correlation ID counter for messages
    SIZE_T correlation_id_postfix;
} kvs_pc_session_t;

#ifdef __cplusplus
}
#endif

#endif /* __KVS_WEBRTC_INTERNAL_H__ */
