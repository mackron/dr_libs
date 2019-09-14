@echo off
SET c_compiler=gcc
SET cpp_compiler=g++
SET options=-Wall -Wpedantic -std=c89 -ansi -pedantic -O2 -s

:: Configure the "arch" option to test different instruction sets.
SET arch=
::SET arch=-msse4.1
::SET arch=-mfpu=neon
@echo on

%c_compiler% ./flac/dr_flac_test_0.c -o ./bin/dr_flac_test_0.exe %options% %arch%