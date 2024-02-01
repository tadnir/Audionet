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


/** Play Multiple Sounds ******************************************************************************/
#define CHANNEL_COUNT 2
#define NUM_WAVES 3
ma_uint32 read_and_mix_pcm_frames_f32(ma_waveform* pWaveform, float* pOutputF32, ma_uint32 frameCount)
{
//    return ma_waveform_read_pcm_frames(pWaveform, pOutputF32, frameCount, NULL);
    /*
    The way mixing works is that we just read into a temporary buffer, then take the contents of that buffer and mix it with the
    contents of the output buffer by simply adding the samples together. You could also clip the samples to -1..+1, but I'm not
    doing that in this example.
    */
    ma_result result;
    float temp[4096];
    ma_uint32 tempCapInFrames = (sizeof(temp) / sizeof(temp[0])) / CHANNEL_COUNT;
    ma_uint32 totalFramesRead = 0;

    while (totalFramesRead < frameCount) {
        ma_uint64 iSample;
        ma_uint64 framesReadThisIteration;
        ma_uint32 totalFramesRemaining = frameCount - totalFramesRead;
        ma_uint32 framesToReadThisIteration = tempCapInFrames;
        if (framesToReadThisIteration > totalFramesRemaining) {
            framesToReadThisIteration = totalFramesRemaining;
        }

        result = ma_waveform_read_pcm_frames(pWaveform, temp, framesToReadThisIteration, &framesReadThisIteration);
        if (result != MA_SUCCESS || framesReadThisIteration == 0) {
            break;
        }

        /* Mix the frames together. */
        for (iSample = 0; iSample < framesReadThisIteration*CHANNEL_COUNT; ++iSample) {
            pOutputF32[totalFramesRead*CHANNEL_COUNT + iSample] += temp[iSample];
        }

        totalFramesRead += (ma_uint32)framesReadThisIteration;

        if (framesReadThisIteration < (ma_uint32)framesToReadThisIteration) {
            break;  /* Reached EOF. */
        }
    }

    return totalFramesRead;
}

void multi_play_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_waveform* pSineWave;

    assert(pDevice->playback.channels == DEVICE_CHANNELS);

    pSineWave = (ma_waveform*)pDevice->pUserData;
    assert(pSineWave != NULL);

    float* pfOutput = pOutput;
    float* temp = malloc(frameCount * sizeof(float)*CHANNEL_COUNT);
    if (temp == NULL) {
        printf("ERR\n");
    }
    memset(pOutput, 0, frameCount * sizeof(float)*CHANNEL_COUNT);

    for (int i = 0; i < NUM_WAVES; ++i) {
        ma_waveform_read_pcm_frames(&pSineWave[i], temp, frameCount, NULL);
        for (int j = 0; j < frameCount*sizeof(float); j++) {
            pfOutput[j] += temp[j]/NUM_WAVES;
        }
//        read_and_mix_pcm_frames_f32(&pSineWave[i], pOutput, frameCount);
    }

    free(temp);

    (void)pInput;   /* Unused. */
}

//int play_multi_sine() {
int poc() {
    ma_waveform sineWave[NUM_WAVES];
    ma_device_config deviceConfig;
    ma_device device;


    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = DEVICE_FORMAT;
    deviceConfig.playback.channels = CHANNEL_COUNT;
    deviceConfig.sampleRate        = ma_standard_sample_rate_44100;
    deviceConfig.dataCallback      = multi_play_callback;
    deviceConfig.pUserData         = sineWave;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return -4;
    }

    printf("Device Name: %s\n", device.playback.name);

    ma_waveform_config sineWaveConfig1 = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, 0.8, 100);
    ma_waveform_config sineWaveConfig2 = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, 0.8, 500);
    ma_waveform_config sineWaveConfig3 = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, 0.8, 900);
    ma_waveform_init(&sineWaveConfig1, &sineWave[0]);
    ma_waveform_init(&sineWaveConfig2, &sineWave[1]);
    ma_waveform_init(&sineWaveConfig3, &sineWave[2]);

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
#define MAX_FREQUENCIES 8
    if (freqs[0].magnitude > 0.1) {
        for (int i = 0; i < MAX_FREQUENCIES; i++) {
            if (freqs[i].magnitude < 0.1) {
                break;
            }
            printf("%dHz(%.2f), ", (uint32_t) freqs[i].frequency, freqs[i].magnitude);
        }
        printf("\n");
    }

    free(freqs);
}

int fft_poc() {
    int ret = -1;
    ma_result result = -1;
    ma_uint32 frameCount = 3306; // Adjust buffer size as needed

    /* Initialize fftw plan */
    struct fft_params params;
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