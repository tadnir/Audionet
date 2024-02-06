#ifndef AUDIONET_FFT_H
#define AUDIONET_FFT_H


struct frequency_and_magnitude {
    float frequency;
    float magnitude;
};

typedef void (*frequencies_callback_t)(void* context, const struct frequency_and_magnitude* frequencies, size_t length);

/**
 * The FFT interface type.
 */
typedef struct fft_s fft_t;

fft_t* FFT__initialize(int frame_count, int sample_rate);

void FFT__free(fft_t* fft);

int FFT__calculate(fft_t* fft, const float* data, size_t frame_count, struct frequency_and_magnitude** frequencies, size_t* out_length);


#endif //AUDIONET_FFT_H
