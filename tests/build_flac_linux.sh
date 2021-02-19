#!/bin/sh
gcc ./flac/dr_flac_test_0.c -o ./bin/dr_flac_test_0 -std=c89 -ansi -pedantic -O3 -s -Wall -ldl
gcc ./flac/dr_flac_decoding.c -o ./bin/dr_flac_decoding -std=c89 -ansi -pedantic -Wall -O3 -s -lFLAC -ldl
gcc ./flac/dr_flac_seeking.c -o ./bin/dr_flac_seeking -std=c89 -ansi -pedantic -Wall -O3 -s -lFLAC -ldl
