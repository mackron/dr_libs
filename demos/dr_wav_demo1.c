
#define _CRT_SECURE_NO_WARNINGS

#define DR_WAV_IMPLEMENTATION
#include "../dr_wav.h"

#define DR_AUDIO_IMPLEMENTATION
#include "../dr_audio.h"

#include <stdio.h>
#include <conio.h>

int main(int argc, char** argv)
{
    //drwav* pWav = drwav_open_file("M1F1-uint8-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-float64-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-int32-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-int16-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-int24-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-Alaw-AFsp.wav");
    drwav* pWav = drwav_open_file("M1F1-mulaw-AFsp.wav");
    if (pWav == NULL) {
        return -1;
    }

    drwav_info info = drwav_get_info(pWav);

    unsigned int dataSize = info.sampleCount * sizeof(float);
    float* pData = malloc(dataSize);

    drwav_read_f32(pWav, info.sampleCount, pData);

    easyaudio_context* pContext = draudio_create_context();
    if (pContext == NULL) {
        return -2;
    }

    easyaudio_device* pDevice = draudio_create_output_device(pContext, 0);
    if (pDevice == NULL) {
        return -3;
    }

    draudio_buffer_desc bufferDesc;
    bufferDesc.format        = draudio_format_float;
    bufferDesc.channels      = info.channels;
    bufferDesc.sampleRate    = info.sampleRate;
    bufferDesc.bitsPerSample = sizeof(float)*8;
    bufferDesc.sizeInBytes   = dataSize;
    bufferDesc.pData         = pData;
    
    draudio_buffer* pBuffer = draudio_create_buffer(pDevice, &bufferDesc, 0);
    if (pBuffer == NULL) {
        return -4;
    }


    draudio_play(pBuffer, false);


    _getch();
    return 0;
}