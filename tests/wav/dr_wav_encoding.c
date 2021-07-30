#define DR_WAV_IMPLEMENTATION
#include "../../dr_wav.h"
#include <math.h>

void generate_sine_wave(float* pOutput, drwav_uint64 frameCount, drwav_uint32 channels, drwav_uint32 sampleRate, float frequency, float* t)
{
    float x = *t;
    float a = 1.0f / sampleRate;
    drwav_uint64 iFrame;
    drwav_uint32 iChannel;
    
    for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
        float s = (float)(sin(3.1415965*2 * x * frequency) * 0.25);
        for (iChannel = 0; iChannel < channels; iChannel += 1) {
            pOutput[iFrame*channels + iChannel] = s;
        }

        x += a;
    }

    *t = x;
}

int main(int argc, char** argv)
{
    drwav_data_format format;
    drwav wav;
    float t = 0;
    float tempFrames[4096];
    drwav_uint64 totalFramesToWrite;
    drwav_uint64 totalFramesWritten = 0;
    

    if (argc < 2) {
        printf("No output file specified.\n");
        return -1;
    }

    format.container     = drwav_container_riff;
    format.format        = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels      = 2;
    format.sampleRate    = 44100;
    format.bitsPerSample = 32;
    if (!drwav_init_file_write(&wav, argv[1], &format, NULL)) {
        printf("Failed to open file.\n");
        return -1;
    }

    totalFramesToWrite = format.sampleRate * 1;
    totalFramesWritten = 0;

    while (totalFramesToWrite > totalFramesWritten) {
        drwav_uint64 framesRemaining = totalFramesToWrite - totalFramesWritten;
        drwav_uint64 framesToWriteNow = drwav_countof(tempFrames) / format.channels;
        if (framesToWriteNow > framesRemaining) {
            framesToWriteNow = framesRemaining;
        }

        generate_sine_wave(tempFrames, framesToWriteNow, format.channels, format.sampleRate, 400, &t);
        drwav_write_pcm_frames(&wav, framesToWriteNow, tempFrames);

        totalFramesWritten += framesToWriteNow;
    }

    drwav_uninit(&wav);

    return 0;
}