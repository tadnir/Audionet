#include <malloc.h>
#include "multi_waveform_data_source.h"
#include "utils/logger.h"
#include "utils/utils.h"

/**
 * Miniaudio API - implements reading of the next audio frame from the multi waveform datasource.
 *
 * @param pDataSource The multi-waveform datasource to read from.
 * @param pFramesOut The sound buffer to write to.
 * @param frameCount The size of the output sound buffer.
 * @param pFramesRead Returns the amount of frames written to the output sound buffer.
 * @return MA_SUCCESS on success, other enum values otherwise.
 */
static ma_result multi_waveform_data_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead) {
    if (frameCount == 0 || pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pFramesRead != NULL) {
        *pFramesRead = 0;
    }

    struct multi_waveform_data_source* dataSource = pDataSource;
    if (dataSource->format != ma_format_f32) {
        LOG_ERROR("Mixing is currently not supported for non ma_format_f32 formats");
        return MA_ERROR;
    }

    /* We initialize the buffer to silence in case we wouldn't fill it all. */
    ma_silence_pcm_frames(pFramesOut, frameCount, dataSource->format, dataSource->channels);

    /* The actual amount of frames we will output may be lower than frameCount if we reach the end of the stream. */
    ma_uint64 frames_to_output = min(frameCount, dataSource->length_frames - dataSource->frame_cursor);
    if (frames_to_output == 0) {
        /* In case there's nothing more to output. */
        return MA_AT_END;
    }

    /* We allocate a temporary buffer for mixing the different waveforms. */
    void* temp = malloc(frames_to_output * ma_get_bytes_per_sample(dataSource->format) * dataSource->channels);
    if (temp == NULL) {
        LOG_ERROR("Failed to allocate temporary output calculation buffer");
        return MA_ERROR;
    }

    ma_result result;
    for (int i = 0; i < dataSource->waveforms_count; ++i) {
        /* There's no need to check the waveform's actual read output since on success they always fill the buffer. */
        result = ma_waveform_read_pcm_frames(&dataSource->waveforms[i], temp, frames_to_output, NULL);
        if (result != MA_SUCCESS) {
            LOG_ERROR("Failed to read from waveform");
            goto l_cleanup;
        }

        /* Mix the waveforms together with a simple average. */
        for (int j = 0; j < frames_to_output * dataSource->channels; j++) {
            ((float*)pFramesOut)[j] += ((float*)temp)[j] / (float) dataSource->waveforms_count;
        }
    }

    /* Update the cursor with the last frame size. */
    dataSource->frame_cursor += frames_to_output;
    if (pFramesRead != NULL) {
        *pFramesRead = frames_to_output;
    }

l_cleanup:
    if (temp != NULL) {
        free(temp);
    }

    return result;
}

/**
 * Miniaudio API - Seek to a specific frame in the datasource.
 *
 * @param pDataSource The multi-waveform datasource to seek.
 * @param frameIndex The frame index to seek to.
 * @return MA_SUCCESS on success, other enum values otherwise.
 */
static ma_result multi_waveform_data_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex) {
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    /* We need to iterate and seek each sub-waveform. */
    ma_result result;
    struct multi_waveform_data_source* dataSource = pDataSource;
    for (int i = 0; i < dataSource->waveforms_count; ++i) {
        result = ma_data_source_seek_to_pcm_frame(&dataSource->waveforms[i], frameIndex);
        if (result != MA_SUCCESS) {
            LOG_ERROR("Failed to seek waveform from %d to %llu", dataSource->frame_cursor, frameIndex);
            return result;
        }
    }

    /* After seeking all sub-waveform we can update the frame cursor */
    dataSource->frame_cursor = frameIndex;

    return MA_SUCCESS;
}

/**
 * Miniaudio API - Get the datasource data format and configurations.
 *
 * @param pDataSource The multi-waveform to get it's configurations
 * @param pFormat Returns the data format of the datasource.
 * @param pChannels Returns the channel count of the datasource.
 * @param pSampleRate Returns the sample rate of the datasource.
 * @param pChannelMap Returns the channel mapping of the datasource.
 * @param channelMapCap The length of the channelMap parameter.
 * @return MA_SUCCESS on success, other enum values otherwise.
 */
static ma_result multi_waveform_data_source_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Since each waveform is configured identically, we can just return the configuration of the first one. */
    return ma_data_source_get_data_format(
            &((struct multi_waveform_data_source*) pDataSource)->waveforms[0],
            pFormat, pChannels, pSampleRate,
            pChannelMap, channelMapCap
    );
}

/**
 * Miniaudio API - Get the current position of the cursor.
 *
 * @param pDataSource The multi-waveform to get it's cursor position.
 * @param pCursor Returns the current cursor value of the data source.
 * @return MA_SUCCESS on success, other enum values otherwise.
 */
static ma_result multi_waveform_data_source_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor) {
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    /* We can simply return the current position of our cursor */
    struct multi_waveform_data_source* data_source = pDataSource;
    if (pCursor != NULL) {
        *pCursor = data_source->frame_cursor;
    }

    return MA_SUCCESS;
}

/**
 * Miniaudio API - Get the length of the datasource in PCM frames.
 *
 * @param pDataSource The multi-waveform to get it's length.
 * @param pLength Returns the length of the data source.
 * @return MA_SUCCESS on success, other enum values otherwise.
 */
static ma_result multi_waveform_data_source_get_length(ma_data_source* pDataSource, ma_uint64* pLength) {
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    /* We can simply return the configured length of the multi-waveform as each sub-waveform is infinite */
    struct multi_waveform_data_source* data_source = pDataSource;
    if (pLength != NULL) {
        *pLength = data_source->length_frames;
    }

    return MA_SUCCESS;
}

/**
 * This configures the API of the multi-waveform datasource for Miniaudio.
 */
static ma_data_source_vtable g_multi_waveform_data_source_vtable = {
        multi_waveform_data_source_read,
        multi_waveform_data_source_seek,
        multi_waveform_data_source_get_data_format,
        multi_waveform_data_source_get_cursor,
        multi_waveform_data_source_get_length
};


ma_result multi_waveform_data_source_init(
        struct multi_waveform_data_source **multi_waveform,
        ma_format format, ma_uint32 channels, ma_uint32 sampleRate,
        ma_uint32 *frequencies, ma_uint32 frequencies_count, ma_uint32 length_frames
) {
    ma_result result;

    /* Validate parameters constraints */
    if (format != ma_format_f32) {
        LOG_ERROR("Mixing is currently not supported for non ma_format_f32 formats");
        return MA_ERROR;
    }

    /* Allocate a new multi-waveform and initialize it's attributes. */
    struct multi_waveform_data_source* temp_multi_waveform = malloc(sizeof(struct multi_waveform_data_source));
    if (temp_multi_waveform == NULL) {
        LOG_ERROR("Failed to allocate mutli waveform");
        return MA_ERROR;
    }

    temp_multi_waveform->format = format;
    temp_multi_waveform->channels = channels;
    temp_multi_waveform->waveforms_count = frequencies_count;
    temp_multi_waveform->length_frames = length_frames;
    temp_multi_waveform->frame_cursor = 0;
    temp_multi_waveform->waveforms = malloc(frequencies_count * sizeof(ma_waveform));
    if (temp_multi_waveform == NULL) {
        LOG_ERROR("Failed to allocate waveforms");
        free(temp_multi_waveform);
        return MA_ERROR;
    }

    /* Initialize the multi-waveform as a miniaudio datasource */
    ma_data_source_config baseConfig = ma_data_source_config_init();
    baseConfig.vtable = &g_multi_waveform_data_source_vtable;
    result = ma_data_source_init(&baseConfig, &temp_multi_waveform->base);
    if (result != MA_SUCCESS) {
        LOG_ERROR("Failed to initialize multi waveform data_source_base");
        goto l_cleanup;
    }

    /* Initialize sub-datasources of single frequency waveforms */
    for (int i = 0; i < frequencies_count; ++i) {
        ma_waveform_config sineWaveDefaultConfig = ma_waveform_config_init(
                format, channels, sampleRate, ma_waveform_type_sine,
                /* Amplitude, we will later mix them together so it can be 1 for now */ 1,
                frequencies[i]);
        result = ma_waveform_init(&sineWaveDefaultConfig, &temp_multi_waveform->waveforms[i]);
        if (result != MA_SUCCESS) {
            LOG_ERROR("Failed to initialize waveform %d", i);
            for (int j = 0; j < i; ++j) {
                ma_waveform_uninit(&temp_multi_waveform->waveforms[j]);
            }
            goto l_cleanup;
        }
    }

    *multi_waveform = temp_multi_waveform;
    temp_multi_waveform = NULL;
    result = MA_SUCCESS;
l_cleanup:
    /* Destroy the waveform on error */
    if (temp_multi_waveform != NULL) {
        if (temp_multi_waveform->waveforms != NULL) {
            free(temp_multi_waveform->waveforms);
            temp_multi_waveform->waveforms = NULL;
        }

        free(temp_multi_waveform);
        temp_multi_waveform = NULL;
    }

    return result;
}


void multi_waveform_data_source_uninit(struct multi_waveform_data_source* pMultiWaveformDataSource) {
    /* uninitialize each sub-waveform and free the array */
    for (int i = 0; i < pMultiWaveformDataSource->waveforms_count; ++i) {
        ma_waveform_uninit(&pMultiWaveformDataSource->waveforms[i]);
    }

    free(pMultiWaveformDataSource->waveforms);

    /* Uninitialize the multi-waveform and free it */
    ma_data_source_uninit(&pMultiWaveformDataSource->base);
    free(pMultiWaveformDataSource);
}
