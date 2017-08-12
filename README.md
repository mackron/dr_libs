# About
This repository is a container for some of my single-file libraries. All libraries are written
in C, but they should be compilable by a C++ compiler.

Not every library is versioned which usually means it's APIs are unstable. Versioned libraries
have stable APIs, but on rare occasions I may need to make some minor changes.

Library                                         | Version | Description
----------------------------------------------- | ------- | -----------
[dr_flac](dr_flac.h)                            | 0.8     | FLAC audio decoder.
[dr_wav](dr_wav.h)                              | 0.5g    | WAV audio loader.
[dr_audio](dr_audio.h)                          | -       | Audio playback.
[dr_pcx](dr_pcx.h)                              | 0.2a    | PCX image loader.
[dr_obj](dr_obj.h)                              | -       | Wavefront OBJ model loader.
[dr_fs](dr_fs.h)                                | -       | File system abstraction for loading files from the native file system and archives.
[dr_fsw](dr_fsw.h)                              | -       | Watch for changes to the file system. Windows only.
[dr_math](dr_math.h)                            | -       | Vector math library. Very incomplete, and only updated as I need it.
[dr](dr.h)                                      | -       | Miscellaneous stuff that doesn't belong to any specific category.


# Other Libraries
Below are some of my other libraries you may be interested in. These are all located in their
own repositories just for ease of maintenance.

## [mini_al](https://github.com/dr-soft/mini_al)
A public domain, single file library for audio playback and recording. This is replacing dr_audio.

## [Mintaro](https://github.com/dr-soft/mintaro)
A small framework for making simple games. Public domain, single file.