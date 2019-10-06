:: NOTES
::
:: These tests use libsndfile as a benchmark. Since Windows doesn't have good standard paths for library files, this
:: script will use "wav/include" as an additional search path for headers. You will need to place sndfile.h to the
:: "wav/include" directory. Tests will link to libsndfile dynamically at run-time. On the Windows build you'll just
:: need to put a copy of the 32- and 64-bit versions of libsndfile-1.dll into the "bin" directory, with the names
:: libsndfile-1-x86.dll and libsndfile-1-x64.dll respectively. Both versions are required so that both the 32- and
:: 64-bit builds can be tested and benchmarked.

@echo off

SET c_compiler=gcc
SET cpp_compiler=g++

:: Configure the "arch" option to test different instruction sets.
SET arch=
SET arch=-msse4.1
::SET arch=-mfpu=neon

:: libsndfile is required for benchmarking.
SET libsndfile=-I./wav/include

:: C options
SET c_options=-std=c89 -ansi

:: C++ options
SET cpp_options=

SET options=-Wall -Wpedantic -pedantic -O3 -s -DNDEBUG %arch% %libsndfile%

SET buildc=%c_compiler% %c_options% %options%
SET buildcpp=%cpp_compiler% %cpp_options% %options%
@echo on

%buildc% ./wav/dr_wav_test_0.c -o ./bin/dr_wav_test_0.exe
%buildcpp% ./wav/dr_wav_test_0.cpp -o ./bin/dr_wav_test_0_cpp.exe
%buildc% ./wav/dr_wav_decoding.c -o ./bin/dr_wav_decoding.exe
%buildcpp% ./wav/dr_wav_decoding.cpp -o ./bin/dr_wav_decoding_cpp.exe