#define DR_FLAC_IMPLEMENTATION
#include "../dr_flac.h"

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <FLAC/stream_decoder.h>
#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#endif

typedef struct
{
    drflac_container container;
    drflac_uint32 channels;
    drflac_uint32 sampleRate;
    drflac_uint32 bitsPerSample;
    drflac_uint16 maxBlockSizeInPCMFrames;
    drflac_uint64 declaredFrameCount;
    drflac_uint64 frameCount;
    size_t sampleDataSize;
    void* pSamples;
} dr_flac_decoded_data;

typedef struct
{
    dr_flac_decoded_data s32;
    dr_flac_decoded_data s16;
    dr_flac_decoded_data f32;
} dr_flac_reference_data;

static drflac_uint64 dr_flac_read_frames(drflac* pFlac, dr_test_sample_format format, drflac_uint64 frameCount, void* pSamples)
{
    if (format == dr_test_sample_format_s16) {
        return drflac_read_pcm_frames_s16(pFlac, frameCount, (drflac_int16*)pSamples);
    }

    if (format == dr_test_sample_format_f32) {
        return drflac_read_pcm_frames_f32(pFlac, frameCount, (float*)pSamples);
    }

    return drflac_read_pcm_frames_s32(pFlac, frameCount, (drflac_int32*)pSamples);
}

static int dr_flac_read_all(drflac* pFlac, dr_test_sample_format format, dr_flac_decoded_data* pData)
{
    size_t bytesPerFrame;
    size_t capacityFrames;
    int exitCode = 1;

    memset(pData, 0, sizeof(*pData));

    bytesPerFrame = dr_test_sample_size(format) * pFlac->channels;

    pData->container               = pFlac->container;
    pData->channels                = pFlac->channels;
    pData->sampleRate              = pFlac->sampleRate;
    pData->bitsPerSample           = pFlac->bitsPerSample;
    pData->maxBlockSizeInPCMFrames = pFlac->maxBlockSizeInPCMFrames;
    pData->declaredFrameCount      = pFlac->totalPCMFrameCount;

    capacityFrames = 4096;
    if (pData->declaredFrameCount != 0) {
        /* Use the declared count as an initial capacity, but keep reading to actual EOF. */
        capacityFrames = (size_t)pData->declaredFrameCount + 1;
    }

    pData->pSamples = malloc(capacityFrames * bytesPerFrame);
    if (pData->pSamples == NULL) {
        goto cleanup;
    }

    for (;;) {
        drflac_uint64 framesToRead;
        drflac_uint64 framesRead;
        void* pFramesOut;

        if (pData->frameCount == capacityFrames) {
            size_t newCapacityFrames;
            void* pNewSamples;

            newCapacityFrames = capacityFrames * 2;

            pNewSamples = realloc(pData->pSamples, newCapacityFrames * bytesPerFrame);
            if (pNewSamples == NULL) {
                goto cleanup;
            }

            pData->pSamples = pNewSamples;
            capacityFrames = newCapacityFrames;
        }

        framesToRead = capacityFrames - (size_t)pData->frameCount;
        pFramesOut = (unsigned char*)pData->pSamples + (size_t)pData->frameCount * bytesPerFrame;
        framesRead = dr_flac_read_frames(pFlac, format, framesToRead, pFramesOut);
        if (framesRead == 0) {
            break;
        }

        if (framesRead > framesToRead) {
            goto cleanup;
        }

        pData->frameCount += framesRead;
    }

    pData->sampleDataSize = (size_t)pData->frameCount * bytesPerFrame;
    exitCode = 0;

cleanup:
    if (exitCode != 0) {
        free(pData->pSamples);
        pData->pSamples = NULL;
    }

    return exitCode;
}

/* Takes ownership of the decoder and reads all samples in the selected format. */
static int dr_flac_read_all_and_close(drflac* pFlac, dr_test_sample_format format, dr_flac_decoded_data* pData)
{
    int result;

    result = dr_flac_read_all(pFlac, format, pData);
    drflac_close(pFlac);

    return result;
}

typedef struct
{
    dr_flac_decoded_data decoded;
    const dr_test_log* pLog;
    drflac_uint64 capacityFrames;
    dr_bool32 hasStreamInfo;
    dr_bool32 readingMetadata;
    dr_bool32 recoveringMetadata;
    dr_bool32 failed;
} dr_libflac_decode_context;

static void dr_libflac_on_metadata(const FLAC__StreamDecoder* pDecoder, const FLAC__StreamMetadata* pMetadata, void* pUserData)
{
    dr_libflac_decode_context* pContext = (dr_libflac_decode_context*)pUserData;

    (void)pDecoder;

    if (pMetadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        pContext->decoded.channels           = pMetadata->data.stream_info.channels;
        pContext->decoded.sampleRate         = pMetadata->data.stream_info.sample_rate;
        pContext->decoded.bitsPerSample      = pMetadata->data.stream_info.bits_per_sample;
        pContext->decoded.declaredFrameCount = pMetadata->data.stream_info.total_samples;
        pContext->hasStreamInfo = DR_TRUE;
    }
}

static void dr_libflac_on_error(const FLAC__StreamDecoder* pDecoder, FLAC__StreamDecoderErrorStatus status, void* pUserData)
{
    dr_libflac_decode_context* pContext = (dr_libflac_decode_context*)pUserData;

    (void)pDecoder;

    if (status == FLAC__STREAM_DECODER_ERROR_STATUS_BAD_METADATA && pContext->readingMetadata && pContext->hasStreamInfo && pContext->decoded.container == drflac_container_native) {
        pContext->recoveringMetadata = DR_TRUE;
        dr_test_log_warningf(pContext->pLog, "libFLAC reported malformed metadata. Attempting audio recovery.\n");
        return;
    }

    if (status == FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC && pContext->recoveringMetadata) {
        /* Searching past malformed metadata can lose sync before the first audio frame. */
        return;
    }

    pContext->failed = DR_TRUE;
    dr_test_log_errorf(pContext->pLog, "libFLAC decoding error: %s.\n", FLAC__StreamDecoderErrorStatusString[status]);
}

static FLAC__StreamDecoderWriteStatus dr_libflac_on_write(const FLAC__StreamDecoder* pDecoder, const FLAC__Frame* pFrame, const FLAC__int32* const pSamples[], void* pUserData)
{
    dr_libflac_decode_context* pContext = (dr_libflac_decode_context*)pUserData;
    drflac_int64 scale;
    unsigned int iFrame;

    (void)pDecoder;

    if (pContext->failed || !pContext->hasStreamInfo) {
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    pContext->recoveringMetadata = DR_FALSE;

    if (pFrame->header.channels != pContext->decoded.channels) {
        pContext->failed = DR_TRUE;
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    if (pFrame->header.blocksize > pContext->capacityFrames - pContext->decoded.frameCount) {
        pContext->failed = DR_TRUE;
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    scale = (drflac_int64)1 << (32 - pFrame->header.bits_per_sample);

    for (iFrame = 0; iFrame < pFrame->header.blocksize; iFrame += 1) {
        unsigned int iChannel;

        for (iChannel = 0; iChannel < pContext->decoded.channels; iChannel += 1) {
            size_t iSample;
            drflac_int64 sample;

            iSample = ((size_t)pContext->decoded.frameCount + iFrame) * pContext->decoded.channels + iChannel;
            sample = pSamples[iChannel][iFrame];

            /* Multiplication avoids left-shifting negative signed samples. */
            sample *= scale;
            if (sample < (-2147483647 - 1) || sample > 2147483647) {
                pContext->failed = DR_TRUE;
                return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
            }

            ((drflac_int32*)pContext->decoded.pSamples)[iSample] = (drflac_int32)sample;
        }
    }

    pContext->decoded.frameCount += pFrame->header.blocksize;
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static int dr_test_flac_decoding_libflac(const dr_test_log* pLog, const char* pFilePath, const dr_flac_decoded_data* pReference)
{
    FLAC__StreamDecoder* pDecoder = NULL;
    FLAC__StreamDecoderInitStatus initStatus;
    dr_libflac_decode_context context = {0};
    dr_bool32 initialized = DR_FALSE;
    FLAC__bool metadataResult;
    dr_bool32 formatMatches;
    size_t allocationSize;
    int exitCode = 1;

    context.pLog = pLog;
    context.capacityFrames = pReference->frameCount;
    context.decoded.container = pReference->container;

    pDecoder = FLAC__stream_decoder_new();
    if (pDecoder == NULL) {
        dr_test_log_errorf(pLog, "Failed to create a libFLAC decoder.\n");
        return 1;
    }

    FLAC__stream_decoder_set_md5_checking(pDecoder, 1);
    FLAC__stream_decoder_set_metadata_respond_all(pDecoder);
    if (pReference->container == drflac_container_ogg) {
        initStatus = FLAC__stream_decoder_init_ogg_file(pDecoder, pFilePath, dr_libflac_on_write, dr_libflac_on_metadata, dr_libflac_on_error, &context);
    } else {
        initStatus = FLAC__stream_decoder_init_file(pDecoder, pFilePath, dr_libflac_on_write, dr_libflac_on_metadata, dr_libflac_on_error, &context);
    }

    if (initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        dr_test_log_errorf(pLog, "Failed to open with libFLAC: %s.\n", FLAC__StreamDecoderInitStatusString[initStatus]);
        goto cleanup;
    }

    initialized = DR_TRUE;

    context.readingMetadata = DR_TRUE;
    metadataResult = FLAC__stream_decoder_process_until_end_of_metadata(pDecoder);
    context.readingMetadata = DR_FALSE;

    if (!metadataResult) {
        /* libFLAC can stop on malformed metadata but remain ready to search for audio. */
        if (!context.recoveringMetadata || FLAC__stream_decoder_get_state(pDecoder) != FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC) {
            dr_test_log_errorf(pLog, "Failed to read metadata with libFLAC.\n");
            goto cleanup;
        }
    }

    if (!context.hasStreamInfo || context.failed) {
        dr_test_log_errorf(pLog, "Failed to read metadata with libFLAC.\n");
        goto cleanup;
    }

    formatMatches = context.decoded.channels == pReference->channels;
    formatMatches = formatMatches && context.decoded.sampleRate == pReference->sampleRate;
    formatMatches = formatMatches && context.decoded.bitsPerSample == pReference->bitsPerSample;
    formatMatches = formatMatches && context.decoded.declaredFrameCount == pReference->declaredFrameCount;
    if (!formatMatches || context.decoded.bitsPerSample == 0 || context.decoded.bitsPerSample > 32) {
        dr_test_log_errorf(pLog, "Format properties differ with libFLAC.\n");
        goto cleanup;
    }

    allocationSize = pReference->sampleDataSize;
    if (allocationSize == 0) {
        allocationSize = sizeof(drflac_int32);
    }

    context.decoded.pSamples = malloc(allocationSize);
    if (context.decoded.pSamples == NULL) {
        dr_test_log_errorf(pLog, "Out of memory while decoding with libFLAC.\n");
        goto cleanup;
    }

    if (!FLAC__stream_decoder_process_until_end_of_stream(pDecoder) || context.failed || FLAC__stream_decoder_get_state(pDecoder) != FLAC__STREAM_DECODER_END_OF_STREAM) {
        dr_test_log_errorf(pLog, "Failed to decode with libFLAC.\n");
        goto cleanup;
    }

    if (context.decoded.frameCount != pReference->frameCount) {
        dr_test_log_errorf(pLog, "Decoded frame counts differ with libFLAC.\n");
        goto cleanup;
    }

    if (memcmp(pReference->pSamples, context.decoded.pSamples, pReference->sampleDataSize) != 0) {
        dr_test_log_errorf(pLog, "Decoded s32 samples differ with libFLAC.\n");
        goto cleanup;
    }

    exitCode = 0;

cleanup:
    if (initialized) {
        if (!FLAC__stream_decoder_finish(pDecoder)) {
            if (exitCode == 0) {
                dr_test_log_errorf(pLog, "libFLAC reported an MD5 mismatch.\n");
            }

            exitCode = 1;
        }
    }

    FLAC__stream_decoder_delete(pDecoder);
    free(context.decoded.pSamples);

    return exitCode;
}

static int dr_flac_load_reference(const dr_test_log* pLog, const char* pFilePath, dr_test_sample_format format, dr_flac_decoded_data* pReference)
{
    drflac* pFlac = NULL;

    pFlac = drflac_open_file(pFilePath, NULL);
    if (pFlac == NULL) {
        dr_test_log_errorf(pLog, "Failed to open with drflac_open_file().\n");
        return 1;
    }

    if (dr_flac_read_all_and_close(pFlac, format, pReference) != 0) {
        dr_test_log_errorf(pLog, "Failed to decode %s reference.\n", dr_test_sample_format_name(format));
        return 1;
    }

    return 0;
}

static int dr_test_flac_compare_format(const dr_test_log* pLog, const dr_flac_decoded_data* pReference, dr_test_sample_format format, const dr_flac_decoded_data* pDecoded)
{
    const drflac_int32* pSamples;
    size_t sampleCount;
    size_t iSample;
    dr_bool32 formatMatches;

    sampleCount = pReference->sampleDataSize / sizeof(drflac_int32);
    formatMatches = pReference->container == pDecoded->container;
    formatMatches = formatMatches && pReference->channels == pDecoded->channels;
    formatMatches = formatMatches && pReference->sampleRate == pDecoded->sampleRate;
    formatMatches = formatMatches && pReference->bitsPerSample == pDecoded->bitsPerSample;
    formatMatches = formatMatches && pReference->maxBlockSizeInPCMFrames == pDecoded->maxBlockSizeInPCMFrames;
    formatMatches = formatMatches && pReference->declaredFrameCount == pDecoded->declaredFrameCount;
    formatMatches = formatMatches && pReference->frameCount == pDecoded->frameCount;
    formatMatches = formatMatches && pDecoded->sampleDataSize == sampleCount * dr_test_sample_size(format);
    if (!formatMatches) {
        dr_test_log_errorf(pLog, "FLAC %s format properties or frame count differ from s32.\n", dr_test_sample_format_name(format));
        return 1;
    }

    pSamples = (const drflac_int32*)pReference->pSamples;
    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        if (format == dr_test_sample_format_s16) {
            drflac_int32 expected;

            /* Floor division discards low bits without a signed right shift. */
            expected = pSamples[iSample] / 65536;
            if (pSamples[iSample] < 0 && pSamples[iSample] % 65536 != 0) {
                expected -= 1;
            }

            if (((const drflac_int16*)pDecoded->pSamples)[iSample] != expected) {
                dr_test_log_errorf(pLog, "FLAC s16 sample %lu differs from s32.\n", (unsigned long)iSample);
                return 1;
            }
        } else {
            float expected;

            expected = (float)(pSamples[iSample] / 2147483648.0);
            if (!dr_test_f32_equal(((const float*)pDecoded->pSamples)[iSample], expected)) {
                dr_test_log_errorf(pLog, "FLAC f32 sample %lu differs from s32.\n", (unsigned long)iSample);
                return 1;
            }
        }
    }

    return 0;
}

static int dr_test_flac_decoding_zero_read(const dr_test_log* pLog, drflac* pFlac, dr_test_sample_format format, void* pSamples, size_t bufferSize)
{
    drflac_uint64 cursor;
    size_t iByte;

    cursor = pFlac->currentPCMFrame;
    memset(pSamples, 0xA5, bufferSize);
    if (dr_flac_read_frames(pFlac, format, 0, pSamples) != 0 || pFlac->currentPCMFrame != cursor) {
        dr_test_log_errorf(pLog, "FLAC %s zero-frame read returned frames or changed position.\n", dr_test_sample_format_name(format));
        return 1;
    }

    for (iByte = 0; iByte < bufferSize; iByte += 1) {
        if (((const unsigned char*)pSamples)[iByte] != 0xA5) {
            dr_test_log_errorf(pLog, "FLAC %s zero-frame read changed the output buffer.\n", dr_test_sample_format_name(format));
            return 1;
        }
    }

    if (dr_flac_read_frames(pFlac, format, 0, NULL) != 0 || pFlac->currentPCMFrame != cursor) {
        dr_test_log_errorf(pLog, "FLAC %s zero-frame NULL-output read returned frames or changed position.\n", dr_test_sample_format_name(format));
        return 1;
    }

    return 0;
}

/* Takes ownership of the decoder. framesPerRead must be greater than zero. */
static int dr_test_flac_decoding_read_size(const dr_test_log* pLog, drflac* pFlac, dr_test_sample_format format, const dr_flac_decoded_data* pReference, drflac_uint64 framesPerRead)
{
    void* pSamples = NULL;
    drflac_uint64 totalFramesRead = 0;
    size_t bytesPerFrame;
    size_t bufferSize;
    dr_bool32 formatMatches;
    int exitCode = 1;

    bytesPerFrame = dr_test_sample_size(format) * pReference->channels;
    bufferSize = (size_t)framesPerRead * bytesPerFrame;

    if (pFlac == NULL) {
        dr_test_log_errorf(pLog, "Failed to initialize for chunked FLAC reads.\n");
        return 1;
    }

    formatMatches = pFlac->container == pReference->container;
    formatMatches = formatMatches && pFlac->channels == pReference->channels;
    formatMatches = formatMatches && pFlac->sampleRate == pReference->sampleRate;
    formatMatches = formatMatches && pFlac->bitsPerSample == pReference->bitsPerSample;
    formatMatches = formatMatches && pFlac->maxBlockSizeInPCMFrames == pReference->maxBlockSizeInPCMFrames;
    formatMatches = formatMatches && pFlac->totalPCMFrameCount == pReference->declaredFrameCount;
    if (!formatMatches) {
        dr_test_log_errorf(pLog, "Format properties differ for chunked FLAC reads.\n");
        goto cleanup;
    }

    pSamples = malloc(bufferSize);
    if (pSamples == NULL) {
        dr_test_log_errorf(pLog, "Out of memory for chunked FLAC reads.\n");
        goto cleanup;
    }

    if (dr_test_flac_decoding_zero_read(pLog, pFlac, format, pSamples, bufferSize) != 0) {
        goto cleanup;
    }

    for (;;) {
        drflac_uint64 framesRead;
        const void* pExpected;

        framesRead = dr_flac_read_frames(pFlac, format, framesPerRead, pSamples);
        if (framesRead > framesPerRead || framesRead > pReference->frameCount - totalFramesRead) {
            dr_test_log_errorf(pLog, "Chunked FLAC read returned too many frames.\n");
            goto cleanup;
        }

        if (pFlac->currentPCMFrame != totalFramesRead + framesRead) {
            dr_test_log_errorf(pLog, "Chunked FLAC read changed the cursor incorrectly.\n");
            goto cleanup;
        }

        pExpected = (const unsigned char*)pReference->pSamples + (size_t)totalFramesRead * bytesPerFrame;
        if (memcmp(pExpected, pSamples, (size_t)framesRead * bytesPerFrame) != 0) {
            dr_test_log_errorf(pLog, "Chunked FLAC %s output differs from the reference.\n", dr_test_sample_format_name(format));
            goto cleanup;
        }

        /* Check buffered state after the first read without repeating this for every chunk. */
        if (totalFramesRead == 0 && dr_test_flac_decoding_zero_read(pLog, pFlac, format, pSamples, bufferSize) != 0) {
            goto cleanup;
        }

        totalFramesRead += framesRead;
        if (framesRead < framesPerRead) {
            if (totalFramesRead != pReference->frameCount) {
                dr_test_log_errorf(pLog, "Chunked FLAC read reached EOF too early.\n");
                goto cleanup;
            }

            break;
        }
    }

    if (dr_test_flac_decoding_zero_read(pLog, pFlac, format, pSamples, bufferSize) != 0) {
        goto cleanup;
    }

    if (dr_flac_read_frames(pFlac, format, framesPerRead, pSamples) != 0 || pFlac->currentPCMFrame != totalFramesRead) {
        dr_test_log_errorf(pLog, "Chunked FLAC read returned frames or changed position at EOF.\n");
        goto cleanup;
    }

    exitCode = 0;

cleanup:
    free(pSamples);
    drflac_close(pFlac);

    return exitCode;
}

static int dr_test_flac_decoding_block_reads(const dr_test_log* pLog, const char* pFilePath, const dr_flac_decoded_data* pReference)
{
    drflac* pFlac = NULL;
    drflac_int32* pSamples = NULL;
    drflac_uint64 framesPerRead;
    drflac_uint64 cursor = 0;
    size_t bytesPerFrame;
    size_t iBlock;
    int exitCode = 1;

    framesPerRead = pReference->maxBlockSizeInPCMFrames;
    if (framesPerRead == 0) {
        framesPerRead = 4096;
    }

    bytesPerFrame = sizeof(drflac_int32) * pReference->channels;

    pFlac = drflac_open_file(pFilePath, NULL);
    if (pFlac == NULL) {
        dr_test_log_errorf(pLog, "Failed to open for FLAC block-sized reads.\n");
        return 1;
    }

    pSamples = (drflac_int32*)malloc((size_t)framesPerRead * bytesPerFrame);
    if (pSamples == NULL) {
        dr_test_log_errorf(pLog, "Out of memory for FLAC block-sized reads.\n");
        goto cleanup;
    }

    /* Check only three blocks instead of decoding the whole file again. */
    for (iBlock = 0; iBlock < 3; iBlock += 1) {
        drflac_uint64 expectedCount;
        const void* pExpected;

        expectedCount = pReference->frameCount - cursor;
        if (expectedCount > framesPerRead) {
            expectedCount = framesPerRead;
        }

        if (drflac_read_pcm_frames_s32(pFlac, framesPerRead, pSamples) != expectedCount || pFlac->currentPCMFrame != cursor + expectedCount) {
            dr_test_log_errorf(pLog, "FLAC block-sized read returned an incorrect frame count or cursor.\n");
            goto cleanup;
        }

        pExpected = (const unsigned char*)pReference->pSamples + (size_t)cursor * bytesPerFrame;
        if (memcmp(pExpected, pSamples, (size_t)expectedCount * bytesPerFrame) != 0) {
            dr_test_log_errorf(pLog, "FLAC block-sized read differs from the reference.\n");
            goto cleanup;
        }

        cursor += expectedCount;
        if (expectedCount < framesPerRead) {
            break;
        }
    }

    exitCode = 0;

cleanup:
    free(pSamples);
    drflac_close(pFlac);

    return exitCode;
}

static void dr_flac_on_metadata(void* pUserData, drflac_metadata* pMetadata)
{
    (void)pUserData;
    (void)pMetadata;
}

static int dr_test_flac_decoding_initialization(const dr_test_log* pLog, const char* pFilePath, const dr_flac_decoded_data* pReference)
{
    static const char* variantNames[] = {
        "drflac_open_file_with_metadata",
        "drflac_open_memory",
        "drflac_open_memory_with_metadata"
    };
    void* pFileData = NULL;
    size_t fileDataSize;
    size_t iVariant;
    fs_result result;
    int exitCode = 1;

    result = fs_file_open_and_read(NULL, pFilePath, FS_FORMAT_BINARY, &pFileData, &fileDataSize);
    if (result != FS_SUCCESS) {
        dr_test_log_errorf(pLog, "Failed to read: %s.\n", fs_result_description(result));
        return 1;
    }

    for (iVariant = 0; iVariant < sizeof(variantNames) / sizeof(variantNames[0]); iVariant += 1) {
        drflac* pFlac = NULL;

        if (iVariant == 0) {
            pFlac = drflac_open_file_with_metadata(pFilePath, dr_flac_on_metadata, NULL, NULL);
        } else if (iVariant == 1) {
            pFlac = drflac_open_memory(pFileData, fileDataSize, NULL);
        } else {
            pFlac = drflac_open_memory_with_metadata(pFileData, fileDataSize, dr_flac_on_metadata, NULL, NULL);
        }

        if (dr_test_flac_decoding_read_size(pLog, pFlac, dr_test_sample_format_s32, pReference, 1234) != 0) {
            dr_test_log_errorf(pLog, "FLAC s32 decoding failed with %s().\n", variantNames[iVariant]);
            goto cleanup;
        }
    }

    exitCode = 0;

cleanup:
    fs_free(pFileData, NULL);

    return exitCode;
}

static int dr_test_flac_decoding_skip(const dr_test_log* pLog, const char* pFilePath, const dr_flac_decoded_data* pReference)
{
    drflac* pFlac = NULL;
    drflac_int32* pSamples = NULL;
    drflac_uint64 skipSizes[3];
    drflac_uint64 cursor = 0;
    size_t bytesPerFrame;
    size_t iBlock;
    int exitCode = 1;

    if (pReference->frameCount < 2) {
        return 0;  /* Need at least one frame to skip and one to read. */
    }

    skipSizes[0] = 17;
    skipSizes[1] = pReference->maxBlockSizeInPCMFrames + 1;
    skipSizes[2] = 1234;
    
    bytesPerFrame = sizeof(drflac_int32) * pReference->channels;

    pFlac = drflac_open_file(pFilePath, NULL);
    if (pFlac == NULL) {
        dr_test_log_errorf(pLog, "Failed to open for FLAC NULL-output reads.\n");
        return 1;
    }

    pSamples = (drflac_int32*)malloc(1234 * bytesPerFrame);
    if (pSamples == NULL) {
        dr_test_log_errorf(pLog, "Out of memory for FLAC NULL-output reads.\n");
        goto cleanup;
    }

    /* Check only three skip/read pairs, including a skip across a FLAC frame boundary. */
    for (iBlock = 0; iBlock < sizeof(skipSizes) / sizeof(skipSizes[0]) && pReference->frameCount - cursor > 1; iBlock += 1) {
        drflac_uint64 framesToSkip;
        drflac_uint64 framesToRead;
        const void* pExpected;

        framesToSkip = skipSizes[iBlock];
        if (framesToSkip >= pReference->frameCount - cursor) {
            framesToSkip = (pReference->frameCount - cursor) / 2;
        }

        if (drflac_read_pcm_frames_s32(pFlac, framesToSkip, NULL) != framesToSkip || pFlac->currentPCMFrame != cursor + framesToSkip) {
            dr_test_log_errorf(pLog, "FLAC NULL-output read returned an incorrect frame count or cursor.\n");
            goto cleanup;
        }

        cursor += framesToSkip;
        framesToRead = pReference->frameCount - cursor;
        if (framesToRead > 1234) {
            framesToRead = 1234;
        }

        if (drflac_read_pcm_frames_s32(pFlac, framesToRead, pSamples) != framesToRead || pFlac->currentPCMFrame != cursor + framesToRead) {
            dr_test_log_errorf(pLog, "FLAC read after a NULL-output read returned an incorrect frame count or cursor.\n");
            goto cleanup;
        }

        pExpected = (const unsigned char*)pReference->pSamples + (size_t)cursor * bytesPerFrame;
        if (memcmp(pExpected, pSamples, (size_t)framesToRead * bytesPerFrame) != 0) {
            dr_test_log_errorf(pLog, "FLAC samples differ after a NULL-output read.\n");
            goto cleanup;
        }

        cursor += framesToRead;
    }

    exitCode = 0;

cleanup:
    free(pSamples);
    drflac_close(pFlac);

    return exitCode;
}

static int dr_test_flac_decoding(const dr_test_log* pLog, const char* pFilePath, dr_flac_reference_data* pReferences, unsigned int flags)
{
    drflac* pFlac = NULL;

    if (dr_flac_load_reference(pLog, pFilePath, dr_test_sample_format_s32, &pReferences->s32) != 0) {
        return 1;
    }

    if ((flags & dr_test_flag_skip_reference) == 0) {
        if (dr_test_flac_decoding_libflac(pLog, pFilePath, &pReferences->s32) != 0) {
            return 1;
        }
    }

    if (dr_flac_load_reference(pLog, pFilePath, dr_test_sample_format_s16, &pReferences->s16) != 0) {
        return 1;
    }

    if (dr_test_flac_compare_format(pLog, &pReferences->s32, dr_test_sample_format_s16, &pReferences->s16) != 0) {
        return 1;
    }

    if (dr_flac_load_reference(pLog, pFilePath, dr_test_sample_format_f32, &pReferences->f32) != 0) {
        return 1;
    }

    if (dr_test_flac_compare_format(pLog, &pReferences->s32, dr_test_sample_format_f32, &pReferences->f32) != 0) {
        return 1;
    }

    pFlac = drflac_open_file(pFilePath, NULL);
    if (dr_test_flac_decoding_read_size(pLog, pFlac, dr_test_sample_format_s32, &pReferences->s32, 1234) != 0) {
        return 1;
    }

    pFlac = drflac_open_file(pFilePath, NULL);
    if (dr_test_flac_decoding_read_size(pLog, pFlac, dr_test_sample_format_s16, &pReferences->s16, 1234) != 0) {
        return 1;
    }

    pFlac = drflac_open_file(pFilePath, NULL);
    if (dr_test_flac_decoding_read_size(pLog, pFlac, dr_test_sample_format_f32, &pReferences->f32, 1234) != 0) {
        return 1;
    }

    if (dr_test_flac_decoding_initialization(pLog, pFilePath, &pReferences->s32) != 0) {
        return 1;
    }

    if (dr_test_flac_decoding_block_reads(pLog, pFilePath, &pReferences->s32) != 0) {
        return 1;
    }

    if (dr_test_flac_decoding_skip(pLog, pFilePath, &pReferences->s32) != 0) {
        return 1;
    }

    return 0;
}

typedef enum
{
    dr_flac_seek_method_table,
    dr_flac_seek_method_binary,
    dr_flac_seek_method_brute,
    dr_flac_seek_method_ogg
} dr_flac_seek_method;

static dr_bool32 dr_test_flac_set_seek_method(drflac* pFlac, dr_flac_seek_method method)
{
    if (pFlac->container == drflac_container_ogg) {
        return method == dr_flac_seek_method_ogg;
    }

    /*
    We're being a little bit naughty here and inspecting and modifying the internal state of the
    pFlac object, but it's practical for keeping the test code simple.
    */
    if (method == dr_flac_seek_method_table) {
        drflac_uint32 iPoint;

        if (pFlac->pSeekpoints == NULL || pFlac->seekpointCount == 0 || pFlac->pSeekpoints[0].firstPCMFrame != 0) {
            return DR_FALSE;
        }

        /* These cases require a table that works without a fallback to another method. */
        for (iPoint = 0; iPoint < pFlac->seekpointCount; iPoint += 1) {
            if (pFlac->pSeekpoints[iPoint].pcmFrameCount == 0 || pFlac->pSeekpoints[iPoint].pcmFrameCount > pFlac->maxBlockSizeInPCMFrames) {
                return DR_FALSE;
            }

            if (iPoint > 0 && (pFlac->pSeekpoints[iPoint].firstPCMFrame <= pFlac->pSeekpoints[iPoint - 1].firstPCMFrame || pFlac->pSeekpoints[iPoint].flacFrameOffset <= pFlac->pSeekpoints[iPoint - 1].flacFrameOffset)) {
                return DR_FALSE;
            }
        }
    } else if (method == dr_flac_seek_method_binary) {
        #ifdef DR_FLAC_NO_CRC
        {
            return DR_FALSE;
        }
        #else
        {
            if (pFlac->totalPCMFrameCount == 0) {
                return DR_FALSE;
            }
        }
        #endif
    } else if (method != dr_flac_seek_method_brute) {
        return DR_FALSE;
    }

    pFlac->_noSeekTableSeek    = method != dr_flac_seek_method_table;
    pFlac->_noBinarySearchSeek = method != dr_flac_seek_method_binary;
    pFlac->_noBruteForceSeek   = method != dr_flac_seek_method_brute;

    return DR_TRUE;
}

static int dr_test_flac_seek_to(const dr_test_log* pLog, drflac* pFlac, drflac_uint64 target)
{
    if (!drflac_seek_to_pcm_frame(pFlac, target)) {
        dr_test_log_errorf(pLog, "FLAC seek failed at PCM frame %llu.\n", target);
        return 1;
    }

    if (pFlac->currentPCMFrame != target) {
        dr_test_log_errorf(pLog, "FLAC cursor differs from seek target %llu.\n", target);
        return 1;
    }

    return 0;
}

static drflac_uint64 dr_test_flac_seek_read_count(const dr_flac_decoded_data* pReference, drflac_uint64 target)
{
    drflac_uint64 count;
    drflac_uint64 limit;

    count = pReference->frameCount - target;
    limit = pReference->maxBlockSizeInPCMFrames * 2 + 2;

    if (count > limit) {
        count = limit;
    }

    return count;
}

static int dr_test_flac_seek_read_and_compare(const dr_test_log* pLog, drflac* pFlac, const dr_flac_decoded_data* pReference, drflac_uint64 target, drflac_uint64 count, drflac_int32* pSamples)
{
    size_t bytesPerFrame;

    if (pFlac->currentPCMFrame != target) {
        dr_test_log_errorf(pLog, "FLAC cursor differs before reading PCM frame %llu.\n", target);
        return 1;
    }

    bytesPerFrame = sizeof(drflac_int32) * pReference->channels;

    while (count > 0) {
        drflac_uint64 framesToRead;
        const void* pExpected;

        framesToRead = count;
        if (framesToRead > 1234) {
            framesToRead = 1234;
        }

        if (drflac_read_pcm_frames_s32(pFlac, framesToRead, pSamples) != framesToRead || pFlac->currentPCMFrame != target + framesToRead) {
            dr_test_log_errorf(pLog, "FLAC read after seeking returned an incorrect frame count or cursor.\n");
            return 1;
        }

        pExpected = (const unsigned char*)pReference->pSamples + (size_t)target * bytesPerFrame;

        if (memcmp(pExpected, pSamples, (size_t)framesToRead * bytesPerFrame) != 0) {
            dr_test_log_errorf(pLog, "FLAC samples differ after seeking at PCM frame %llu.\n", target);
            return 1;
        }

        target += framesToRead;
        count  -= framesToRead;
    }

    if (target == pReference->frameCount && (drflac_read_pcm_frames_s32(pFlac, 1234, pSamples) != 0 || pFlac->currentPCMFrame != target)) {
        dr_test_log_errorf(pLog, "FLAC read returned frames or changed position at EOF after seeking.\n");
        return 1;
    }

    return 0;
}

static int dr_test_flac_seek_and_compare(const dr_test_log* pLog, drflac* pFlac, const dr_flac_decoded_data* pReference, drflac_uint64 target, drflac_int32* pSamples)
{
    if (dr_test_flac_seek_to(pLog, pFlac, target) != 0) {
        return 1;
    }

    return dr_test_flac_seek_read_and_compare(pLog, pFlac, pReference, target, dr_test_flac_seek_read_count(pReference, target), pSamples);
}

static int dr_test_flac_seek_near(const dr_test_log* pLog, drflac* pFlac, const dr_flac_decoded_data* pReference, drflac_uint64 boundary, drflac_int32* pSamples)
{
    unsigned int iOffset;

    for (iOffset = 0; iOffset < 3; iOffset += 1) {
        drflac_uint64 target;

        target = boundary;
        if (iOffset == 0) {
            if (target == 0) {
                continue;
            }

            target -= 1;
        } else if (iOffset == 2) {
            target += 1;
        }

        if (target >= pReference->frameCount) {
            continue;
        }

        if (dr_test_flac_seek_and_compare(pLog, pFlac, pReference, target, pSamples) != 0) {
            return 1;
        }
    }

    return 0;
}

static int dr_test_flac_seek_boundaries(const dr_test_log* pLog, drflac* pFlac, const dr_flac_decoded_data* pReference, const drflac_uint64* pAnchors, drflac_int32* pSamples)
{
    size_t iAnchor;

    for (iAnchor = 0; iAnchor < 3; iAnchor += 1) {
        drflac_uint64 frameStart;
        drflac_uint64 frameEnd;

        if (dr_test_flac_seek_to(pLog, pFlac, pAnchors[iAnchor]) != 0 || dr_test_flac_seek_read_and_compare(pLog, pFlac, pReference, pAnchors[iAnchor], 1, pSamples) != 0) {
            return 1;
        }

        /* Probe actual boundaries, including streams with variable block sizes. */
        frameStart = pFlac->currentPCMFrame - (pFlac->currentFLACFrame.header.blockSizeInPCMFrames - pFlac->currentFLACFrame.pcmFramesRemaining);
        frameEnd   = pFlac->currentPCMFrame + pFlac->currentFLACFrame.pcmFramesRemaining;

        if (dr_test_flac_seek_near(pLog, pFlac, pReference, frameStart, pSamples) != 0 || dr_test_flac_seek_near(pLog, pFlac, pReference, frameEnd, pSamples) != 0) {
            return 1;
        }
    }

    if (pFlac->pSeekpoints != NULL && pFlac->seekpointCount > 0) {
        drflac_uint32 indices[3];
        size_t iPoint;

        indices[0] = 0;
        indices[1] = pFlac->seekpointCount / 2;
        indices[2] = pFlac->seekpointCount - 1;

        for (iPoint = 0; iPoint < 3; iPoint += 1) {
            drflac_uint64 target;

            target = pFlac->pSeekpoints[indices[iPoint]].firstPCMFrame;
            if (target < pReference->frameCount && dr_test_flac_seek_near(pLog, pFlac, pReference, target, pSamples) != 0) {
                return 1;
            }
        }
    }

    return 0;
}

static int dr_test_flac_seek_partial_reads(const dr_test_log* pLog, drflac* pFlac, const dr_flac_decoded_data* pReference, const drflac_uint64* pAnchors, drflac_int32* pSamples)
{
    size_t iAnchor;
    unsigned int iTarget;

    for (iAnchor = 0; iAnchor < 2; iAnchor += 1) {
        for (iTarget = 0; iTarget < 3; iTarget += 1) {
            drflac_uint64 count;
            drflac_uint64 frameStart;
            drflac_uint64 frameEnd;
            drflac_uint64 target;

            count = pReference->frameCount - pAnchors[iAnchor];
            if (count > 17) {
                count = 17;
            }

            if (dr_test_flac_seek_to(pLog, pFlac, pAnchors[iAnchor]) != 0 || dr_test_flac_seek_read_and_compare(pLog, pFlac, pReference, pAnchors[iAnchor], count, pSamples) != 0) {
                return 1;
            }

            frameStart = pFlac->currentPCMFrame - (pFlac->currentFLACFrame.header.blockSizeInPCMFrames - pFlac->currentFLACFrame.pcmFramesRemaining);
            frameEnd   = pFlac->currentPCMFrame + pFlac->currentFLACFrame.pcmFramesRemaining;

            if (iTarget == 0) {
                target = frameStart + (pFlac->currentPCMFrame - frameStart) / 2;
            } else if (iTarget == 1) {
                if (pFlac->currentFLACFrame.pcmFramesRemaining < 2) {
                    continue;
                }

                target = pFlac->currentPCMFrame + pFlac->currentFLACFrame.pcmFramesRemaining / 2;
            } else {
                if (frameStart > 0) {
                    target = 0;
                } else if (frameEnd < pReference->frameCount) {
                    target = pReference->frameCount - 1;
                } else {
                    continue;  /* This file has no other FLAC frame to seek to. */
                }
            }

            if (dr_test_flac_seek_and_compare(pLog, pFlac, pReference, target, pSamples) != 0) {
                return 1;
            }
        }
    }

    return 0;
}

static int dr_test_flac_seek_cases(const dr_test_log* pLog, drflac* pFlac, const dr_flac_decoded_data* pReference, drflac_int32* pSamples)
{
    drflac_uint64 anchors[3] = {0};
    drflac_uint64 sequence[6] = {0};
    size_t iTarget;

    /* The reference decoder is already at EOF; other methods read only the tail to get there. */
    if (pFlac->currentPCMFrame != pReference->frameCount && pReference->frameCount > 0) {
        if (dr_test_flac_seek_and_compare(pLog, pFlac, pReference, pReference->frameCount - 1, pSamples) != 0) {
            return 1;
        }
    }

    if (dr_test_flac_seek_read_and_compare(pLog, pFlac, pReference, pReference->frameCount, 0, pSamples) != 0 || dr_test_flac_seek_and_compare(pLog, pFlac, pReference, 0, pSamples) != 0) {
        dr_test_log_errorf(pLog, "FLAC rewind after EOF failed.\n");
        return 1;
    }

    if (pReference->frameCount == 0) {
        return 0;
    }

    /* Seek directly to EOF using the decoded frame count. */
    if (dr_test_flac_seek_and_compare(pLog, pFlac, pReference, pReference->frameCount, pSamples) != 0) {
        dr_test_log_errorf(pLog, "FLAC seek to EOF failed.\n");
        return 1;
    }


    anchors[1] = pReference->frameCount / 2;
    anchors[2] = pReference->frameCount - 1;

    for (iTarget = 0; iTarget < 3; iTarget += 1) {
        if (dr_test_flac_seek_and_compare(pLog, pFlac, pReference, anchors[iTarget], pSamples) != 0) {
            dr_test_log_errorf(pLog, "FLAC basic-position seek failed.\n");
            return 1;
        }
    }


    sequence[1] = anchors[2];
    sequence[3] = anchors[1];

    sequence[4] = pReference->frameCount / 3;
    if (pReference->frameCount > 1) {
        sequence[2] = 1;
    }

    for (iTarget = 0; iTarget < sizeof(sequence) / sizeof(sequence[0]); iTarget += 1) {
        if (dr_test_flac_seek_and_compare(pLog, pFlac, pReference, sequence[iTarget], pSamples) != 0) {
            dr_test_log_errorf(pLog, "FLAC forward/backward seek sequence failed.\n");
            return 1;
        }
    }


    if (dr_test_flac_seek_boundaries(pLog, pFlac, pReference, anchors, pSamples) != 0) {
        dr_test_log_errorf(pLog, "FLAC frame/seek-table boundary test failed.\n");
        return 1;
    }

    for (iTarget = 0; iTarget < 3; iTarget += 1) {
        drflac_uint64 cursor;

        if (dr_test_flac_seek_to(pLog, pFlac, anchors[iTarget]) != 0 || dr_test_flac_seek_to(pLog, pFlac, anchors[iTarget]) != 0) {
            return 1;
        }

        if (dr_test_flac_seek_read_and_compare(pLog, pFlac, pReference, anchors[iTarget], dr_test_flac_seek_read_count(pReference, anchors[iTarget]), pSamples) != 0) {
            return 1;
        }

        cursor = pFlac->currentPCMFrame;

        if (dr_test_flac_seek_and_compare(pLog, pFlac, pReference, cursor, pSamples) != 0) {
            dr_test_log_errorf(pLog, "FLAC seek to the current position failed.\n");
            return 1;
        }
    }

    if (dr_test_flac_seek_partial_reads(pLog, pFlac, pReference, anchors, pSamples) != 0) {
        dr_test_log_errorf(pLog, "FLAC seek after a partial read failed.\n");
        return 1;
    }

    return 0;
}

static int dr_test_flac_seeking(const dr_test_log* pLog, const char* pFilePath)
{
    static const dr_flac_seek_method methods[] = {
        dr_flac_seek_method_table,
        dr_flac_seek_method_binary,
        dr_flac_seek_method_brute,
        dr_flac_seek_method_ogg
    };
    
    static const char* methodNames[] = {
        "seek table",
        "binary search",
        "brute force",
        "Ogg"
    };

    drflac* pFlac = NULL;
    dr_flac_decoded_data reference = {0};
    drflac_int32* pSamples = NULL;
    size_t iMethod;
    int exitCode = 1;

    pFlac = drflac_open_file(pFilePath, NULL);
    if (pFlac == NULL) {
        dr_test_log_errorf(pLog, "Failed to open for FLAC seeking.\n");
        return 1;
    }

    /* No libFLAC comparison here. Keep this decoder at EOF for the first available method. */
    if (dr_flac_read_all(pFlac, dr_test_sample_format_s32, &reference) != 0) {
        dr_test_log_errorf(pLog, "Failed to decode the seeking reference.\n");
        goto cleanup;
    }

    if (reference.declaredFrameCount != 0 && reference.declaredFrameCount != reference.frameCount) {
        dr_test_log_warningf(pLog, "Skipping FLAC seek comparisons: STREAMINFO declares %llu PCM frames, but decoded %llu.\n", reference.declaredFrameCount, reference.frameCount);
        exitCode = 0;
        goto cleanup;
    }

    pSamples = (drflac_int32*)malloc(1234 * reference.channels * sizeof(drflac_int32));
    if (pSamples == NULL) {
        dr_test_log_errorf(pLog, "Out of memory for FLAC seeking tests.\n");
        goto cleanup;
    }

    exitCode = 0;
    for (iMethod = 0; iMethod < sizeof(methods) / sizeof(methods[0]); iMethod += 1) {
        if (reference.container == drflac_container_ogg) {
            if (methods[iMethod] != dr_flac_seek_method_ogg) {
                continue;
            }
        } else {
            if (methods[iMethod] == dr_flac_seek_method_ogg) {
                continue;
            }
        }

        if (pFlac == NULL) {
            pFlac = drflac_open_file(pFilePath, NULL);
            if (pFlac == NULL) {
                dr_test_log_errorf(pLog, "Failed to reopen for FLAC seeking.\n");
                exitCode = 1;
                break;
            }
        }

        if (!dr_test_flac_set_seek_method(pFlac, methods[iMethod])) {
            continue;
        }

        if (dr_test_flac_seek_cases(pLog, pFlac, &reference, pSamples) != 0) {
            dr_test_log_errorf(pLog, "FLAC seeking failed using %s.\n", methodNames[iMethod]);
            exitCode = 1;
        }

        drflac_close(pFlac);
        pFlac = NULL;
    }

cleanup:
    free(pSamples);
    free(reference.pSamples);
    drflac_close(pFlac);

    return exitCode;
}

static int dr_test_flac_seeking_basic(const dr_test_log* pLog, const char* pFilePath)
{
    static const drflac_uint64 targets[] = {17, 1234, 0};
    drflac* pFlac = NULL;
    size_t iTarget;

    pFlac = drflac_open_file(pFilePath, NULL);
    if (pFlac == NULL) {
        dr_test_log_errorf(pLog, "Failed to open for basic FLAC seeking.\n");
        return 1;
    }
    
    /* Failure is allowed. These requests check for crashes without requiring valid audio or a known length. */
    for (iTarget = 0; iTarget < sizeof(targets) / sizeof(targets[0]); iTarget += 1) {
        (void)drflac_seek_to_pcm_frame(pFlac, targets[iTarget]);
    }

    drflac_close(pFlac);

    return 0;
}

static int dr_test_flac_file(const dr_test_log* pLog, const char* pFilePath, dr_test_stage stage, unsigned int flags)
{
    dr_flac_reference_data references = {0};
    int result;

    if (stage == dr_test_stage_seeking_basic) {
        return dr_test_flac_seeking_basic(pLog, pFilePath);
    }

    if (stage == dr_test_stage_seeking) {
        return dr_test_flac_seeking(pLog, pFilePath);
    }

    result = dr_test_flac_decoding(pLog, pFilePath, &references, flags);
    if (result != 0) {
        dr_test_log_errorf(pLog, "FLAC correctness test failed.\n");
    }

    free(references.f32.pSamples);
    free(references.s16.pSamples);
    free(references.s32.pSamples);

    return result;
}
