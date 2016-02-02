// Public domain. See "unlicense" statement at the end of this file.

//
// QUICK NOTES
//
//
//

//
// OPTIONS
//
//
//

#ifndef dr_flac_h
#define dr_flac_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    /// The number of channels making up the audio data. When this is set to 1 it is mono, 2 is stereo, etc.
    unsigned int channels;

    /// The sample rate. Usually set to something like 44100.
    unsigned int sampleRate;

    /// The number of bits per sample.
    unsigned int bitsPerSample;

    /// The total number of samples making up the audio data. Use <sampleCount> * (<bitsPerSample> / 8) to
    /// calculate the required size of a buffer to hold the entire audio data.
    unsigned int sampleCount;

} drflac_info;

typedef struct drflac drflac;

/// Callback for when data is read. Return value is the number of bytes actually read.
typedef size_t (* drflac_read_proc)(void* userData, void* bufferOut, size_t bytesToRead);

/// Callback for when data needs to be seeked. Offset is always relative to the current position. Return value
/// is 0 on failure, non-zero success.
typedef int (* drflac_seek_proc)(void* userData, int offset);


/// Opens a flac decoder.
drflac* drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, void* userData);

/// Closes the given flac decoder.
void drflac_close(drflac* flac);

/// Retrieves information about the given flac decoder.
drflac_info drflac_get_info(drflac* flac);




#ifdef __cplusplus
}
#endif

#endif

/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/
