/*
 * libFuzzer harness for the dr_flac.h seek machinery, metadata callbacks and
 * the read/decode surface the minimal dr_flac_fuzzer loop never reaches.
 *
 * compile with
 * clang++ -g -O1 -std=c++11 -fsanitize=fuzzer,address -o dr_flac_seek_fuzzer tests/fuzzing/dr_flac_seek_fuzzer.cc
 *
 * and run ./dr_flac_seek_fuzzer to run fuzz testing. For more options, run ./dr_flac_seek_fuzzer -help=1
 *
 * Threat model
 * ------------
 * Untrusted input: the fuzzer input as a FLAC/Ogg-FLAC byte stream (what an
 * attacker-supplied .flac looks like to an application). The harness enters at
 * the public trust boundary: drflac_open_memory /
 * drflac_open_memory_with_metadata, then drives the public decode
 * (s32/s16/f32), seek (drflac_seek_to_pcm_frame, including past-the-end and
 * back-to-start), and the documented metadata iterator APIs (vorbis comment,
 * cuesheet tracks) on whatever the stream parsed. The one-pass full-read
 * helpers (drflac_open_memory_and_read_pcm_frames_*) are driven on raw fuzz
 * bytes and released with drflac_free.
 * The attacker controls values only; buffers and API contracts are honoured
 * (buffers sized to the FLAC channel limit, allocations freed, iterators used
 * as documented), so any sanitizer finding is a library bug.
 *
 * Input layout (FuzzedDataProvider: integrals from the BACK, blobs from the
 * FRONT; seeds in seeds/dr_flac_seek/ follow this exactly):
 *   uint64-range seekTarget1 (0..2000000)
 *   uint64-range seekTarget2 (0..2000000)
 *   byte ctl      bit0 = open with metadata callback, bit1 = run one full-read
 *                 helper, bit2 = run the vorbis-comment/cuesheet iterators in
 *                 the metadata callback, bit3 = interleave seeks with reads
 *   byte seq      width/seek alternation bits
 *   byte pick     which full-read helper (0..2) when ctl bit1 set
 *   front         the stream bytes
 */
#include <cstdint>
#include <cstring>
#include <fuzzer/FuzzedDataProvider.h>

#define DR_FLAC_IMPLEMENTATION
#include "../../dr_flac.h"

namespace {

constexpr drflac_uint64 kMaxFramesPerRead = 1024;
constexpr int kMaxChannels = 8; /* FLAC spec limit */
constexpr int kMaxReadIterations = 8;

/* Control values consumed from the back of the input before the stream. */
struct TailCfg {
    drflac_uint64 seekTarget1;
    drflac_uint64 seekTarget2;
    uint8_t ctl;
};

struct MetaTouch {
    unsigned ctl;
    volatile uint32_t touches;
};

void MetaCallback(void* pUserData, drflac_metadata* pMetadata) {
    MetaTouch* touch = static_cast<MetaTouch*>(pUserData);
    if (pMetadata == NULL) {
        return;
    }
    touch->touches += pMetadata->type;

    switch (pMetadata->type) {
        case DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO:
            touch->touches += static_cast<uint32_t>(
                pMetadata->data.streaminfo.totalPCMFrameCount & 0xFF);
            break;
        case DRFLAC_METADATA_BLOCK_TYPE_APPLICATION:
            touch->touches += pMetadata->data.application.dataSize;
            break;
        case DRFLAC_METADATA_BLOCK_TYPE_SEEKTABLE: {
            const drflac_uint32 n = pMetadata->data.seektable.seekpointCount;
            for (drflac_uint32 i = 0; i < n && i < 4096; ++i) {
                touch->touches += static_cast<uint32_t>(
                    pMetadata->data.seektable.pSeekpoints[i].firstPCMFrame & 0xFF);
            }
            break;
        }
        case DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT: {
            touch->touches += pMetadata->data.vorbis_comment.commentCount;
            if ((touch->ctl & 0x04) == 0) {
                break;
            }
            drflac_vorbis_comment_iterator iter;
            drflac_init_vorbis_comment_iterator(
                &iter, pMetadata->data.vorbis_comment.commentCount,
                pMetadata->data.vorbis_comment.pComments);
            for (drflac_uint32 i = 0;
                 i < pMetadata->data.vorbis_comment.commentCount && i < 4096; ++i) {
                drflac_uint32 len = 0;
                const char* comment = drflac_next_vorbis_comment(&iter, &len);
                if (comment == NULL) {
                    break;
                }
                touch->touches += len;
            }
            break;
        }
        case DRFLAC_METADATA_BLOCK_TYPE_CUESHEET: {
            touch->touches += pMetadata->data.cuesheet.trackCount;
            if ((touch->ctl & 0x04) == 0) {
                break;
            }
            drflac_cuesheet_track_iterator iter;
            drflac_init_cuesheet_track_iterator(
                &iter, pMetadata->data.cuesheet.trackCount,
                pMetadata->data.cuesheet.pTrackData);
            for (drflac_uint32 i = 0;
                 i < pMetadata->data.cuesheet.trackCount && i < 4096; ++i) {
                drflac_cuesheet_track track;
                if (!drflac_next_cuesheet_track(&iter, &track)) {
                    break;
                }
                touch->touches += track.indexCount;
            }
            break;
        }
        case DRFLAC_METADATA_BLOCK_TYPE_PICTURE:
            touch->touches += pMetadata->data.picture.pictureDataSize;
            break;
        default:
            break;
    }
}

void RunFullReadHelper(const uint8_t* data, size_t size, uint8_t pick) {
    unsigned int channels = 0;
    unsigned int sampleRate = 0;
    drflac_uint64 totalFrames = 0;
    switch (pick % 3) {
        case 0: {
            drflac_int16* samples = drflac_open_memory_and_read_pcm_frames_s16(
                data, size, &channels, &sampleRate, &totalFrames, NULL);
            if (samples != NULL && totalFrames > 0 && channels > 0) {
                volatile drflac_int16 first = samples[0];
                (void)first;
            }
            drflac_free(samples, NULL);
            break;
        }
        case 1: {
            drflac_int32* samples = drflac_open_memory_and_read_pcm_frames_s32(
                data, size, &channels, &sampleRate, &totalFrames, NULL);
            if (samples != NULL && totalFrames > 0 && channels > 0) {
                volatile drflac_int32 first = samples[0];
                (void)first;
            }
            drflac_free(samples, NULL);
            break;
        }
        default: {
            float* samples = drflac_open_memory_and_read_pcm_frames_f32(
                data, size, &channels, &sampleRate, &totalFrames, NULL);
            if (samples != NULL && totalFrames > 0 && channels > 0) {
                volatile float first = samples[0];
                (void)first;
            }
            drflac_free(samples, NULL);
            break;
        }
    }
}

void DecodeAndSeek(drflac* pFlac, const TailCfg& tail, uint8_t seq) {
    static drflac_int32 bufferS32[kMaxFramesPerRead * kMaxChannels];
    static drflac_int16 bufferS16[kMaxFramesPerRead * kMaxChannels];
    static float bufferF32[kMaxFramesPerRead * kMaxChannels];

    for (int i = 0; i < kMaxReadIterations; ++i) {
        const uint8_t width = (seq >> (2 * (i & 3))) & 0x03;
        drflac_uint64 framesRead;
        switch (width % 3) {
            case 0:
                framesRead = drflac_read_pcm_frames_s32(pFlac, kMaxFramesPerRead, bufferS32);
                break;
            case 1:
                framesRead = drflac_read_pcm_frames_s16(pFlac, kMaxFramesPerRead, bufferS16);
                break;
            default:
                framesRead = drflac_read_pcm_frames_f32(pFlac, kMaxFramesPerRead, bufferF32);
                break;
        }
        if (framesRead < kMaxFramesPerRead) {
            break; /* hit the end of the stream */
        }
        if ((tail.ctl & 0x08) && (i & 1)) {
            (void)drflac_seek_to_pcm_frame(
                pFlac, (i & 2) ? tail.seekTarget1 : tail.seekTarget2);
        }
    }

    /* Back-to-start seek (exercises the reverse-seek path), then one read. */
    (void)drflac_seek_to_pcm_frame(pFlac, 0);
    (void)drflac_read_pcm_frames_s16(pFlac, kMaxFramesPerRead, bufferS16);
    (void)drflac_seek_to_pcm_frame(pFlac, tail.seekTarget1);
    (void)drflac_read_pcm_frames_f32(pFlac, kMaxFramesPerRead, bufferF32);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzedDataProvider fdp(data, size);

    TailCfg tail;
    tail.seekTarget1 = fdp.ConsumeIntegralInRange<drflac_uint64>(0, 2000000);
    tail.seekTarget2 = fdp.ConsumeIntegralInRange<drflac_uint64>(0, 2000000);
    tail.ctl = fdp.ConsumeIntegral<uint8_t>();
    uint8_t seq = fdp.ConsumeIntegral<uint8_t>();
    uint8_t pick = fdp.ConsumeIntegral<uint8_t>();

    size_t remaining = fdp.remaining_bytes();
    if (remaining == 0) {
        return 0;
    }
    std::vector<uint8_t> stream = fdp.ConsumeBytes<uint8_t>(remaining);

    if (tail.ctl & 0x01) {
        MetaTouch touch;
        touch.ctl = tail.ctl;
        touch.touches = 0;
        drflac* pFlac = drflac_open_memory_with_metadata(
            stream.data(), stream.size(), MetaCallback, &touch, NULL);
        if (pFlac != NULL) {
            DecodeAndSeek(pFlac, tail, seq);
            drflac_close(pFlac);
        }
    } else {
        drflac* pFlac = drflac_open_memory(stream.data(), stream.size(), NULL);
        if (pFlac != NULL) {
            DecodeAndSeek(pFlac, tail, seq);
            drflac_close(pFlac);
        }
    }

    if (tail.ctl & 0x02) {
        RunFullReadHelper(stream.data(), stream.size(), pick);
    }
    return 0;
}
