/*
 * libFuzzer harness for dr_mp3.h
 *
 * compile with
 * clang++ -g -O1 -std=c++11 -fsanitize=fuzzer,address -o dr_mp3_fuzzer tests/mp3/dr_mp3_fuzzer.cc
 *
 * and run ./dr_mp3_fuzzer to run fuzz testing. For more options, run ./dr_mp3_fuzzer -help=1
 */
#include <cstdint>

#define DR_MP3_IMPLEMENTATION
#include "../../dr_mp3.h"

namespace {
constexpr drmp3_uint64 kMaxFramesPerRead = 4096;
constexpr int kMaxChannels = 2; /* MP3 spec limit: mono or stereo */
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    drmp3 mp3;
    if (!drmp3_init_memory(&mp3, data, size, nullptr)) {
        return 0;
    }

    static float buffer[kMaxFramesPerRead * kMaxChannels];
    drmp3_uint64 framesRead;
    do {
        framesRead = drmp3_read_pcm_frames_f32(&mp3, kMaxFramesPerRead, buffer);
    } while (framesRead == kMaxFramesPerRead);

    drmp3_uninit(&mp3);
    return 0;
}
