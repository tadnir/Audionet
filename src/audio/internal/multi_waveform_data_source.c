#include <malloc.h>
#include "multi_waveform_data_source.h"
#include "logger.h"
#include "utils/minmax.h"

static ma_result multi_waveform_data_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    if (pFramesRead != NULL) {
        *pFramesRead = 0;
    }

    if (frameCount == 0 || pDataSource == NULL) {
        return MA_INVALID_ARGS;
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

// Seek to a specific PCM frame here. Return MA_NOT_IMPLEMENTED if seeking is not supported.
static ma_result multi_waveform_data_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex) {
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_result result;
    struct multi_waveform_data_source* dataSource = pDataSource;
    for (int i = 0; i < dataSource->waveforms_count; ++i) {
        result = ma_data_source_seek_to_pcm_frame(&dataSource->waveforms[i], frameIndex);
        if (result != MA_SUCCESS) {
            LOG_ERROR("Failed to seek waveform from %d to %llu", dataSource->frame_cursor, frameIndex);
            return result;
        }
    }

    dataSource->frame_cursor = frameIndex;

    return MA_SUCCESS;
}

static ma_result multi_waveform_data_source_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    // Return the format of the data here.
    return ma_data_source_get_data_format(&((struct multi_waveform_data_source*) pDataSource)->waveforms[0], pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

/**
 * Retrieve the current position of the cursor here.
 *
 * @param pDataSource The data source.
 * @param pLength Returns the current cursor value of the data source.
 * @return MA_SUCCESS on success, MA_ERROR on error.
 */
static ma_result multi_waveform_data_source_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    if (pDataSource == NULL) {
        return MA_ERROR;
    }

    struct multi_waveform_data_source* data_source = pDataSource;
    if (pCursor != NULL) {
        *pCursor = data_source->frame_cursor;
    }
    return MA_SUCCESS;
}

/**
 * Retrieve the length in PCM frames here.
 *
 * @param pDataSource The data source.
 * @param pLength Returns the length of the data source.
 * @return MA_SUCCESS on success, MA_ERROR on error.
 */
static ma_result multi_waveform_data_source_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
{
    if (pDataSource == NULL) {
        return MA_ERROR;
    }

    struct multi_waveform_data_source* data_source = pDataSource;
    if (pLength != NULL) {
        *pLength = data_source->length_frames;
    }
    return MA_SUCCESS;
}

static ma_data_source_vtable g_multi_waveform_data_source_vtable =
        {
                multi_waveform_data_source_read,
                multi_waveform_data_source_seek,
                multi_waveform_data_source_get_data_format,
                multi_waveform_data_source_get_cursor,
                multi_waveform_data_source_get_length
        };

ma_result
multi_waveform_data_source_init(
        struct multi_waveform_data_source **multi_waveform,
        ma_format format, ma_uint32 channels, ma_uint32 sampleRate,
        ma_uint32 *frequencies, ma_uint32 frequencies_count, ma_uint32 length_frames
) {
    ma_result result;
    if (format != ma_format_f32) {
        LOG_ERROR("Mixing is currently not supported for non ma_format_f32 formats");
        return MA_ERROR;
    }

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

    ma_data_source_config baseConfig = ma_data_source_config_init();
    baseConfig.vtable = &g_multi_waveform_data_source_vtable;
    result = ma_data_source_init(&baseConfig, &temp_multi_waveform->base);
    if (result != MA_SUCCESS) {
        LOG_ERROR("Failed to initialize multi waveform data_source_base");
        goto l_cleanup;
    }

    /* Initialize output waveforms */
    for (int i = 0; i < frequencies_count; ++i) {
        ma_waveform_config sineWaveDefaultConfig = ma_waveform_config_init(
                format, channels, sampleRate, ma_waveform_type_sine,
                /* Amplitude */ 1, frequencies[i]);
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

void multi_waveform_data_source_uninit(struct multi_waveform_data_source* pMultiWaveformDataSource)
{
    for (int i = 0; i < pMultiWaveformDataSource->waveforms_count; ++i) {
        ma_waveform_uninit(&pMultiWaveformDataSource->waveforms[i]);
    }

    free(pMultiWaveformDataSource->waveforms);

    // You must uninitialize the base data source.
    ma_data_source_uninit(&pMultiWaveformDataSource->base);

    free(pMultiWaveformDataSource);
}
