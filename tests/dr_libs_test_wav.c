#define DR_WAV_LIBSNDFILE_COMPAT
#define DR_WAV_IMPLEMENTATION
#include "../dr_wav.h"

typedef struct
{
    drwav_container container;
    drwav_uint16 formatTag;
    drwav_uint32 channels;
    drwav_uint32 sampleRate;
    drwav_uint32 bitsPerSample;
    drwav_uint64 framesPerBlock;
    drwav_uint64 declaredFrameCount;
    drwav_uint64 frameCount;
    size_t sampleDataSize;
    void* pSamples;
} dr_wav_decoded_data;

typedef struct
{
    dr_wav_decoded_data s32;
    dr_wav_decoded_data s16;
    dr_wav_decoded_data f32;
    void* pFloatSamples;
} dr_wav_reference_data;

static drwav_uint64 dr_wav_frames_per_block(const drwav* pWav)
{
    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ADPCM) {
        return 2 + ((drwav_uint64)pWav->fmt.blockAlign - 7 * pWav->channels) * 2 / pWav->channels;
    }

    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_DVI_ADPCM) {
        return 1 + ((drwav_uint64)pWav->fmt.blockAlign - 4 * pWav->channels) * 2 / pWav->channels;
    }

    return 0;
}

static drwav_uint64 dr_wav_read_frames(drwav* pWav, dr_test_sample_format format, drwav_uint64 frameCount, void* pSamples)
{
    if (format == dr_test_sample_format_s16) {
        return drwav_read_pcm_frames_s16(pWav, frameCount, (drwav_int16*)pSamples);
    }

    if (format == dr_test_sample_format_f32) {
        return drwav_read_pcm_frames_f32(pWav, frameCount, (float*)pSamples);
    }

    return drwav_read_pcm_frames_s32(pWav, frameCount, (drwav_int32*)pSamples);
}

static int dr_wav_read_all(drwav* pWav, dr_test_sample_format format, dr_wav_decoded_data* pData)
{
    size_t bytesPerFrame;
    size_t capacityFrames;
    int exitCode = 1;

    memset(pData, 0, sizeof(*pData));

    bytesPerFrame = dr_test_sample_size(format) * pWav->channels;

    pData->container               = pWav->container;
    pData->formatTag               = pWav->translatedFormatTag;
    pData->channels                = pWav->channels;
    pData->sampleRate              = pWav->sampleRate;
    pData->bitsPerSample           = pWav->bitsPerSample;
    pData->framesPerBlock          = dr_wav_frames_per_block(pWav);
    pData->declaredFrameCount      = pWav->totalPCMFrameCount;

    capacityFrames = 4096;
    if (pData->declaredFrameCount != 0) {
        /* Leave space to detect output beyond the declared frame count. */
        capacityFrames = (size_t)pData->declaredFrameCount + 1;
    }

    pData->pSamples = malloc(capacityFrames * bytesPerFrame);
    if (pData->pSamples == NULL) {
        goto cleanup;
    }

    for (;;) {
        drwav_uint64 framesToRead;
        drwav_uint64 framesRead;
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
        framesRead = dr_wav_read_frames(pWav, format, framesToRead, pFramesOut);
        if (framesRead == 0) {
            break;
        }

        if (framesRead > framesToRead) {
            goto cleanup;
        }

        pData->frameCount += framesRead;
        if (pData->declaredFrameCount != 0 && pData->frameCount > pData->declaredFrameCount) {
            goto cleanup;
        }
    }

    if (pData->declaredFrameCount != 0 && pData->frameCount != pData->declaredFrameCount) {
        goto cleanup;
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
static int dr_wav_read_all_and_close(drwav* pWav, dr_test_sample_format format, dr_wav_decoded_data* pData)
{
    int result;

    result = dr_wav_read_all(pWav, format, pData);
    if (drwav_uninit(pWav) != DRWAV_SUCCESS) {
        free(pData->pSamples);
        pData->pSamples = NULL;
        result = 1;
    }

    return result;
}

static int dr_test_wav_decoding_sndfile(const dr_test_log* pLog, const char* pFilePath, dr_wav_reference_data* pReferences)
{
    const dr_wav_decoded_data* pReference;
    SF_INFO info = {0};
    SNDFILE* pFile = NULL;
    drwav_int32* pSamples = NULL;
    sf_count_t framesRead;
    size_t sampleCount;
    size_t allocationSize;
    int sourceFormat;
    dr_bool32 formatMatches;
    int exitCode = 1;

    pReference = &pReferences->s32;

    pFile = sf_open(pFilePath, SFM_READ, &info);
    if (pFile == NULL) {
        dr_test_log_errorf(pLog, "Failed to open with libsndfile: %s.\n", sf_strerror(NULL));
        return 1;
    }

    formatMatches = info.channels > 0 && (drwav_uint32)info.channels == pReference->channels;
    formatMatches = formatMatches && info.samplerate > 0 && (drwav_uint32)info.samplerate == pReference->sampleRate;
    formatMatches = formatMatches && info.frames >= 0 && (drwav_uint64)info.frames == pReference->frameCount;
    if (!formatMatches) {
        dr_test_log_errorf(pLog, "Format properties or frame counts differ with libsndfile.\n");
        goto cleanup;
    }

    sampleCount = (size_t)pReference->frameCount * pReference->channels;
    allocationSize = sampleCount;
    if (allocationSize == 0) {
        allocationSize = 1;
    }

    pSamples = (drwav_int32*)malloc(allocationSize * sizeof(*pSamples));
    if (pSamples == NULL) {
        dr_test_log_errorf(pLog, "Out of memory for libsndfile decoding.\n");
        goto cleanup;
    }

    sourceFormat = info.format & SF_FORMAT_SUBMASK;
    if (pReference->formatTag == DR_WAVE_FORMAT_IEEE_FLOAT) {
        /* Keep source float samples: s32 cannot preserve their range or precision. */
        if (sourceFormat == SF_FORMAT_FLOAT && pReference->bitsPerSample == 32) {
            pReferences->pFloatSamples = malloc(allocationSize * sizeof(float));
            if (pReferences->pFloatSamples == NULL) {
                goto cleanup;
            }

            framesRead = sf_readf_float(pFile, (float*)pReferences->pFloatSamples, info.frames);
            if (framesRead == info.frames) {
                drwav_f32_to_s32(pSamples, (const float*)pReferences->pFloatSamples, sampleCount);
            }
        } else if (sourceFormat == SF_FORMAT_DOUBLE && pReference->bitsPerSample == 64) {
            pReferences->pFloatSamples = malloc(allocationSize * sizeof(double));
            if (pReferences->pFloatSamples == NULL) {
                goto cleanup;
            }

            framesRead = sf_readf_double(pFile, (double*)pReferences->pFloatSamples, info.frames);
            if (framesRead == info.frames) {
                drwav_f64_to_s32(pSamples, (const double*)pReferences->pFloatSamples, sampleCount);
            }
        } else {
            dr_test_log_errorf(pLog, "Float source formats differ with libsndfile.\n");
            goto cleanup;
        }
    } else {
        framesRead = sf_readf_int(pFile, (int*)pSamples, info.frames);
    }

    if (framesRead != info.frames || sf_error(pFile) != SF_ERR_NO_ERROR) {
        dr_test_log_errorf(pLog, "Failed to decode with libsndfile.\n");
        goto cleanup;
    }

    if (memcmp(pReference->pSamples, pSamples, pReference->sampleDataSize) != 0) {
        dr_test_log_errorf(pLog, "Decoded s32 samples differ with libsndfile.\n");
        goto cleanup;
    }

    exitCode = 0;

cleanup:
    free(pSamples);
    if (sf_close(pFile) != 0) {
        exitCode = 1;
    }

    return exitCode;
}

static int dr_wav_load_reference(const dr_test_log* pLog, const char* pFilePath, dr_test_sample_format format, dr_wav_decoded_data* pReference)
{
    drwav wav;

    if (!drwav_init_file(&wav, pFilePath, NULL)) {
        dr_test_log_errorf(pLog, "Failed to open with drwav_init_file().\n");
        return 1;
    }

    if (dr_wav_read_all_and_close(&wav, format, pReference) != 0) {
        dr_test_log_errorf(pLog, "Failed to decode %s reference.\n", dr_test_sample_format_name(format));
        return 1;
    }

    return 0;
}

static int dr_test_wav_compare_format(const dr_test_log* pLog, const dr_wav_reference_data* pReferences, dr_test_sample_format format, const dr_wav_decoded_data* pDecoded)
{
    const dr_wav_decoded_data* pReference;
    const drwav_int32* pSamples;
    size_t sampleCount;
    size_t iSample;
    dr_bool32 formatMatches;

    pReference = &pReferences->s32;

    sampleCount = pReference->sampleDataSize / sizeof(drwav_int32);
    formatMatches = pReference->container == pDecoded->container;
    formatMatches = formatMatches && pReference->formatTag == pDecoded->formatTag;
    formatMatches = formatMatches && pReference->channels == pDecoded->channels;
    formatMatches = formatMatches && pReference->sampleRate == pDecoded->sampleRate;
    formatMatches = formatMatches && pReference->bitsPerSample == pDecoded->bitsPerSample;
    formatMatches = formatMatches && pReference->framesPerBlock == pDecoded->framesPerBlock;
    formatMatches = formatMatches && pReference->declaredFrameCount == pDecoded->declaredFrameCount;
    formatMatches = formatMatches && pReference->frameCount == pDecoded->frameCount;
    formatMatches = formatMatches && pDecoded->sampleDataSize == sampleCount * dr_test_sample_size(format);
    if (!formatMatches) {
        dr_test_log_errorf(pLog, "WAV %s format properties or frame count differ from s32.\n", dr_test_sample_format_name(format));
        return 1;
    }

    pSamples = (const drwav_int32*)pReference->pSamples;
    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        if (format == dr_test_sample_format_s16) {
            drwav_int32 expected;

            /* Floor division discards low bits without a signed right shift. */
            expected = pSamples[iSample] / 65536;
            if (pSamples[iSample] < 0 && pSamples[iSample] % 65536 != 0) {
                expected -= 1;
            }

            if (pReferences->pFloatSamples != NULL) {
                double x;

                if (pReference->bitsPerSample == 32) {
                    x = ((const float*)pReferences->pFloatSamples)[iSample];
                } else {
                    x = ((const double*)pReferences->pFloatSamples)[iSample];
                }

                if (x != x) {
                    expected = 0;
                } else if (x <= -1) {
                    expected = -32768;
                } else if (x >= 1) {
                    expected = 32767;
                } else {
                    expected = (drwav_int32)(x * 32768.0);
                }
            }

            if (((const drwav_int16*)pDecoded->pSamples)[iSample] != expected) {
                dr_test_log_errorf(pLog, "WAV s16 sample %lu differs from the expected conversion.\n", (unsigned long)iSample);
                return 1;
            }
        } else {
            float expected;

            expected = (float)(pSamples[iSample] / 2147483648.0);
            if (pReferences->pFloatSamples != NULL) {
                if (pReference->bitsPerSample == 32) {
                    expected = ((const float*)pReferences->pFloatSamples)[iSample];
                } else {
                    expected = (float)((const double*)pReferences->pFloatSamples)[iSample];
                }
            }

            if (!dr_test_f32_equal(((const float*)pDecoded->pSamples)[iSample], expected)) {
                dr_test_log_errorf(pLog, "WAV f32 sample %lu differs from the expected conversion.\n", (unsigned long)iSample);
                return 1;
            }
        }
    }

    return 0;
}

static int dr_test_wav_decoding_zero_read(const dr_test_log* pLog, drwav* pWav, dr_test_sample_format format, void* pSamples, size_t bufferSize)
{
    drwav_uint64 cursor;
    size_t iByte;

    cursor = pWav->readCursorInPCMFrames;
    memset(pSamples, 0xA5, bufferSize);
    if (dr_wav_read_frames(pWav, format, 0, pSamples) != 0 || pWav->readCursorInPCMFrames != cursor) {
        dr_test_log_errorf(pLog, "WAV %s zero-frame read returned frames or changed position.\n", dr_test_sample_format_name(format));
        return 1;
    }

    for (iByte = 0; iByte < bufferSize; iByte += 1) {
        if (((const unsigned char*)pSamples)[iByte] != 0xA5) {
            dr_test_log_errorf(pLog, "WAV %s zero-frame read changed the output buffer.\n", dr_test_sample_format_name(format));
            return 1;
        }
    }

    if (dr_wav_read_frames(pWav, format, 0, NULL) != 0 || pWav->readCursorInPCMFrames != cursor) {
        dr_test_log_errorf(pLog, "WAV %s zero-frame NULL-output read returned frames or changed position.\n", dr_test_sample_format_name(format));
        return 1;
    }

    return 0;
}

/* Uninitializes the decoder before returning. framesPerRead must be greater than zero. */
static int dr_test_wav_decoding_read_size(const dr_test_log* pLog, drwav* pWav, dr_test_sample_format format, const dr_wav_decoded_data* pReference, drwav_uint64 framesPerRead)
{
    void* pSamples = NULL;
    drwav_uint64 totalFramesRead = 0;
    size_t bytesPerFrame;
    size_t bufferSize;
    dr_bool32 formatMatches;
    int exitCode = 1;

    bytesPerFrame = dr_test_sample_size(format) * pReference->channels;
    bufferSize = (size_t)framesPerRead * bytesPerFrame;

    formatMatches = pWav->container == pReference->container;
    formatMatches = formatMatches && pWav->translatedFormatTag == pReference->formatTag;
    formatMatches = formatMatches && pWav->channels == pReference->channels;
    formatMatches = formatMatches && pWav->sampleRate == pReference->sampleRate;
    formatMatches = formatMatches && pWav->bitsPerSample == pReference->bitsPerSample;
    formatMatches = formatMatches && dr_wav_frames_per_block(pWav) == pReference->framesPerBlock;
    formatMatches = formatMatches && pWav->totalPCMFrameCount == pReference->declaredFrameCount;
    if (!formatMatches) {
        dr_test_log_errorf(pLog, "Format properties differ for chunked WAV reads.\n");
        goto cleanup;
    }

    pSamples = malloc(bufferSize);
    if (pSamples == NULL) {
        dr_test_log_errorf(pLog, "Out of memory for chunked WAV reads.\n");
        goto cleanup;
    }

    if (dr_test_wav_decoding_zero_read(pLog, pWav, format, pSamples, bufferSize) != 0) {
        goto cleanup;
    }

    for (;;) {
        drwav_uint64 framesRead;
        const void* pExpected;

        framesRead = dr_wav_read_frames(pWav, format, framesPerRead, pSamples);
        if (framesRead > framesPerRead || framesRead > pReference->frameCount - totalFramesRead) {
            dr_test_log_errorf(pLog, "Chunked WAV read returned too many frames.\n");
            goto cleanup;
        }

        if (pWav->readCursorInPCMFrames != totalFramesRead + framesRead) {
            dr_test_log_errorf(pLog, "Chunked WAV read changed the cursor incorrectly.\n");
            goto cleanup;
        }

        pExpected = (const unsigned char*)pReference->pSamples + (size_t)totalFramesRead * bytesPerFrame;
        if (memcmp(pExpected, pSamples, (size_t)framesRead * bytesPerFrame) != 0) {
            dr_test_log_errorf(pLog, "Chunked WAV %s output differs from the reference.\n", dr_test_sample_format_name(format));
            goto cleanup;
        }

        /* Check buffered state after the first read without repeating this for every chunk. */
        if (totalFramesRead == 0 && dr_test_wav_decoding_zero_read(pLog, pWav, format, pSamples, bufferSize) != 0) {
            goto cleanup;
        }

        totalFramesRead += framesRead;
        if (framesRead < framesPerRead) {
            if (totalFramesRead != pReference->frameCount) {
                dr_test_log_errorf(pLog, "Chunked WAV read reached EOF too early.\n");
                goto cleanup;
            }

            break;
        }
    }

    if (dr_test_wav_decoding_zero_read(pLog, pWav, format, pSamples, bufferSize) != 0) {
        goto cleanup;
    }

    if (dr_wav_read_frames(pWav, format, framesPerRead, pSamples) != 0 || pWav->readCursorInPCMFrames != totalFramesRead) {
        dr_test_log_errorf(pLog, "Chunked WAV read returned frames or changed position at EOF.\n");
        goto cleanup;
    }

    exitCode = 0;

cleanup:
    free(pSamples);
    if (drwav_uninit(pWav) != DRWAV_SUCCESS) {
        exitCode = 1;
    }

    return exitCode;
}

static int dr_test_wav_decoding_initialization(const dr_test_log* pLog, const char* pFilePath, const dr_wav_decoded_data* pReference)
{
    static const char* variantNames[] = {
        "drwav_init_file_with_metadata",
        "drwav_init_memory",
        "drwav_init_memory_with_metadata"
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
        drwav wav;
        drwav_bool32 initialized;

        if (iVariant == 0) {
            initialized = drwav_init_file_with_metadata(&wav, pFilePath, 0, NULL);
        } else if (iVariant == 1) {
            initialized = drwav_init_memory(&wav, pFileData, fileDataSize, NULL);
        } else {
            initialized = drwav_init_memory_with_metadata(&wav, pFileData, fileDataSize, 0, NULL);
        }

        if (!initialized) {
            dr_test_log_errorf(pLog, "Failed to open with %s().\n", variantNames[iVariant]);
            goto cleanup;
        }

        if (dr_test_wav_decoding_read_size(pLog, &wav, dr_test_sample_format_s32, pReference, 1234) != 0) {
            dr_test_log_errorf(pLog, "WAV s32 decoding failed with %s().\n", variantNames[iVariant]);
            goto cleanup;
        }
    }

    exitCode = 0;

cleanup:
    fs_free(pFileData, NULL);

    return exitCode;
}

static int dr_test_wav_decoding_skip(const dr_test_log* pLog, const char* pFilePath, const dr_wav_decoded_data* pReference)
{
    drwav wav;
    drwav* pWav = NULL;
    drwav_int32* pSamples = NULL;
    drwav_uint64 skipSizes[3];
    drwav_uint64 cursor = 0;
    size_t bytesPerFrame;
    size_t iBlock;
    int exitCode = 1;

    if (pReference->frameCount < 2) {
        return 0;  /* Need at least one frame to skip and one to read. */
    }

    pWav = &wav;

    skipSizes[0] = 17;
    skipSizes[1] = pReference->framesPerBlock + 1;
    skipSizes[2] = 1234;

    bytesPerFrame = sizeof(drwav_int32) * pReference->channels;

    if (!drwav_init_file(pWav, pFilePath, NULL)) {
        dr_test_log_errorf(pLog, "Failed to open for WAV NULL-output reads.\n");
        return 1;
    }

    pSamples = (drwav_int32*)malloc(1234 * bytesPerFrame);
    if (pSamples == NULL) {
        dr_test_log_errorf(pLog, "Out of memory for WAV NULL-output reads.\n");
        goto cleanup;
    }

    /* Check only three skip/read pairs, including an ADPCM block boundary when present. */
    for (iBlock = 0; iBlock < sizeof(skipSizes) / sizeof(skipSizes[0]) && pReference->frameCount - cursor > 1; iBlock += 1) {
        drwav_uint64 framesToSkip;
        drwav_uint64 framesToRead;
        const void* pExpected;

        framesToSkip = skipSizes[iBlock];
        if (framesToSkip >= pReference->frameCount - cursor) {
            framesToSkip = (pReference->frameCount - cursor) / 2;
        }

        if (drwav_read_pcm_frames_s32(pWav, framesToSkip, NULL) != framesToSkip || pWav->readCursorInPCMFrames != cursor + framesToSkip) {
            dr_test_log_errorf(pLog, "WAV NULL-output read returned an incorrect frame count or cursor.\n");
            goto cleanup;
        }

        cursor += framesToSkip;
        framesToRead = pReference->frameCount - cursor;
        if (framesToRead > 1234) {
            framesToRead = 1234;
        }

        if (drwav_read_pcm_frames_s32(pWav, framesToRead, pSamples) != framesToRead || pWav->readCursorInPCMFrames != cursor + framesToRead) {
            dr_test_log_errorf(pLog, "WAV read after a NULL-output read returned an incorrect frame count or cursor.\n");
            goto cleanup;
        }

        pExpected = (const unsigned char*)pReference->pSamples + (size_t)cursor * bytesPerFrame;
        if (memcmp(pExpected, pSamples, (size_t)framesToRead * bytesPerFrame) != 0) {
            dr_test_log_errorf(pLog, "WAV samples differ after a NULL-output read.\n");
            goto cleanup;
        }

        cursor += framesToRead;
    }

    exitCode = 0;

cleanup:
    free(pSamples);
    if (drwav_uninit(pWav) != DRWAV_SUCCESS) {
        exitCode = 1;
    }

    return exitCode;
}

static int dr_test_wav_decoding(const dr_test_log* pLog, const char* pFilePath, dr_wav_reference_data* pReferences)
{
    drwav wav;

    if (dr_wav_load_reference(pLog, pFilePath, dr_test_sample_format_s32, &pReferences->s32) != 0) {
        return 1;
    }

    if (dr_test_wav_decoding_sndfile(pLog, pFilePath, pReferences) != 0) {
        return 1;
    }

    if (dr_wav_load_reference(pLog, pFilePath, dr_test_sample_format_s16, &pReferences->s16) != 0) {
        return 1;
    }

    if (dr_test_wav_compare_format(pLog, pReferences, dr_test_sample_format_s16, &pReferences->s16) != 0) {
        return 1;
    }

    if (dr_wav_load_reference(pLog, pFilePath, dr_test_sample_format_f32, &pReferences->f32) != 0) {
        return 1;
    }

    if (dr_test_wav_compare_format(pLog, pReferences, dr_test_sample_format_f32, &pReferences->f32) != 0) {
        return 1;
    }

    if (!drwav_init_file(&wav, pFilePath, NULL)) {
        dr_test_log_errorf(pLog, "Failed to open for chunked WAV reads.\n");
        return 1;
    }

    if (dr_test_wav_decoding_read_size(pLog, &wav, dr_test_sample_format_s32, &pReferences->s32, 1234) != 0) {
        return 1;
    }

    if (!drwav_init_file(&wav, pFilePath, NULL)) {
        dr_test_log_errorf(pLog, "Failed to open for chunked WAV reads.\n");
        return 1;
    }

    if (dr_test_wav_decoding_read_size(pLog, &wav, dr_test_sample_format_s16, &pReferences->s16, 1234) != 0) {
        return 1;
    }

    if (!drwav_init_file(&wav, pFilePath, NULL)) {
        dr_test_log_errorf(pLog, "Failed to open for chunked WAV reads.\n");
        return 1;
    }

    if (dr_test_wav_decoding_read_size(pLog, &wav, dr_test_sample_format_f32, &pReferences->f32, 1234) != 0) {
        return 1;
    }

    if (dr_test_wav_decoding_initialization(pLog, pFilePath, &pReferences->s32) != 0) {
        return 1;
    }

    if (dr_test_wav_decoding_skip(pLog, pFilePath, &pReferences->s32) != 0) {
        return 1;
    }

    return 0;
}

static int dr_test_wav_seek_to(const dr_test_log* pLog, drwav* pWav, drwav_uint64 target)
{
    if (!drwav_seek_to_pcm_frame(pWav, target)) {
        dr_test_log_errorf(pLog, "WAV seek failed at PCM frame %llu.\n", target);
        return 1;
    }

    if (pWav->readCursorInPCMFrames != target) {
        dr_test_log_errorf(pLog, "WAV cursor differs from seek target %llu.\n", target);
        return 1;
    }

    return 0;
}

static drwav_uint64 dr_test_wav_seek_read_count(const dr_wav_decoded_data* pReference, drwav_uint64 target)
{
    drwav_uint64 count;
    drwav_uint64 limit;

    count = pReference->frameCount - target;
    limit = 2468;
    if (pReference->framesPerBlock != 0) {
        limit = pReference->framesPerBlock * 2 + 2;
    }

    if (count > limit) {
        count = limit;
    }

    return count;
}

static int dr_test_wav_seek_read_and_compare(const dr_test_log* pLog, drwav* pWav, const dr_wav_decoded_data* pReference, drwav_uint64 target, drwav_uint64 count, drwav_int32* pSamples)
{
    size_t bytesPerFrame;

    if (pWav->readCursorInPCMFrames != target) {
        dr_test_log_errorf(pLog, "WAV cursor differs before reading PCM frame %llu.\n", target);
        return 1;
    }

    bytesPerFrame = sizeof(drwav_int32) * pReference->channels;

    while (count > 0) {
        drwav_uint64 framesToRead;
        const void* pExpected;

        framesToRead = count;
        if (framesToRead > 1234) {
            framesToRead = 1234;
        }

        if (drwav_read_pcm_frames_s32(pWav, framesToRead, pSamples) != framesToRead || pWav->readCursorInPCMFrames != target + framesToRead) {
            dr_test_log_errorf(pLog, "WAV read after seeking returned an incorrect frame count or cursor.\n");
            return 1;
        }

        pExpected = (const unsigned char*)pReference->pSamples + (size_t)target * bytesPerFrame;

        if (memcmp(pExpected, pSamples, (size_t)framesToRead * bytesPerFrame) != 0) {
            dr_test_log_errorf(pLog, "WAV samples differ after seeking at PCM frame %llu.\n", target);
            return 1;
        }

        target += framesToRead;
        count  -= framesToRead;
    }

    if (target == pReference->frameCount && (drwav_read_pcm_frames_s32(pWav, 1234, pSamples) != 0 || pWav->readCursorInPCMFrames != target)) {
        dr_test_log_errorf(pLog, "WAV read returned frames or changed position at EOF after seeking.\n");
        return 1;
    }

    return 0;
}

static int dr_test_wav_seek_and_compare(const dr_test_log* pLog, drwav* pWav, const dr_wav_decoded_data* pReference, drwav_uint64 target, drwav_int32* pSamples)
{
    if (dr_test_wav_seek_to(pLog, pWav, target) != 0) {
        return 1;
    }

    return dr_test_wav_seek_read_and_compare(pLog, pWav, pReference, target, dr_test_wav_seek_read_count(pReference, target), pSamples);
}

static int dr_test_wav_seek_near(const dr_test_log* pLog, drwav* pWav, const dr_wav_decoded_data* pReference, drwav_uint64 boundary, drwav_int32* pSamples)
{
    unsigned int iOffset;

    for (iOffset = 0; iOffset < 3; iOffset += 1) {
        drwav_uint64 target;

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

        if (dr_test_wav_seek_and_compare(pLog, pWav, pReference, target, pSamples) != 0) {
            return 1;
        }
    }

    return 0;
}

static int dr_test_wav_seek_boundaries(const dr_test_log* pLog, drwav* pWav, const dr_wav_decoded_data* pReference, const drwav_uint64* pAnchors, drwav_int32* pSamples)
{
    size_t iAnchor;

    if (pReference->framesPerBlock == 0) {
        return 0;  /* Uncompressed formats have no decoder block state. */
    }

    for (iAnchor = 0; iAnchor < 3; iAnchor += 1) {
        drwav_uint64 blockStart;
        drwav_uint64 blockEnd;

        blockStart = pAnchors[iAnchor] / pReference->framesPerBlock * pReference->framesPerBlock;
        blockEnd = blockStart + pReference->framesPerBlock;

        if (dr_test_wav_seek_near(pLog, pWav, pReference, blockStart, pSamples) != 0 || dr_test_wav_seek_near(pLog, pWav, pReference, blockEnd, pSamples) != 0) {
            return 1;
        }
    }

    return 0;
}

static int dr_test_wav_seek_partial_reads(const dr_test_log* pLog, drwav* pWav, const dr_wav_decoded_data* pReference, const drwav_uint64* pAnchors, drwav_int32* pSamples)
{
    size_t iAnchor;
    unsigned int iTarget;
    drwav_uint64 blockSize;

    blockSize = pReference->framesPerBlock;
    if (blockSize == 0) {
        blockSize = 1234;  /* Use logical read boundaries for uncompressed formats. */
    }

    for (iAnchor = 0; iAnchor < 2; iAnchor += 1) {
        for (iTarget = 0; iTarget < 3; iTarget += 1) {
            drwav_uint64 count;
            drwav_uint64 frameStart;
            drwav_uint64 frameEnd;
            drwav_uint64 target;

            count = pReference->frameCount - pAnchors[iAnchor];
            if (count > 17) {
                count = 17;
            }

            if (dr_test_wav_seek_to(pLog, pWav, pAnchors[iAnchor]) != 0 || dr_test_wav_seek_read_and_compare(pLog, pWav, pReference, pAnchors[iAnchor], count, pSamples) != 0) {
                return 1;
            }

            frameStart = (pWav->readCursorInPCMFrames - 1) / blockSize * blockSize;
            frameEnd = frameStart + blockSize;
            if (frameEnd > pReference->frameCount) {
                frameEnd = pReference->frameCount;
            }

            if (iTarget == 0) {
                target = frameStart + (pWav->readCursorInPCMFrames - frameStart) / 2;
            } else if (iTarget == 1) {
                if (frameEnd - pWav->readCursorInPCMFrames < 2) {
                    continue;
                }

                target = pWav->readCursorInPCMFrames + (frameEnd - pWav->readCursorInPCMFrames) / 2;
            } else {
                if (frameStart > 0) {
                    target = 0;
                } else if (frameEnd < pReference->frameCount) {
                    target = pReference->frameCount - 1;
                } else {
                    continue;  /* This file has no other block to seek to. */
                }
            }

            if (dr_test_wav_seek_and_compare(pLog, pWav, pReference, target, pSamples) != 0) {
                return 1;
            }
        }
    }

    return 0;
}

static int dr_test_wav_seek_cases(const dr_test_log* pLog, drwav* pWav, const dr_wav_decoded_data* pReference, drwav_int32* pSamples)
{
    drwav_uint64 anchors[3] = {0};
    drwav_uint64 sequence[6] = {0};
    size_t iTarget;

    /* The reference decoder is already at EOF. */
    if (pWav->readCursorInPCMFrames != pReference->frameCount && pReference->frameCount > 0) {
        if (dr_test_wav_seek_and_compare(pLog, pWav, pReference, pReference->frameCount - 1, pSamples) != 0) {
            return 1;
        }
    }

    if (dr_test_wav_seek_read_and_compare(pLog, pWav, pReference, pReference->frameCount, 0, pSamples) != 0 || dr_test_wav_seek_and_compare(pLog, pWav, pReference, 0, pSamples) != 0) {
        dr_test_log_errorf(pLog, "WAV rewind after EOF failed.\n");
        return 1;
    }

    if (pReference->frameCount == 0) {
        return 0;
    }


    anchors[1] = pReference->frameCount / 2;
    anchors[2] = pReference->frameCount - 1;

    for (iTarget = 0; iTarget < 3; iTarget += 1) {
        if (dr_test_wav_seek_and_compare(pLog, pWav, pReference, anchors[iTarget], pSamples) != 0) {
            dr_test_log_errorf(pLog, "WAV basic-position seek failed.\n");
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
        if (dr_test_wav_seek_and_compare(pLog, pWav, pReference, sequence[iTarget], pSamples) != 0) {
            dr_test_log_errorf(pLog, "WAV forward/backward seek sequence failed.\n");
            return 1;
        }
    }


    if (dr_test_wav_seek_boundaries(pLog, pWav, pReference, anchors, pSamples) != 0) {
        dr_test_log_errorf(pLog, "WAV ADPCM block boundary test failed.\n");
        return 1;
    }

    for (iTarget = 0; iTarget < 3; iTarget += 1) {
        drwav_uint64 cursor;

        if (dr_test_wav_seek_to(pLog, pWav, anchors[iTarget]) != 0 || dr_test_wav_seek_to(pLog, pWav, anchors[iTarget]) != 0) {
            return 1;
        }

        if (dr_test_wav_seek_read_and_compare(pLog, pWav, pReference, anchors[iTarget], dr_test_wav_seek_read_count(pReference, anchors[iTarget]), pSamples) != 0) {
            return 1;
        }

        cursor = pWav->readCursorInPCMFrames;

        if (dr_test_wav_seek_and_compare(pLog, pWav, pReference, cursor, pSamples) != 0) {
            dr_test_log_errorf(pLog, "WAV seek to the current position failed.\n");
            return 1;
        }
    }

    if (dr_test_wav_seek_partial_reads(pLog, pWav, pReference, anchors, pSamples) != 0) {
        dr_test_log_errorf(pLog, "WAV seek after a partial read failed.\n");
        return 1;
    }

    return 0;
}

static int dr_test_wav_seeking(const dr_test_log* pLog, const char* pFilePath)
{
    drwav wav;
    dr_wav_decoded_data reference = {0};
    drwav_int32* pSamples = NULL;
    int exitCode = 1;

    if (!drwav_init_file(&wav, pFilePath, NULL)) {
        dr_test_log_errorf(pLog, "Failed to open for WAV seeking.\n");
        return 1;
    }

    /* No libsndfile comparison here. Keep this decoder at EOF for the seek cases. */
    if (dr_wav_read_all(&wav, dr_test_sample_format_s32, &reference) != 0) {
        dr_test_log_errorf(pLog, "Failed to decode the seeking reference.\n");
        goto cleanup;
    }

    pSamples = (drwav_int32*)malloc(1234 * reference.channels * sizeof(drwav_int32));
    if (pSamples == NULL) {
        dr_test_log_errorf(pLog, "Out of memory for WAV seeking tests.\n");
        goto cleanup;
    }

    if (dr_test_wav_seek_cases(pLog, &wav, &reference, pSamples) != 0) {
        dr_test_log_errorf(pLog, "WAV seeking failed.\n");
        goto cleanup;
    }

    exitCode = 0;

cleanup:
    free(pSamples);
    free(reference.pSamples);
    if (drwav_uninit(&wav) != DRWAV_SUCCESS) {
        exitCode = 1;
    }

    return exitCode;
}

static int dr_test_wav_seeking_basic(const dr_test_log* pLog, const char* pFilePath)
{
    static const drwav_uint64 targets[] = {17, 1234, 0};
    drwav wav;
    size_t iTarget;

    if (!drwav_init_file(&wav, pFilePath, NULL)) {
        dr_test_log_errorf(pLog, "Failed to open for basic WAV seeking.\n");
        return 1;
    }

    /* Failure is allowed. These requests check for crashes without requiring valid audio or a known length. */
    for (iTarget = 0; iTarget < sizeof(targets) / sizeof(targets[0]); iTarget += 1) {
        (void)drwav_seek_to_pcm_frame(&wav, targets[iTarget]);
    }

    drwav_uninit(&wav);

    return 0;
}

static int dr_test_wav_file(const dr_test_log* pLog, const char* pFilePath, dr_test_stage stage)
{
    dr_wav_reference_data references = {0};
    int result;

    if (stage == dr_test_stage_seeking_basic) {
        return dr_test_wav_seeking_basic(pLog, pFilePath);
    }

    if (stage == dr_test_stage_seeking) {
        return dr_test_wav_seeking(pLog, pFilePath);
    }

    result = dr_test_wav_decoding(pLog, pFilePath, &references);
    if (result != 0) {
        dr_test_log_errorf(pLog, "WAV correctness test failed.\n");
    }

    free(references.pFloatSamples);
    free(references.f32.pSamples);
    free(references.s16.pSamples);
    free(references.s32.pSamples);

    return result;
}
