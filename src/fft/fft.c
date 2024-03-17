#include <math.h>
#include <fftw3.h>
#include <malloc.h>
#include <stdint.h>
#include <string.h>
#include "fft.h"
#include "utils/logger.h"

struct fft_s {

    /** The expected sample rate for the FFT to process */
    int sample_rate;

    /** The expected frame count for the FFT to process */
    int frame_count;

    /** The preconfigured FFTW plan */
    fftwf_plan plan;

    /** The output buffer for FFTW */
    fftwf_complex *fftBuffer;

    /** The input buffer for FFTW */
    float *audioBuffer;
};

/**
 * Calculates the magnitude of a complex number.
 *
 * @param complex_number The complex number to calculate it's magnitude.
 * @return The magnitude of the complex number
 */
static float complex_magnitude(const fftwf_complex complex_number) {
    float real_part = complex_number[0];
    float complex_part = complex_number[1];
    return sqrt(real_part * real_part + complex_part * complex_part);
}

fft_t* FFT__initialize(int frame_count, int sample_rate) {

    /* Allocate the FFT object */
    fft_t* fft = malloc(sizeof(fft_t));
    if (fft == NULL) {
        LOG_ERROR("Failed to allocate fft struct");
        return NULL;
    }

    /* Set the state */
    fft->frame_count = frame_count;
    fft->sample_rate = sample_rate;

    /* Allocate the FFTW input buffer */
    fft->audioBuffer = (float*)malloc(frame_count * sizeof(float));
    if (fft->audioBuffer == NULL) {
        LOG_ERROR("Failed to allocate audio buffer");
        free(fft);
        return NULL;
    }

    /* Allocate fftw plan */
    fft->fftBuffer = (fftwf_complex*)fftwf_malloc(frame_count * sizeof(fftwf_complex));
    if (fft->audioBuffer == NULL) {
        LOG_ERROR("Failed to allocation fft buffer");
        free(fft->audioBuffer);
        free(fft);
        return NULL;
    }

    /* Initialize the FFTW plan */
    fft->plan = fftwf_plan_dft_r2c_1d(frame_count, fft->audioBuffer, fft->fftBuffer, FFTW_MEASURE);

    return fft;
}

void FFT__free(fft_t* fft) {
    /* Reverse order de-allocation/initiation */
    fftwf_destroy_plan(fft->plan);
    fftwf_free(fft->fftBuffer);
    free(fft->audioBuffer);
}

int FFT__calculate(fft_t* fft, const float* sample, size_t frame_count, struct frequency_and_magnitude** frequencies, size_t* out_length) {
    /* Validate the parameters fit the planned configuration */
    if (frame_count != fft->frame_count) {
        printf("Frame Count %lld\n", frame_count);
        return -1;
    }

    uint32_t number_of_bins = frame_count / 2 + 1;
    uint32_t bins_size = fft->sample_rate / frame_count;

    /* Execute the FFT calculation */
    memcpy(fft->audioBuffer, sample, frame_count * sizeof(float));
    fftwf_execute(fft->plan);

    /* Export the complex numbers to frequency/magnitude struct list */
    struct frequency_and_magnitude* freqs = malloc(number_of_bins * sizeof(struct frequency_and_magnitude));
    if (freqs == NULL) {
        LOG_ERROR("Failed to allocate frequencies output buffer");
        return -1;
    }

    for (int i = 0; i < number_of_bins; ++i) {
        freqs[i].frequency = i * bins_size;
        freqs[i].magnitude = complex_magnitude(fft->fftBuffer[i]);
    }

    *out_length = number_of_bins;
    *frequencies = freqs;

    return 0;
}
