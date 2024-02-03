#include <miniaudio/miniaudio.h>
#include <stdio.h>

#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "logger.h"
#include "audio.h"

/**
 * The actual encapsulated definition of the audio_t interface.
 */
struct audio_s {
    /**
     * The miniaudio library audio device, used for both recording/playing.
     */
    ma_device audio_device;

    recording_callback_t recording_callback;

    void* recording_callback_context;

    ma_waveform output_waveforms[MAX_OUTPUT_WAVEFORMS];

    int output_waveforms_count;

};

static void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    audio_t* audio = pDevice->pUserData;
    if (audio == NULL) {
        LOG_ERROR("pDevice->pUserData is NULL!!!");
        return;
    }

    if (pDevice->capture.channels != 1) {
        LOG_ERROR("Unsupported recording channel count: %d", pDevice->capture.channels);
    } else if (audio->recording_callback != NULL) {
        audio->recording_callback(audio->recording_callback_context, pInput, frameCount);
    }

//    LOG_DEBUG("Output channels %d", pDevice->playback.channels);

    float* pfOutput = pOutput;
    float* temp = malloc(frameCount * sizeof(float) * pDevice->playback.channels);
    if (temp == NULL) {
        LOG_ERROR("Failed to allocate temporary output calculation buffer");
        return;
    }

    memset(pOutput, 0, frameCount * sizeof(float) * pDevice->playback.channels);
    for (int i = 0; i < audio->output_waveforms_count; ++i) {
        ma_waveform_read_pcm_frames(&audio->output_waveforms[i], temp, frameCount, NULL);
        for (int j = 0; j < frameCount * pDevice->playback.channels; j++) {
            pfOutput[j] += temp[j] / audio->output_waveforms_count;
        }
    }
//    ma_data_source_

    free(temp);
}

audio_t* AUDIO__initialize(enum standard_sample_rate framerate) {
    ma_result result;
    audio_t* audio = (audio_t*) malloc(sizeof(audio_t));
    if (audio == NULL) {
        LOG_ERROR("Failed to allocate audio struct");
        return NULL;
    }

    /* Initialize state */
    audio->recording_callback = NULL;
    audio->recording_callback_context = NULL;
    audio->output_waveforms_count = 0;

    /* Configure miniaudio device config */
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_duplex);
    deviceConfig.capture.format   = ma_format_f32;
    deviceConfig.capture.channels = 1;
    deviceConfig.playback.format  = ma_format_f32;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate       = framerate;
    deviceConfig.dataCallback     = audio_callback;
    deviceConfig.pUserData        = audio;

    /* Initialize miniaudio device */
    result = ma_device_init(NULL, &deviceConfig, &audio->audio_device);
    if (result != MA_SUCCESS) {
        LOG_ERROR("Failed to initialize audio device, result: %d", result);
        free(audio);
        return NULL;
    }

    /* Initialize output waveforms */
    ma_waveform_config sineWaveDefaultConfig = ma_waveform_config_init(audio->audio_device.playback.format, audio->audio_device.playback.channels, audio->audio_device.sampleRate, ma_waveform_type_sine, 0, 100);
    for (int i = 0; i < MAX_OUTPUT_WAVEFORMS; ++i) {
        result = ma_waveform_init(&sineWaveDefaultConfig, &audio->output_waveforms[i]);
        if (result != MA_SUCCESS) {
            LOG_ERROR("Failed to initialize waveform %d", i);
            for (int j = 0; j < i; ++j) {
                ma_waveform_uninit(&audio->output_waveforms[j]);
            }
            ma_device_uninit(&audio->audio_device);
            free(audio);
            return NULL;
        }
    }

    LOG_INFO("Initialized device: %s", audio->audio_device.playback.name);

    return audio;
}

void AUDIO__free(audio_t* audio) {
    ma_device_uninit(&audio->audio_device);
    free(audio);
}

int AUDIO__start(audio_t* audio) {
    ma_result result = ma_device_start(&audio->audio_device);
    if (result != MA_SUCCESS) {
        LOG_ERROR("Failed to start audio device");
        return -1;
    }

    return 0;
}

int AUDIO__stop(audio_t* audio) {
    ma_result result = ma_device_stop(&audio->audio_device);
    if (result != MA_SUCCESS) {
        LOG_ERROR("Failed to stop audio device");
        return -1;
    }

    return 0;
}

void AUDIO__set_recording_callback(audio_t* audio, recording_callback_t callback, void* callback_context) {
    audio->recording_callback = callback;
    audio->recording_callback_context = callback_context;
}

int AUDIO__set_playing_frequencies(audio_t* audio, struct frequency_output* frequencies, int frequencies_length) {
    if (frequencies_length > MAX_OUTPUT_WAVEFORMS) {
        LOG_ERROR("Given more frequencies than capable to output");
        return -1;
    }

    audio->output_waveforms_count = frequencies_length;

    for (int i = 0; i < frequencies_length; i++) {
        ma_waveform_set_frequency(&audio->output_waveforms[i], frequencies[i].frequency);
        ma_waveform_set_amplitude(&audio->output_waveforms[i], frequencies[i].amplitude);
        ma_data_source_set_range_in_pcm_frames(&audio->output_waveforms[i], 0, 5);
        ma_data_source_set_looping(&audio->output_waveforms[i], false);
    }

//    ma_data_source_read_pcm_frames()

    return 0;
}

