/*
 * libFuzzer harness for the dr_wav/dr_flac/dr_mp3 file (stdio) layer: the
 * fopen/wfopen wrappers, the stdio on_read/on_seek/on_tell callbacks and the
 * file-based open/read APIs that no memory-based harness reaches.
 *
 * compile with
 * clang++ -g -O1 -std=c++11 -fsanitize=fuzzer,address -o dr_libs_fileio_fuzzer tests/fuzzing/dr_libs_fileio_fuzzer.cc
 *
 * and run ./dr_libs_fileio_fuzzer to run fuzz testing. For more options, run ./dr_libs_fileio_fuzzer -help=1
 *
 * Threat model
 * ------------
 * Untrusted input: the fuzzer input becomes the *content* of a file the
 * harness opens through the libraries' own file APIs - exactly what an
 * application does with an attacker-supplied media file. This is the only
 * surface that exercises the stdio callbacks, the wfopen/fopen wrappers and
 * the errno-mapped open paths, which Introspector flags as the project's top
 * 0%-coverage functions-of-interest. The attacker controls the file bytes and
 * (through tiny control bytes) which API family runs; buffers, ownership
 * (drwav_free/drflac_free) and file lifetime are handled by the harness per
 * the API contracts, so any sanitizer finding is a library bug.
 *
 * Input layout (FuzzedDataProvider: integrals from the BACK, file bytes from
 * the FRONT; seeds in seeds/fileio/ follow this exactly):
 *   byte lib   0 = dr_wav, 1 = dr_flac, 2 = dr_mp3
 *   byte api   API family index within the library (modulo the family size)
 *   byte extra format/width pick for write and full-read families
 *   front      the file content bytes
 */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fuzzer/FuzzedDataProvider.h>

#define DR_WAV_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#include "../../dr_wav.h"
#include "../../dr_flac.h"
#include "../../dr_mp3.h"

namespace {

constexpr drwav_uint64 kMaxFramesPerRead = 1024;
constexpr int kMaxChannels = 8;
constexpr unsigned kWrittenFrames = 8;

struct WavChunkPeek {
    volatile uint32_t touches;
};

/* dr_wav chunk-iteration callback for init_file_ex (peek, never hold). */
drwav_uint64 WavChunkCallback(void* pChunkUserData, drwav_read_proc onRead,
                              drwav_seek_proc onSeek, void* pReadSeekUserData,
                              const drwav_chunk_header* pChunkHeader,
                              drwav_container container, const drwav_fmt* pFMT) {
    WavChunkPeek* peek = static_cast<WavChunkPeek*>(pChunkUserData);
    uint8_t scratch[8];
    (void)onSeek;
    (void)container;
    (void)pFMT;
    if (pChunkHeader == NULL) {
        return 0;
    }
    size_t toRead = pChunkHeader->sizeInBytes < sizeof(scratch)
                        ? static_cast<size_t>(pChunkHeader->sizeInBytes)
                        : sizeof(scratch);
    size_t bytesRead = onRead(pReadSeekUserData, scratch, toRead);
    for (size_t i = 0; i < bytesRead; ++i) {
        peek->touches += scratch[i];
    }
    return bytesRead;
}

void FlacMetaCallback(void* pUserData, drflac_metadata* pMetadata) {
    volatile uint32_t* touches = static_cast<volatile uint32_t*>(pUserData);
    if (pMetadata == NULL) {
        return;
    }
    *touches += pMetadata->type;
}

void Mp3MetaCallback(void* pUserData, const drmp3_metadata* pMetadata) {
    volatile uint32_t* touches = static_cast<volatile uint32_t*>(pUserData);
    if (pMetadata == NULL) {
        return;
    }
    *touches += pMetadata->type;
}

void WavDrain(drwav* pWav) {
    static drwav_int16 bufferS16[kMaxFramesPerRead * kMaxChannels];
    drwav_uint64 framesRead;
    do {
        framesRead = drwav_read_pcm_frames_s16(pWav, kMaxFramesPerRead, bufferS16);
    } while (framesRead == kMaxFramesPerRead);
}

void FlacDrain(drflac* pFlac) {
    static drflac_int32 bufferS32[kMaxFramesPerRead * kMaxChannels];
    drflac_uint64 framesRead;
    do {
        framesRead = drflac_read_pcm_frames_s32(pFlac, kMaxFramesPerRead, bufferS32);
    } while (framesRead == kMaxFramesPerRead);
}

void Mp3Drain(drmp3* pMP3) {
    static float bufferF32[kMaxFramesPerRead * kMaxChannels];
    drmp3_uint64 framesRead;
    do {
        framesRead = drmp3_read_pcm_frames_f32(pMP3, kMaxFramesPerRead, bufferF32);
    } while (framesRead == kMaxFramesPerRead);
}

const drwav_uint32 kWavAllMetadataFlags =
    drwav_metadata_type_unknown | drwav_metadata_type_smpl |
    drwav_metadata_type_inst | drwav_metadata_type_cue |
    drwav_metadata_type_acid | drwav_metadata_type_bext |
    drwav_metadata_type_list_label | drwav_metadata_type_list_note |
    drwav_metadata_type_list_labelled_cue_region |
    drwav_metadata_type_list_all_info_strings;

void RunWav(const char* path, uint8_t api, uint8_t extra) {
    drwav wav;
    memset(&wav, 0, sizeof(wav));
    uint32_t touches = 0;

    switch (api % 9) {
        case 0:
            if (drwav_init_file(&wav, path, NULL)) {
                WavDrain(&wav);
                drwav_uninit(&wav);
            }
            break;
        case 1:
            if (drwav_init_file_with_metadata(&wav, path, kWavAllMetadataFlags, NULL)) {
                for (drwav_uint32 i = 0; i < wav.metadataCount; ++i) {
                    touches += static_cast<uint32_t>(wav.pMetadata[i].type);
                }
                WavDrain(&wav);
                drwav_uninit(&wav);
            }
            break;
        case 2: {
            WavChunkPeek peek;
            peek.touches = 0;
            if (drwav_init_file_ex(&wav, path, WavChunkCallback, &peek, 0, NULL)) {
                WavDrain(&wav);
                drwav_uninit(&wav);
            }
            break;
        }
        case 3: {
            /* Write through the stdio layer, then read back through it. */
            drwav_data_format format;
            format.container = drwav_container_riff;
            format.format = DR_WAVE_FORMAT_PCM;
            format.channels = 2;
            format.sampleRate = 44100;
            format.bitsPerSample = 16;
            static drwav_int16 frames[kWrittenFrames * 2];
            if ((extra & 0x01)
                ? drwav_init_file_write_sequential_pcm_frames(
                      &wav, path, &format, kWrittenFrames, NULL)
                : drwav_init_file_write(&wav, path, &format, NULL)) {
                (void)drwav_write_pcm_frames(&wav, kWrittenFrames, frames);
                (void)drwav_write_raw(&wav, 4, frames);
                drwav_uninit(&wav);
                if (drwav_init_file(&wav, path, NULL)) {
                    WavDrain(&wav);
                    drwav_uninit(&wav);
                }
            }
            break;
        }
        case 4:
        case 5:
        case 6: {
            unsigned int channels = 0;
            unsigned int sampleRate = 0;
            drwav_uint64 totalFrames = 0;
            if (api % 9 == 4) {
                drwav_int16* samples = drwav_open_file_and_read_pcm_frames_s16(
                    path, &channels, &sampleRate, &totalFrames, NULL);
                if (samples != NULL && totalFrames > 0 && channels > 0) {
                    volatile drwav_int16 first = samples[0];
                    (void)first;
                }
                drwav_free(samples, NULL);
            } else if (api % 9 == 5) {
                float* samples = drwav_open_file_and_read_pcm_frames_f32(
                    path, &channels, &sampleRate, &totalFrames, NULL);
                if (samples != NULL && totalFrames > 0 && channels > 0) {
                    volatile float first = samples[0];
                    (void)first;
                }
                drwav_free(samples, NULL);
            } else {
                drwav_int32* samples = drwav_open_file_and_read_pcm_frames_s32(
                    path, &channels, &sampleRate, &totalFrames, NULL);
                if (samples != NULL && totalFrames > 0 && channels > 0) {
                    volatile drwav_int32 first = samples[0];
                    (void)first;
                }
                drwav_free(samples, NULL);
            }
            break;
        }
        default: {
            /* Wide-char entry point: exercises the wfopen conversion path. */
            wchar_t widePath[64];
            if (mbstowcs(widePath, path, 63) == static_cast<size_t>(-1)) {
                break;
            }
            widePath[63] = L'\0';
            unsigned int channels = 0;
            unsigned int sampleRate = 0;
            drwav_uint64 totalFrames = 0;
            drwav_int16* samples = drwav_open_file_and_read_pcm_frames_s16_w(
                widePath, &channels, &sampleRate, &totalFrames, NULL);
            if (samples != NULL && totalFrames > 0 && channels > 0) {
                volatile drwav_int16 first = samples[0];
                (void)first;
            }
            drwav_free(samples, NULL);
            break;
        }
    }
}

void RunFlac(const char* path, uint8_t api) {
    uint32_t touches = 0;
    switch (api % 5) {
        case 0: {
            drflac* pFlac = drflac_open_file(path, NULL);
            if (pFlac != NULL) {
                FlacDrain(pFlac);
                drflac_close(pFlac);
            }
            break;
        }
        case 1: {
            drflac* pFlac = drflac_open_file_with_metadata(path, FlacMetaCallback,
                                                           &touches, NULL);
            if (pFlac != NULL) {
                FlacDrain(pFlac);
                drflac_close(pFlac);
            }
            break;
        }
        case 2:
        case 3:
        case 4: {
            unsigned int channels = 0;
            unsigned int sampleRate = 0;
            drflac_uint64 totalFrames = 0;
            if (api % 5 == 2) {
                drflac_int32* samples = drflac_open_file_and_read_pcm_frames_s32(
                    path, &channels, &sampleRate, &totalFrames, NULL);
                if (samples != NULL && totalFrames > 0 && channels > 0) {
                    volatile drflac_int32 first = samples[0];
                    (void)first;
                }
                drflac_free(samples, NULL);
            } else if (api % 5 == 3) {
                drflac_int16* samples = drflac_open_file_and_read_pcm_frames_s16(
                    path, &channels, &sampleRate, &totalFrames, NULL);
                if (samples != NULL && totalFrames > 0 && channels > 0) {
                    volatile drflac_int16 first = samples[0];
                    (void)first;
                }
                drflac_free(samples, NULL);
            } else {
                float* samples = drflac_open_file_and_read_pcm_frames_f32(
                    path, &channels, &sampleRate, &totalFrames, NULL);
                if (samples != NULL && totalFrames > 0 && channels > 0) {
                    volatile float first = samples[0];
                    (void)first;
                }
                drflac_free(samples, NULL);
            }
            break;
        }
    }
}

void RunMp3(const char* path, uint8_t api) {
    switch (api % 4) {
        case 0: {
            drmp3 mp3;
            memset(&mp3, 0, sizeof(mp3));
            if (drmp3_init_file(&mp3, path, NULL)) {
                Mp3Drain(&mp3);
                drmp3_uninit(&mp3);
            }
            break;
        }
        case 1: {
            uint32_t touches = 0;
            drmp3 mp3;
            memset(&mp3, 0, sizeof(mp3));
            if (drmp3_init_file_with_metadata(&mp3, path, Mp3MetaCallback,
                                              &touches, NULL)) {
                Mp3Drain(&mp3);
                drmp3_uninit(&mp3);
            }
            break;
        }
        case 2:
        case 3: {
            drmp3_config config;
            memset(&config, 0, sizeof(config));
            drmp3_uint64 totalFrames = 0;
            if (api % 4 == 2) {
                float* samples = drmp3_open_file_and_read_pcm_frames_f32(
                    path, &config, &totalFrames, NULL);
                if (samples != NULL && totalFrames > 0 && config.channels > 0) {
                    volatile float first = samples[0];
                    (void)first;
                }
                drmp3_free(samples, NULL);
            } else {
                drmp3_int16* samples = drmp3_open_file_and_read_pcm_frames_s16(
                    path, &config, &totalFrames, NULL);
                if (samples != NULL && totalFrames > 0 && config.channels > 0) {
                    volatile drmp3_int16 first = samples[0];
                    (void)first;
                }
                drmp3_free(samples, NULL);
            }
            break;
        }
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzedDataProvider fdp(data, size);

    const uint8_t lib = fdp.ConsumeIntegral<uint8_t>();
    const uint8_t api = fdp.ConsumeIntegral<uint8_t>();
    const uint8_t extra = fdp.ConsumeIntegral<uint8_t>();

    size_t remaining = fdp.remaining_bytes();
    if (remaining == 0) {
        return 0;
    }
    std::vector<uint8_t> content = fdp.ConsumeBytes<uint8_t>(remaining);

    /* The fuzz input becomes the file content; the libraries then open it
     * through their own stdio wrappers - the attacker-supplied media file
     * scenario. */
    char path[128];
    snprintf(path, sizeof(path), "/tmp/dr_libs_fileio_%d.bin", (int)getpid());
    FILE* fp = fopen(path, "wb");
    if (fp == NULL) {
        return 0;
    }
    if (fwrite(content.data(), 1, content.size(), fp) != content.size()) {
        fclose(fp);
        remove(path);
        return 0;
    }
    fclose(fp);

    switch (lib % 3) {
        case 0:
            RunWav(path, api, extra);
            break;
        case 1:
            RunFlac(path, api);
            break;
        default:
            RunMp3(path, api);
            break;
    }

    remove(path);
    return 0;
}
