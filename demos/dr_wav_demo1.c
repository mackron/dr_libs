
#define _CRT_SECURE_NO_WARNINGS

#define DR_WAV_IMPLEMENTATION
#include "../dr_wav.h"

#define DR_AUDIO_IMPLEMENTATION
#include "../dr_audio.h"

#include <stdio.h>
#include <conio.h>

int main(int argc, char** argv)
{
    drwav* pWav = drwav_open_file("M1F1-uint8-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-uint8WE-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-int12-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-int12WE-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-int16-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-int16WE-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-int24-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-int24WE-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-int32-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-int32WE-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-float32-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-float32WE-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-float64-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-float64WE-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-Alaw-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-AlawWE-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-mulaw-AFsp.wav");
    //drwav* pWav = drwav_open_file("M1F1-mulawWE-AFsp.wav");
    //drwav* pWav = drwav_open_file("stereol.wav");
    //drwav* pWav = drwav_open_file("stereofl.wav");
    //drwav* pWav = drwav_open_file("drmapan.wav");
    
    //drwav* pWav = drwav_open_file("Utopia Critical Stop.WAV");
    //drwav* pWav = drwav_open_file("GLASS.WAV");
    //drwav* pWav = drwav_open_file("Ptjunk.wav");
    //drwav* pWav = drwav_open_file("Pmiscck.wav"); 
    if (pWav == NULL) {
        return -1;
    }

    drwav_info info = drwav_get_info(pWav);

    unsigned int dataSize = info.sampleCount * sizeof(float);
    float* pData = malloc(dataSize);

    // This horribly ineffcient loop is just to test reading of audio files where the bits per sample do
    // not cleanly fit within a byte (for example 12 bits per sample).
    /*float* pRunningData = pData;
    while (drwav_read_f32(pWav, 1, pRunningData) > 0) {
        pRunningData += 1;
    }*/
    drwav_read_f32(pWav, info.sampleCount, pData);
    

    draudio_context* pContext = draudio_create_context();
    if (pContext == NULL) {
        return -2;
    }

    draudio_device* pDevice = draudio_create_output_device(pContext, 0);
    if (pDevice == NULL) {
        return -3;
    }

    draudio_buffer_desc bufferDesc;
    memset(&bufferDesc, 0, sizeof(&bufferDesc));
    bufferDesc.format        = draudio_format_float;
    bufferDesc.channels      = info.fmt.channels;
    bufferDesc.sampleRate    = info.fmt.sampleRate;
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