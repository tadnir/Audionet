#ifndef AUDIONET_FFT_H
#define AUDIONET_FFT_H


/**
 * The FFT interface type.
 */
typedef struct fft_s fft_t;

/**
 * This struct holds the frequency and amplitude for each result calculated from the FTT.
 */
struct frequency_and_magnitude {
    /** The frequency value found */
    float frequency;

    /** The magnitude of the given frequency */
    float magnitude;
};

/**
 * Initializes the FFT module.
 *
 * @param frame_count The expected samples size.
 * @param sample_rate The expected samples rate.
 * @return The initialized FFT module interface.
 */
fft_t* FFT__initialize(int frame_count, int sample_rate);

/**
 * Frees a previously allocated FFT interface initialized with FFT__initialize.
 *
 * @param fft The FFT interface to free.
 */
void FFT__free(fft_t* fft);

/**
 * Preforms the FFT calculation of the given sample data and outputs the resulting frequencies and amplitudes.
 *
 * @param fft The FFT interface.
 * @param sample The raw sample data.
 * @param frame_count The size of the sample data.
 * @param frequencies Returns the calculated frequencies array - Expected to be freed by the user via `free`.
 * @param out_length Returns the size of the frequencies array.
 * @return 0 On Success, negative on failure.
 */
int FFT__calculate(
        fft_t* fft,
        const float* sample, size_t frame_count,
        struct frequency_and_magnitude** frequencies, size_t* out_length
);


#endif //AUDIONET_FFT_H
