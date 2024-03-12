#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "audio_encoding.h"
#include "utils/logger.h"
#include "utils/utils.h"


#define FREQUENCY_TO_CHANNEL_INDEX(frequency) (                                                           \
    (ROUND_TO((frequency), CHANNEL_FREQUENCY_BAND_WIDTH) / CHANNEL_FREQUENCY_BAND_WIDTH)                  \
    - (ROUND_TO(BASE_CHANNEL_FREQUENCY, CHANNEL_FREQUENCY_BAND_WIDTH) / CHANNEL_FREQUENCY_BAND_WIDTH)     \
)
#define CHANNEL_INDEX_TO_FREQUENCY(channel) (                                                             \
    (channel) * CHANNEL_FREQUENCY_BAND_WIDTH + (CHANNEL_FREQUENCY_BAND_WIDTH/2) + BASE_CHANNEL_FREQUENCY  \
)

static uint64_t calculate_channel_value(unsigned int normalized_total_channels, unsigned int number_of_lower_order_channels, unsigned int normalized_channel) {
    /* We need to sum the number of arrangements of the lower order channels for each position of channel up to it's current value.
     * For example, when having 3 channels and 5 total channels we have these values:
     * [0, 1, 2] -> 0,  [0, 1, 3] -> 1,  [0, 1, 4] -> 2
     * [0, 2, 3] -> 3,  [0, 2, 4] -> 4,  [0, 3, 4] -> 5
     * [1, 2, 3] -> 6,  [1, 2, 4] -> 7,  [1, 3, 4] -> 8,  [2, 3, 4] -> 9
     * Looking at calculating the value of the second channel, between [0, 1, 2] and [0, 2, 3] there are 3 arrangements (3 possible values for the last channel),
     * which we can calculate as: The number of remaining values (2,3,4 -> 3) nCr The number of lower order channels (1) --> 3C1=3.
     * We need to do it for every position the channel has moved through to get to it's current value in order to
     * calculate the channel numerical value.
     * But, we actually don't care about previous channel values (e.g, in this example it's the first channel)
     * so we can use just the normalized values.
     */
    uint64_t sum = 0;
    for (int i = 0; i < normalized_channel; ++i) {
        sum += comb(normalized_total_channels - i - 1, number_of_lower_order_channels);
    }

    return sum;
}

static uint64_t decode_channels_recurse(unsigned int normalized_total_channels, unsigned int zero_channel_number, size_t channels_length, unsigned int *channels) {
    /* This is the end condition for the recursion, when there's no channels to decode. */
    if (channels_length == 0) {
        return 0;
    }

    unsigned int normalized_channel = channels[0] - zero_channel_number;
    uint64_t channel_value = calculate_channel_value(
            normalized_total_channels,
        channels_length - 1, /* The amount of lower order channels is the remaining channels without the current */
        normalized_channel
    );
    return channel_value + decode_channels_recurse(
        /* The amount of remaining channels is what we had minus the amount we took (since the channels must be in ascending order) */
        normalized_total_channels - normalized_channel - 1,
        /* The next channel zero number is the next number after the current channel number */
        channels[0] + 1,
        /* We advance the channels array to the next element */
        channels_length - 1,
        channels + 1
    );
}

static uint64_t decode_channels(unsigned int total_channels, size_t channels_length, unsigned int *channels) {
    /* We need to have the channels sorted for the algorithm */
    qsort(channels, channels_length, sizeof(unsigned int), (int (*)(const void *, const void *)) compare_unsigned_ints);
    return decode_channels_recurse(total_channels, 0, channels_length, channels);
}

static int encode_channels_recurse(unsigned int normalized_total_channels, unsigned int zero_channel_number, uint64_t remaining_to_encode, size_t channels_length, unsigned int *channels) {
    /* End condition for the recursion */
    if (channels_length == 0) {
        if (remaining_to_encode != 0) {
            /* We couldn't encode the remaining_to_encode properly with the amount of channels we had. */
            return -1;
        }
        return 0;
    }

    /* We iterate through the possible values for the current channel number,
     * for each we calculate the channel numerical value and try to take the
     * largest value that fits without overshooting remaining_to_encode */
    unsigned int best_normalized_channel = 0;
    uint64_t best_value = 0;
    unsigned int number_of_lower_order_channels = channels_length - 1;
    /* There's no need to calculate for 0, it's 0... */
    for (int i = 1; i < normalized_total_channels - number_of_lower_order_channels; ++i) {
        uint64_t test_value = calculate_channel_value(normalized_total_channels, number_of_lower_order_channels, i);
        if (test_value > remaining_to_encode) {
            /* The current value is too big, we can take the last value */
            break;
        }

        best_normalized_channel = i;
        best_value = test_value;

        if (test_value == remaining_to_encode) {
            /* The current value is perfect, we are taking it */
            break;
        }
    }

    /* We can update the channels */
    channels[0] = best_normalized_channel + zero_channel_number;
    return encode_channels_recurse(
        /* The remaining total channels is reduced by what the current channel have taken */
        normalized_total_channels - best_normalized_channel - 1,
        /* The zero number for the next channel is the next channel after this one */
        best_normalized_channel + zero_channel_number + 1,
        /* Reduce what remains_to_encode by what we have encoded */
        remaining_to_encode - best_value,
        /* Advance the channels array */
        channels_length - 1,
        channels + 1
    );
}

static int encode_channels(unsigned int total_channels, uint64_t value, size_t channels_length, unsigned int *channels) {
    return encode_channels_recurse(total_channels, 0, value, channels_length, channels);
}

static int compare_magnitudes(const struct frequency_and_magnitude* a, const struct frequency_and_magnitude* b) {
    // Note: Reverse comparison for descending order
    return -1 * compare_floats(&a->magnitude, &b->magnitude);
}

int AUDIO_ENCODING__decode_frequencies(uint64_t* value_out, size_t frequencies_count,
                                       struct frequency_and_magnitude frequencies[]) {
    if (frequencies_count < NUMBER_OF_CONCURRENT_CHANNELS) {
        LOG_ERROR("Expected at least %d frequencies, got: %lld", NUMBER_OF_CONCURRENT_CHANNELS, frequencies_count);
        return -1;
    }

    /* Sort the frequencies by their magnitudes */
    qsort((void*)frequencies, frequencies_count, sizeof(struct frequency_and_magnitude),
          (int (*)(const void *, const void *)) compare_magnitudes);

    /* If there aren't at least NUMBER_OF_CONCURRENT_CHANNELS frequencies with some noticeable sound,
     * we consider it as quiet and there's no reason to continue. */
    if (frequencies[NUMBER_OF_CONCURRENT_CHANNELS - 1].magnitude <= AMPLITUDE_MAGNITUDE_THRESHOLD) {
        return AUDIO_DECODE_RET_QUIET;
    }

    int channels_found = 0;
    unsigned int channels[NUMBER_OF_CONCURRENT_CHANNELS];
    for (int i = 0; i < frequencies_count && channels_found < NUMBER_OF_CONCURRENT_CHANNELS; i++) {
        if (frequencies[i].magnitude <= AMPLITUDE_MAGNITUDE_THRESHOLD) {
            LOG_VERBOSE("sound died out by frequency index %d", i);
            break;
        }

        unsigned int channel = FREQUENCY_TO_CHANNEL_INDEX(frequencies[i].frequency);
        if (channel >= NUMBER_OF_CHANNELS) {
            LOG_VERBOSE("Invalid channel number: %d (probably due to noise)", channel);
            continue;
        }

        if (array_contains(channel, channels_found, channels)) {
            LOG_VERBOSE("Channel %d found twice, might be due to noise, multiple speakers or general collision. skipping", channel);
            continue;
        }

        LOG_VERBOSE("Found channel: %d (%.0fHz)", channel, frequencies[i].frequency);
        channels[channels_found] = channel;
        channels_found++;
    }

    /* Couldn't find enough channels */
    if (channels_found < NUMBER_OF_CONCURRENT_CHANNELS) {
        return AUDIO_DECODE_RET_QUIET;
    }

    LOG_VERBOSE("Trying to decode %d %d %d", channels[0], channels[1], channels[2]);
    *value_out = decode_channels(NUMBER_OF_CHANNELS, NUMBER_OF_CONCURRENT_CHANNELS, channels);
#ifdef VERBOSE
    printf("Decoded %llu: ", *value_out);
    for(int i=0; i < NUMBER_OF_CONCURRENT_CHANNELS; ++i) {
        printf("%d ", CHANNEL_INDEX_TO_FREQUENCY(channels[i]));
    }

    printf("\n");
#endif
    return 0;
}

int AUDIO_ENCODING__encode_frequencies(uint64_t value, size_t frequencies_count,
                                       uint32_t frequencies[]) {
    unsigned int channels[AUDIO_ENCODE_MAXIMUM_CONCURRENT_FREQUENCIES];
    if (frequencies_count >= AUDIO_ENCODE_MAXIMUM_CONCURRENT_FREQUENCIES) {
        LOG_ERROR("Encode frequencies count exceeded maximum allowed");
        return -1;
    }

    int ret = encode_channels(NUMBER_OF_CHANNELS, value, frequencies_count, channels);
    if (ret != 0) {
        LOG_ERROR("Failed to encode value to channels");
        return ret;
    }

    for (int i = 0; i < frequencies_count; ++i) {
        frequencies[i] = CHANNEL_INDEX_TO_FREQUENCY(channels[i]);
    }

#ifdef VERBOSE
    printf("Encoded %llu: ", value);
    for(int i=0; i < NUMBER_OF_CONCURRENT_CHANNELS; ++i) {
        printf("%d ", frequencies[i]);
    }

    printf("\n");
#endif

    return 0;
}