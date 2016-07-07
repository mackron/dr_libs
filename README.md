# About
This repository is a container for single-file, public domain libraries. All libraries are
written in C, but they should be compilable by a C++ compiler.

Not every library is versioned which usually means it's APIs are unstable. Versioned libraries
have stable APIs, but on rare occasions I may need to make some minor changes.

Library                                         | Version | Description
----------------------------------------------- | ------- | -----------
[dr_flac](dr_flac.h)                            | 0.3d    | FLAC audio decoder.
[dr_wav](dr_wav.h)                              | 0.3a    | WAV audio loader.
[dr_pcx](dr_pcx.h)                              | 0.1     | PCX image loader.
[dr_path](dr_path.h)                            | 0.1b    | Path manipulation.
dr_obj                                          | -       | Wavefront OBJ model loader.
dr_fs                                           | -       | File system abstraction for loading files from the native file system and archives.
dr_fsw                                          | -       | Watch for changes to the file system. Windows only.
dr_gui                                          | -       | Low level GUI system.
dr_gl                                           | -       | OpenGL 2.1 API loader.
dr_wnd                                          | -       | Basic window management. Windows only at the moment.
dr_mtl                                          | -       | Loads material files and converts it to shader source (GLSL, etc.)
dr_math                                         | -       | Vector math library. Very incomplete, and only updated as I need it.
dr_audio (WIP)                                  | -       | Audio playback. Work in progress.
dr_2d (WIP)                                     | -       | 2D graphics rendering. Work in progress.
dr                                              | -       | Miscellaneous stuff that doesn't belong to any specific category.
