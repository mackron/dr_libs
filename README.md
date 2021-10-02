<h4 align="center">Public domain, single file audio decoding libraries for C and C++.</h4>

<p align="center">
    <a href="https://discord.gg/9vpqbjU"><img src="https://img.shields.io/discord/712952679415939085?label=discord&logo=discord" alt="discord"></a>
    <a href="https://twitter.com/mackron"><img src="https://img.shields.io/twitter/follow/mackron?style=flat&label=twitter&color=1da1f2&logo=twitter" alt="twitter"></a>
</p>


Library                                         | Description
----------------------------------------------- | -----------
[dr_flac](dr_flac.h)                            | FLAC audio decoder.
[dr_mp3](dr_mp3.h)                              | MP3 audio decoder. Based off [minimp3](https://github.com/lieff/minimp3).
[dr_wav](dr_wav.h)                              | WAV audio loader and writer.

# dr_flac
## Introduction
dr_flac is no longer a single file library. To use it, just include the corresponding
header.

```c
#include <dr_libs/dr_flac.h>
```

You can then #include this file in other parts of the program as you would with
any other header file. To decode audio data, do something like the following:

```c
drflac* pFlac = drflac_open_file("MySong.flac", NULL);
if (pFlac == NULL) {
    // Failed to open FLAC file
}

drflac_int32* pSamples = malloc(pFlac->totalPCMFrameCount * pFlac->channels
* sizeof(drflac_int32)); drflac_uint64 numberOfInterleavedSamplesActuallyRead =
drflac_read_pcm_frames_s32(pFlac, pFlac->totalPCMFrameCount, pSamples);
```

The drflac object represents the decoder. It is a transparent type so all the
information you need, such as the number of channels and the bits per sample,
should be directly accessible - just make sure you don't change their values.
Samples are always output as interleaved signed 32-bit PCM. In the example above
a native FLAC stream was opened, however dr_flac has seamless support for Ogg
encapsulated FLAC streams as well.

You do not need to decode the entire stream in one go - you just specify how
many samples you'd like at any given time and the decoder will give you as many
samples as it can, up to the amount requested. Later on when you need the next
batch of samples, just call it again. Example:

```c
while (drflac_read_pcm_frames_s32(pFlac, chunkSizeInPCMFrames,
pChunkSamples) > 0) { do_something();
}
```

You can seek to a specific PCM frame with `drflac_seek_to_pcm_frame()`.

If you just want to quickly decode an entire FLAC file in one go you can do
something like this:

```c
unsigned int channels;
unsigned int sampleRate;
drflac_uint64 totalPCMFrameCount;
drflac_int32* pSampleData =
drflac_open_file_and_read_pcm_frames_s32("MySong.flac", &channels, &sampleRate,
&totalPCMFrameCount, NULL); if (pSampleData == NULL) {
    // Failed to open and decode FLAC file.
}

...

drflac_free(pSampleData, NULL);
```

You can read samples as signed 16-bit integer and 32-bit floating-point PCM with
the *_s16() and *_f32() family of APIs respectively, but note that these should
be considered lossy.


If you need access to metadata (album art, etc.), use
`drflac_open_with_metadata()`, `drflac_open_file_with_metdata()` or
`drflac_open_memory_with_metadata()`. The rationale for keeping these APIs
separate is that they're slightly slower than the normal versions and also just
a little bit harder to use. dr_flac reports metadata to the application through
the use of a callback, and every metadata block is reported before
`drflac_open_with_metdata()` returns.

The main opening APIs (`drflac_open()`, etc.) will fail if the header is not
present. The presents a problem in certain scenarios such as broadcast style
streams or internet radio where the header may not be present because the user
has started playback mid-stream. To handle this, use the relaxed APIs:

    `drflac_open_relaxed()`
    `drflac_open_with_metadata_relaxed()`

It is not recommended to use these APIs for file based streams because a missing
header would usually indicate a corrupt or perverse file. In addition, these
APIs can take a long time to initialize because they may need to spend a lot of
time finding the first frame.



## Build Options
#define these options before including this file.

#define DR_FLAC_NO_STDIO
  Disable `drflac_open_file()` and family.

#define DR_FLAC_NO_OGG
  Disables support for Ogg/FLAC streams.

#define DR_FLAC_BUFFER_SIZE <number>
  Defines the size of the internal buffer to store data from onRead(). This
buffer is used to reduce the number of calls back to the client for more data.
  Larger values means more memory, but better performance. My tests show
diminishing returns after about 4KB (which is the default). Consider reducing
this if you have a very efficient implementation of onRead(), or increase it if
it's very inefficient. Must be a multiple of 8.

#define DR_FLAC_NO_CRC
  Disables CRC checks. This will offer a performance boost when CRC is
unnecessary. This will disable binary search seeking. When seeking, the seek
table will be used if available. Otherwise the seek will be performed using
brute force.

#define DR_FLAC_NO_SIMD
  Disables SIMD optimizations (SSE on x86/x64 architectures, NEON on ARM
architectures). Use this if you are having compatibility issues with your
compiler.



## Notes
- dr_flac does not support changing the sample rate nor channel count mid
stream.
- dr_flac is not thread-safe, but its APIs can be called from any thread so long
as you do your own synchronization.
- When using Ogg encapsulation, a corrupted metadata block will result in
`drflac_open_with_metadata()` and `drflac_open()` returning inconsistent samples
due to differences in corrupted stream recorvery logic between the two APIs.

# dr_mp3
## Introduction
dr_mp3 is no longer a single file library. To use it, just include the corresponding
header.

```c
#include <dr_libs/dr_mp3.h>
```

To decode audio data, do something like the following:

```c
drmp3 mp3;
if (!drmp3_init_file(&mp3, "MySong.mp3", NULL)) {
    // Failed to open file
}

...

drmp3_uint64 framesRead = drmp3_read_pcm_frames_f32(pMP3, framesToRead, pFrames);
```

The drmp3 object is transparent so you can get access to the channel count and
sample rate like so:

```c
drmp3_uint32 channels = mp3.channels;
drmp3_uint32 sampleRate = mp3.sampleRate;
```

The example above initializes a decoder from a file, but you can also initialize
it from a block of memory and read and seek callbacks with `drmp3_init_memory()`
and `drmp3_init()` respectively.

You do not need to do any annoying memory management when reading PCM frames -
this is all managed internally. You can request any number of PCM frames in each
call to `drmp3_read_pcm_frames_f32()` and it will return as many PCM frames as
it can, up to the requested amount.

You can also decode an entire file in one go with
`drmp3_open_and_read_pcm_frames_f32()`,
`drmp3_open_memory_and_read_pcm_frames_f32()` and
`drmp3_open_file_and_read_pcm_frames_f32()`.


## Build Options
#define these options before including this file.

#define DR_MP3_NO_STDIO
  Disable drmp3_init_file(), etc.

#define DR_MP3_NO_SIMD
  Disable SIMD optimizations.

# dr_wav
## Introduction
This is no longer a single file library. To use it, just include the corresponding
header.

```c
#include <dr_libs/dr_wav.h>
```

You can then #include this file in other parts of the program as you would with
any other header file. Do something like the following to read audio data:

```c
drwav wav;
if (!drwav_init_file(&wav, "my_song.wav", NULL)) {
    // Error opening WAV file.
}

drwav_int32* pDecodedInterleavedPCMFrames = malloc(wav.totalPCMFrameCount *
wav.channels * sizeof(drwav_int32)); size_t numberOfSamplesActuallyDecoded =
drwav_read_pcm_frames_s32(&wav, wav.totalPCMFrameCount,
pDecodedInterleavedPCMFrames);

...

drwav_uninit(&wav);
```

If you just want to quickly open and read the audio data in a single operation
you can do something like this:

```c
unsigned int channels;
unsigned int sampleRate;
drwav_uint64 totalPCMFrameCount;
float* pSampleData = drwav_open_file_and_read_pcm_frames_f32("my_song.wav",
&channels, &sampleRate, &totalPCMFrameCount, NULL); if (pSampleData == NULL) {
    // Error opening and reading WAV file.
}

...

drwav_free(pSampleData, NULL);
```

The examples above use versions of the API that convert the audio data to a
consistent format (32-bit signed PCM, in this case), but you can still output
the audio data in its internal format (see notes below for supported formats):

```c
size_t framesRead = drwav_read_pcm_frames(&wav, wav.totalPCMFrameCount,
pDecodedInterleavedPCMFrames);
```

You can also read the raw bytes of audio data, which could be useful if dr_wav
does not have native support for a particular data format:

```c
size_t bytesRead = drwav_read_raw(&wav, bytesToRead, pRawDataBuffer);
```

dr_wav can also be used to output WAV files. This does not currently support
compressed formats. To use this, look at `drwav_init_write()`,
`drwav_init_file_write()`, etc. Use `drwav_write_pcm_frames()` to write samples,
or `drwav_write_raw()` to write raw data in the "data" chunk.

```c
drwav_data_format format;
format.container = drwav_container_riff;     // <-- drwav_container_riff =
normal WAV files, drwav_container_w64 = Sony Wave64. format.format =
DR_WAVE_FORMAT_PCM;          // <-- Any of the DR_WAVE_FORMAT_* codes.
format.channels = 2;
format.sampleRate = 44100;
format.bitsPerSample = 16;
drwav_init_file_write(&wav, "data/recording.wav", &format, NULL);

...

drwav_uint64 framesWritten = drwav_write_pcm_frames(pWav, frameCount,
pSamples);
```

dr_wav has seamless support the Sony Wave64 format. The decoder will
automatically detect it and it should Just Work without any manual intervention.


## Build Options
#define these options before including this file.

#define DR_WAV_NO_CONVERSION_API
  Disables conversion APIs such as `drwav_read_pcm_frames_f32()` and
`drwav_s16_to_f32()`.

#define DR_WAV_NO_STDIO
  Disables APIs that initialize a decoder from a file such as
`drwav_init_file()`, `drwav_init_file_write()`, etc.



## Notes
- Samples are always interleaved.
- The default read function does not do any data conversion. Use
`drwav_read_pcm_frames_f32()`, `drwav_read_pcm_frames_s32()` and
`drwav_read_pcm_frames_s16()` to read and convert audio data to 32-bit floating
point, signed 32-bit integer and signed 16-bit integer samples respectively.
Tested and supported internal formats include the following:
  - Unsigned 8-bit PCM
  - Signed 12-bit PCM
  - Signed 16-bit PCM
  - Signed 24-bit PCM
  - Signed 32-bit PCM
  - IEEE 32-bit floating point
  - IEEE 64-bit floating point
  - A-law and u-law
  - Microsoft ADPCM
  - IMA ADPCM (DVI, format code 0x11)
- dr_wav will try to read the WAV file as best it can, even if it's not strictly
conformant to the WAV format.


# Other Libraries
Below are some of my other libraries you may be interested in.

Library                                           | Description
------------------------------------------------- | -----------
[miniaudio](https://github.com/mackron/miniaudio) | A public domain, single file library for audio playback and recording.
