#include "dr_mp3_common.c"

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "../external/miniaudio/miniaudio.h"

void data_callback(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    drmp3* pMP3;

    pMP3 = (drmp3*)pDevice->pUserData;
    DRMP3_ASSERT(pMP3 != NULL);

    if (pDevice->playback.format == ma_format_f32) {
        drmp3_read_pcm_frames_f32(pMP3, frameCount, pFramesOut);
    } else if (pDevice->playback.format == ma_format_s16) {
        drmp3_read_pcm_frames_s16(pMP3, frameCount, pFramesOut);
    } else {
        DRMP3_ASSERT(DRMP3_FALSE);  /* Should never get here. */
    }

    (void)pFramesIn;
}

int main(int argc, char** argv)
{
    drmp3 mp3;
    ma_result resultMA;
    ma_device_config deviceConfig;
    ma_device device;
    const char* pInputFilePath;

    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    pInputFilePath = argv[1];

    if (!drmp3_init_file(&mp3, pInputFilePath, NULL)) {
        printf("Failed to open file \"%s\"", pInputFilePath);
        return -1;
    }

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_s16;
    deviceConfig.playback.channels = mp3.channels;
    deviceConfig.sampleRate        = mp3.sampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &mp3;
    resultMA = ma_device_init(NULL, &deviceConfig, &device);
    if (resultMA != MA_SUCCESS) {
        drmp3_uninit(&mp3);
        printf("Failed to initialize playback device: %s.\n", ma_result_description(resultMA));
        return -1;
    }

    resultMA = ma_device_start(&device);
    if (resultMA != MA_SUCCESS) {
        ma_device_uninit(&device);
        drmp3_uninit(&mp3);
        printf("Failed to start playback device: %s.\n", ma_result_description(resultMA));
        return -1;
    }

    printf("Press Enter to quit...");
    getchar();

    /* We're done. */
    ma_device_uninit(&device);
    drmp3_uninit(&mp3);
    
    return 0;
}