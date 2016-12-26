# About
This repository is a container for single-file, public domain libraries. All libraries are
written in C, but they should be compilable by a C++ compiler.

Not every library is versioned which usually means it's APIs are unstable. Versioned libraries
have stable APIs, but on rare occasions I may need to make some minor changes.

Library                                         | Version | Description
----------------------------------------------- | ------- | -----------
[dr_flac](dr_flac.h)                            | 0.4d    | FLAC audio decoder.
[dr_wav](dr_wav.h)                              | 0.5c    | WAV audio loader.
[dr_audio](dr_audio.h)                          | -       | Audio playback.
[dr_pcx](dr_pcx.h)                              | 0.2     | PCX image loader.
[dr_obj](dr_obj.h)                              | -       | Wavefront OBJ model loader.
[dr_fs](dr_fs.h)                                | -       | File system abstraction for loading files from the native file system and archives.
[dr_fsw](dr_fsw.h)                              | -       | Watch for changes to the file system. Windows only.
[dr_math](dr_math.h)                            | -       | Vector math library. Very incomplete, and only updated as I need it.
[dr](dr.h)                                      | -       | Miscellaneous stuff that doesn't belong to any specific category.
