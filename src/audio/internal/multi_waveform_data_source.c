#include <malloc.h>
#include <string.h>
#include "multi_waveform_data_source.h"
#include "logger.h"

static ma_result multi_waveform_data_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    struct multi_waveform_data_source* dataSource = pDataSource;

    if (dataSource->format != ma_format_f32) {
        LOG_ERROR("Mixing is currently not supported for non ma_format_f32 formats");
        return MA_ERROR;
    }

    // Read data here. Output in the same format returned by multi_waveform_data_source_get_data_format().
    void* temp = malloc(frameCount * ma_get_bytes_per_sample(dataSource->format) * dataSource->channels);
    if (temp == NULL) {
        LOG_ERROR("Failed to allocate temporary output calculation buffer");
        return MA_ERROR;
    }

    memset(temp, 0, frameCount * ma_get_bytes_per_sample(dataSource->format) * dataSource->channels);
    ma_result result;
    for (int i = 0; i < dataSource->waveforms_count; ++i) {
        result = ma_waveform_read_pcm_frames(&dataSource->waveforms[i], temp, frameCount, pFramesRead);
        if (result != MA_SUCCESS) {
            LOG_ERROR("Failed to read from waveform");
            goto l_cleanup;
        }

        ma_uint64 sampleCount = frameCount * dataSource->channels;
        for (int iSample = 0; iSample < sampleCount; iSample++) {
            ((float*)pFramesOut)[iSample] += ((float*)temp)[iSample] / dataSource->waveforms_count;
        }
    }

l_cleanup:
    if (temp != NULL) {
        free(temp);
    }

    return result;
}

static ma_result multi_waveform_data_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    // Seek to a specific PCM frame here. Return MA_NOT_IMPLEMENTED if seeking is not supported.
    return MA_NOT_IMPLEMENTED;
}

static ma_result multi_waveform_data_source_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    // Return the format of the data here.
    return ma_data_source_get_data_format(&((struct multi_waveform_data_source*) pDataSource)->waveforms[0], pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result multi_waveform_data_source_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    // Retrieve the current position of the cursor here. Return MA_NOT_IMPLEMENTED and set *pCursor to 0 if there is no notion of a cursor.
    *pCursor = 0;
    return MA_NOT_IMPLEMENTED;
}

static ma_result multi_waveform_data_source_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
{
    // Retrieve the length in PCM frames here. Return MA_NOT_IMPLEMENTED and set *pLength to 0 if there is no notion of a length or if the length is unknown.
    *pLength = 0;
    return MA_NOT_IMPLEMENTED;
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
        struct multi_waveform_data_source **pOutMultiWaveformDataSource,
        ma_format format, ma_uint32 channels, ma_uint32 sampleRate,
        struct frequency_output *frequencies, int frequencies_length
) {
    ma_result result;
    ma_data_source_config baseConfig;

    if (format != ma_format_f32) {
        LOG_ERROR("Mixing is currently not supported for non ma_format_f32 formats");
        return MA_ERROR;
    }


    struct multi_waveform_data_source* pMultiWaveformDataSource = malloc(sizeof(struct multi_waveform_data_source));
    if (pMultiWaveformDataSource == NULL) {
        return MA_ERROR;
    }

    pMultiWaveformDataSource->format = format;
    pMultiWaveformDataSource->channels = channels;
    pMultiWaveformDataSource->waveforms_count = frequencies_length;
    pMultiWaveformDataSource->waveforms = malloc(frequencies_length * sizeof(ma_waveform));
    if (pMultiWaveformDataSource == NULL) {
        return MA_ERROR;
    }

    baseConfig = ma_data_source_config_init();
    baseConfig.vtable = &g_multi_waveform_data_source_vtable;
    result = ma_data_source_init(&baseConfig, &pMultiWaveformDataSource->base);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* Initialize output waveforms */
    for (int i = 0; i < frequencies_length; ++i) {
        ma_waveform_config sineWaveDefaultConfig = ma_waveform_config_init(
                format, channels, sampleRate, ma_waveform_type_sine,
                frequencies[i].amplitude, frequencies[i].frequency);
        result = ma_waveform_init(&sineWaveDefaultConfig, &pMultiWaveformDataSource->waveforms[i]);
        if (result != MA_SUCCESS) {
            LOG_ERROR("Failed to initialize waveform %d", i);
            for (int j = 0; j < i; ++j) {
                ma_waveform_uninit(&pMultiWaveformDataSource->waveforms[j]);
            }
            return MA_ERROR;
        }
    }

    *pOutMultiWaveformDataSource = pMultiWaveformDataSource;
    return MA_SUCCESS;
}

void multi_waveform_data_source_uninit(struct multi_waveform_data_source* pMultiWaveformDataSource)
{
    for (int i = 0; i < pMultiWaveformDataSource->waveforms_count; ++i) {
        ma_waveform_uninit(&pMultiWaveformDataSource->waveforms[i]);
    }

    free(pMultiWaveformDataSource->waveforms);

    // You must uninitialize the base data source.
    ma_data_source_uninit(&pMultiWaveformDataSource->base);
}
