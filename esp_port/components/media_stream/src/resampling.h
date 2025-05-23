/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#ifndef _RESAMPLING_H_
#define _RESAMPLING_H_

#ifdef __cplusplus
extern "C" {
#endif

#define INPCM_DELAY_SIZE (6)

/**
 * @brief
 * Config structure
 */
typedef struct {
    short inpcm[INPCM_DELAY_SIZE * 2];  ///the pcm value of last time calling.  maximum should be 6: 48000/8000;
    int innum;  /// the total input pcm number
    int outnum; /// the total outnum pcm number
    float hp_mem[4]; /// for filter, the first two is for first channel, the last two is for second channel
    int resample_ratio; /// (out_freq * 512)/in_freq. if prev resample ratio is not same as current, reset whole structure.
} audio_resample_config_t;

/**
 * @brief Up/dowm sample audio signal, supports only mono and stereo
 *
 * @param in_buf input buffer of audio data, this should not be NULL
 * @param out_buf output buffer of resampled audio data, this should also not be null
 * @param in_freq source audio sampling frequency
 * @param out_freq target audio sampling frequency
 * @param in_buf_size input buffer size in short(Number of 16-bit samples)
 * @param out_buf_size output buf size in short
 * @param ch_num number of channels of audio source
 * @param resample config structure
 *
 * @return
 *     - (0) error
 *     - (Other, > 0) output buffer bytes
 */
int audio_resample(short *in_buf, short *out_buf, int in_freq, int out_freq, int in_buf_size, int out_buf_size, int ch_num, audio_resample_config_t *resample_cfg);

/**
 * @brief Convert mono audio buffer to stereo
 *
 * @param in_buf input buffer of audio data, this should not be NULL
 * @param out_buf output buffer of resampled audio data, this should also not be null
 * @param in_freq source audio sampling frequency
 * @param out_freq target audio sampling frequency
 * @param in_buf_size input buffer size in short(Number of 16-bit samples)
 * @param out_buf_size output buf size in short
 * @param resample config structure
 *
 * @return
 *     - (0) error
 *     - (Other, > 0) output buffer bytes
 */
int audio_resample_up_channel(short *in_buf, short *out_buf, int in_freq, int out_freq, int in_buf_size, int out_buf_size, audio_resample_config_t *resample_cfg);

/**
 * @brief Convert stereo audio buffer to mono
 *
 * @param in_buf input buffer of audio data, this should not be NULL
 * @param out_buf output buffer of resampled audio data, this should also not be null
 * @param in_freq source audio sampling frequency
 * @param out_freq target audio sampling frequency
 * @param in_buf_size input buffer size in short(Number of 16-bit samples)
 * @param out_buf_size output buf size in short
 * @param index buffer start address
 * @param resample config structure
 *
 * @return
 *     - (0) error
 *     - (Other, > 0) output buffer bytes
 */
int audio_resample_down_channel(short *in_buf, short *out_buf, int in_freq, int out_freq, int in_buf_size, int out_buf_size, int index, audio_resample_config_t *resample_cfg);

#ifdef __cplusplus
}
#endif

#endif/*_RESAMPLING_H_*/
