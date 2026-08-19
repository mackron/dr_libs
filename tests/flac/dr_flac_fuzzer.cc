/*
 * libFuzzer harness for dr_flac.h
 *
 * compile with
 * clang++ -g -O1 -std=c++11 -fsanitize=fuzzer,address -o dr_flac_fuzzer tests/flac/dr_flac_fuzzer.cc
 *
 * and run ./dr_flac_fuzzer to run fuzz testing. For more options, run ./dr_flac_fuzzer -help=1
 */
#include <cstdint>

#define DR_FLAC_IMPLEMENTATION
#include "../../dr_flac.h"

namespace {
constexpr drflac_uint64 kMaxFramesPerRead = 4096;
constexpr int kMaxChannels = 8; /* FLAC spec limit */
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    drflac *pFlac = drflac_open_memory(data, size, nullptr);
    if (pFlac == nullptr) {
        return 0;
    }

    static drflac_int32 buffer[kMaxFramesPerRead * kMaxChannels];
    drflac_uint64 framesRead;
    do {
        framesRead = drflac_read_pcm_frames_s32(pFlac, kMaxFramesPerRead, buffer);
    } while (framesRead == kMaxFramesPerRead);

    drflac_close(pFlac);
    return 0;
}
