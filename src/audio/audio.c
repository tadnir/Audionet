#include <miniaudio/miniaudio.h>
#include <stdio.h>

#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "utils/logger.h"
#include "audio.h"
#include "internal/multi_waveform_data_source.h"

/**
 * The definition of the audio_t interface.
 */
struct audio_s {
    /** The miniaudio library audio device, used for both recording/playing. */
    ma_device audio_device;

    /** User supplied callback for outputting recorded frames. */
    recording_callback_t recording_callback;

    /** User supplied general context pointer to be passed to the `recording_callback`. */
    void* recording_callback_context;

    /** The currently playing datasource (or NULL if not playing). */
    ma_data_source* sounds_playback;

    /** An event to playback has reached it's end. */
    ma_event sounds_playback_finished_event;

    /**
     * Saves the playback result upon it's end,
     * ready for consumption when `sounds_playback_finished_event` is signaled.
     */
    ma_result sounds_playback_result;

    /** Configures whether we allow recording (e.g invoking `recording_callback`) while playback is running. */
    bool full_duplex;
};

/**
 * This callback is called by Miniaudio whenever there's a ready
 * recorded frame to read and an output buffer to write playback frames into
 *
 * @param pDevice The miniaudio device we've configured this callback with.
 * @param pOutput An output buffer we may write our playback frames into.
 * @param pInput An input buffer we may read recorded frames from.
 * @param frameCount The frames count in both `pOutput` and `pInput`.
 */
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

    /* Allocate the audio struct */
    audio_t* audio = (audio_t*) malloc(sizeof(audio_t));
    if (audio == NULL) {
        LOG_ERROR("Failed to allocate audio struct");
        return NULL;
    }

    /* Initialize the audio state */
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

/**
 * Destroys all the datasources in a playback.
 *
 * @param playback The playback to destroy.
 */
static void destroy_playback(ma_data_source* playback) {
    if (playback == NULL) {
        return;
    }

    destroy_playback(ma_data_source_get_next(playback));
    multi_waveform_data_source_uninit(playback);
}

/**
 * Creates a playback datasource with multiple sounds playing in succession.
 * To play a sound we will create a multi-waveform datasource from the given frequencies in the sound.
 * In order to play them in succession, we can use Miniaudio's `ma_data_source_set_next` function
 * that will cause the datasources to be linked and play seamlessly one after the other as a single datasource.
 *
 * @param sounds The sounds to play.
 * @param sounds_count The number of sounds.
 * @param playback Returns the playback datasource.
 * @return 0 On Success, -1 On Failure.
 */
static int create_sounds_playback(struct sound_s* sounds, uint32_t sounds_count, ma_data_source** playback) {
    int ret = -1;
    ma_result result;

    /* Initialize the first datasource,
     * it's a special case since this will be the datasource we'll use to access the others */
    ma_data_source* first;
    result = multi_waveform_data_source_init(
            (struct multi_waveform_data_source **) &first,
            audio->audio_device.playback.format, audio->audio_device.playback.channels, audio->audio_device.sampleRate,
            sounds[0].frequencies, sounds[0].number_of_frequencies,
            audio->audio_device.sampleRate / 1000 * sounds[0].length_milliseconds);
    if (result != MA_SUCCESS) {
        LOG_ERROR("Failed to initialize multi waveform");
        ret = -1;
        goto l_cleanup;
    }

    ma_data_source* last = first;
    ma_data_source* current = NULL;
    for (int i = 1; i < sounds_count; ++i) {
        /* Create a new datasource for the current sound. */
        result = multi_waveform_data_source_init(
                (struct multi_waveform_data_source **) &current,
                audio->audio_device.playback.format, audio->audio_device.playback.channels, audio->audio_device.sampleRate,
                sounds[i].frequencies, sounds[i].number_of_frequencies,
                audio->audio_device.sampleRate / 1000 * sounds[i].length_milliseconds);
        if (result != MA_SUCCESS) {
            LOG_ERROR("Failed to initialize multi waveform");
            ret = -1;
            goto l_cleanup;
        }

        /* Link the current datasource to the previous one. */
        result = ma_data_source_set_next(last, current);
        if (result != MA_SUCCESS) {
            LOG_ERROR("Failed to update sounds chain");
            ret = -1;
            goto l_cleanup;
        }

        last = current;
        current = NULL;
    }

    /* Pass the responsibility for the playback outside. */
    *playback = first;
    first = NULL;
    ret = 0;

l_cleanup:
    if (first != NULL) {
        /* If we haven't passed the responsibility for the playback outside then we need to clean it. */
        destroy_playback(first);
        first = NULL;
    }

    return ret;
}

int AUDIO__play_sounds(audio_t* audio, struct sound_s* sounds, uint32_t sounds_count) {
    int ret = -1;
    ma_result result;

    /* Validate parameters. */
    if (sounds_count == 0 || sounds == NULL || audio == NULL) {
        LOG_ERROR("Invalid parameters");
        return -1;
    }

    /* Cannot play multiple sounds at the same time, validate there's no collisions. */
    if (audio->sounds_playback != NULL) {
        LOG_ERROR("Another sound is currently playing");
        return -1;
    }

    /* Create the playback from the given sounds. */
    ma_data_source* playback = NULL;
    ret = create_sounds_playback(sounds, sounds_count, &playback);
    if (ret != 0) {
        LOG_ERROR("Failed to create sounds playback");
        return ret;
    }

    /* Set the playback as the datasource played, and wait until the playback is finished. */
    audio->sounds_playback = playback;
    result = ma_event_wait(&audio->sounds_playback_finished_event);
    if (result != MA_SUCCESS) {
        LOG_ERROR("Failed waiting on playback to finish");
        return -1;
    }

    /* Get the playback result. */
    result = audio->sounds_playback_result;
    if (result != MA_SUCCESS) {
        LOG_ERROR("Failed playing sounds");
        return -1;
    }

    /* Clean the playback. */
    destroy_playback(playback);
    return 0;
}

