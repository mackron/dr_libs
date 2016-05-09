# About
This repository is a container for single-file, public domain libraries written in C. Each
file in this library are independant of the others, except where noted. Click the links for
more detailed information.

All libraries are written in C, but they should be compilable by a C++ compiler. However, I
don't regularly work in C++ so there could be compiler errors every now and then. If you
come across one, feel free to submit an issue real quick and I'll get it sorted out.

Not every library is versioned. Usually this means it's pretty early in development and I'm
still experimenting with the API design and whatnot. Versioned libraries should have fairly
stable APIs, but they may change every now and then, albeit rarely.

Library                                         | Version | Description
----------------------------------------------- | ------- | -----------
[dr_flac](https://mackron.github.io/dr_flac)    | 0.2a    | FLAC audio decoder.
[dr_wav](https://mackron.github.io/dr_wav)      | 0.1a    | WAV audio loader.
[dr_pcx](https://mackron.github.io/dr_pcx)      | 0.1     | PCX image loader.
dr_obj                                          | -       | Wavefront OBJ model loader.
dr_fs                                           | -       | File system abstraction for loading files from the native file system and archives.
dr_fsw                                          | -       | Watch for changes to the file system. Windows only.
dr_path                                         | 0.1     | Path manipulation.
dr_gui                                          | -       | Low level GUI system.
dr_util                                         | -       | Miscellaneous stuff that doesn't belong to any specific category.
dr_mtl                                          | -       | Loads material files and converts it to shader source (GLSL, etc.)
dr_math                                         | -       | Vector math library. Very incomplete.
dr_audio (WIP)                                  | -       | Audio playback. Work in progress.
dr_2d (WIP)                                     | -       | 2D graphics rendering. Work in progress.
