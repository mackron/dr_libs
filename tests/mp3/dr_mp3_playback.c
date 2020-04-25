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

int do_decoding_validation(const char* pFilePath)
{
    int result = 0;
    drmp3 mp3Memory;
    drmp3 mp3File;
    size_t dataSize;
    void* pData;

    /*
    When opening from a memory buffer, dr_mp3 will take a different path for decoding which is optimized to reduce data movement. Since it's
    running on a separate path, we need to ensure it's returning consistent results with the other code path which will be used when decoding
    from a file.
    */
    
    /* Initialize the memory decoder. */
    pData = dr_open_and_read_file(pFilePath, &dataSize);
    if (pData == NULL) {
        printf("Failed to open file \"%s\"", pFilePath);
        return -1;
    }

    if (!drmp3_init_memory(&mp3Memory, pData, dataSize, NULL)) {
        free(pData);
        printf("Failed to init MP3 decoder \"%s\"", pFilePath);
        return -1;
    }


    /* Initialize the file decoder. */
    if (!drmp3_init_file(&mp3File, pFilePath, NULL)) {
        drmp3_uninit(&mp3Memory);
        free(pData);
        printf("Failed to open file \"%s\"", pFilePath);
        return -1;
    }

    DRMP3_ASSERT(mp3Memory.channels == mp3File.channels);


    /* Compare. */
    for (;;) {
        drmp3_uint64 iSample;
        drmp3_uint64 pcmFrameCountMemory;
        drmp3_uint64 pcmFrameCountFile;
        drmp3_int16 pcmFramesMemory[4096];
        drmp3_int16 pcmFramesFile[4096];

        pcmFrameCountMemory = drmp3_read_pcm_frames_s16(&mp3Memory, DRMP3_COUNTOF(pcmFramesMemory) / mp3Memory.channels, pcmFramesMemory);
        pcmFrameCountFile   = drmp3_read_pcm_frames_s16(&mp3File,   DRMP3_COUNTOF(pcmFramesFile)   / mp3File.channels,   pcmFramesFile);

        /* Check the frame count first. */
        if (pcmFrameCountMemory != pcmFrameCountFile) {
            printf("Frame counts differ: memory = %d; file = %d\n", (int)pcmFrameCountMemory, (int)pcmFrameCountFile);
            result = -1;
            break;
        }

        /* Check individual frames. */
        DRMP3_ASSERT(pcmFrameCountMemory == pcmFrameCountFile);
        for (iSample = 0; iSample < pcmFrameCountMemory * mp3Memory.channels; iSample += 1) {
            if (pcmFramesMemory[iSample] != pcmFramesFile[iSample]) {
                printf("Samples differ: memory = %d; file = %d\n", (int)pcmFramesMemory[iSample], (int)pcmFramesFile[iSample]);
                result = -1;
                break;
            }
        }

        /* We've reached the end if we didn't return any PCM frames. */
        if (pcmFrameCountMemory == 0 || pcmFrameCountFile == 0) {
            break;
        }
    }


    drmp3_uninit(&mp3File);
    drmp3_uninit(&mp3Memory);
    free(pData);
    return result;
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

    /* Quick validation test first. */
    if (do_decoding_validation(pInputFilePath) != 0) {
        return -1;
    }


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