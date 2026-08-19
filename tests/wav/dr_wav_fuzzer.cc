/*
 * libFuzzer harness for dr_wav.h
 *
 * compile with
 * clang++ -g -O1 -std=c++11 -fsanitize=fuzzer,address -o dr_wav_fuzzer tests/wav/dr_wav_fuzzer.cc
 *
 * and run ./dr_wav_fuzzer to run fuzz testing. For more options, run ./dr_wav_fuzzer -help=1
 */
#include <cstdint>

#define DR_WAV_IMPLEMENTATION
#include "../../dr_wav.h"

namespace {
constexpr drwav_uint64 kMaxFramesPerRead = 4096;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    drwav wav;
    if (!drwav_init_memory(&wav, data, size, nullptr)) {
        return 0;
    }

    static drwav_int16 buffer[kMaxFramesPerRead * DRWAV_MAX_CHANNELS];
    drwav_uint64 framesRead;
    do {
        framesRead = drwav_read_pcm_frames_s16(&wav, kMaxFramesPerRead, buffer);
    } while (framesRead == kMaxFramesPerRead);

    drwav_uninit(&wav);
    return 0;
}
