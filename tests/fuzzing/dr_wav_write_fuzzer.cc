/*
 * libFuzzer harness for the dr_wav.h write API, metadata handling and the
 * read-back surface (seek, raw reads, alternate sample-width conversions,
 * chunk iteration) that the minimal dr_wav_fuzzer loop never reaches.
 *
 * compile with
 * clang++ -g -O1 -std=c++11 -fsanitize=fuzzer,address -o dr_wav_write_fuzzer tests/fuzzing/dr_wav_write_fuzzer.cc
 *
 * and run ./dr_wav_write_fuzzer to run fuzz testing. For more options, run ./dr_wav_write_fuzzer -help=1
 *
 * Threat model
 * ------------
 * Untrusted input: the fuzzer input. Two uses:
 *   1. Round-trip (modes 0-2): the bytes drive a typed write configuration
 *      (container, format, write style, fuzzed drwav_metadata entries) - the
 *      shape a hostile/untrusted input file takes when an application saves it
 *      back out - then the produced bytes are re-parsed with metadata capture,
 *      seeks and alternate-width drains.
 *   2. Direct parse (mode 3): the raw bytes are parsed as a WAV/W64/RF64/AIFF
 *      stream with full metadata capture, exercising the chunk parsers
 *      (bext/smpl/inst/cue/acid/LIST-adtl/LIST-info/unknown) on attacker
 *      bytes straight away.
 * Everything the attacker controls here is values, never API validity: the
 * harness always passes well-formed config structs, valid callbacks and
 * sufficiently large buffers, so any sanitizer finding is a library bug, not a
 * contract violation. All loops and buffers are bounded by the input.
 *
 * Input layout (consumed in this exact order; seeds in seeds/dr_wav_write/
 * follow it):
 *   byte 0        mode: %4 0,1,2 = write round-trip, 3 = direct parse
 *   round-trip:
 *     byte 1      cfg: bits 0-2 container (0 RIFF, 1 RIFX, 2 W64, 3 RF64,
 *                 4 AIFF, 5-7 RIFF); bits 3-5 write style; bit 6 metadata on
 *     byte 2      bits 0-2 format tag index; bits 3-5 bits-per-sample index;
 *                 bits 6-7 channel count - 1
 *     uint16      sample rate (8000..48000), little-endian as FDP emits
 *     byte        write frame count = %33
 *     bytes       PCM source payload (exact size computed, zero-padded)
 *     (metadata on: byte entry count %4, then per entry: byte type index
 *      and bounded values/strings, in BuildMetadataEntry order)
 *     uint64 in range [0,100000]   seek target 1
 *     uint64 in range [0,100000]   seek target 2
 *     byte        bit 0: chunk-iteration pass; bit 1: take ownership of
 *                 read-side metadata (direct mode only)
 *   direct:
 *     all remaining bytes = the stream to parse; the three tail bytes above
 *     are consumed from the BACK of the input by FuzzedDataProvider's ranged
 *     integrals (they are the last bytes of the seed file)
 */
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <fuzzer/FuzzedDataProvider.h>

#define DR_WAV_IMPLEMENTATION
#include "../../dr_wav.h"

namespace {

constexpr drwav_uint64 kMaxFramesPerRead = 64;
constexpr unsigned kMaxWriteFrames = 32;
constexpr size_t kMaxSinkBytes = 1 << 20;
constexpr size_t kMaxMetaEntries = 3;
constexpr size_t kMaxStringBytes = 32;
constexpr size_t kMaxCodingHistoryBytes = 64;
constexpr size_t kMaxSamplerDataBytes = 64;
constexpr size_t kMaxChunkPeekBytes = 8;

/* Memory sink with seek support for non-sequential writers. */
struct MemSink {
    std::vector<uint8_t> bytes;
};

size_t MemSinkWrite(void* pUserData, const void* pData, size_t bytesToWrite) {
    MemSink* sink = static_cast<MemSink*>(pUserData);
    if (sink->bytes.size() + bytesToWrite > kMaxSinkBytes) {
        return 0; /* bounded sink: refuse to grow past the cap */
    }
    const uint8_t* p = static_cast<const uint8_t*>(pData);
    sink->bytes.insert(sink->bytes.end(), p, p + bytesToWrite);
    return bytesToWrite;
}

drwav_bool32 MemSinkSeek(void* pUserData, int offset, drwav_seek_origin origin) {
    MemSink* sink = static_cast<MemSink*>(pUserData);
    drwav_uint64 target;
    if (origin == DRWAV_SEEK_CUR) {
        target = sink->bytes.size() + static_cast<drwav_uint64>(offset);
    } else if (origin == DRWAV_SEEK_END) {
        drwav_uint64 back = static_cast<drwav_uint64>(offset);
        target = (back <= sink->bytes.size()) ? sink->bytes.size() - back : 0;
    } else {
        if (offset < 0) {
            return DRWAV_FALSE;
        }
        target = static_cast<drwav_uint64>(offset);
    }
    if (target > kMaxSinkBytes) {
        return DRWAV_FALSE;
    }
    sink->bytes.resize(static_cast<size_t>(target), 0);
    return DRWAV_TRUE;
}

/* Chunk-iteration callback for the init_memory_ex pass. */
drwav_uint64 ChunkCallback(void* pChunkUserData, drwav_read_proc onRead,
                           drwav_seek_proc onSeek, void* pReadSeekUserData,
                           const drwav_chunk_header* pChunkHeader,
                           drwav_container container, const drwav_fmt* pFMT) {
    uint8_t scratch[kMaxChunkPeekBytes];
    (void)pChunkUserData;
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
    volatile uint8_t sum = 0;
    for (size_t i = 0; i < bytesRead; ++i) {
        sum = static_cast<uint8_t>(sum + scratch[i]);
    }
    return bytesRead;
}

struct MetaSlot {
    std::vector<drwav_cue_point> cuePoints;
    std::vector<drwav_smpl_loop> smplLoops;
    std::vector<uint8_t> samplerData;
    std::vector<uint8_t> umid;
    std::string description;
    std::string originatorName;
    std::string originatorReference;
    std::string codingHistory;
    std::string text;
};

void FillString(FuzzedDataProvider& fdp, std::string& out) {
    size_t len = fdp.ConsumeIntegralInRange<size_t>(0, kMaxStringBytes);
    out = fdp.ConsumeBytesAsString(len);
}

bool BuildMetadataEntry(FuzzedDataProvider& fdp, MetaSlot& slot, drwav_metadata* entry) {
    memset(entry, 0, sizeof(*entry));
    uint8_t type = fdp.ConsumeIntegral<uint8_t>() % 12;
    switch (type) {
        case 0: {
            entry->type = drwav_metadata_type_smpl;
            uint8_t loopCount = 1 + fdp.ConsumeIntegral<uint8_t>() % 2;
            slot.smplLoops.resize(loopCount);
            for (uint8_t i = 0; i < loopCount; ++i) {
                memset(&slot.smplLoops[i], 0, sizeof(drwav_smpl_loop));
                slot.smplLoops[i].cuePointId = fdp.ConsumeIntegral<uint32_t>();
                slot.smplLoops[i].type = fdp.ConsumeIntegral<uint32_t>() % 3;
                slot.smplLoops[i].firstSampleOffset = fdp.ConsumeIntegral<uint32_t>();
                slot.smplLoops[i].lastSampleOffset = fdp.ConsumeIntegral<uint32_t>();
                slot.smplLoops[i].sampleFraction = fdp.ConsumeIntegral<uint32_t>();
                slot.smplLoops[i].playCount = fdp.ConsumeIntegral<uint32_t>();
            }
            size_t samplerDataSize = fdp.ConsumeIntegralInRange<size_t>(0, kMaxSamplerDataBytes);
            slot.samplerData = fdp.ConsumeBytes<uint8_t>(samplerDataSize);
            entry->data.smpl.manufacturerId = fdp.ConsumeIntegral<uint32_t>();
            entry->data.smpl.productId = fdp.ConsumeIntegral<uint32_t>();
            entry->data.smpl.samplePeriodNanoseconds = fdp.ConsumeIntegral<uint32_t>();
            entry->data.smpl.midiUnityNote = fdp.ConsumeIntegral<uint32_t>();
            entry->data.smpl.midiPitchFraction = fdp.ConsumeIntegral<uint32_t>();
            entry->data.smpl.smpteFormat = fdp.ConsumeIntegral<uint32_t>();
            entry->data.smpl.smpteOffset = fdp.ConsumeIntegral<uint32_t>();
            entry->data.smpl.sampleLoopCount = loopCount;
            entry->data.smpl.pLoops = slot.smplLoops.data();
            entry->data.smpl.samplerSpecificDataSizeInBytes =
                static_cast<drwav_uint32>(slot.samplerData.size());
            entry->data.smpl.pSamplerSpecificData =
                slot.samplerData.empty() ? NULL : slot.samplerData.data();
            break;
        }
        case 1: {
            entry->type = drwav_metadata_type_inst;
            entry->data.inst.midiUnityNote = static_cast<drwav_int8>(fdp.ConsumeIntegral<uint8_t>());
            entry->data.inst.fineTuneCents = static_cast<drwav_int8>(fdp.ConsumeIntegral<uint8_t>());
            entry->data.inst.gainDecibels = static_cast<drwav_int8>(fdp.ConsumeIntegral<uint8_t>());
            entry->data.inst.lowNote = static_cast<drwav_int8>(fdp.ConsumeIntegral<uint8_t>());
            entry->data.inst.highNote = static_cast<drwav_int8>(fdp.ConsumeIntegral<uint8_t>());
            entry->data.inst.lowVelocity = static_cast<drwav_int8>(fdp.ConsumeIntegral<uint8_t>());
            entry->data.inst.highVelocity = static_cast<drwav_int8>(fdp.ConsumeIntegral<uint8_t>());
            break;
        }
        case 2: {
            entry->type = drwav_metadata_type_cue;
            uint8_t cueCount = 1 + fdp.ConsumeIntegral<uint8_t>() % 4;
            slot.cuePoints.resize(cueCount);
            for (uint8_t i = 0; i < cueCount; ++i) {
                memset(&slot.cuePoints[i], 0, sizeof(drwav_cue_point));
                slot.cuePoints[i].id = fdp.ConsumeIntegral<uint32_t>();
                slot.cuePoints[i].sampleOffset = fdp.ConsumeIntegral<uint32_t>();
            }
            entry->data.cue.cuePointCount = cueCount;
            entry->data.cue.pCuePoints = slot.cuePoints.data();
            break;
        }
        case 3: {
            entry->type = drwav_metadata_type_acid;
            entry->data.acid.flags = fdp.ConsumeIntegral<uint32_t>();
            entry->data.acid.midiUnityNote = fdp.ConsumeIntegral<uint16_t>();
            entry->data.acid.reserved1 = fdp.ConsumeIntegral<uint16_t>();
            entry->data.acid.reserved2 = 0.0f;
            entry->data.acid.numBeats = fdp.ConsumeIntegral<uint32_t>();
            entry->data.acid.meterDenominator = fdp.ConsumeIntegral<uint16_t>();
            entry->data.acid.meterNumerator = fdp.ConsumeIntegral<uint16_t>();
            entry->data.acid.tempo = 0.0f;
            break;
        }
        case 4: {
            entry->type = drwav_metadata_type_bext;
            FillString(fdp, slot.description);   /* kept <= 32; spec allows 256 */
            FillString(fdp, slot.originatorName);
            FillString(fdp, slot.originatorReference);
            FillString(fdp, slot.codingHistory); /* kept <= 64 */
            slot.umid = fdp.ConsumeBytes<uint8_t>(64);
            if (slot.umid.size() != 64) {
                slot.umid.resize(64, 0);
            }
            entry->data.bext.pDescription = &slot.description[0];
            entry->data.bext.pOriginatorName = &slot.originatorName[0];
            entry->data.bext.pOriginatorReference = &slot.originatorReference[0];
            memset(entry->data.bext.pOriginationDate, 0,
                   sizeof(entry->data.bext.pOriginationDate));
            memset(entry->data.bext.pOriginationTime, 0,
                   sizeof(entry->data.bext.pOriginationTime));
            entry->data.bext.timeReference = fdp.ConsumeIntegral<uint64_t>();
            entry->data.bext.version = static_cast<drwav_uint16>(fdp.ConsumeIntegral<uint32_t>() % 3);
            entry->data.bext.pCodingHistory =
                slot.codingHistory.empty() ? NULL : &slot.codingHistory[0];
            entry->data.bext.codingHistorySize =
                static_cast<drwav_uint32>(slot.codingHistory.size());
            entry->data.bext.pUMID = &slot.umid[0];
            entry->data.bext.loudnessValue = fdp.ConsumeIntegral<uint16_t>();
            entry->data.bext.loudnessRange = fdp.ConsumeIntegral<uint16_t>();
            entry->data.bext.maxTruePeakLevel = fdp.ConsumeIntegral<uint16_t>();
            entry->data.bext.maxMomentaryLoudness = fdp.ConsumeIntegral<uint16_t>();
            entry->data.bext.maxShortTermLoudness = fdp.ConsumeIntegral<uint16_t>();
            break;
        }
        case 5:
        case 6: {
            entry->type = (type == 5) ? drwav_metadata_type_list_label
                                      : drwav_metadata_type_list_note;
            FillString(fdp, slot.text);
            entry->data.labelOrNote.cuePointId = fdp.ConsumeIntegral<uint32_t>();
            entry->data.labelOrNote.stringLength =
                static_cast<drwav_uint32>(slot.text.size());
            entry->data.labelOrNote.pString = slot.text.empty() ? NULL : &slot.text[0];
            break;
        }
        case 7: {
            entry->type = drwav_metadata_type_list_labelled_cue_region;
            FillString(fdp, slot.text);
            entry->data.labelledCueRegion.cuePointId = fdp.ConsumeIntegral<uint32_t>();
            entry->data.labelledCueRegion.sampleLength = fdp.ConsumeIntegral<uint32_t>();
            for (size_t i = 0; i < 4; ++i) {
                entry->data.labelledCueRegion.purposeId[i] = fdp.ConsumeIntegral<uint8_t>();
            }
            entry->data.labelledCueRegion.country = fdp.ConsumeIntegral<uint16_t>();
            entry->data.labelledCueRegion.language = fdp.ConsumeIntegral<uint16_t>();
            entry->data.labelledCueRegion.dialect = fdp.ConsumeIntegral<uint16_t>();
            entry->data.labelledCueRegion.codePage = fdp.ConsumeIntegral<uint16_t>();
            entry->data.labelledCueRegion.stringLength =
                static_cast<drwav_uint32>(slot.text.size());
            entry->data.labelledCueRegion.pString =
                slot.text.empty() ? NULL : &slot.text[0];
            break;
        }
        default: {
            static const drwav_metadata_type kInfoTypes[] = {
                drwav_metadata_type_list_info_software,
                drwav_metadata_type_list_info_title,
                drwav_metadata_type_list_info_artist,
                drwav_metadata_type_list_info_comment
            };
            entry->type = kInfoTypes[type % 4];
            FillString(fdp, slot.text);
            entry->data.infoText.stringLength =
                static_cast<drwav_uint32>(slot.text.size());
            entry->data.infoText.pString = slot.text.empty() ? NULL : &slot.text[0];
            break;
        }
    }
    return true;
}

const drwav_uint32 kAllMetadataFlags =
    drwav_metadata_type_unknown | drwav_metadata_type_smpl |
    drwav_metadata_type_inst | drwav_metadata_type_cue |
    drwav_metadata_type_acid | drwav_metadata_type_bext |
    drwav_metadata_type_list_label | drwav_metadata_type_list_note |
    drwav_metadata_type_list_labelled_cue_region |
    drwav_metadata_type_list_all_info_strings;

/* Control bytes consumed from the back of the input before anything else. */
struct TailCfg {
    drwav_uint64 seekTarget1;
    drwav_uint64 seekTarget2;
    uint8_t ctl; /* bit 0: take ownership of read metadata; bit 1: chunk pass */
};

/* Shared read-back pass: drains in all three sample widths (plus the
 * little/big-endian forced variants and raw reads), seeks, queries the
 * cursor/length. Bounded by the stream's real length. */
void ReadBack(drwav* pWav, const TailCfg& tail) {
    static drwav_int16 bufferS16[kMaxFramesPerRead * DRWAV_MAX_CHANNELS];
    static drwav_int32 bufferS32[kMaxFramesPerRead * DRWAV_MAX_CHANNELS];
    static float bufferF32[kMaxFramesPerRead * DRWAV_MAX_CHANNELS];

    drwav_uint64 framesRead;
    do {
        framesRead = drwav_read_pcm_frames_s16(pWav, kMaxFramesPerRead, bufferS16);
    } while (framesRead == kMaxFramesPerRead);

    do {
        framesRead = drwav_read_pcm_frames_f32(pWav, kMaxFramesPerRead, bufferF32);
    } while (framesRead == kMaxFramesPerRead);

    do {
        framesRead = drwav_read_pcm_frames_s32(pWav, kMaxFramesPerRead, bufferS32);
    } while (framesRead == kMaxFramesPerRead);

    (void)drwav_read_pcm_frames_s16le(pWav, kMaxFramesPerRead, bufferS16);
    (void)drwav_read_pcm_frames_s16be(pWav, kMaxFramesPerRead, bufferS16);
    (void)drwav_read_pcm_frames_f32le(pWav, kMaxFramesPerRead, bufferF32);
    (void)drwav_read_pcm_frames_f32be(pWav, kMaxFramesPerRead, bufferF32);
    (void)drwav_read_pcm_frames_s32le(pWav, kMaxFramesPerRead, bufferS32);
    (void)drwav_read_pcm_frames_s32be(pWav, kMaxFramesPerRead, bufferS32);

    static uint8_t rawBuffer[kMaxFramesPerRead * DRWAV_MAX_CHANNELS * 2];
    (void)drwav_read_raw(pWav, sizeof(rawBuffer), rawBuffer);

    (void)drwav_seek_to_pcm_frame(pWav, tail.seekTarget1);
    (void)drwav_seek_to_pcm_frame(pWav, tail.seekTarget2);

    drwav_uint64 cursor = 0;
    drwav_uint64 length = 0;
    (void)drwav_get_cursor_in_pcm_frames(pWav, &cursor);
    (void)drwav_get_length_in_pcm_frames(pWav, &length);
}

void ParseWithMetadata(const uint8_t* data, size_t size, const TailCfg& tail) {
    drwav wav;
    if (!drwav_init_memory_with_metadata(&wav, data, size, kAllMetadataFlags, NULL)) {
        return;
    }

    /* Touch every parsed metadata object (owned by the drwav until uninit). */
    volatile uint32_t metaTouch = 0;
    for (drwav_uint32 i = 0; i < wav.metadataCount; ++i) {
        metaTouch += static_cast<uint32_t>(wav.pMetadata[i].type);
    }

    ReadBack(&wav, tail);

    if ((tail.ctl & 0x01) && wav.metadataCount > 0) {
        /* Single heap allocation; ownership transfer must be released with
         * drwav_free(). */
        drwav_metadata* taken = drwav_take_ownership_of_metadata(&wav);
        if (taken != NULL) {
            drwav_free(taken, NULL);
        }
    }
    drwav_uninit(&wav);
    (void)metaTouch;
}

void RunChunkIterationPass(const uint8_t* data, size_t size) {
    drwav wav;
    if (!drwav_init_memory_ex(&wav, data, size, ChunkCallback, NULL, 0, NULL)) {
        return;
    }
    drwav_uninit(&wav);
}

void RunRoundTrip(FuzzedDataProvider& fdp, const TailCfg& tail) {
    uint8_t cfg = fdp.ConsumeIntegral<uint8_t>();
    drwav_container container = static_cast<drwav_container>(cfg & 0x07);
    if (container > drwav_container_aiff) {
        container = drwav_container_riff;
    }
    uint8_t style = (cfg >> 3) & 0x07;
    bool withMetadata = (cfg & 0x40) != 0;

    uint8_t fmtByte = fdp.ConsumeIntegral<uint8_t>();
    static const drwav_uint32 kFormatTags[] = {
        DR_WAVE_FORMAT_PCM, DR_WAVE_FORMAT_IEEE_FLOAT, DR_WAVE_FORMAT_ALAW,
        DR_WAVE_FORMAT_MULAW, DR_WAVE_FORMAT_ADPCM, DR_WAVE_FORMAT_EXTENSIBLE
    };
    drwav_uint32 formatTag = kFormatTags[fmtByte % 6];
    static const drwav_uint16 kBitsPerSample[] = {8, 16, 24, 32, 64};
    drwav_uint16 bitsPerSample = kBitsPerSample[(fmtByte >> 3) % 5];
    if (bitsPerSample > 32 && formatTag != DR_WAVE_FORMAT_PCM) {
        bitsPerSample = 32;
    }
    uint32_t channels = 1u + (fmtByte >> 6);

    uint16_t sampleRate = fdp.ConsumeIntegralInRange<uint16_t>(8000, 48000);
    unsigned writeFrames = fdp.ConsumeIntegral<uint8_t>() % (kMaxWriteFrames + 1);

    drwav_data_format format;
    format.container = container;
    format.format = formatTag;
    format.channels = channels;
    format.sampleRate = sampleRate;
    format.bitsPerSample = bitsPerSample;

    /* Exact-size PCM source payload for the typed write calls. */
    size_t bytesPerSample = bitsPerSample / 8u;
    size_t payloadBytes = writeFrames * channels * bytesPerSample;
    if (payloadBytes > (1u << 16)) {
        payloadBytes = 1u << 16;
    }
    std::vector<uint8_t> payload = fdp.ConsumeBytes<uint8_t>(payloadBytes);
    if (payload.size() < payloadBytes) {
        payload.resize(payloadBytes, 0);
    }

    /* Optional fuzzed metadata entries (typed, bounded). */
    drwav_metadata metas[kMaxMetaEntries];
    MetaSlot slots[kMaxMetaEntries];
    drwav_uint32 metadataCount = 0;
    if (withMetadata) {
        uint8_t entryCount = fdp.ConsumeIntegral<uint8_t>() % (kMaxMetaEntries + 1);
        for (uint8_t i = 0; i < entryCount; ++i) {
            if (!BuildMetadataEntry(fdp, slots[i], &metas[metadataCount])) {
                break;
            }
            ++metadataCount;
        }
    }

    (void)drwav_target_write_size_bytes(&format, writeFrames, metas, metadataCount);

    drwav wav;
    memset(&wav, 0, sizeof(wav));
    MemSink sink;
    void* memData = NULL;
    size_t memDataSize = 0;
    bool initialised = false;

    switch (style) {
        case 0:
            initialised = drwav_init_memory_write(&wav, &memData, &memDataSize,
                                                  &format, NULL);
            break;
        case 1:
            initialised = drwav_init_memory_write_sequential(
                &wav, &memData, &memDataSize, &format,
                static_cast<drwav_uint64>(writeFrames) * channels, NULL);
            break;
        case 2:
            initialised = drwav_init_memory_write_sequential_pcm_frames(
                &wav, &memData, &memDataSize, &format, writeFrames, NULL);
            break;
        case 3:
            initialised = drwav_init_write(&wav, &format, MemSinkWrite,
                                           MemSinkSeek, &sink, NULL);
            break;
        case 4:
            initialised = drwav_init_write_sequential(
                &wav, &format, static_cast<drwav_uint64>(writeFrames) * channels,
                MemSinkWrite, &sink, NULL);
            break;
        case 5:
            initialised = drwav_init_write_sequential_pcm_frames(
                &wav, &format, writeFrames, MemSinkWrite, &sink, NULL);
            break;
        default:
            initialised = drwav_init_write_with_metadata(
                &wav, &format, MemSinkWrite, MemSinkSeek, &sink, NULL, metas,
                metadataCount);
            break;
    }
    if (!initialised) {
        return; /* invalid combos exercise the writer's validation paths */
    }

    /* Write via every direction of the API. */
    (void)drwav_write_pcm_frames(&wav, writeFrames, payload.data());
    (void)drwav_write_pcm_frames_le(&wav, 1, payload.data());
    (void)drwav_write_pcm_frames_be(&wav, 1, payload.data());
    size_t rawOut = payload.size() < 16 ? payload.size() : 16;
    (void)drwav_write_raw(&wav, rawOut, payload.data());

    drwav_uint64 producedBytes = 0;
    if (drwav_uninit(&wav) == DRWAV_SUCCESS) {
        producedBytes = (style <= 2) ? memDataSize : sink.bytes.size();
    }

    const uint8_t* reparseData = (style <= 2)
        ? static_cast<const uint8_t*>(memData)
        : sink.bytes.data();
    size_t reparseSize = static_cast<size_t>(producedBytes);

    if (reparseSize > 0) {
        ParseWithMetadata(reparseData, reparseSize, tail);
        if (tail.ctl & 0x02) {
            RunChunkIterationPass(reparseData, reparseSize);
        }
    }
    if (style <= 2 && memData != NULL) {
        drwav_free(memData, NULL);
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzedDataProvider fdp(data, size);

    /* Control bytes come from the back of the input first so the front can
     * carry the payload/stream (see the layout comment at the top). */
    TailCfg tail;
    tail.seekTarget1 = fdp.ConsumeIntegralInRange<drwav_uint64>(0, 100000);
    tail.seekTarget2 = fdp.ConsumeIntegralInRange<drwav_uint64>(0, 100000);
    tail.ctl = fdp.ConsumeIntegral<uint8_t>();

    uint8_t mode = fdp.ConsumeIntegral<uint8_t>() % 4;

    if (mode == 3) {
        size_t remaining = fdp.remaining_bytes();
        if (remaining == 0) {
            return 0;
        }
        std::vector<uint8_t> stream = fdp.ConsumeBytes<uint8_t>(remaining);
        ParseWithMetadata(stream.data(), stream.size(), tail);
        if (tail.ctl & 0x02) {
            RunChunkIterationPass(stream.data(), stream.size());
        }
        return 0;
    }

    RunRoundTrip(fdp, tail);
    return 0;
}
