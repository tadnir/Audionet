#include <miniaudio/miniaudio.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "audio.h"
#include <fftw3.h>
#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>


#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     2
#define DEVICE_SAMPLE_RATE  48000


/** Play Sound ****************************************************************************************/
void play_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_waveform* pSineWave;

    assert(pDevice->playback.channels == DEVICE_CHANNELS);

    pSineWave = (ma_waveform*)pDevice->pUserData;
    assert(pSineWave != NULL);

    ma_waveform_read_pcm_frames(pSineWave, pOutput, frameCount, NULL);

    (void)pInput;   /* Unused. */
}

int play_sine() {
    ma_waveform sineWave;
    ma_device_config deviceConfig;
    ma_device device;
    ma_waveform_config sineWaveConfig;

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
    deviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback      = play_callback;
    deviceConfig.pUserData         = &sineWave;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return -4;
    }

    printf("Device Name: %s\n", device.playback.name);

    sineWaveConfig = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, 0.2, 220);
    ma_waveform_init(&sineWaveConfig, &sineWave);

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        return -5;
    }

    printf("Press Enter to quit...\n");
    getchar();

    ma_device_uninit(&device);
    return 0;
}


/** Record Sound **************************************************************************************/
void record_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_encoder* pEncoder = (ma_encoder*)pDevice->pUserData;
    assert(pEncoder != NULL);

    ma_encoder_write_pcm_frames(pEncoder, pInput, frameCount, NULL);

    (void)pOutput;
}

int record_to_file(const char* path) {
    int ret = -1;

    ma_result result;
    ma_encoder_config encoderConfig;
    ma_encoder encoder;
    ma_device_config deviceConfig;
    ma_device device;

    encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 44100);

    if (ma_encoder_init_file(path, &encoderConfig, &encoder) != MA_SUCCESS) {
        printf("Failed to initialize output file.\n");
        return -1;
    }

    deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format   = encoder.config.format;
    deviceConfig.capture.channels = encoder.config.channels;
    deviceConfig.sampleRate       = encoder.config.sampleRate;
    deviceConfig.dataCallback     = record_callback;
    deviceConfig.pUserData        = &encoder;

    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize capture device.\n");
        return -2;
    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        printf("Failed to start device.\n");
        return -3;
    }

    printf("Press Enter to stop recording...\n");
    getchar();

    ma_device_uninit(&device);
    ma_encoder_uninit(&encoder);

    return 0;
}


/** FFT Sound *****************************************************************************************/

struct fft_params {
    float *audioBuffer;
    fftwf_plan plan;
    fftwf_complex *fftBuffer;
};

struct frequency_and_magnitude {
    float frequency;
    float magnitude;
};

int compare_floats(const void* a, const void* b) {
    // Note: Reverse comparison for descending order
    if ((*(float*)b - *(float*)a) > 0) {
        return 1;
    } else if ((*(float*)b - *(float*)a) < 0) {
        return -1;
    }

    return 0;
}

int compare_magnitudes(const struct frequency_and_magnitude* a, const struct frequency_and_magnitude* b) {
    return compare_floats(&a->magnitude, &b->magnitude);
}

float complex_magnitude(const fftwf_complex complex_number) {
    float real_part = complex_number[0];
    float complex_part = complex_number[1];
    return sqrt(real_part * real_part + complex_part*complex_part);
}

void fft_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    if (frameCount != 3306) {
        printf("Frame Count %d\n", frameCount);
        exit(-1);
    }

    uint32_t number_of_bins = frameCount / 2 + 1;
    uint32_t bins_size = ma_standard_sample_rate_44100 / frameCount;
    struct fft_params* params = (struct fft_params*) pDevice->pUserData;

    /* Execute the FFT calculation */
    memcpy(params->audioBuffer, pInput, frameCount * sizeof(float));
    fftwf_execute(params->plan);

    /* Calculate frequencies/magnitudes */
    struct frequency_and_magnitude* freqs = malloc(number_of_bins * sizeof(struct frequency_and_magnitude));
    for (int i = 0; i < number_of_bins; ++i) {
        freqs[i].frequency = i * bins_size;
        freqs[i].magnitude = complex_magnitude(params->fftBuffer[i]);
    }

    /* Sort the frequencies by their magnitudes */
    qsort(freqs, number_of_bins, sizeof(struct frequency_and_magnitude),
          (int (*)(const void *, const void *)) compare_magnitudes);

    /* Print the most significant frequencies */
    if (freqs[0].magnitude < 0.1) {
//        printf("Quiet\n");
    } else {
        for (int i = 0; i < 8; i++) {
            if (freqs[i].magnitude < 0.1) {
                break;
            }
            printf("%dHz(%.2f), ", (uint32_t) freqs[i].frequency, freqs[i].magnitude);
        }
        printf("\n");
    }

    free(freqs);
}

int poc() {
    int ret = -1;
    ma_result result = -1;
    ma_uint32 frameCount = 3306; // Adjust buffer size as needed

    /* Initialize fftw plan */
    struct fft_params params = { 0 };
    params.audioBuffer = (float*)malloc(frameCount * sizeof(float));
    params.fftBuffer = (fftwf_complex*)fftwf_malloc(frameCount * sizeof(fftwf_complex));
    params.plan = fftwf_plan_dft_r2c_1d(frameCount, params.audioBuffer, params.fftBuffer, FFTW_MEASURE);

    /* Configure miniaudio device config */
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format   = ma_format_f32;
    deviceConfig.capture.channels = 1;
    deviceConfig.sampleRate       = ma_standard_sample_rate_44100;
    deviceConfig.dataCallback     = fft_callback;
    deviceConfig.pUserData        = &params;

    /* Initialize miniaudio device */
    ma_device device;
    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize capture device.\n");
        return -2;
    }

    /* Start the miniaudio device */
    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        printf("Failed to start device.\n");
        return -3;
    }

    printf("Press Enter to stop recording...\n");
    getchar();

    ma_device_stop(&device);
    ma_device_uninit(&device);

    fftwf_destroy_plan(params.plan);
    fftwf_free(params.fftBuffer);
    free(params.audioBuffer);

    return 0;
}