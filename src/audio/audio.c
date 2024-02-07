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

    ma_data_source* sounds_playback;

    ma_event sounds_playback_finished_event;

    ma_result sounds_playback_result;

    bool full_duplex;
};

static void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    audio_t *audio = pDevice->pUserData;
    if (audio == NULL) {
        LOG_ERROR("pDevice->pUserData is NULL");
        return;
    }

    if (audio->sounds_playback != NULL) {
        /* Set the result as interrupt, this being the default status in case the Audio is uninitialized mid playing */
        audio->sounds_playback_result = MA_INTERRUPT;

        /* Output the sound to the speakers */
        ma_result result = ma_data_source_read_pcm_frames(audio->sounds_playback, pOutput, frameCount, NULL);
        if (result != MA_SUCCESS) {
            if (result == MA_AT_END) {
                /* Signal that we've exhausted the data source. */
                audio->sounds_playback_result = MA_SUCCESS;
            } else {
                /* Signal we encountered an error on playback. */
                LOG_ERROR("Failed to read audio sounds_playback from data source %d", result);
                audio->sounds_playback_result = result;
            }

            /* Remove the current sounds playing, freeing it is the play function responsibility */
            audio->sounds_playback = NULL;

            /* Signal the play function we finished playing */
            result = ma_event_signal(&audio->sounds_playback_finished_event);
            if (result != MA_SUCCESS) {
                LOG_FATAL("Failed to signal playback finished");
            }
        }

        /* Return because we're not allowing to record ourself in half duplex mode. */
        if (!audio->full_duplex) {
            return;
        }
    }

    if (pDevice->capture.channels != 1) {
        LOG_ERROR("Unsupported recording channel count: %d", pDevice->capture.channels);
    } else if (audio->recording_callback != NULL) {
        /* Pass the recorded frames to the recording callback */
        audio->recording_callback(audio->recording_callback_context, pInput, frameCount);
    }
}

audio_t* AUDIO__initialize(enum standard_sample_rate framerate, bool full_duplex) {
    ma_result result;
    audio_t* audio = (audio_t*) malloc(sizeof(audio_t));
    if (audio == NULL) {
        LOG_ERROR("Failed to allocate audio struct");
        return NULL;
    }

    /* Initialize state */
    audio->recording_callback = NULL;
    audio->recording_callback_context = NULL;
    audio->sounds_playback = NULL;
    audio->full_duplex = full_duplex;

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

    /* Initialize the playback finished event */
    result = ma_event_init(&audio->sounds_playback_finished_event);
    if (result != MA_SUCCESS) {
        LOG_ERROR("Failed to initialize playback finished event");
        ma_device_uninit(&audio->audio_device);
        free(audio);
        return NULL;
    }

    LOG_INFO("Initialized device: %s", audio->audio_device.playback.name);

    return audio;
}

void AUDIO__free(audio_t* audio) {
    /* Make sure the thread is stopped */
    AUDIO__stop(audio);

    /* We signal the event in case some thread is still waiting on it */
    ma_event_signal(&audio->sounds_playback_finished_event);

    /* Uninitialized the resources,
     * note that there's no need to uninitialize the sounds since it's the play responsibility */
    ma_device_uninit(&audio->audio_device);
    ma_event_uninit(&audio->sounds_playback_finished_event);
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

int AUDIO__play_sounds(audio_t* audio, struct sound_s* sounds, uint32_t sounds_count) {
    ma_result result;
    if (sounds_count == 0 || sounds == NULL || audio == NULL) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }

    if (audio->sounds_playback != NULL) {
        LOG_ERROR("Another sound is currently playing");
        return -1;
    }

    ma_data_source* first;
    result = multi_waveform_data_source_init(
            (struct multi_waveform_data_source **) &first,
            audio->audio_device.playback.format, audio->audio_device.playback.channels, audio->audio_device.sampleRate,
            sounds[0].frequencies, sounds[0].number_of_frequencies,
            audio->audio_device.sampleRate / 1000 * sounds[0].length_milliseconds);
    if (result != MA_SUCCESS) {
        LOG_ERROR("Failed to initialize multi waveform");
        return -1;
    }

    ma_data_source* last = first;
    ma_data_source* current = NULL;
    for (int i = 1; i < sounds_count; ++i) {
        result = multi_waveform_data_source_init(
            (struct multi_waveform_data_source **) &current,
                    audio->audio_device.playback.format, audio->audio_device.playback.channels, audio->audio_device.sampleRate,
                    sounds[i].frequencies, sounds[i].number_of_frequencies,
                    audio->audio_device.sampleRate / 1000 * sounds[i].length_milliseconds);
        if (result != MA_SUCCESS) {
            LOG_ERROR("Failed to initialize multi waveform");
            return -1;
        }

        result = ma_data_source_set_next(last, current);
        if (result != MA_SUCCESS) {
            LOG_ERROR("Failed to update sounds chain");
            return -1;
        }

        last = current;
        current = NULL;
    }

    audio->sounds_playback = first;
    result = ma_event_wait(&audio->sounds_playback_finished_event);
    if (result != MA_SUCCESS) {
        LOG_ERROR("Failed waiting on playback to finish");
        return -1;
    }

    result = audio->sounds_playback_result;
    if (result != MA_SUCCESS) {
        LOG_ERROR("Failed playing sounds");
        return -1;
    }
    // TODO: free sounds

    return 0;
}

