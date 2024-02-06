#ifndef AUDIONET_AUDIO_ENCODING_H
#define AUDIONET_AUDIO_ENCODING_H

#include "fft/fft.h"

#define BASE_CHANNEL_FREQUENCY (100)
#define CHANNEL_FREQUENCY_BAND_WIDTH (100)
#define NUMBER_OF_CHANNELS (13)
#define NUMBER_OF_CONCURRENT_CHANNELS (3)
#define AMPLITUDE_MAGNITUDE_THRESHOLD (0.1)

#define AUDIO_DECODE_RET_QUIET (-2)
#define AUDIO_ENCODE_MAXIMUM_CONCURRENT_FREQUENCIES (20)

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

int AUDIO_ENCODING__encode_frequencies(uint64_t value, size_t frequencies_count,
                                       uint32_t frequencies[]);
#endif //AUDIONET_AUDIO_ENCODING_H
