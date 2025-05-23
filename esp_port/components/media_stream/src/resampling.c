// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
// All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "esp_log.h"
#include "resampling.h"

static const char *TAG = "[resampling]";

/**
 * Signed saturate a 32 bit value to 16 bits keeping output in 32 bit variable.
 */
static inline int32_t esp_saturate16(int32_t in)
{
#if defined ESP32_ASM
    asm volatile("clamps %0, %0, 15" : "+a"(in));
#else
    if (in > INT16_MAX) {
        return INT16_MAX;
    }
    if (in < INT16_MIN) {
        return INT16_MIN;
    }
#endif
    return in;
}

static int resampling_mono(short *inpcm, short *outpcm, int srcrate, int tarrate, int innum, audio_resample_config_t *resample_cfg)
{
    int outnum = 0;
    float nk;
    int n;
    int n_1;
    int value;
    float w[2];
    float ratio = (float)srcrate / (float)tarrate;
    int pcmval[2];
    while (1) {
        nk = ratio * (resample_cfg->outnum + outnum);
        n = (int)nk;
        w[0] = nk - n;
        w[1] = 1 - w[0];

        n = n - resample_cfg->innum;
        n_1 = n + 1;

        if (n_1 >= innum) {
            memcpy(resample_cfg->inpcm, &inpcm[innum - INPCM_DELAY_SIZE], sizeof(short)*INPCM_DELAY_SIZE);
            break;
        }
        if (n < 0) ///need use the saved pcm
            pcmval[0] = resample_cfg->inpcm[INPCM_DELAY_SIZE + n];
        else
            pcmval[0] = inpcm[n];
        if (n_1 < 0)
            pcmval[1] = resample_cfg->inpcm[INPCM_DELAY_SIZE + n_1];
        else
            pcmval[1] = inpcm[n_1];
        value = w[0] * pcmval[1] + w[1] * pcmval[0];
        value = esp_saturate16(value);
        outpcm[outnum] = value;

        //fprintf(debugfile, "%d => %f : %d : %d : %d\n", outcnt++, nk, pcmval[0], pcmval[1], outpcm[outnum]);
        outnum++;
    }

    return outnum;
}

static int resampling_dual(short *inpcm, short *outpcm, int srcrate, int tarrate, int innum, audio_resample_config_t *resample_cfg)
{
    int outnum = 0;

    float nk;
    int n;
    int n_1;
    float w[2];
    float ratio = (float)srcrate / (float)tarrate;
    int pcmval[2];
    int value;
    while (1) {
        nk = ratio * (resample_cfg->outnum + outnum);
        n = (int)nk;
        w[0] = nk - n;
        w[1] = 1 - w[0];

        n = n - resample_cfg->innum;
        n_1 = n + 1;

        if (n_1 >= innum) {
            memcpy(resample_cfg->inpcm, &inpcm[innum * 2 - INPCM_DELAY_SIZE * 2], sizeof(short)*INPCM_DELAY_SIZE * 2);
            break;
        }

        ////first channel
        if (n < 0) ///need use the saved pcm
            pcmval[0] = resample_cfg->inpcm[INPCM_DELAY_SIZE * 2 + 2 * n];
        else
            pcmval[0] = inpcm[2 * n];
        if ( n_1 < 0)
            pcmval[1] = resample_cfg->inpcm[INPCM_DELAY_SIZE * 2 + 2 * n_1];
        else
            pcmval[1] = inpcm[2 * n_1];
        value = w[0] * pcmval[1] + w[1] * pcmval[0];
        value = esp_saturate16(value);
        outpcm[outnum * 2] = value;
        //printf("%s: %d : %d : %d\n", TAG, pcmval[0], pcmval[1], outnum);
        //fprintf(debugfile, "%d => %d : %f : %d : %d : %d\n", outcnt++, resample_cfgresample_cfgresample_cfg->outnum, nk, pcmval[0], pcmval[1], outpcm[outnum * 2]);
        //fprintf(debugfile, "%d => %f : %d : %d : %d\n", outcnt++, nk, pcmval[0], pcmval[1], value);
        //fprintf(debugfile, "%d => %d\n", outcnt++, value);

        ////second channel
        if (n < 0) ///need use the saved pcm
            pcmval[0] = resample_cfg->inpcm[INPCM_DELAY_SIZE * 2 + 2 * n + 1];
        else
            pcmval[0] = inpcm[2 * n + 1];
        if (n_1 < 0)
            pcmval[1] = resample_cfg->inpcm[INPCM_DELAY_SIZE * 2 + 2 * n_1 + 1];
        else
            pcmval[1] = inpcm[2 * n_1 + 1];
        value = w[0] * pcmval[1] + w[1] * pcmval[0];
        value = esp_saturate16(value);
        outpcm[outnum * 2 + 1] = value;
        // fprintf(debugfile, "%d => %d : %f : %d : %d : %d\n", outcnt++, resample_cfg->outnum, nk, pcmval[0], pcmval[1], outpcm[outnum * 2 + 1]);
        outnum++;
    }

    return outnum;
}

static int resampling_dual_down_ch(short *inpcm, int channelidx, short *outpcm, int srcrate, int tarrate, int innum, audio_resample_config_t *resample_cfg)
{
    int outnum = 0;
    int i;
    float nk;
    int n;
    int n_1;
    float w[2];
    float ratio = (float)srcrate / (float)tarrate;
    int pcmval[2];
    int value;
    while (1) {
        nk = ratio * (resample_cfg->outnum + outnum);
        n = (int)nk;
        w[0] = nk - n;
        w[1] = 1 - w[0];

        n = n - resample_cfg->innum;
        n_1 = n + 1;

        if (n_1 >= innum) {
            for (i = 0; i < INPCM_DELAY_SIZE; i++)
                resample_cfg->inpcm[i] = inpcm[(innum - INPCM_DELAY_SIZE) * 2 + channelidx + 2 * i];
            break;
        }
        if (n < 0) ///need use the saved pcm
            pcmval[0] = resample_cfg->inpcm[INPCM_DELAY_SIZE + n];
        else
            pcmval[0] = inpcm[n * 2 + channelidx];
        if (n_1 < 0)
            pcmval[1] = resample_cfg->inpcm[INPCM_DELAY_SIZE + n_1];
        else
            pcmval[1] = inpcm[n_1 * 2 + channelidx];
        value = w[0] * pcmval[1] + w[1] * pcmval[0];
        value = esp_saturate16(value);
        outpcm[outnum] = value;

        // fprintf(debugfile, "%d => %f : %d : %d : %d\n", outcnt++, nk, pcmval[0], pcmval[1], outpcm[outnum]);
        outnum++;
    }

    return outnum;
}

#define VERY_SMALL (1e-30f)

static void dc_reject_mono(short *inpcm, int cutoff_Hz, short *outpcm, float *hp_mem, int len, int Fs)
{
    int i, value;
    float coef;

    coef = 4.0f * cutoff_Hz / Fs;
    for (i = 0; i < len; i++) {
        float x, tmp, y;
        x = inpcm[i];
        /* First stage */
        tmp = x - hp_mem[0];
        hp_mem[0] = hp_mem[0] + coef * (x - hp_mem[0]) + VERY_SMALL;
        /* Second stage */
        y = tmp - hp_mem[1];
        hp_mem[1] = hp_mem[1] + coef * (tmp - hp_mem[1]) + VERY_SMALL;
        value = y;
        value = esp_saturate16(value);
        outpcm[i] = value;
    }
}

static void dc_reject_dual(short *inpcm, int cutoff_Hz, short *outpcm, float *hp_mem, int len, int Fs)
{
    int i, value;
    float coef;

    coef = 4.0f * cutoff_Hz / Fs;

    for (i = 0; i < len; i += 2) { ///first channel
        float x, tmp, y;
        x = inpcm[i];
        /* First stage */
        tmp = x - hp_mem[0];
        hp_mem[0] = hp_mem[0] + coef * (x - hp_mem[0]) + VERY_SMALL;
        /* Second stage */
        y = tmp - hp_mem[1];
        hp_mem[1] = hp_mem[1] + coef * (tmp - hp_mem[1]) + VERY_SMALL;
        value = y;
        value = esp_saturate16(value);
        outpcm[i] = value;
        //fprintf(debugfile, "%d : %f : %f => %d\n", outcnt++, hp_mem[0], hp_mem[1], value);
    }

    for (i = 1; i < len; i += 2) { ///second chanel
        float x, tmp, y;
        x = inpcm[i];
        /* First stage */
        tmp = x - hp_mem[2];
        hp_mem[2] = hp_mem[2] + coef * (x - hp_mem[2]) + VERY_SMALL;
        /* Second stage */
        y = tmp - hp_mem[3];
        hp_mem[3] = hp_mem[3] + coef * (tmp - hp_mem[3]) + VERY_SMALL;
        value = y;
        value = esp_saturate16(value);
        outpcm[i] = value;
    }
}

static int unit_resample_mono(short *inpcm, short *outpcm, int srcrate, int tarrate, int innum, audio_resample_config_t *resample_cfg)
{
    int outnum;

    outnum = resampling_mono(inpcm, outpcm, srcrate, tarrate, innum, resample_cfg);
    resample_cfg->innum += innum;
    resample_cfg->outnum += outnum;

    if ((resample_cfg->innum >= srcrate) && (resample_cfg->outnum >= tarrate)) {
        resample_cfg->innum -= srcrate;
        resample_cfg->outnum -= tarrate;
    }

    dc_reject_mono(outpcm, 3, outpcm, resample_cfg->hp_mem, outnum, tarrate);

    return outnum;
}

static int unit_resample_dual(short *inpcm, short *outpcm, int srcrate, int tarrate, int innum, audio_resample_config_t *resample_cfg)
{
    int outnum;
    //printf("%s: original sample = %d\n", TAG,innum);
    innum /= 2;

    outnum = resampling_dual(inpcm, outpcm, srcrate, tarrate, innum, resample_cfg);
    resample_cfg->innum += innum;
    resample_cfg->outnum += outnum;

    //fprintf(debugfile,"outnum = %d : %d : %d : %d\n", outnum, innum, resample_cfg->innum, resample_cfg->outnum);

    if ((resample_cfg->innum >= srcrate) && (resample_cfg->outnum >= tarrate)) {
        resample_cfg->innum -= srcrate;
        resample_cfg->outnum -= tarrate;
    }

    outnum *= 2;
    dc_reject_dual(outpcm, 3, outpcm, resample_cfg->hp_mem, outnum, tarrate);

    return outnum;
}

static int unit_resample_dual_down_ch(short *inpcm, int channelidx, short *outpcm, int srcrate, int tarrate, int innum, audio_resample_config_t *resample_cfg)
{
    int outnum;
    innum /= 2;

    outnum = resampling_dual_down_ch(inpcm, channelidx, outpcm, srcrate, tarrate, innum, resample_cfg);
    resample_cfg->innum += innum;
    resample_cfg->outnum += outnum;

    if ((resample_cfg->innum >= srcrate) && (resample_cfg->outnum >= tarrate)) {
        resample_cfg->innum -= srcrate;
        resample_cfg->outnum -= tarrate;
    }

    dc_reject_mono(outpcm, 3, outpcm, resample_cfg->hp_mem, outnum, tarrate);

    return outnum;
}


///only support 1 or 2 channel
int audio_resample(short *in_buf, short *out_buf, int in_freq, int out_freq, int in_buf_size, int out_buf_size, int ch_num, audio_resample_config_t *resample_cfg)
{
    int outnum;

    if ((in_buf_size % ch_num) != 0) {
        ESP_LOGE(TAG, "input pcm number error. For two channel case, left & right channel should has same number");
        return 0;
    }
    if (out_buf_size < ((out_freq * in_buf_size) /in_freq)) {
        ESP_LOGE(TAG, "output buffer size is less, it should be atleast %d", ((out_freq * in_buf_size) /in_freq));
        return 0;
    }
    if (in_freq == out_freq) {
        memcpy(out_buf, in_buf, sizeof(short)*in_buf_size);
        outnum = in_buf_size;
    } else { ////attention, perhaps the output number has several point more or less, so need to check and adjust

        /// We need to reset resample_cfg if out_freq/in_freq ratio changes.
        int resample_ratio = 512 * out_freq / in_freq; /// 512 is just a random factor to keep it simple
        if (__builtin_expect((resample_ratio != resample_cfg->resample_ratio), 0)) {
            bzero(resample_cfg, sizeof(audio_resample_config_t));
            resample_cfg->resample_ratio = resample_ratio;
        }

        if (ch_num == 1)
            outnum = unit_resample_mono(in_buf, out_buf, in_freq, out_freq, in_buf_size, resample_cfg);
        else
            outnum = unit_resample_dual(in_buf, out_buf, in_freq, out_freq, in_buf_size, resample_cfg);
    }

    return outnum;
}

int audio_resample_up_channel(short *in_buf, short *out_buf, int in_freq, int out_freq, int in_buf_size, int out_buf_size, audio_resample_config_t *resample_cfg)
{
    int outnum;
    int  i;

    if (out_buf_size < ((out_freq * in_buf_size) /in_freq)) {
        ESP_LOGE(TAG, "output buffer size is less, it should be atleast %d", ((out_freq * in_buf_size) /in_freq));
        return 0;
    }
    if (in_freq == out_freq) {
        memcpy(out_buf, in_buf, sizeof(short)*in_buf_size);
        outnum = in_buf_size;
    } else { ////attention, perhaps the output number has several point more or less, so need to check and adjust

        /// We need to reset resample_cfg if out_freq/in_freq ratio changes.
        int resample_ratio = 512 * out_freq / in_freq; /// 512 is just a random factor to keep it simple
        if (__builtin_expect((resample_ratio != resample_cfg->resample_ratio), 0)) {
            bzero(resample_cfg, sizeof(audio_resample_config_t));
            resample_cfg->resample_ratio = resample_ratio;
        }

        outnum = unit_resample_mono(in_buf, out_buf, in_freq, out_freq, in_buf_size, resample_cfg);
    }
    ///copy one channel data to another
    for (i = outnum - 1; i >= 0; i--) {
        out_buf[i * 2 + 1] = out_buf[i];
        out_buf[i * 2] = out_buf[i];
    }
    outnum *= 2;

    return outnum;
}

int audio_resample_down_channel(short *in_buf, short *out_buf, int in_freq, int out_freq, int in_buf_size, int out_buf_size, int index, audio_resample_config_t *resample_cfg)
{
    int outnum;
    int  i;

    if ((in_buf_size & 1) != 0) {
        ESP_LOGE(TAG, "input pcm number error. For two channel case, left & right channel should has same number");
        return 0;
    }
    if (out_buf_size < ((out_freq * in_buf_size) /in_freq)) {
        ESP_LOGE(TAG, "output buffer size is less, it should be atleast %d", ((out_freq * in_buf_size) /in_freq));
        return 0;
    }
    if (in_freq == out_freq) {
        outnum = 0;
        for (i = index; i < in_buf_size; i += 2) {
            out_buf[outnum++] = in_buf[i];
        }
    } else { ////attention, perhaps the output number has several point more or less, so need to check and adjust

        /// We need to reset resample_cfg if out_freq/in_freq ratio changes.
        int resample_ratio = 512 * out_freq / in_freq; /// 512 is just a random factor to keep it simple
        if (__builtin_expect((resample_ratio != resample_cfg->resample_ratio), 0)) {
            bzero(resample_cfg, sizeof(audio_resample_config_t));
            resample_cfg->resample_ratio = resample_ratio;
        }

        outnum = unit_resample_dual_down_ch(in_buf, index, out_buf, in_freq, out_freq, in_buf_size, resample_cfg);
    }

    return outnum;
}
