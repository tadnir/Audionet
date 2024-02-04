#include <miniaudio/miniaudio.h>
#include <stdio.h>

#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "logger.h"
#include "audio.h"
#include "internal/multi_waveform_data_source.h"

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

    ma_data_source* output;
};

static void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    audio_t *audio = pDevice->pUserData;
    if (audio == NULL) {
        LOG_ERROR("pDevice->pUserData is NULL!!!");
        return;
    }

    if (pDevice->capture.channels != 1) {
        LOG_ERROR("Unsupported recording channel count: %d", pDevice->capture.channels);
    } else if (audio->recording_callback != NULL) {
        audio->recording_callback(audio->recording_callback_context, pInput, frameCount);
    }

    if (audio->output != NULL) {
        ma_result result = ma_data_source_read_pcm_frames(audio->output, pOutput, frameCount, NULL);
        if (result != MA_SUCCESS) {
            LOG_ERROR("Failed to read audio output from data source");
            // TODO: Signal failure to send caller
        }
    }
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
    audio->output = NULL;

    /* Configure miniaudio device config */
    ma_device_config deviceConfig  = ma_device_config_init(ma_device_type_duplex);
    deviceConfig.capture.format    = ma_format_f32;
    deviceConfig.capture.channels  = 1;
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate        = framerate;
    deviceConfig.dataCallback      = audio_callback;
    deviceConfig.pUserData         = audio;

    /* Initialize miniaudio device */
    result = ma_device_init(NULL, &deviceConfig, &audio->audio_device);
    if (result != MA_SUCCESS) {
        LOG_ERROR("Failed to initialize audio device, result: %d", result);
        free(audio);
        return NULL;
    }

    LOG_INFO("Initialized device: %s", audio->audio_device.playback.name);

    return audio;
}

void AUDIO__free(audio_t* audio) {
    ma_device_uninit(&audio->audio_device);
    multi_waveform_data_source_uninit(audio->output);
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
    audio->recording_callback_context = callback_context;
    audio->recording_callback = callback;
}

int AUDIO__set_playing_frequencies(audio_t* audio, struct frequency_output* frequencies, int frequencies_length) {
    ma_result result = multi_waveform_data_source_init(
            (struct multi_waveform_data_source **) &audio->output,
            audio->audio_device.playback.format, audio->audio_device.playback.channels, audio->audio_device.sampleRate,
            frequencies, frequencies_length);
    if (result != MA_SUCCESS) {
        LOG_ERROR("asdasdfasdf");
        return -1;
    }

    return 0;
}

