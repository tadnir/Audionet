#ifndef AUDIONET_MULTI_WAVEFORM_DATA_SOURCE_H
#define AUDIONET_MULTI_WAVEFORM_DATA_SOURCE_H

#include "miniaudio/miniaudio.h"

struct multi_waveform_data_source {
    ma_data_source_base base;
    ma_waveform* waveforms;
    ma_uint32 waveforms_count;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 length_frames;
    ma_uint32 frame_cursor;
};


ma_result
multi_waveform_data_source_init(struct multi_waveform_data_source **multi_waveform,
                                ma_format format, ma_uint32 channels, ma_uint32 sampleRate,
                                ma_uint32 *frequencies, ma_uint32 frequencies_count, ma_uint32 length_frames);
void multi_waveform_data_source_uninit(struct multi_waveform_data_source* pMultiWaveformDataSource);

#endif //AUDIONET_MULTI_WAVEFORM_DATA_SOURCE_H
