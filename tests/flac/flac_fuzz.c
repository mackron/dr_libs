/*
 * Fuzz tester for dr_flac.h
 *
 * compile with
 * clang -g -O1 -fsanitize=fuzzer,address -o fuzz_dr_flac fuzz_dr_flac.c
 *
 * and run ./fuzz_dr_flac to run fuzz testing
 *
 * Other sanitizers are possible, for example
 * -fsanitize=fuzzer,memory
 * -fsanitize=fuzzer,undefined
 *
 * For more options, run ./fuzz_dr_flac -help=1
 *
 * If a problem is found, the problematic input is saved and can be
 * rerun (with for example a debugger) with
 *
 * ./fuzz_dr_flac file
 *
 */

#include <stdint.h>

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_CRC
#define DR_FLAC_NO_STDIO
#include "../../dr_flac.h"

#define MIN(a,b) (((a)<(b))?(a):(b))

uint8_t fuzz_flacstream[4096] = {0};
size_t fuzz_flacstream_position;
size_t fuzz_flacstream_length;

static size_t read_fuzz_flacstream(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    size_t readsize = MIN(bytesToRead, fuzz_flacstream_length-fuzz_flacstream_position);
    if (readsize > 0) {
        memcpy(bufferOut, fuzz_flacstream+fuzz_flacstream_position, readsize);
        fuzz_flacstream_position += readsize;
        return readsize;
    } else {
        return 0;
    }
}

static drflac_bool32 seek_fuzz_flacstream(void* pUserData, int offset, drflac_seek_origin origin)
{
    if ((int)fuzz_flacstream_position+offset < 0 || (int)fuzz_flacstream_position+offset > fuzz_flacstream_length) {
        return 1;
    } else {
        fuzz_flacstream_position = (int)fuzz_flacstream_position+offset;
        return 0;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 2) {
        drflac * drflac_fuzzer;
        drflac_int32 drflac_fuzzer_out[2048] = {0}; /* 256 samples over 8 channels */
        drflac_container container = data[0] & 1 ? drflac_container_native : drflac_container_ogg;

        memcpy(fuzz_flacstream, data, (size-1)<4096?(size-1):4096);

        fuzz_flacstream_position = 0;
        fuzz_flacstream_length = size-1;

        drflac_fuzzer = drflac_open_relaxed(read_fuzz_flacstream, seek_fuzz_flacstream, container, NULL, NULL);

        while (drflac_read_pcm_frames_s32(drflac_fuzzer, 256, drflac_fuzzer_out));

        drflac_close(drflac_fuzzer);
    }
    return 0;
}
