/**
 * This file defines a new kind of `datasource` for the miniaudio library to use.
 * The application of this datasource is to play multiple waveforms overlaid together.
 */

#ifndef AUDIONET_MULTI_WAVEFORM_DATA_SOURCE_H
#define AUDIONET_MULTI_WAVEFORM_DATA_SOURCE_H

#include "miniaudio/miniaudio.h"

/**
 * A datasource overlaying multiple waveforms together.
 */
struct multi_waveform_data_source {
    /** Miniaudio datasource base */
    ma_data_source_base base;

    /** The list of waveforms to be overlaid */
    ma_waveform* waveforms;

    /** The amount of waveforms to be overlaid */
    ma_uint32 waveforms_count;

    /** The miniaudio format type to output */
    ma_format format;

    /** The amount of channels to output */
    ma_uint32 channels;

    /** The amount of frames to output, effectively settings the length of the data source */
    ma_uint32 length_frames;

    /** The current frame index */
    ma_uint32 frame_cursor;
};


/**
 * Initializes a new multi-waveform datasource.
 *
 * @param multi_waveform Returns the initialized multi-waveform.
 * @param format The miniaudio format to output.
 * @param channels The amount of channels to output.
 * @param sampleRate The sample rate at which to output.
 * @param frequencies The list of frequencies to output simultaneously.
 * @param frequencies_count The amount of frequencies.
 * @param length_frames The number of frames to output before this datasource is finished.
 * @return MA_SUCCESS on success, other enum values otherwise.
 */
ma_result multi_waveform_data_source_init(
    struct multi_waveform_data_source **multi_waveform,
    ma_format format, ma_uint32 channels, ma_uint32 sampleRate,
    ma_uint32 *frequencies, ma_uint32 frequencies_count, ma_uint32 length_frames
);


/**
 * Uninitializes and frees a multi-waveform previously initialized with multi_waveform_data_source_init.
 *
 * @param pMultiWaveformDataSource The waveform to free.
 */
void multi_waveform_data_source_uninit(struct multi_waveform_data_source* pMultiWaveformDataSource);

#endif //AUDIONET_MULTI_WAVEFORM_DATA_SOURCE_H
