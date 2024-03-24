#ifndef AUDIONET_AUDIO_ENCODING_H
#define AUDIONET_AUDIO_ENCODING_H

#include "fft/fft.h"

/** The lowest frequency transmitted. */
#define BASE_CHANNEL_FREQUENCY (100)

/** The separation width between transmitted frequencies. */
#define CHANNEL_FREQUENCY_BAND_WIDTH (150)

/** The number of different frequencies channels. */
#define NUMBER_OF_CHANNELS (13)

/** The number of frequency channel that are used simultaneously. */
#define NUMBER_OF_CONCURRENT_CHANNELS (3)

/** The minimal frequency amplitude that is considered "heard". */
#define AMPLITUDE_MAGNITUDE_THRESHOLD (0.1)

/** The return code from the decode function to signify quiet recording. */
#define AUDIO_DECODE_RET_QUIET (-2)

/**
 * Decodes recorded frequencies to integer value.
 *
 * @param value_out On success, returns the decoded value.
 * @param frequencies_count The length of frequencies.
 * @param frequencies Array of recorded frequencies.
 * @return 0 on Success, -1 on Error, -2 on Quiet.
 */
int AUDIO_ENCODING__decode_frequencies(uint64_t* value_out, size_t frequencies_count,
                                       struct frequency_and_magnitude frequencies[]);

/**
 * Encodes integer value to frequencies.
 *
 * @param value The value to encode.
 * @param frequencies_count The number of frequencies in the array.
 * @param frequencies Output array of frequencies encoding the value.
 * @return
 */
int AUDIO_ENCODING__encode_frequencies(uint64_t value, size_t frequencies_count,
                                       uint32_t frequencies[]);
#endif //AUDIONET_AUDIO_ENCODING_H
