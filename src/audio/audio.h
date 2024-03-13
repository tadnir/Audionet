#ifndef AUDIONET_AUDIO_H
#define AUDIONET_AUDIO_H

/**
 * The maximum amount of concurrent frequencies in a sound.
 */
#define SOUND_MAX_CONCURRENT_FREQUENCIES (5)

/**
 * The various available sample rates for audio recording/playing.
 */
enum standard_sample_rate {
    /* Most common */
    SAMPLE_RATE_48000  = 48000,
    SAMPLE_RATE_44100  = 44100,

    /* Lows */
    SAMPLE_RATE_32000  = 32000,
    SAMPLE_RATE_24000  = 24000,
    SAMPLE_RATE_22050  = 22050,

    /* Highs */
    SAMPLE_RATE_88200  = 88200,
    SAMPLE_RATE_96000  = 96000,
    SAMPLE_RATE_176400 = 176400,
    SAMPLE_RATE_192000 = 192000,

    /* Extreme lows */
    SAMPLE_RATE_16000  = 16000,
    SAMPLE_RATE_11025  = 11025,
    SAMPLE_RATE_8000   = 8000,

    /* Extreme highs */
    SAMPLE_RATE_352800 = 352800,
    SAMPLE_RATE_384000 = 384000,
};

/**
 * The audio interface type.
 */
typedef struct audio_s audio_t;

/**
 * The type definition for audio recording callback.
 */
typedef void (*recording_callback_t)(void* context, const float* recorded_frame, size_t size);

/**
 * Allocates and initializes an audio interface.
 * May be used for both recording and playing with the same interface.
 * Creating multiple interfaces leads to undefined behaviour.
 *
 * @param sample_rate The sample rate at which to record/play.
 * @return The initialize audio interface. Returns NULL on failure.
 */
audio_t* AUDIO__initialize(enum standard_sample_rate sample_rate, bool full_duplex);

/**
 * Frees and uninitializes the audio interface.
 *
 * @param audio The audio interface to free.
 */
void AUDIO__free(audio_t* audio);

/**
 * Starts the audio recorder/speaker.
 *
 * @param audio The audio to start.
 * @return 0 On Success, -1 On Failure.
 */
int AUDIO__start(audio_t* audio);

/**
 * Starts the audio recorder/speaker.
 *
 * @param audio The audio to start.
 * @return 0 On Success, -1 On Failure.
 */
int AUDIO__stop(audio_t* audio);

/**
 * Sets the user callback to be called each time there's a recorded audio buffer.
 *
 * @param audio The audio interface to set.
 * @param callback The callback function to call with recorded data.
 * @param callback_context An optional context param passed to the callback function.
 */
void AUDIO__set_recording_callback(audio_t* audio, recording_callback_t callback, void* callback_context);

/**
 * A single sound that can be played,
 * composed from multiple frequencies playing together for some duration.
 */
struct sound_s {
    /**
     * The length of the sound.
     */
    uint32_t length_milliseconds;

    /**
     * A list of frequencies to be played overlaid together as a sound.
     * At a maximum amount of SOUND_MAX_CONCURRENT_FREQUENCIES.
     * The more frequencies there are the less pronounced each of them will be.
     */
    uint32_t frequencies[SOUND_MAX_CONCURRENT_FREQUENCIES];

    /**
     * The amount of frequencies in the sound.
     */
    uint32_t number_of_frequencies;
};

/**
 * Plays an array of given sounds in succession.
 * The function blocks until the sounds has been played,
 * cannot call this function concurrently.
 *
 * @param audio The audio interface to play from.
 * @param sounds The list of sounds to play.
 * @param sounds_count The amount of sounds.
 * @return 0 On Success, -1 On Failure.
 */
int AUDIO__play_sounds(audio_t* audio, struct sound_s* sounds, uint32_t sounds_count);

#endif //AUDIONET_AUDIO_H
