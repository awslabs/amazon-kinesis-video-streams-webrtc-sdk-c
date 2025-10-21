#include "MjpegH264Extractor.h"

STATUS initH264Extractor(PH264ExtractorContext pContext)
{
    STATUS retStatus = STATUS_SUCCESS;
    
    CHK_ERR(pContext != NULL, STATUS_NULL_ARG, "H264 extractor context is NULL");
    
    // コンテキストを初期化
    MEMSET(pContext, 0, SIZEOF(H264ExtractorContext));
    
    // SPS/PPSバッファを割り当て
    pContext->spsData = (PBYTE) MEMALLOC(MAX_SPS_PPS_SIZE);
    CHK_ERR(pContext->spsData != NULL, STATUS_NOT_ENOUGH_MEMORY, "Failed to allocate SPS buffer");
    
    pContext->ppsData = (PBYTE) MEMALLOC(MAX_SPS_PPS_SIZE);
    CHK_ERR(pContext->ppsData != NULL, STATUS_NOT_ENOUGH_MEMORY, "Failed to allocate PPS buffer");
    
    // H.264出力バッファを割り当て
    pContext->h264Buffer = (PBYTE) MEMALLOC(MAX_H264_BUFFER_SIZE);
    CHK_ERR(pContext->h264Buffer != NULL, STATUS_NOT_ENOUGH_MEMORY, "Failed to allocate H264 buffer");
    
    pContext->h264BufferSize = MAX_H264_BUFFER_SIZE;
    pContext->hasSps = FALSE;
    pContext->hasPps = FALSE;
    
CleanUp:
    if (STATUS_FAILED(retStatus)) {
        freeH264Extractor(pContext);
    }
    
    return retStatus;
}

STATUS freeH264Extractor(PH264ExtractorContext pContext)
{
    if (pContext != NULL) {
        if (pContext->spsData != NULL) {
            MEMFREE(pContext->spsData);
            pContext->spsData = NULL;
        }
        if (pContext->ppsData != NULL) {
            MEMFREE(pContext->ppsData);
            pContext->ppsData = NULL;
        }
        if (pContext->h264Buffer != NULL) {
            MEMFREE(pContext->h264Buffer);
            pContext->h264Buffer = NULL;
        }
        pContext->h264BufferSize = 0;
        pContext->hasSps = FALSE;
        pContext->hasPps = FALSE;
    }
    
    return STATUS_SUCCESS;
}

BOOL isStartCode3(PBYTE pData, UINT32 size)
{
    return (size >= 3 && pData[0] == 0x00 && pData[1] == 0x00 && pData[2] == 0x01);
}

BOOL isStartCode4(PBYTE pData, UINT32 size)
{
    return (size >= 4 && pData[0] == 0x00 && pData[1] == 0x00 && pData[2] == 0x00 && pData[3] == 0x01);
}

UINT8 getNalUnitType(PBYTE pNalData)
{
    return pNalData[0] & 0x1F;
}

STATUS extractH264FromMjpeg(PH264ExtractorContext pContext, 
                           PBYTE pMjpegData, UINT32 mjpegSize,
                           PBYTE* ppH264Data, PUINT32 pH264Size)
{
    STATUS retStatus = STATUS_SUCCESS;
    PBYTE pCurrent = pMjpegData;
    PBYTE pEnd = pMjpegData + mjpegSize;
    PBYTE pAppData = NULL;
    UINT32 appDataSize = 0;
    UINT32 totalAppSize = 0;
    UINT32 h264OutputSize = 0;
    
    CHK_ERR(pContext != NULL, STATUS_NULL_ARG, "H264 extractor context is NULL");
    CHK_ERR(pMjpegData != NULL, STATUS_NULL_ARG, "MJPEG data is NULL");
    CHK_ERR(ppH264Data != NULL, STATUS_NULL_ARG, "H264 data pointer is NULL");
    CHK_ERR(pH264Size != NULL, STATUS_NULL_ARG, "H264 size pointer is NULL");
    
    // JPEG SOI (Start of Image) マーカーをチェック
    CHK_ERR(mjpegSize >= 4, STATUS_INVALID_ARG, "MJPEG data too small");
    CHK_ERR(pCurrent[0] == 0xFF && pCurrent[1] == 0xD8, STATUS_INVALID_ARG, "Invalid JPEG SOI marker");
    
    pCurrent += 2; // SOIをスキップ
    
    // 一時的なAPPデータ収集用バッファ
    PBYTE pTempAppBuffer = (PBYTE) MEMALLOC(mjpegSize);
    CHK_ERR(pTempAppBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY, "Failed to allocate temp APP buffer");
    
    // JPEGセグメントを解析してAPPnセグメントを収集
    while (pCurrent + 4 <= pEnd) {
        // 0xFFマーカーを探す
        if (*pCurrent != 0xFF) {
            pCurrent++;
            continue;
        }
        
        // 連続する0xFFをスキップ
        while (pCurrent < pEnd && *pCurrent == 0xFF) {
            pCurrent++;
        }
        
        if (pCurrent >= pEnd) break;
        
        UINT8 marker = *pCurrent++;
        
        // スタンドアロンマーカー（長さフィールドなし）
        if (marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) {
            continue;
        }
        
        // 長さフィールドをチェック
        if (pCurrent + 2 > pEnd) break;
        
        UINT16 segmentLength = (pCurrent[0] << 8) | pCurrent[1];
        pCurrent += 2;
        
        if (segmentLength < 2 || pCurrent + (segmentLength - 2) > pEnd) break;
        
        // SOS (Start of Scan) に到達したら終了
        if (marker == 0xDA) break;
        
        // APPnセグメント (0xE0-0xEF) の場合、データを収集
        if (marker >= 0xE0 && marker <= 0xEF) {
            UINT32 payloadSize = segmentLength - 2;
            if (totalAppSize + payloadSize <= mjpegSize) {
                MEMCPY(pTempAppBuffer + totalAppSize, pCurrent, payloadSize);
                totalAppSize += payloadSize;
            }
        }
        
        pCurrent += (segmentLength - 2);
    }
    
    if (totalAppSize == 0) {
        DLOGI("[H264 Extractor] No APP segments found in MJPEG");
        MEMFREE(pTempAppBuffer);
        *ppH264Data = NULL;
        *pH264Size = 0;
        return STATUS_SUCCESS;
    }
    
    DLOGI("[H264 Extractor] Found %d bytes of APP data", totalAppSize);
    
    // 収集したAPPデータからH.264 NALユニットを抽出
    pCurrent = pTempAppBuffer;
    pEnd = pTempAppBuffer + totalAppSize;
    
    // SPS/PPSを先頭に追加（キャッシュされている場合）
    if (pContext->hasSps && pContext->hasPps) {
        // 4バイトスタートコード + SPS
        static const UINT8 startCode4[] = {0x00, 0x00, 0x00, 0x01};
        MEMCPY(pContext->h264Buffer + h264OutputSize, startCode4, 4);
        h264OutputSize += 4;
        MEMCPY(pContext->h264Buffer + h264OutputSize, pContext->spsData, pContext->spsSize);
        h264OutputSize += pContext->spsSize;
        
        // 4バイトスタートコード + PPS
        MEMCPY(pContext->h264Buffer + h264OutputSize, startCode4, 4);
        h264OutputSize += 4;
        MEMCPY(pContext->h264Buffer + h264OutputSize, pContext->ppsData, pContext->ppsSize);
        h264OutputSize += pContext->ppsSize;
    }
    
    // APPデータ内のスタートコードを検索してNALユニットを抽出
    while (pCurrent < pEnd) {
        // スタートコードを探す
        PBYTE pStartCode = NULL;
        BOOL isStartCode4Found = FALSE;
        
        for (PBYTE p = pCurrent; p < pEnd; p++) {
            if (isStartCode4(p, pEnd - p)) {
                pStartCode = p;
                isStartCode4Found = TRUE;
                break;
            } else if (isStartCode3(p, pEnd - p)) {
                pStartCode = p;
                isStartCode4Found = FALSE;
                break;
            }
        }
        
        if (pStartCode == NULL) break;
        
        // NALユニットの開始位置
        PBYTE pNalStart = pStartCode + (isStartCode4Found ? 4 : 3);
        
        // 次のスタートコードを探してNALユニットの終了位置を決定
        PBYTE pNextStartCode = NULL;
        for (PBYTE p = pNalStart; p < pEnd; p++) {
            if (isStartCode4(p, pEnd - p) || isStartCode3(p, pEnd - p)) {
                pNextStartCode = p;
                break;
            }
        }
        
        PBYTE pNalEnd = (pNextStartCode != NULL) ? pNextStartCode : pEnd;
        UINT32 nalSize = pNalEnd - pNalStart;
        
        if (nalSize > 0 && pNalStart < pEnd) {
            // NALユニットタイプを取得
            UINT8 nalType = getNalUnitType(pNalStart);
            
            // SPS (7) またはPPS (8) をキャッシュ
            if (nalType == 7 && nalSize <= MAX_SPS_PPS_SIZE) { // SPS
                MEMCPY(pContext->spsData, pNalStart, nalSize);
                pContext->spsSize = nalSize;
                pContext->hasSps = TRUE;
                DLOGI("[H264 Extractor] Cached SPS (%d bytes)", nalSize);
            } else if (nalType == 8 && nalSize <= MAX_SPS_PPS_SIZE) { // PPS
                MEMCPY(pContext->ppsData, pNalStart, nalSize);
                pContext->ppsSize = nalSize;
                pContext->hasPps = TRUE;
                DLOGI("[H264 Extractor] Cached PPS (%d bytes)", nalSize);
            }
            
            // 4バイトスタートコードで正規化して出力バッファに追加
            if (h264OutputSize + 4 + nalSize <= pContext->h264BufferSize) {
                static const UINT8 startCode4[] = {0x00, 0x00, 0x00, 0x01};
                MEMCPY(pContext->h264Buffer + h264OutputSize, startCode4, 4);
                h264OutputSize += 4;
                MEMCPY(pContext->h264Buffer + h264OutputSize, pNalStart, nalSize);
                h264OutputSize += nalSize;
            } else {
                DLOGE("[H264 Extractor] Output buffer overflow");
                retStatus = STATUS_BUFFER_TOO_SMALL;
                goto CleanUp;
            }
        }
        
        pCurrent = pNalEnd;
    }
    
    if (h264OutputSize > 0) {
        *ppH264Data = pContext->h264Buffer;
        *pH264Size = h264OutputSize;
        DLOGI("[H264 Extractor] Extracted %d bytes of H.264 data", h264OutputSize);
    } else {
        *ppH264Data = NULL;
        *pH264Size = 0;
        DLOGI("[H264 Extractor] No H.264 data extracted");
    }

CleanUp:
    if (pTempAppBuffer != NULL) {
        MEMFREE(pTempAppBuffer);
    }
    
    return retStatus;
}