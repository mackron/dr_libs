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


# Other Libraries
Below are some of my other libraries you may be interested in.

Library                                           | Description
------------------------------------------------- | -----------
[miniaudio](https://github.com/mackron/miniaudio) | A public domain, single file library for audio playback and recording.
