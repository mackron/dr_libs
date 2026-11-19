/*
 * libFuzzer harness for the dr_mp3.h seek machinery, metadata callback and
 * the read/decode surface the minimal dr_mp3_fuzzer loop never reaches.
 *
 * compile with
 * clang++ -g -O1 -std=c++11 -fsanitize=fuzzer,address -o dr_mp3_seek_fuzzer tests/fuzzing/dr_mp3_seek_fuzzer.cc
 *
 * and run ./dr_mp3_seek_fuzzer to run fuzz testing. For more options, run ./dr_mp3_seek_fuzzer -help=1
 *
 * Threat model
 * ------------
 * Untrusted input: the fuzzer input as an MP3 byte stream (optionally with an
 * ID3v2 tag prepended by the seeds - what an attacker-supplied .mp3 looks
 * like). The harness enters at the public trust boundary:
 * drmp3_init_memory / drmp3_init_memory_with_metadata (ID3v2/LAME tag
 * parsing), then drives the public decode (f32/s16), the seek machinery
 * (drmp3_seek_to_pcm_frame via the brute-force path and, when bound, the
 * seek-table path built with drmp3_calculate_seek_points +
 * drmp3_bind_seek_table), the frame-count queries, the one-pass full-read
 * helpers and the low-level drmp3dec_* component decoder.
 * The attacker controls values only; buffers honour the API contract
 * (2-channel MP3 maximum, fixed per-frame buffers, allocations freed), so any
 * sanitizer finding is a library bug.
 *
 * Input layout (FuzzedDataProvider: integrals from the BACK, the stream from
 * the FRONT; seeds in seeds/dr_mp3_seek/ follow this exactly):
 *   uint64-range seekTarget1 (0..500000)
 *   uint64-range seekTarget2 (0..500000)
 *   byte ctl   bit0 = open with metadata callback, bit1 = calculate+bind the
 *              seek table, bit2 = low-level drmp3dec_* component decode,
 *              bit3 = one full-read helper
 *   byte seq   width alternation / full-read pick
 *   front      the stream bytes
 */
#include <cstdint>
#include <cstring>
#include <vector>
#include <fuzzer/FuzzedDataProvider.h>

#define DR_MP3_IMPLEMENTATION
#include "../../dr_mp3.h"

namespace {

constexpr drmp3_uint64 kMaxFramesPerRead = 1024;
constexpr int kMaxChannels = 2; /* MP3 spec limit: mono or stereo */
constexpr int kMaxReadIterations = 6;
constexpr drmp3_uint32 kDesiredSeekPoints = 8;

/* Control values consumed from the back of the input before the stream. */
struct TailCfg {
    drmp3_uint64 seekTarget1;
    drmp3_uint64 seekTarget2;
    uint8_t ctl;
    uint8_t seq;
};

void MetaCallback(void* pUserData, const drmp3_metadata* pMetadata) {
    volatile uint32_t* touches = static_cast<volatile uint32_t*>(pUserData);
    if (pMetadata == NULL) {
        return;
    }
    *touches += pMetadata->type;
    const drmp3_uint8* raw = static_cast<const drmp3_uint8*>(pMetadata->pRawData);
    size_t n = pMetadata->rawDataSize < 32 ? pMetadata->rawDataSize : 32;
    for (size_t i = 0; i < n; ++i) {
        *touches += raw[i];
    }
}

void DecodeAndSeek(drmp3* pMP3, const TailCfg& tail) {
    static float bufferF32[kMaxFramesPerRead * kMaxChannels];
    static drmp3_int16 bufferS16[kMaxFramesPerRead * kMaxChannels];

    /* Brute-force seeks first (no table bound yet). */
    (void)drmp3_seek_to_pcm_frame(pMP3, tail.seekTarget1);
    (void)drmp3_seek_to_pcm_frame(pMP3, tail.seekTarget2);

    for (int i = 0; i < kMaxReadIterations; ++i) {
        const uint8_t width = (tail.seq >> (i & 1)) & 0x01;
        drmp3_uint64 framesRead;
        if (width) {
            framesRead = drmp3_read_pcm_frames_f32(pMP3, kMaxFramesPerRead, bufferF32);
        } else {
            framesRead = drmp3_read_pcm_frames_s16(pMP3, kMaxFramesPerRead, bufferS16);
        }
        if (framesRead < kMaxFramesPerRead) {
            break;
        }
        (void)drmp3_seek_to_pcm_frame(pMP3, (i & 1) ? tail.seekTarget1 : tail.seekTarget2);
    }
}

void RunComponentDecode(const uint8_t* data, size_t size) {
    drmp3dec dec;
    drmp3dec_init(&dec);
    static float pcm[DRMP3_MAX_SAMPLES_PER_FRAME];
    drmp3dec_frame_info info;
    memset(&info, 0, sizeof(info));
    int samples = drmp3dec_decode_frame(&dec, data, (int)size, pcm, &info);
    if (samples > 0 && info.channels > 0) {
        static drmp3_int16 out[DRMP3_MAX_SAMPLES_PER_FRAME];
        drmp3dec_f32_to_s16(pcm, out, (size_t)samples * info.channels);
    }
}

void RunFullReadHelper(const uint8_t* data, size_t size, uint8_t pick) {
    drmp3_config config;
    memset(&config, 0, sizeof(config));
    drmp3_uint64 totalFrames = 0;
    if ((pick & 0x01) == 0) {
        float* samples = drmp3_open_memory_and_read_pcm_frames_f32(
            data, size, &config, &totalFrames, NULL);
        if (samples != NULL && totalFrames > 0 && config.channels > 0) {
            volatile float first = samples[0];
            (void)first;
        }
        drmp3_free(samples, NULL);
    } else {
        drmp3_int16* samples = drmp3_open_memory_and_read_pcm_frames_s16(
            data, size, &config, &totalFrames, NULL);
        if (samples != NULL && totalFrames > 0 && config.channels > 0) {
            volatile drmp3_int16 first = samples[0];
            (void)first;
        }
        drmp3_free(samples, NULL);
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzedDataProvider fdp(data, size);

    TailCfg tail;
    tail.seekTarget1 = fdp.ConsumeIntegralInRange<drmp3_uint64>(0, 500000);
    tail.seekTarget2 = fdp.ConsumeIntegralInRange<drmp3_uint64>(0, 500000);
    tail.ctl = fdp.ConsumeIntegral<uint8_t>();
    tail.seq = fdp.ConsumeIntegral<uint8_t>();

    size_t remaining = fdp.remaining_bytes();
    if (remaining == 0) {
        return 0;
    }
    std::vector<uint8_t> stream = fdp.ConsumeBytes<uint8_t>(remaining);

    drmp3 mp3;
    memset(&mp3, 0, sizeof(mp3));
    /* Client-owned seek table: drmp3_bind_seek_table() stores this pointer,
     * so it must outlive every seek performed through the drmp3 object. */
    drmp3_seek_point seekPoints[kDesiredSeekPoints];
    uint32_t metaTouches = 0;
    bool initialised;
    if (tail.ctl & 0x01) {
        initialised = drmp3_init_memory_with_metadata(
            &mp3, stream.data(), stream.size(), MetaCallback,
            &metaTouches, NULL) == DRMP3_TRUE;
    } else {
        initialised = drmp3_init_memory(&mp3, stream.data(), stream.size(), NULL) == DRMP3_TRUE;
    }

    if (initialised) {
        drmp3_uint64 pcmCount = drmp3_get_pcm_frame_count(&mp3);
        drmp3_uint64 mp3Count = drmp3_get_mp3_frame_count(&mp3);
        drmp3_uint64 mp3AndPcm = 0;
        (void)drmp3_get_mp3_and_pcm_frame_count(&mp3, &mp3AndPcm, &pcmCount);
        metaTouches += static_cast<uint32_t>(pcmCount & 0xFF);
        metaTouches += static_cast<uint32_t>(mp3Count & 0xFF);
        metaTouches += static_cast<uint32_t>(mp3AndPcm & 0xFF);

        if (tail.ctl & 0x02) {
            /* Seek-table path: calculate, bind, then seek through the table. */
            drmp3_uint32 seekPointCount = kDesiredSeekPoints;
            if (drmp3_calculate_seek_points(&mp3, &seekPointCount, seekPoints)) {
                (void)drmp3_bind_seek_table(&mp3, seekPointCount, seekPoints);
            }
        }

        DecodeAndSeek(&mp3, tail);
        drmp3_uninit(&mp3);
    }

    if (tail.ctl & 0x04) {
        RunComponentDecode(stream.data(), stream.size());
    }
    if (tail.ctl & 0x08) {
        RunFullReadHelper(stream.data(), stream.size(), tail.seq);
    }
    return 0;
}
