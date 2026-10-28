#define DR_MP3_IMPLEMENTATION
#include "../dr_mp3.h"

#include <mpg123.h>

typedef struct
{
    drmp3_uint32 channels;
    drmp3_uint32 sampleRate;
    drmp3_uint64 frameCount;
    size_t sampleDataSize;
    void* pSamples;
} dr_mp3_decoded_data;

typedef struct
{
    dr_mp3_decoded_data s16;
    dr_mp3_decoded_data f32;
} dr_mp3_reference_data;

static drmp3_uint64 dr_mp3_read_frames(drmp3* pMP3, dr_test_sample_format format, drmp3_uint64 frameCount, void* pSamples)
{
    if (format == dr_test_sample_format_s16) {
        return drmp3_read_pcm_frames_s16(pMP3, frameCount, (drmp3_int16*)pSamples);
    }

    return drmp3_read_pcm_frames_f32(pMP3, frameCount, (float*)pSamples);
}

static drmp3_uint64 dr_mp3_cursor(const drmp3* pMP3)
{
    /* currentPCMFrame counts encoder delay; seek targets and returned audio do not. */
    if (pMP3->currentPCMFrame > pMP3->delayInPCMFrames) {
        return pMP3->currentPCMFrame - pMP3->delayInPCMFrames;
    }

    return 0;
}

static int dr_mp3_read_all(drmp3* pMP3, dr_test_sample_format format, dr_mp3_decoded_data* pData)
{
    size_t bytesPerFrame;
    size_t capacityFrames;
    int exitCode = 1;

    memset(pData, 0, sizeof(*pData));

    bytesPerFrame = dr_test_sample_size(format) * pMP3->channels;

    pData->channels   = pMP3->channels;
    pData->sampleRate = pMP3->sampleRate;
    capacityFrames = 4096;

    pData->pSamples = malloc(capacityFrames * bytesPerFrame);
    if (pData->pSamples == NULL) {
        goto cleanup;
    }

    for (;;) {
        drmp3_uint64 framesRead;
        drmp3_uint64 framesToRead;
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
        framesRead = dr_mp3_read_frames(pMP3, format, framesToRead, pFramesOut);
        if (framesRead > framesToRead || dr_mp3_cursor(pMP3) != pData->frameCount + framesRead) {
            goto cleanup;
        }

        pData->frameCount += framesRead;
        if (framesRead == 0) {
            break;
        }
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
static int dr_mp3_read_all_and_close(drmp3* pMP3, dr_test_sample_format format, dr_mp3_decoded_data* pData)
{
    int result;

    result = dr_mp3_read_all(pMP3, format, pData);
    drmp3_uninit(pMP3);

    return result;
}

static int dr_test_mp3_decoding_mpg123(const dr_test_log* pLog, const char* pFilePath, const dr_mp3_decoded_data* pReference)
{
    mpg123_handle* pDecoder = NULL;
    short samples[4096];
    drmp3_uint64 totalFrames = 0;
    int encoding;
    size_t sampleSize;
    size_t bufferSize;
    const long* pRates = NULL;
    size_t rateCount = 0;
    size_t iRate;
    double tolerance;
    double maximumDifference = 0;
    int status;
    int error = 0;
    dr_bool32 opened = DR_FALSE;
    int exitCode = 1;

    /* Match the decoder's precision, including s16 clipping for its default integer core. */
    encoding = MPG123_ENC_SIGNED_16;
    sampleSize = sizeof(samples[0]);
    bufferSize = sizeof(samples);
    tolerance = 2;  /* Two s16 units for independent scaling and rounding. */

    if (mpg123_init() != MPG123_OK) {
        dr_test_log_errorf(pLog, "Failed to initialize libmpg123.\n");
        return 1;
    }

    pDecoder = mpg123_new(NULL, &error);
    if (pDecoder == NULL) {
        dr_test_log_errorf(pLog, "Failed to create a libmpg123 decoder: %s.\n", mpg123_plain_strerror(error));
        goto cleanup;
    }

    /* Preserve native rate, channels and gain; use the same gapless decoding policy. */
    if (mpg123_param(pDecoder, MPG123_ADD_FLAGS, MPG123_GAPLESS, 0) != MPG123_OK) {
        goto decoder_error;
    }

    if (mpg123_param(pDecoder, MPG123_REMOVE_FLAGS, MPG123_AUTO_RESAMPLE, 0) != MPG123_OK) {
        goto decoder_error;
    }

    if (mpg123_param(pDecoder, MPG123_RVA, MPG123_RVA_OFF, 0) != MPG123_OK) {
        goto decoder_error;
    }

    if (mpg123_format_none(pDecoder) != MPG123_OK) {
        goto decoder_error;
    }

    mpg123_rates(&pRates, &rateCount);
    if (rateCount == 0) {
        dr_test_log_errorf(pLog, "libmpg123 reports no supported sample rates.\n");
        goto cleanup;
    }

    /* Allow all native formats so a dr_mp3 format error cannot force the reference to match. */
    for (iRate = 0; iRate < rateCount; iRate += 1) {
        if (mpg123_format(pDecoder, pRates[iRate], MPG123_MONO | MPG123_STEREO, encoding) != MPG123_OK) {
            goto decoder_error;
        }
    }

    if (mpg123_open(pDecoder, pFilePath) != MPG123_OK) {
        goto decoder_error;
    }

    opened = DR_TRUE;
    do {
        long sampleRate;
        int channels;
        int actualEncoding;
        size_t bytesRead = 0;
        size_t framesRead;
        size_t sampleCount;
        size_t iSample;

        status = mpg123_read(pDecoder, &samples, bufferSize, &bytesRead);
        if (status != MPG123_OK && status != MPG123_DONE && status != MPG123_NEW_FORMAT) {
            goto decoder_error;
        }

        if (mpg123_getformat(pDecoder, &sampleRate, &channels, &actualEncoding) != MPG123_OK) {
            goto decoder_error;
        }

        if (sampleRate != (long)pReference->sampleRate || channels != (int)pReference->channels || actualEncoding != encoding) {
            dr_test_log_errorf(pLog, "Format properties differ with libmpg123.\n");
            goto cleanup;
        }

        if (bytesRead > bufferSize || bytesRead % (sampleSize * pReference->channels) != 0) {
            dr_test_log_errorf(pLog, "libmpg123 returned an invalid PCM byte count.\n");
            goto cleanup;
        }

        framesRead = bytesRead / sampleSize / pReference->channels;
        if (framesRead > pReference->frameCount - totalFrames) {
            dr_test_log_errorf(pLog, "libmpg123 returned more PCM frames than dr_mp3.\n");
            goto cleanup;
        }

        sampleCount = framesRead * pReference->channels;
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            size_t referenceIndex;
            double expected;
            double actual;
            double difference;

            referenceIndex = (size_t)totalFrames * pReference->channels + iSample;
            expected = samples[iSample];
            actual = ((const drmp3_int16*)pReference->pSamples)[referenceIndex];

            difference = fabs(expected - actual);
            if (difference > maximumDifference) {
                maximumDifference = difference;
            }
        }

        totalFrames += framesRead;
        if (status == MPG123_OK && bytesRead == 0) {
            dr_test_log_errorf(pLog, "libmpg123 made no decoding progress.\n");
            goto cleanup;
        }
    } while (status != MPG123_DONE);

    if (totalFrames != pReference->frameCount) {
        dr_test_log_errorf(pLog, "Decoded PCM frame counts differ with libmpg123.\n");
        goto cleanup;
    }

    if (maximumDifference > tolerance) {
        dr_test_log_errorf(pLog, "Decoded MP3 s16 samples differ with libmpg123: maximum difference %.9g exceeds %.9g.\n", maximumDifference, tolerance);
        goto cleanup;
    }

    exitCode = 0;
    goto cleanup;

decoder_error:
    dr_test_log_errorf(pLog, "Failed to decode with libmpg123: %s.\n", mpg123_strerror(pDecoder));

cleanup:
    if (opened && mpg123_close(pDecoder) != MPG123_OK) {
        dr_test_log_errorf(pLog, "Failed to close with libmpg123.\n");
        exitCode = 1;
    }

    if (pDecoder != NULL) {
        mpg123_delete(pDecoder);
    }

    mpg123_exit();

    return exitCode;
}

static int dr_mp3_load_reference(const dr_test_log* pLog, const char* pFilePath, dr_test_sample_format format, dr_mp3_decoded_data* pReference)
{
    drmp3 mp3;
    drmp3_uint64 frameCount = 0;
    drmp3_uint64 cursor;

    if (!drmp3_init_file(&mp3, pFilePath, NULL)) {
        dr_test_log_errorf(pLog, "Failed to open with drmp3_init_file().\n");
        return 1;
    }

    cursor = mp3.currentPCMFrame;
    frameCount = drmp3_get_pcm_frame_count(&mp3);
    if (mp3.currentPCMFrame != cursor) {
        dr_test_log_errorf(pLog, "Initial MP3 frame-count query changed position.\n");
        drmp3_uninit(&mp3);
        return 1;
    }

    if (dr_mp3_read_all_and_close(&mp3, format, pReference) != 0) {
        dr_test_log_errorf(pLog, "Failed to decode %s reference.\n", dr_test_sample_format_name(format));
        return 1;
    }

    if (frameCount != pReference->frameCount) {
        dr_test_log_errorf(pLog, "Initial MP3 frame-count query differs from the decoded frame count.\n");
        return 1;
    }

    return 0;
}

static int dr_test_mp3_compare_format(const dr_test_log* pLog, const dr_mp3_decoded_data* pReference, const dr_mp3_decoded_data* pDecoded)
{
    const drmp3_int16* pSamples;
    size_t sampleCount;
    size_t iSample;
    dr_bool32 formatMatches;

    sampleCount = pReference->sampleDataSize / sizeof(drmp3_int16);
    formatMatches = pReference->channels == pDecoded->channels;
    formatMatches = formatMatches && pReference->sampleRate == pDecoded->sampleRate;
    formatMatches = formatMatches && pReference->frameCount == pDecoded->frameCount;
    formatMatches = formatMatches && pDecoded->sampleDataSize == sampleCount * sizeof(float);
    if (!formatMatches) {
        dr_test_log_errorf(pLog, "MP3 f32 format properties or frame count differ from s16.\n");
        return 1;
    }

    pSamples = (const drmp3_int16*)pReference->pSamples;
    for (iSample = 0; iSample < sampleCount; iSample += 1) {
#ifdef DR_MP3_FLOAT_OUTPUT
        drmp3_int32 converted;
        float x;

        /* Quantize the float output: the s16 reference cannot preserve its range or precision. */
        x = ((const float*)pDecoded->pSamples)[iSample];
        if (x != x || x > FLT_MAX || x < -FLT_MAX) {
            dr_test_log_errorf(pLog, "Non-finite MP3 f32 sample.\n");
            return 1;
        }

        if (x < -1) {
            x = -1;
        } else if (x > 1) {
            x = 1;
        }

        converted = (drmp3_int32)(x * 32767.0f);
        if (converted != pSamples[iSample]) {
            dr_test_log_errorf(pLog, "MP3 f32 sample %lu differs from s16 after conversion.\n", (unsigned long)iSample);
            return 1;
        }
#else
        float expected;

        /* The default integer core converts s16 to f32 without loss. */
        expected = pSamples[iSample] / 32768.0f;
        if (!dr_test_f32_equal(((const float*)pDecoded->pSamples)[iSample], expected)) {
            dr_test_log_errorf(pLog, "MP3 f32 sample %lu differs from s16.\n", (unsigned long)iSample);
            return 1;
        }
#endif
    }

    return 0;
}

static int dr_test_mp3_decoding_zero_read(const dr_test_log* pLog, drmp3* pMP3, dr_test_sample_format format, void* pSamples, size_t bufferSize)
{
    drmp3_uint64 cursor;
    size_t iByte;

    cursor = dr_mp3_cursor(pMP3);
    memset(pSamples, 0xA5, bufferSize);
    if (dr_mp3_read_frames(pMP3, format, 0, pSamples) != 0 || dr_mp3_cursor(pMP3) != cursor) {
        dr_test_log_errorf(pLog, "MP3 %s zero-frame read returned frames or changed position.\n", dr_test_sample_format_name(format));
        return 1;
    }

    for (iByte = 0; iByte < bufferSize; iByte += 1) {
        if (((const unsigned char*)pSamples)[iByte] != 0xA5) {
            dr_test_log_errorf(pLog, "MP3 %s zero-frame read changed the output buffer.\n", dr_test_sample_format_name(format));
            return 1;
        }
    }

    if (dr_mp3_read_frames(pMP3, format, 0, NULL) != 0 || dr_mp3_cursor(pMP3) != cursor) {
        dr_test_log_errorf(pLog, "MP3 %s zero-frame NULL-output read returned frames or changed position.\n", dr_test_sample_format_name(format));
        return 1;
    }

    return 0;
}

/* Uninitializes the decoder before returning. framesPerRead must be greater than zero. */
static int dr_test_mp3_decoding_read_size(const dr_test_log* pLog, drmp3* pMP3, dr_test_sample_format format, const dr_mp3_decoded_data* pReference, drmp3_uint64 framesPerRead)
{
    void* pSamples = NULL;
    drmp3_uint64 totalFramesRead = 0;
    size_t bytesPerFrame;
    size_t bufferSize;
    dr_bool32 formatMatches;
    int exitCode = 1;

    bytesPerFrame = dr_test_sample_size(format) * pReference->channels;
    bufferSize = (size_t)framesPerRead * bytesPerFrame;

    formatMatches = pMP3->channels == pReference->channels;
    formatMatches = formatMatches && pMP3->sampleRate == pReference->sampleRate;
    if (!formatMatches) {
        dr_test_log_errorf(pLog, "Format properties differ for chunked MP3 reads.\n");
        goto cleanup;
    }

    pSamples = malloc(bufferSize);
    if (pSamples == NULL) {
        dr_test_log_errorf(pLog, "Out of memory for chunked MP3 reads.\n");
        goto cleanup;
    }

    if (dr_test_mp3_decoding_zero_read(pLog, pMP3, format, pSamples, bufferSize) != 0) {
        goto cleanup;
    }

    for (;;) {
        drmp3_uint64 framesRead;
        const void* pExpected;

        framesRead = dr_mp3_read_frames(pMP3, format, framesPerRead, pSamples);
        if (framesRead > framesPerRead || framesRead > pReference->frameCount - totalFramesRead) {
            dr_test_log_errorf(pLog, "Chunked MP3 read returned too many frames.\n");
            goto cleanup;
        }

        if (dr_mp3_cursor(pMP3) != totalFramesRead + framesRead) {
            dr_test_log_errorf(pLog, "Chunked MP3 read changed the cursor incorrectly.\n");
            goto cleanup;
        }

        pExpected = (const unsigned char*)pReference->pSamples + (size_t)totalFramesRead * bytesPerFrame;
        if (memcmp(pExpected, pSamples, (size_t)framesRead * bytesPerFrame) != 0) {
            dr_test_log_errorf(pLog, "Chunked MP3 %s output differs from the reference.\n", dr_test_sample_format_name(format));
            goto cleanup;
        }

        /* Check buffered state after the first read without repeating this for every chunk. */
        if (totalFramesRead == 0 && dr_test_mp3_decoding_zero_read(pLog, pMP3, format, pSamples, bufferSize) != 0) {
            goto cleanup;
        }

        totalFramesRead += framesRead;
        if (framesRead < framesPerRead) {
            if (totalFramesRead != pReference->frameCount) {
                dr_test_log_errorf(pLog, "Chunked MP3 read reached EOF too early.\n");
                goto cleanup;
            }

            break;
        }
    }

    if (dr_test_mp3_decoding_zero_read(pLog, pMP3, format, pSamples, bufferSize) != 0) {
        goto cleanup;
    }

    if (dr_mp3_read_frames(pMP3, format, framesPerRead, pSamples) != 0 || dr_mp3_cursor(pMP3) != totalFramesRead) {
        dr_test_log_errorf(pLog, "Chunked MP3 read returned frames or changed position at EOF.\n");
        goto cleanup;
    }

    exitCode = 0;

cleanup:
    free(pSamples);
    drmp3_uninit(pMP3);

    return exitCode;
}

static void dr_mp3_on_metadata(void* pUserData, const drmp3_metadata* pMetadata)
{
    (void)pUserData;
    (void)pMetadata;
}

static int dr_test_mp3_decoding_initialization(const dr_test_log* pLog, const char* pFilePath, const dr_mp3_decoded_data* pReference)
{
    static const char* variantNames[] = {
        "drmp3_init_file_with_metadata",
        "drmp3_init_memory",
        "drmp3_init_memory_with_metadata"
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
        drmp3 mp3;
        drmp3_bool32 initialized;

        if (iVariant == 0) {
            initialized = drmp3_init_file_with_metadata(&mp3, pFilePath, dr_mp3_on_metadata, NULL, NULL);
        } else if (iVariant == 1) {
            initialized = drmp3_init_memory(&mp3, pFileData, fileDataSize, NULL);
        } else {
            initialized = drmp3_init_memory_with_metadata(&mp3, pFileData, fileDataSize, dr_mp3_on_metadata, NULL, NULL);
        }

        if (!initialized) {
            dr_test_log_errorf(pLog, "Failed to open with %s().\n", variantNames[iVariant]);
            goto cleanup;
        }

        if (dr_test_mp3_decoding_read_size(pLog, &mp3, dr_test_sample_format_s16, pReference, 1234) != 0) {
            dr_test_log_errorf(pLog, "MP3 s16 decoding failed with %s().\n", variantNames[iVariant]);
            goto cleanup;
        }
    }

    exitCode = 0;

cleanup:
    fs_free(pFileData, NULL);

    return exitCode;
}

static int dr_test_mp3_decoding_skip(const dr_test_log* pLog, const char* pFilePath, const dr_mp3_decoded_data* pReference)
{
    drmp3 mp3;
    drmp3* pMP3 = NULL;
    drmp3_int16* pSamples = NULL;
    drmp3_uint64 skipSizes[3];
    drmp3_uint64 cursor = 0;
    size_t bytesPerFrame;
    size_t iBlock;
    int exitCode = 1;

    if (pReference->frameCount < 2) {
        return 0;  /* Need at least one frame to skip and one to read. */
    }

    pMP3 = &mp3;

    skipSizes[0] = 17;
    skipSizes[1] = 1153;
    skipSizes[2] = 1234;

    bytesPerFrame = sizeof(drmp3_int16) * pReference->channels;

    if (!drmp3_init_file(pMP3, pFilePath, NULL)) {
        dr_test_log_errorf(pLog, "Failed to open for MP3 NULL-output reads.\n");
        return 1;
    }

    pSamples = (drmp3_int16*)malloc(1234 * bytesPerFrame);
    if (pSamples == NULL) {
        dr_test_log_errorf(pLog, "Out of memory for MP3 NULL-output reads.\n");
        goto cleanup;
    }

    /* Check only three skip/read pairs, including an MP3 frame boundary. */
    for (iBlock = 0; iBlock < sizeof(skipSizes) / sizeof(skipSizes[0]) && pReference->frameCount - cursor > 1; iBlock += 1) {
        drmp3_uint64 framesToSkip;
        drmp3_uint64 framesToRead;
        drmp3_uint64 framesSkipped;
        const void* pExpected;

        framesToSkip = skipSizes[iBlock];
        if (framesToSkip >= pReference->frameCount - cursor) {
            framesToSkip = (pReference->frameCount - cursor) / 2;
        }

        /* Only the native output path supports NULL; conversion paths require a buffer. */
#ifdef DR_MP3_FLOAT_OUTPUT
        framesSkipped = drmp3_read_pcm_frames_f32(pMP3, framesToSkip, NULL);
#else
        framesSkipped = drmp3_read_pcm_frames_s16(pMP3, framesToSkip, NULL);
#endif
        if (framesSkipped != framesToSkip || dr_mp3_cursor(pMP3) != cursor + framesToSkip) {
            dr_test_log_errorf(pLog, "MP3 NULL-output read returned an incorrect frame count or cursor.\n");
            goto cleanup;
        }

        cursor += framesToSkip;
        framesToRead = pReference->frameCount - cursor;
        if (framesToRead > 1234) {
            framesToRead = 1234;
        }

        if (drmp3_read_pcm_frames_s16(pMP3, framesToRead, pSamples) != framesToRead || dr_mp3_cursor(pMP3) != cursor + framesToRead) {
            dr_test_log_errorf(pLog, "MP3 read after a NULL-output read returned an incorrect frame count or cursor.\n");
            goto cleanup;
        }

        pExpected = (const unsigned char*)pReference->pSamples + (size_t)cursor * bytesPerFrame;
        if (memcmp(pExpected, pSamples, (size_t)framesToRead * bytesPerFrame) != 0) {
            dr_test_log_errorf(pLog, "MP3 samples differ after a NULL-output read.\n");
            goto cleanup;
        }

        cursor += framesToRead;
    }

    exitCode = 0;

cleanup:
    free(pSamples);
    drmp3_uninit(pMP3);

    return exitCode;
}

static int dr_test_mp3_decoding(const dr_test_log* pLog, const char* pFilePath, dr_mp3_reference_data* pReferences)
{
    drmp3 mp3;

    if (dr_mp3_load_reference(pLog, pFilePath, dr_test_sample_format_s16, &pReferences->s16) != 0) {
        return 1;
    }

    if (dr_test_mp3_decoding_mpg123(pLog, pFilePath, &pReferences->s16) != 0) {
        return 1;
    }

    if (dr_mp3_load_reference(pLog, pFilePath, dr_test_sample_format_f32, &pReferences->f32) != 0) {
        return 1;
    }

    if (dr_test_mp3_compare_format(pLog, &pReferences->s16, &pReferences->f32) != 0) {
        return 1;
    }

    if (!drmp3_init_file(&mp3, pFilePath, NULL)) {
        dr_test_log_errorf(pLog, "Failed to open for chunked MP3 reads.\n");
        return 1;
    }

    if (dr_test_mp3_decoding_read_size(pLog, &mp3, dr_test_sample_format_s16, &pReferences->s16, 1234) != 0) {
        return 1;
    }

    if (!drmp3_init_file(&mp3, pFilePath, NULL)) {
        dr_test_log_errorf(pLog, "Failed to open for chunked MP3 reads.\n");
        return 1;
    }

    if (dr_test_mp3_decoding_read_size(pLog, &mp3, dr_test_sample_format_f32, &pReferences->f32, 1234) != 0) {
        return 1;
    }

    if (dr_test_mp3_decoding_initialization(pLog, pFilePath, &pReferences->s16) != 0) {
        return 1;
    }

    if (dr_test_mp3_decoding_skip(pLog, pFilePath, &pReferences->s16) != 0) {
        return 1;
    }

    return 0;
}

static int dr_test_mp3_seek_to(const dr_test_log* pLog, drmp3* pMP3, drmp3_uint64 target)
{
    if (!drmp3_seek_to_pcm_frame(pMP3, target)) {
        dr_test_log_errorf(pLog, "MP3 seek failed at PCM frame %llu.\n", target);
        return 1;
    }

    if (dr_mp3_cursor(pMP3) != target) {
        dr_test_log_errorf(pLog, "MP3 cursor differs from seek target %llu.\n", target);
        return 1;
    }

    return 0;
}

static drmp3_uint64 dr_test_mp3_seek_read_count(const dr_mp3_decoded_data* pReference, drmp3_uint64 target)
{
    drmp3_uint64 count;
    drmp3_uint64 limit;

    count = pReference->frameCount - target;
    limit = 2306;  /* Two maximum-size MP3 frames plus two PCM frames. */

    if (count > limit) {
        count = limit;
    }

    return count;
}

/* PR #349 returned the requested cursor but incorrect samples. Keep this comparison exact. */
static int dr_test_mp3_seek_read_and_compare(const dr_test_log* pLog, drmp3* pMP3, const dr_mp3_decoded_data* pReference, drmp3_uint64 target, drmp3_uint64 count, drmp3_int16* pSamples)
{
    size_t bytesPerFrame;

    if (dr_mp3_cursor(pMP3) != target) {
        dr_test_log_errorf(pLog, "MP3 cursor differs before reading PCM frame %llu.\n", target);
        return 1;
    }

    bytesPerFrame = sizeof(drmp3_int16) * pReference->channels;

    while (count > 0) {
        drmp3_uint64 framesToRead;
        const void* pExpected;

        framesToRead = count;
        if (framesToRead > 1234) {
            framesToRead = 1234;
        }

        if (drmp3_read_pcm_frames_s16(pMP3, framesToRead, pSamples) != framesToRead || dr_mp3_cursor(pMP3) != target + framesToRead) {
            dr_test_log_errorf(pLog, "MP3 read after seeking returned an incorrect frame count or cursor.\n");
            return 1;
        }

        pExpected = (const unsigned char*)pReference->pSamples + (size_t)target * bytesPerFrame;

        if (memcmp(pExpected, pSamples, (size_t)framesToRead * bytesPerFrame) != 0) {
            dr_test_log_errorf(pLog, "MP3 samples differ after seeking at PCM frame %llu.\n", target);
            return 1;
        }

        target += framesToRead;
        count  -= framesToRead;
    }

    if (target == pReference->frameCount && (drmp3_read_pcm_frames_s16(pMP3, 1234, pSamples) != 0 || dr_mp3_cursor(pMP3) != target)) {
        dr_test_log_errorf(pLog, "MP3 read returned frames or changed position at EOF after seeking.\n");
        return 1;
    }

    return 0;
}

static int dr_test_mp3_seek_and_compare(const dr_test_log* pLog, drmp3* pMP3, const dr_mp3_decoded_data* pReference, drmp3_uint64 target, drmp3_int16* pSamples)
{
    if (dr_test_mp3_seek_to(pLog, pMP3, target) != 0) {
        return 1;
    }

    return dr_test_mp3_seek_read_and_compare(pLog, pMP3, pReference, target, dr_test_mp3_seek_read_count(pReference, target), pSamples);
}

static int dr_test_mp3_seek_near(const dr_test_log* pLog, drmp3* pMP3, const dr_mp3_decoded_data* pReference, drmp3_uint64 boundary, drmp3_int16* pSamples)
{
    unsigned int iOffset;

    for (iOffset = 0; iOffset < 3; iOffset += 1) {
        drmp3_uint64 target;

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

        if (dr_test_mp3_seek_and_compare(pLog, pMP3, pReference, target, pSamples) != 0) {
            return 1;
        }
    }

    return 0;
}

static void dr_mp3_frame_bounds(const drmp3* pMP3, const dr_mp3_decoded_data* pReference, drmp3_uint64* pStart, drmp3_uint64* pEnd)
{
    *pStart = pMP3->currentPCMFrame - pMP3->pcmFramesConsumedInMP3Frame;
    if (*pStart > pMP3->delayInPCMFrames) {
        *pStart -= pMP3->delayInPCMFrames;
    } else {
        *pStart = 0;
    }

    *pEnd = dr_mp3_cursor(pMP3) + pMP3->pcmFramesRemainingInMP3Frame;
    if (*pEnd > pReference->frameCount) {
        *pEnd = pReference->frameCount;
    }
}

static int dr_test_mp3_seek_boundaries(const dr_test_log* pLog, drmp3* pMP3, const dr_mp3_decoded_data* pReference, const drmp3_uint64* pAnchors, drmp3_int16* pSamples)
{
    size_t iAnchor;

    for (iAnchor = 0; iAnchor < 3; iAnchor += 1) {
        drmp3_uint64 frameStart;
        drmp3_uint64 frameEnd;

        if (dr_test_mp3_seek_to(pLog, pMP3, pAnchors[iAnchor]) != 0 || dr_test_mp3_seek_read_and_compare(pLog, pMP3, pReference, pAnchors[iAnchor], 1, pSamples) != 0) {
            return 1;
        }

        dr_mp3_frame_bounds(pMP3, pReference, &frameStart, &frameEnd);

        if (dr_test_mp3_seek_near(pLog, pMP3, pReference, frameStart, pSamples) != 0 || dr_test_mp3_seek_near(pLog, pMP3, pReference, frameEnd, pSamples) != 0) {
            return 1;
        }
    }

    if (pMP3->pSeekPoints != NULL && pMP3->seekPointCount > 0) {
        drmp3_uint32 indices[3];
        size_t iPoint;

        indices[0] = 0;
        indices[1] = pMP3->seekPointCount / 2;
        indices[2] = pMP3->seekPointCount - 1;

        for (iPoint = 0; iPoint < 3; iPoint += 1) {
            drmp3_uint64 target;

            target = pMP3->pSeekPoints[indices[iPoint]].pcmFrameIndex;
            if (target > pMP3->delayInPCMFrames) {
                target -= pMP3->delayInPCMFrames;
            } else {
                target = 0;
            }

            if (target < pReference->frameCount && dr_test_mp3_seek_near(pLog, pMP3, pReference, target, pSamples) != 0) {
                return 1;
            }
        }
    }

    return 0;
}

static int dr_test_mp3_seek_partial_reads(const dr_test_log* pLog, drmp3* pMP3, const dr_mp3_decoded_data* pReference, const drmp3_uint64* pAnchors, drmp3_int16* pSamples)
{
    size_t iAnchor;
    unsigned int iTarget;

    for (iAnchor = 0; iAnchor < 2; iAnchor += 1) {
        for (iTarget = 0; iTarget < 3; iTarget += 1) {
            drmp3_uint64 count;
            drmp3_uint64 frameStart;
            drmp3_uint64 frameEnd;
            drmp3_uint64 target;

            count = pReference->frameCount - pAnchors[iAnchor];
            if (count > 17) {
                count = 17;
            }

            if (dr_test_mp3_seek_to(pLog, pMP3, pAnchors[iAnchor]) != 0 || dr_test_mp3_seek_read_and_compare(pLog, pMP3, pReference, pAnchors[iAnchor], count, pSamples) != 0) {
                return 1;
            }

            dr_mp3_frame_bounds(pMP3, pReference, &frameStart, &frameEnd);

            if (iTarget == 0) {
                target = frameStart + (dr_mp3_cursor(pMP3) - frameStart) / 2;
            } else if (iTarget == 1) {
                if ((frameEnd - dr_mp3_cursor(pMP3)) < 2) {
                    continue;
                }

                target = dr_mp3_cursor(pMP3) + (frameEnd - dr_mp3_cursor(pMP3)) / 2;
            } else {
                if (frameStart > 0) {
                    target = 0;
                } else if (frameEnd < pReference->frameCount) {
                    target = pReference->frameCount - 1;
                } else {
                    continue;  /* This file has no other MP3 frame to seek to. */
                }
            }

            if (dr_test_mp3_seek_and_compare(pLog, pMP3, pReference, target, pSamples) != 0) {
                return 1;
            }
        }
    }

    return 0;
}

static int dr_test_mp3_seek_cases(const dr_test_log* pLog, drmp3* pMP3, const dr_mp3_decoded_data* pReference, drmp3_int16* pSamples)
{
    drmp3_uint64 anchors[3] = {0};
    drmp3_uint64 sequence[6] = {0};
    size_t iTarget;

    /* The reference decoder is already at EOF; table modes read only the tail to get there. */
    if (dr_mp3_cursor(pMP3) != pReference->frameCount && pReference->frameCount > 0) {
        if (dr_test_mp3_seek_and_compare(pLog, pMP3, pReference, pReference->frameCount - 1, pSamples) != 0) {
            return 1;
        }
    }

    if (dr_test_mp3_seek_read_and_compare(pLog, pMP3, pReference, pReference->frameCount, 0, pSamples) != 0 || dr_test_mp3_seek_and_compare(pLog, pMP3, pReference, 0, pSamples) != 0) {
        dr_test_log_errorf(pLog, "MP3 rewind after EOF failed.\n");
        return 1;
    }

    if (pReference->frameCount == 0) {
        return 0;
    }


    anchors[1] = pReference->frameCount / 2;
    anchors[2] = pReference->frameCount - 1;

    for (iTarget = 0; iTarget < 3; iTarget += 1) {
        if (dr_test_mp3_seek_and_compare(pLog, pMP3, pReference, anchors[iTarget], pSamples) != 0) {
            dr_test_log_errorf(pLog, "MP3 basic-position seek failed.\n");
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
        if (dr_test_mp3_seek_and_compare(pLog, pMP3, pReference, sequence[iTarget], pSamples) != 0) {
            dr_test_log_errorf(pLog, "MP3 forward/backward seek sequence failed.\n");
            return 1;
        }
    }


    if (dr_test_mp3_seek_boundaries(pLog, pMP3, pReference, anchors, pSamples) != 0) {
        dr_test_log_errorf(pLog, "MP3 frame/seek-table boundary test failed.\n");
        return 1;
    }

    for (iTarget = 0; iTarget < 3; iTarget += 1) {
        drmp3_uint64 cursor;

        if (dr_test_mp3_seek_to(pLog, pMP3, anchors[iTarget]) != 0 || dr_test_mp3_seek_to(pLog, pMP3, anchors[iTarget]) != 0) {
            return 1;
        }

        if (dr_test_mp3_seek_read_and_compare(pLog, pMP3, pReference, anchors[iTarget], dr_test_mp3_seek_read_count(pReference, anchors[iTarget]), pSamples) != 0) {
            return 1;
        }

        cursor = dr_mp3_cursor(pMP3);

        if (dr_test_mp3_seek_and_compare(pLog, pMP3, pReference, cursor, pSamples) != 0) {
            dr_test_log_errorf(pLog, "MP3 seek to the current position failed.\n");
            return 1;
        }
    }

    if (dr_test_mp3_seek_partial_reads(pLog, pMP3, pReference, anchors, pSamples) != 0) {
        dr_test_log_errorf(pLog, "MP3 seek after a partial read failed.\n");
        return 1;
    }

    return 0;
}

static int dr_test_mp3_frame_count(const dr_test_log* pLog, drmp3* pMP3, const dr_mp3_decoded_data* pReference)
{
    drmp3_uint64 cursor;
    drmp3_uint64 frameCount;

    cursor = dr_mp3_cursor(pMP3);
    frameCount = drmp3_get_pcm_frame_count(pMP3);
    if (frameCount != pReference->frameCount || dr_mp3_cursor(pMP3) != cursor) {
        dr_test_log_errorf(pLog, "MP3 frame-count query returned an incorrect count or changed position.\n");
        return 1;
    }

    return 0;
}

static int dr_test_mp3_seek_table(const dr_test_log* pLog, drmp3* pMP3, const dr_mp3_decoded_data* pReference, drmp3_uint32 requestedCount, drmp3_seek_point* pSeekPoints, drmp3_int16* pSamples)
{
    drmp3_uint32 seekPointCount;
    drmp3_uint64 target;
    drmp3_uint64 count;
    drmp3_uint64 cursor;

    target = pReference->frameCount / 2;
    count = pReference->frameCount - target;
    if (count > 17) {
        count = 17;
    }

    if (dr_test_mp3_seek_to(pLog, pMP3, target) != 0 || dr_test_mp3_seek_read_and_compare(pLog, pMP3, pReference, target, count, pSamples) != 0) {
        return 1;
    }

    cursor = dr_mp3_cursor(pMP3);
    seekPointCount = requestedCount;
    if (!drmp3_calculate_seek_points(pMP3, &seekPointCount, pSeekPoints) || seekPointCount == 0 || seekPointCount > requestedCount || dr_mp3_cursor(pMP3) != cursor) {
        dr_test_log_errorf(pLog, "MP3 seek-table calculation failed or changed position.\n");
        return 1;
    }

    /* Read without a seek to check that table calculation restored buffered state. */
    if (cursor < pReference->frameCount && dr_test_mp3_seek_read_and_compare(pLog, pMP3, pReference, cursor, 1, pSamples) != 0) {
        return 1;
    }

    if (!drmp3_bind_seek_table(pMP3, seekPointCount, pSeekPoints) || pMP3->pSeekPoints != pSeekPoints || pMP3->seekPointCount != seekPointCount) {
        dr_test_log_errorf(pLog, "Failed to bind the MP3 seek table.\n");
        return 1;
    }

    return 0;
}

static int dr_test_mp3_seek_frame_count(const dr_test_log* pLog, drmp3* pMP3, const dr_mp3_decoded_data* pReference, drmp3_int16* pSamples)
{
    drmp3_uint64 target;
    drmp3_uint64 count;
    drmp3_uint64 cursor;

    target = pReference->frameCount / 2;
    count = pReference->frameCount - target;
    if (count > 17) {
        count = 17;
    }

    if (dr_test_mp3_seek_to(pLog, pMP3, target) != 0 || dr_test_mp3_seek_read_and_compare(pLog, pMP3, pReference, target, count, pSamples) != 0) {
        return 1;
    }

    cursor = dr_mp3_cursor(pMP3);
    if (dr_test_mp3_frame_count(pLog, pMP3, pReference) != 0) {
        return 1;
    }

    count = pReference->frameCount - cursor;
    if (count > 1) {
        count = 1;
    }

    if (dr_test_mp3_seek_read_and_compare(pLog, pMP3, pReference, cursor, count, pSamples) != 0) {
        return 1;
    }

    return 0;
}

static int dr_test_mp3_seeking(const dr_test_log* pLog, const char* pFilePath)
{
    static const drmp3_uint32 tableSizes[] = {0, 1, 64};
    drmp3 mp3;
    dr_mp3_decoded_data reference = {0};
    drmp3_seek_point seekPoints[64];
    drmp3_int16* pSamples = NULL;
    size_t iMethod;
    dr_bool32 initialized = DR_FALSE;
    int exitCode = 1;

    if (!drmp3_init_file(&mp3, pFilePath, NULL)) {
        dr_test_log_errorf(pLog, "Failed to open for MP3 seeking.\n");
        return 1;
    }

    initialized = DR_TRUE;

    /* No libmpg123 comparison here. Keep this decoder at EOF for the first method. */
    if (dr_mp3_read_all(&mp3, dr_test_sample_format_s16, &reference) != 0) {
        dr_test_log_errorf(pLog, "Failed to decode the seeking reference.\n");
        goto cleanup;
    }

    pSamples = (drmp3_int16*)malloc(1234 * reference.channels * sizeof(drmp3_int16));
    if (pSamples == NULL) {
        dr_test_log_errorf(pLog, "Out of memory for MP3 seeking tests.\n");
        goto cleanup;
    }

    for (iMethod = 0; iMethod < sizeof(tableSizes) / sizeof(tableSizes[0]); iMethod += 1) {
        if (!initialized) {
            if (!drmp3_init_file(&mp3, pFilePath, NULL)) {
                dr_test_log_errorf(pLog, "Failed to reopen for MP3 seeking.\n");
                goto cleanup;
            }

            initialized = DR_TRUE;
        }

        if (tableSizes[iMethod] > 0) {
            if (dr_test_mp3_seek_table(pLog, &mp3, &reference, tableSizes[iMethod], seekPoints, pSamples) != 0) {
                dr_test_log_errorf(pLog, "MP3 seek-table setup failed, table request %u.\n", tableSizes[iMethod]);
                goto cleanup;
            }
        }

        if (dr_test_mp3_seek_cases(pLog, &mp3, &reference, pSamples) != 0) {
            dr_test_log_errorf(pLog, "MP3 seeking failed, table request %u.\n", tableSizes[iMethod]);
            goto cleanup;
        }

        if (tableSizes[iMethod] == 0) {
            if (dr_test_mp3_seek_frame_count(pLog, &mp3, &reference, pSamples) != 0) {
                dr_test_log_errorf(pLog, "MP3 frame-count state restoration failed.\n");
                goto cleanup;
            }
        } else {
            if (!drmp3_bind_seek_table(&mp3, 0, NULL) || mp3.pSeekPoints != NULL || mp3.seekPointCount != 0) {
                dr_test_log_errorf(pLog, "Failed to unbind the MP3 seek table.\n");
                goto cleanup;
            }

            if (dr_test_mp3_seek_and_compare(pLog, &mp3, &reference, reference.frameCount / 2, pSamples) != 0) {
                dr_test_log_errorf(pLog, "MP3 seek after unbinding the table failed.\n");
                goto cleanup;
            }
        }

        drmp3_uninit(&mp3);
        initialized = DR_FALSE;
    }

    exitCode = 0;

cleanup:
    free(pSamples);
    free(reference.pSamples);
    if (initialized) {
        drmp3_uninit(&mp3);
    }

    return exitCode;
}

static int dr_test_mp3_seeking_basic(const dr_test_log* pLog, const char* pFilePath)
{
    static const drmp3_uint64 targets[] = {17, 1234, 0};
    drmp3 mp3;
    size_t iTarget;

    if (!drmp3_init_file(&mp3, pFilePath, NULL)) {
        dr_test_log_errorf(pLog, "Failed to open for basic MP3 seeking.\n");
        return 1;
    }

    /* Failure is allowed. These requests check for crashes without requiring valid audio or a known length. */
    for (iTarget = 0; iTarget < sizeof(targets) / sizeof(targets[0]); iTarget += 1) {
        (void)drmp3_seek_to_pcm_frame(&mp3, targets[iTarget]);
    }

    drmp3_uninit(&mp3);

    return 0;
}

static int dr_test_mp3_file(const dr_test_log* pLog, const char* pFilePath, dr_test_stage stage)
{
    dr_mp3_reference_data references = {0};
    int result;

    if (stage == dr_test_stage_seeking_basic) {
        return dr_test_mp3_seeking_basic(pLog, pFilePath);
    }

    if (stage == dr_test_stage_seeking) {
        return dr_test_mp3_seeking(pLog, pFilePath);
    }

    result = dr_test_mp3_decoding(pLog, pFilePath, &references);
    if (result != 0) {
        dr_test_log_errorf(pLog, "MP3 correctness test failed.\n");
    }

    free(references.s16.pSamples);
    free(references.f32.pSamples);

    return result;
}
