:: NOTES
::
:: These tests use libFLAC as a benchmark. Since Windows doesn't have good standard paths for library files, this
:: script will use "flac/include" as an additional search path for headers and "flac/lib/win32" as an additional
:: search path for libraries. You will need to place FLAC headers in the "flac/include/FLAC" directory and libogg
:: headers int the "flac/include/ogg" directory.
::
:: Examples are linked against "-lFLAC" and "-logg". These need to be placed in a standard directory or "flac/lib/win32".

@echo off
SET c_compiler=gcc
SET cpp_compiler=g++

:: Configure the "arch" option to test different instruction sets.
SET arch=
SET arch=-msse4.1
::SET arch=-mfpu=neon

:: libFLAC and libogg are required for benchmarking.
SET libFLAC=-I./flac/include -L./flac/lib/win32 -lFLAC -logg

:: C options
SET c_options=-std=c89 -ansi

:: C++ options
SET cpp_options=

SET options=-Wall -Wpedantic -pedantic -O3 -s -DNDEBUG %arch% %libFLAC%

SET buildc=%c_compiler% %c_options%
SET buildcpp=%cpp_compiler% %cpp_options%
@echo on

%buildc% ./flac/dr_flac_test_0.c -o ./bin/dr_flac_test_0.exe %options%
%buildcpp% ./flac/dr_flac_test_0.cpp -o ./bin/dr_flac_test_0_cpp.exe %options%
%buildc% ./flac/dr_flac_decoding.c -o ./bin/dr_flac_decoding.exe %options%
%buildcpp% ./flac/dr_flac_decoding.cpp -o ./bin/dr_flac_decoding_cpp.exe %options%