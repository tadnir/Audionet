#ifndef AUDIONET_MULTI_WAVEFORM_DATA_SOURCE_H
#define AUDIONET_MULTI_WAVEFORM_DATA_SOURCE_H

#include "miniaudio/miniaudio.h"

struct multi_waveform_data_source {
    ma_data_source_base base;
    ma_waveform* waveforms;
    int waveforms_count;
    ma_format format;
    ma_uint32 channels;
};

/**
 * Frequency output configuration for outputting a specific frequency at some amplitude.
 */
struct frequency_output {
    double frequency;
    double amplitude;
};

ma_result
multi_waveform_data_source_init(struct multi_waveform_data_source **pOutMultiWaveformDataSource,
        ma_format format, ma_uint32 channels, ma_uint32 sampleRate,
        struct frequency_output *frequencies, int frequencies_length);
void multi_waveform_data_source_uninit(struct multi_waveform_data_source* pMultiWaveformDataSource);

#endif //AUDIONET_MULTI_WAVEFORM_DATA_SOURCE_H
