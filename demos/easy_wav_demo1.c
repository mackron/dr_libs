
#define _CRT_SECURE_NO_WARNINGS

#define EASY_WAV_IMPLEMENTATION
#include "../easy_wav.h"
#include "../easy_audio.h"

#include <stdio.h>
#include <conio.h>

int main(int argc, char** argv)
{
    //easywav* pWav = easywav_open_file("M1F1-uint8-AFsp.wav");
    //easywav* pWav = easywav_open_file("M1F1-float64-AFsp.wav");
    //easywav* pWav = easywav_open_file("M1F1-int32-AFsp.wav");
    //easywav* pWav = easywav_open_file("M1F1-int16-AFsp.wav");
    //easywav* pWav = easywav_open_file("M1F1-int24-AFsp.wav");
    //easywav* pWav = easywav_open_file("M1F1-Alaw-AFsp.wav");
    easywav* pWav = easywav_open_file("M1F1-mulaw-AFsp.wav");
    if (pWav == NULL) {
        return -1;
    }

    easywav_info info = easywav_get_info(pWav);

    unsigned int dataSize = info.sampleCount * sizeof(float);
    float* pData = malloc(dataSize);

    easywav_read_f32(pWav, info.sampleCount, pData);

    easyaudio_context* pContext = easyaudio_create_context();
    if (pContext == NULL) {
        return -2;
    }

    easyaudio_device* pDevice = easyaudio_create_output_device(pContext, 0);
    if (pDevice == NULL) {
        return -3;
    }

    easyaudio_buffer_desc bufferDesc;
    bufferDesc.format        = easyaudio_format_float;
    bufferDesc.channels      = info.channels;
    bufferDesc.sampleRate    = info.sampleRate;
    bufferDesc.bitsPerSample = sizeof(float)*8;
    bufferDesc.sizeInBytes   = dataSize;
    bufferDesc.pData         = pData;
    
    easyaudio_buffer* pBuffer = easyaudio_create_buffer(pDevice, &bufferDesc, 0);
    if (pBuffer == NULL) {
        return -4;
    }


    easyaudio_play(pBuffer, false);


    _getch();
    return 0;
}