#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_IMPLEMENTATION
#include "../../../miniaudio/miniaudio.h"

#define DR_WAV_IMPLEMENTATION
#include "../../dr_wav.h"
#include "../common/dr_common.c"

drwav g_wav;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    /* Assuming format is always s16 for now. */
    if (pDevice->playback.format == ma_format_s16) {
        drwav_read_pcm_frames_s16(&g_wav, frameCount, pOutput);
    } else if (pDevice->playback.format == ma_format_f32) {
        drwav_read_pcm_frames_f32(&g_wav, frameCount, pOutput);
    } else {
        /* Unsupported format. */
    }

    (void)pInput;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_device device;
    ma_device_config deviceConfig;

    if (argc < 2) {
        printf("No input file specified.");
        return -1;
    }

    if (!drwav_init_file(&g_wav, argv[1], NULL)) {
        printf("Failed to load file: %s", argv[1]);
        return -1;
    }

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_s16;
    deviceConfig.playback.channels = g_wav.channels;
    deviceConfig.sampleRate        = g_wav.sampleRate;
    deviceConfig.dataCallback      = data_callback;

    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize playback device.");
        drwav_uninit(&g_wav);
        return -1;
    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        printf("Failed to start playback device.");
        ma_device_uninit(&device);
        drwav_uninit(&g_wav);
        return -1;
    }

    printf("Press Enter to quit...");
    getchar();

    ma_device_uninit(&device);
    drwav_uninit(&g_wav);

    return 0;
}