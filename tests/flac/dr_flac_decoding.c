/*#define DR_FLAC_NO_CRC*/
/*#define DR_FLAC_NO_SIMD*/
/*#define DR_FLAC_BUFFER_SIZE 4096*/
#include "dr_flac_common.c"

#define FILE_NAME_WIDTH 40
#define NUMBER_WIDTH    10
#define TABLE_MARGIN    2

#define DEFAULT_SOURCE_DIR  "testvectors/flac/tests"

drflac_result decode_test__read_and_compare_pcm_frames_s32(libflac* pLibFlac, drflac* pFlac, drflac_uint64 pcmFrameCount, drflac_int32* pPCMFrames_libflac, drflac_int32* pPCMFrames_drflac)
{
    drflac_uint64 pcmFrameCount_libflac;
    drflac_uint64 pcmFrameCount_drflac;
    drflac_uint64 iPCMFrame;

    /* To test decoding we just read a number of PCM frames from each decoder and compare. */
    pcmFrameCount_libflac = libflac_read_pcm_frames_s32(pLibFlac, pcmFrameCount, pPCMFrames_libflac);
    pcmFrameCount_drflac = drflac_read_pcm_frames_s32(pFlac, pcmFrameCount, pPCMFrames_drflac);

    /* The total number of frames we decoded need to match. */
    if (pcmFrameCount_libflac != pcmFrameCount_drflac) {
        printf("  Decoded frame counts differ: pcmFrameCount=%d, libFLAC=%d, dr_flac=%d", (int)pcmFrameCount, (int)pcmFrameCount_libflac, (int)pcmFrameCount_drflac);
        return DRFLAC_ERROR;
    }

    /* Each of the decoded PCM frames need to match. */
    DRFLAC_ASSERT(pcmFrameCount_libflac == pcmFrameCount_drflac);

    for (iPCMFrame = 0; iPCMFrame < pcmFrameCount_libflac; iPCMFrame += 1) {
        drflac_int32* pPCMFrame_libflac = pPCMFrames_libflac + (iPCMFrame * pLibFlac->channels);
        drflac_int32* pPCMFrame_drflac  = pPCMFrames_drflac  + (iPCMFrame * pLibFlac->channels);
        drflac_uint32 iChannel;
        drflac_bool32 hasError = DRFLAC_FALSE;

        for (iChannel = 0; iChannel < pLibFlac->channels; iChannel += 1) {
            if (pPCMFrame_libflac[iChannel] != pPCMFrame_drflac[iChannel]) {
                printf("  PCM Frame @ %d[%d] does not match: pcmFrameCount=%d\n", (int)iPCMFrame, iChannel, (int)pcmFrameCount);
                hasError = DRFLAC_TRUE;
                break;
            }
        }

        if (hasError) {
            return DRFLAC_ERROR;    /* Decoded frames do not match. */
        }
    }

    /* Done. */
    return DRFLAC_SUCCESS;
}

drflac_result decode_test__read_and_compare_pcm_frame_chunks_s32(libflac* pLibFlac, drflac* pFlac, drflac_uint64 pcmFrameChunkSize)
{
    drflac_result result = DRFLAC_SUCCESS;
    drflac_uint64 iPCMFrame;
    drflac_int32* pPCMFrames_libflac;
    drflac_int32* pPCMFrames_drflac;

    /* Make sure the decoder's are seeked back to the start first. */
    drflac_seek_to_pcm_frame(pFlac, 0);
    libflac_seek_to_pcm_frame(pLibFlac, 0);

    pPCMFrames_libflac = (drflac_int32*)malloc((size_t)(pcmFrameChunkSize * pLibFlac->channels * sizeof(drflac_int32)));
    if (pPCMFrames_libflac == NULL) {
        printf("  [libFLAC] Out of memory");
        return DRFLAC_ERROR;
    }

    pPCMFrames_drflac = (drflac_int32*)malloc((size_t)(pcmFrameChunkSize * pLibFlac->channels * sizeof(drflac_int32)));
    if (pPCMFrames_drflac == NULL) {
        free(pPCMFrames_libflac);
        printf("  [dr_flac] Out of memory");
        return DRFLAC_ERROR;
    }

    for (iPCMFrame = 0; iPCMFrame < pLibFlac->pcmFrameCount; iPCMFrame += pcmFrameChunkSize) {
        result = decode_test__read_and_compare_pcm_frames_s32(pLibFlac, pFlac, pcmFrameChunkSize, pPCMFrames_libflac, pPCMFrames_drflac);
        if (result != DRFLAC_SUCCESS) {
            break;
        }
    }

    free(pPCMFrames_libflac);
    free(pPCMFrames_drflac);

    return result;
}


drflac_result decode_test__read_and_compare_pcm_frames_f32(libflac* pLibFlac, drflac* pFlac, drflac_uint64 pcmFrameCount, float* pPCMFrames_libflac, float* pPCMFrames_drflac)
{
    drflac_uint64 pcmFrameCount_libflac;
    drflac_uint64 pcmFrameCount_drflac;
    drflac_uint64 iPCMFrame;

    /* To test decoding we just read a number of PCM frames from each decoder and compare. */
    pcmFrameCount_libflac = libflac_read_pcm_frames_f32(pLibFlac, pcmFrameCount, pPCMFrames_libflac);
    pcmFrameCount_drflac = drflac_read_pcm_frames_f32(pFlac, pcmFrameCount, pPCMFrames_drflac);

    /* The total number of frames we decoded need to match. */
    if (pcmFrameCount_libflac != pcmFrameCount_drflac) {
        printf("  Decoded frame counts differ: pcmFrameCount=%d, libFLAC=%d, dr_flac=%d", (int)pcmFrameCount, (int)pLibFlac->currentPCMFrame, (int)pFlac->currentPCMFrame);
        return DRFLAC_ERROR;
    }

    /* Each of the decoded PCM frames need to match. */
    DRFLAC_ASSERT(pcmFrameCount_libflac == pcmFrameCount_drflac);

    for (iPCMFrame = 0; iPCMFrame < pcmFrameCount_libflac; iPCMFrame += 1) {
        float* pPCMFrame_libflac = pPCMFrames_libflac + (iPCMFrame * pLibFlac->channels);
        float* pPCMFrame_drflac  = pPCMFrames_drflac  + (iPCMFrame * pLibFlac->channels);
        drflac_uint32 iChannel;
        drflac_bool32 hasError = DRFLAC_FALSE;

        for (iChannel = 0; iChannel < pLibFlac->channels; iChannel += 1) {
            if (pPCMFrame_libflac[iChannel] != pPCMFrame_drflac[iChannel]) {
                printf("  PCM Frame @ %d[%d] does not match: pcmFrameCount=%d", (int)iPCMFrame, iChannel, (int)pcmFrameCount);
                hasError = DRFLAC_TRUE;
                break;
            }
        }

        if (hasError) {
            return DRFLAC_ERROR;    /* Decoded frames do not match. */
        }
    }

    /* Done. */
    return DRFLAC_SUCCESS;
}

drflac_result decode_test__read_and_compare_pcm_frame_chunks_f32(libflac* pLibFlac, drflac* pFlac, drflac_uint64 pcmFrameChunkSize)
{
    drflac_result result = DRFLAC_SUCCESS;
    drflac_uint64 iPCMFrame;
    float* pPCMFrames_libflac;
    float* pPCMFrames_drflac;

    /* Make sure the decoder's are seeked back to the start first. */
    drflac_seek_to_pcm_frame(pFlac, 0);
    libflac_seek_to_pcm_frame(pLibFlac, 0);

    pPCMFrames_libflac = (float*)malloc((size_t)(pcmFrameChunkSize * pLibFlac->channels * sizeof(float)));
    if (pPCMFrames_libflac == NULL) {
        printf("  [libFLAC] Out of memory");
        return DRFLAC_ERROR;
    }

    pPCMFrames_drflac = (float*)malloc((size_t)(pcmFrameChunkSize * pLibFlac->channels * sizeof(float)));
    if (pPCMFrames_drflac == NULL) {
        free(pPCMFrames_libflac);
        printf("  [dr_flac] Out of memory");
        return DRFLAC_ERROR;
    }

    for (iPCMFrame = 0; iPCMFrame < pLibFlac->pcmFrameCount; iPCMFrame += pcmFrameChunkSize) {
        result = decode_test__read_and_compare_pcm_frames_f32(pLibFlac, pFlac, pcmFrameChunkSize, pPCMFrames_libflac, pPCMFrames_drflac);
        if (result != DRFLAC_SUCCESS) {
            break;
        }
    }

    free(pPCMFrames_libflac);
    free(pPCMFrames_drflac);

    return result;
}


drflac_result decode_test__read_and_compare_pcm_frames_s16(libflac* pLibFlac, drflac* pFlac, drflac_uint64 pcmFrameCount, drflac_int16* pPCMFrames_libflac, drflac_int16* pPCMFrames_drflac)
{
    drflac_uint64 pcmFrameCount_libflac;
    drflac_uint64 pcmFrameCount_drflac;
    drflac_uint64 iPCMFrame;

    /* To test decoding we just read a number of PCM frames from each decoder and compare. */
    pcmFrameCount_libflac = libflac_read_pcm_frames_s16(pLibFlac, pcmFrameCount, pPCMFrames_libflac);
    pcmFrameCount_drflac = drflac_read_pcm_frames_s16(pFlac, pcmFrameCount, pPCMFrames_drflac);

    /* The total number of frames we decoded need to match. */
    if (pcmFrameCount_libflac != pcmFrameCount_drflac) {
        printf("  Decoded frame counts differ: pcmFrameCount=%d, libFLAC=%d, dr_flac=%d", (int)pcmFrameCount, (int)pLibFlac->currentPCMFrame, (int)pFlac->currentPCMFrame);
        return DRFLAC_ERROR;
    }

    /* Each of the decoded PCM frames need to match. */
    DRFLAC_ASSERT(pcmFrameCount_libflac == pcmFrameCount_drflac);

    for (iPCMFrame = 0; iPCMFrame < pcmFrameCount_libflac; iPCMFrame += 1) {
        drflac_int16* pPCMFrame_libflac = pPCMFrames_libflac + (iPCMFrame * pLibFlac->channels);
        drflac_int16* pPCMFrame_drflac  = pPCMFrames_drflac  + (iPCMFrame * pLibFlac->channels);
        drflac_uint32 iChannel;
        drflac_bool32 hasError = DRFLAC_FALSE;

        for (iChannel = 0; iChannel < pLibFlac->channels; iChannel += 1) {
            if (pPCMFrame_libflac[iChannel] != pPCMFrame_drflac[iChannel]) {
                printf("  PCM Frame @ %d[%d] does not match: pcmFrameCount=%d", (int)iPCMFrame, iChannel, (int)pcmFrameCount);
                hasError = DRFLAC_TRUE;
                break;
            }
        }

        if (hasError) {
            return DRFLAC_ERROR;    /* Decoded frames do not match. */
        }
    }

    /* Done. */
    return DRFLAC_SUCCESS;
}

drflac_result decode_test__read_and_compare_pcm_frame_chunks_s16(libflac* pLibFlac, drflac* pFlac, drflac_uint64 pcmFrameChunkSize)
{
    drflac_result result = DRFLAC_SUCCESS;
    drflac_uint64 iPCMFrame;
    drflac_int16* pPCMFrames_libflac;
    drflac_int16* pPCMFrames_drflac;

    /* Make sure the decoder's are seeked back to the start first. */
    drflac_seek_to_pcm_frame(pFlac, 0);
    libflac_seek_to_pcm_frame(pLibFlac, 0);

    pPCMFrames_libflac = (drflac_int16*)malloc((size_t)(pcmFrameChunkSize * pLibFlac->channels * sizeof(drflac_int16)));
    if (pPCMFrames_libflac == NULL) {
        printf("  [libFLAC] Out of memory");
        return DRFLAC_ERROR;
    }

    pPCMFrames_drflac = (drflac_int16*)malloc((size_t)(pcmFrameChunkSize * pLibFlac->channels * sizeof(drflac_int16)));
    if (pPCMFrames_drflac == NULL) {
        free(pPCMFrames_libflac);
        printf("  [dr_flac] Out of memory");
        return DRFLAC_ERROR;
    }

    for (iPCMFrame = 0; iPCMFrame < pLibFlac->pcmFrameCount; iPCMFrame += pcmFrameChunkSize) {
        result = decode_test__read_and_compare_pcm_frames_s16(pLibFlac, pFlac, pcmFrameChunkSize, pPCMFrames_libflac, pPCMFrames_drflac);
        if (result != DRFLAC_SUCCESS) {
            break;
        }
    }

    free(pPCMFrames_libflac);
    free(pPCMFrames_drflac);

    return result;
}



drflac_result decode_test_file_s32(libflac* pLibFlac, drflac* pFlac)
{
    drflac_result result = DRFLAC_SUCCESS;

    /* Start with reading the entire file in one go. */
    if (result == DRFLAC_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_s32(pLibFlac, pFlac, pLibFlac->pcmFrameCount);
    }

    /* Now try with reading one PCM frame at a time.*/
    if (result == DRFLAC_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_s32(pLibFlac, pFlac, 1);
    }

    /* Now test FLAC frame boundaries. */
    if (result == DRFLAC_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_s32(pLibFlac, pFlac, (pFlac->maxBlockSizeInPCMFrames > 0) ? pFlac->maxBlockSizeInPCMFrames : 4096);
    }

    return result;
}

drflac_result decode_test_file_f32(libflac* pLibFlac, drflac* pFlac)
{
    drflac_result result = DRFLAC_SUCCESS;

    /* Start with reading the entire file in one go. */
    if (result == DRFLAC_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_f32(pLibFlac, pFlac, pLibFlac->pcmFrameCount);
    }

    /* Now try with reading one PCM frame at a time.*/
    if (result == DRFLAC_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_f32(pLibFlac, pFlac, 1);
    }

    /* Now test FLAC frame boundaries. */
    if (result == DRFLAC_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_f32(pLibFlac, pFlac, (pFlac->maxBlockSizeInPCMFrames > 0) ? pFlac->maxBlockSizeInPCMFrames : 4096);
    }

    return result;
}

drflac_result decode_test_file_s16(libflac* pLibFlac, drflac* pFlac)
{
    drflac_result result = DRFLAC_SUCCESS;

    /* Start with reading the entire file in one go. */
    if (result == DRFLAC_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_s16(pLibFlac, pFlac, pLibFlac->pcmFrameCount);
    }

    /* Now try with reading one PCM frame at a time.*/
    if (result == DRFLAC_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_s16(pLibFlac, pFlac, 1);
    }

    /* Now test FLAC frame boundaries. */
    if (result == DRFLAC_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_s16(pLibFlac, pFlac, (pFlac->maxBlockSizeInPCMFrames > 0) ? pFlac->maxBlockSizeInPCMFrames : 4096);
    }

    return result;
}

drflac_result decode_test_file(const char* pFilePath)
{
    /* To test seeking we just seek to our target PCM frame and then decode whatever is remaining and compare it against libFLAC. */
    drflac_result result;
    libflac libflac;
    drflac* pFlac;

    dr_printf_fixed_with_margin(FILE_NAME_WIDTH, TABLE_MARGIN, "%s", dr_path_file_name(pFilePath));

    /* First load the decoder from libFLAC. */
    result = libflac_init_file(pFilePath, &libflac);
    if (result != DRFLAC_SUCCESS) {
        printf("  Failed to open via libFLAC.");
        return result;
    }

    /* Now load from dr_flac. */
    pFlac = drflac_open_file(pFilePath, NULL);
    if (pFlac == NULL) {
        printf("  Failed to open via dr_flac.");
        libflac_uninit(&libflac);
        return DRFLAC_ERROR;    /* Failed to load dr_flac decoder. */
    }

    /* At this point we should have both libFLAC and dr_flac decoders open. We can now perform identical operations on each of them and compare. */
    result = decode_test_file_s32(&libflac, pFlac);
    if (result != DRFLAC_SUCCESS) {
        drflac_close(pFlac);
        libflac_uninit(&libflac);
        return result;
    }

    result = decode_test_file_f32(&libflac, pFlac);
    if (result != DRFLAC_SUCCESS) {
        drflac_close(pFlac);
        libflac_uninit(&libflac);
        return result;
    }

    result = decode_test_file_s16(&libflac, pFlac);
    if (result != DRFLAC_SUCCESS) {
        drflac_close(pFlac);
        libflac_uninit(&libflac);
        return result;
    }


    /* We're done with our decoders. */
    drflac_close(pFlac);
    libflac_uninit(&libflac);

    if (result == DRFLAC_SUCCESS) {
        printf("  Passed");
    }

    return result;
}

drflac_result decode_test_directory(const char* pDirectoryPath)
{
    dr_file_iterator iteratorState;
    dr_file_iterator* pFile;

    dr_printf_fixed(FILE_NAME_WIDTH, "%s", pDirectoryPath);
    dr_printf_fixed_with_margin(NUMBER_WIDTH, TABLE_MARGIN, "RESULT");
    printf("\n");

    pFile = dr_file_iterator_begin(pDirectoryPath, &iteratorState);
    while (pFile != NULL) {
        drflac_result result;

        /* Skip directories for now, but we may want to look at doing recursive file iteration. */
        if (!pFile->isDirectory) {
            result = decode_test_file(pFile->absolutePath);
            (void)result;

            printf("\n");
        }

        pFile = dr_file_iterator_next(pFile);
    }

    return DRFLAC_SUCCESS;
}

drflac_result decode_test()
{
    drflac_result result = DRFLAC_SUCCESS;

    /* Directories. */
    {
        result = decode_test_directory(DEFAULT_SOURCE_DIR);
        (void)result;
    }

    return result;
}



drflac_result open_and_read_test_file_s32(libflac* pLibFlac, const char* pFilePath)
{
    drflac_int32* pPCMFrames;
    drflac_uint64 pcmFrameCount;
    drflac_uint32 channels;
    drflac_uint32 sampleRate;
    drflac_uint64 iPCMFrame;

    pPCMFrames = drflac_open_file_and_read_pcm_frames_s32(pFilePath, &channels, &sampleRate, &pcmFrameCount, NULL);
    if (pPCMFrames == NULL) {
        printf("  drflac_open_and_read failed.");
        return DRFLAC_ERROR;    /* Error decoding */
    }

    if (pcmFrameCount != pLibFlac->pcmFrameCount) {
        printf("  Decoded frame counts differ: pcmFrameCount=%d, libFLAC=%d, dr_flac=%d", (int)pcmFrameCount, (int)pLibFlac->currentPCMFrame, (int)pcmFrameCount);
        drflac_free(pPCMFrames, NULL);
        return DRFLAC_ERROR;
    }

    for (iPCMFrame = 0; iPCMFrame < pLibFlac->pcmFrameCount; iPCMFrame += 1) {
        drflac_int32* pPCMFrame_libflac = pLibFlac->pPCMFrames + (iPCMFrame * pLibFlac->channels);
        drflac_int32* pPCMFrame_drflac  =           pPCMFrames + (iPCMFrame * pLibFlac->channels);
        drflac_uint32 iChannel;
        drflac_bool32 hasError = DRFLAC_FALSE;

        for (iChannel = 0; iChannel < pLibFlac->channels; iChannel += 1) {
            if (pPCMFrame_libflac[iChannel] != pPCMFrame_drflac[iChannel]) {
                printf("  PCM Frame @ %d[%d] does not match: pcmFrameCount=%d", (int)iPCMFrame, iChannel, (int)pcmFrameCount);
                hasError = DRFLAC_TRUE;
                break;
            }
        }

        if (hasError) {
            drflac_free(pPCMFrames, NULL);
            return DRFLAC_ERROR;    /* Decoded frames do not match. */
        }
    }

    drflac_free(pPCMFrames, NULL);
    return DRFLAC_SUCCESS;
}

drflac_result open_and_read_test_file_f32(libflac* pLibFlac, const char* pFilePath)
{
    float* pPCMFrames;
    drflac_uint64 pcmFrameCount;
    drflac_uint32 channels;
    drflac_uint32 sampleRate;
    drflac_uint64 iPCMFrame;

    pPCMFrames = drflac_open_file_and_read_pcm_frames_f32(pFilePath, &channels, &sampleRate, &pcmFrameCount, NULL);
    if (pPCMFrames == NULL) {
        printf("  drflac_open_and_read failed.");
        return DRFLAC_ERROR;    /* Error decoding */
    }

    if (pcmFrameCount != pLibFlac->pcmFrameCount) {
        printf("  Decoded frame counts differ: pcmFrameCount=%d, libFLAC=%d, dr_flac=%d", (int)pcmFrameCount, (int)pLibFlac->currentPCMFrame, (int)pcmFrameCount);
        drflac_free(pPCMFrames, NULL);
        return DRFLAC_ERROR;
    }

    for (iPCMFrame = 0; iPCMFrame < pLibFlac->pcmFrameCount; iPCMFrame += 1) {
        drflac_int32* pPCMFrame_libflac = pLibFlac->pPCMFrames + (iPCMFrame * pLibFlac->channels);
        float*        pPCMFrame_drflac  =           pPCMFrames + (iPCMFrame * pLibFlac->channels);
        drflac_uint32 iChannel;
        drflac_bool32 hasError = DRFLAC_FALSE;

        for (iChannel = 0; iChannel < pLibFlac->channels; iChannel += 1) {
            if ((pPCMFrame_libflac[iChannel] / 2147483648.0) != pPCMFrame_drflac[iChannel]) {
                printf("  PCM Frame @ %d[%d] does not match: pcmFrameCount=%d", (int)iPCMFrame, iChannel, (int)pcmFrameCount);
                hasError = DRFLAC_TRUE;
                break;
            }
        }

        if (hasError) {
            drflac_free(pPCMFrames, NULL);
            return DRFLAC_ERROR;    /* Decoded frames do not match. */
        }
    }

    drflac_free(pPCMFrames, NULL);
    return DRFLAC_SUCCESS;
}

drflac_result open_and_read_test_file_s16(libflac* pLibFlac, const char* pFilePath)
{
    drflac_int16* pPCMFrames;
    drflac_uint64 pcmFrameCount;
    drflac_uint32 channels;
    drflac_uint32 sampleRate;
    drflac_uint64 iPCMFrame;

    pPCMFrames = drflac_open_file_and_read_pcm_frames_s16(pFilePath, &channels, &sampleRate, &pcmFrameCount, NULL);
    if (pPCMFrames == NULL) {
        printf("  drflac_open_and_read failed.");
        return DRFLAC_ERROR;    /* Error decoding */
    }

    if (pcmFrameCount != pLibFlac->pcmFrameCount) {
        printf("  Decoded frame counts differ: pcmFrameCount=%d, libFLAC=%d, dr_flac=%d", (int)pcmFrameCount, (int)pLibFlac->currentPCMFrame, (int)pcmFrameCount);
        drflac_free(pPCMFrames, NULL);
        return DRFLAC_ERROR;
    }

    for (iPCMFrame = 0; iPCMFrame < pLibFlac->pcmFrameCount; iPCMFrame += 1) {
        drflac_int32* pPCMFrame_libflac = pLibFlac->pPCMFrames + (iPCMFrame * pLibFlac->channels);
        drflac_int16* pPCMFrame_drflac  =           pPCMFrames + (iPCMFrame * pLibFlac->channels);
        drflac_uint32 iChannel;
        drflac_bool32 hasError = DRFLAC_FALSE;

        for (iChannel = 0; iChannel < pLibFlac->channels; iChannel += 1) {
            if ((pPCMFrame_libflac[iChannel] >> 16) != pPCMFrame_drflac[iChannel]) {
                printf("  PCM Frame @ %d[%d] does not match: pcmFrameCount=%d", (int)iPCMFrame, iChannel, (int)pcmFrameCount);
                hasError = DRFLAC_TRUE;
                break;
            }
        }

        if (hasError) {
            drflac_free(pPCMFrames, NULL);
            return DRFLAC_ERROR;    /* Decoded frames do not match. */
        }
    }

    drflac_free(pPCMFrames, NULL);
    return DRFLAC_SUCCESS;
}

drflac_result open_and_read_test_file(const char* pFilePath)
{
    drflac_result result;
    libflac libflac;

    dr_printf_fixed_with_margin(FILE_NAME_WIDTH, TABLE_MARGIN, "%s", dr_path_file_name(pFilePath));

    /* First load the decoder from libFLAC. */
    result = libflac_init_file(pFilePath, &libflac);
    if (result != DRFLAC_SUCCESS) {
        printf("  Failed to open via libFLAC.");
        return result;
    }

    result = open_and_read_test_file_s32(&libflac, pFilePath);
    if (result != DRFLAC_SUCCESS) {
        return result;
    }

    open_and_read_test_file_f32(&libflac, pFilePath);
    if (result != DRFLAC_SUCCESS) {
        return result;
    }

    open_and_read_test_file_s16(&libflac, pFilePath);
    if (result != DRFLAC_SUCCESS) {
        return result;
    }

    libflac_uninit(&libflac);

    if (result == DRFLAC_SUCCESS) {
        printf("  Passed");
    }

    return result;
}

drflac_result open_and_read_test_directory(const char* pDirectoryPath)
{
    dr_file_iterator iteratorState;
    dr_file_iterator* pFile;

    dr_printf_fixed(FILE_NAME_WIDTH, "%s", pDirectoryPath);
    dr_printf_fixed_with_margin(NUMBER_WIDTH, TABLE_MARGIN, "RESULT");
    printf("\n");

    pFile = dr_file_iterator_begin(pDirectoryPath, &iteratorState);
    while (pFile != NULL) {
        drflac_result result;

        /* Skip directories for now, but we may want to look at doing recursive file iteration. */
        if (!pFile->isDirectory) {
            result = open_and_read_test_file(pFile->absolutePath);
            (void)result;

            printf("\n");
        }

        pFile = dr_file_iterator_next(pFile);
    }

    return DRFLAC_SUCCESS;
}

drflac_result open_and_read_test()
{
    drflac_result result = DRFLAC_SUCCESS;

    /* Directories. */
    {
        result = open_and_read_test_directory(DEFAULT_SOURCE_DIR);
        (void)result;
    }

    return result;
}



drflac_result decode_profiling_file(const char* pFilePath)
{
    drflac_result result;
    libflac libflac;
    drflac* pFlac;
    drflac_int32* pTempBuffer;
    double decodeTimeBeg;
    double decodeTimeEnd;
    double drflacDecodeTimeInSeconds;
    void* pFileData;
    size_t fileSizeInBytes;

    dr_printf_fixed_with_margin(FILE_NAME_WIDTH, 2, "%s", dr_path_file_name(pFilePath));

    /* libFLAC */
    result = libflac_init_file(pFilePath, &libflac);
    if (result != DRFLAC_SUCCESS) {
        printf("  [libFLAC] Failed to load file");
        return result;
    }

    /* dr_flac */
    pFileData = dr_open_and_read_file(pFilePath, &fileSizeInBytes);
    if (pFileData == NULL) {
        printf("  Failed to load file");
        return DRFLAC_ERROR;    /* Failed to open the file. */
    }

    pFlac = drflac_open_memory(pFileData, fileSizeInBytes, NULL);
    if (pFlac == NULL) {
        free(pFileData);
        printf("  [dr_flac] Failed to load file.");
        return DRFLAC_ERROR;
    }

    /* libFLAC decode time. */
    dr_printf_fixed_with_margin(NUMBER_WIDTH, TABLE_MARGIN, "%.2fms", libflac.decodeTimeInSeconds*1000);

    /* dr_flac decode time. */
    pTempBuffer = (drflac_int32*)malloc((size_t)(libflac.pcmFrameCount * libflac.channels * sizeof(drflac_int32)));
    if (pTempBuffer == NULL) {
        libflac_uninit(&libflac);
        drflac_close(pFlac);
        free(pFileData);
        printf("  Out of memory.");
        return DRFLAC_ERROR;    /* Out of memory. */
    }

    DRFLAC_ZERO_MEMORY(pTempBuffer, (size_t)(libflac.pcmFrameCount * libflac.channels * sizeof(drflac_int32)));

    decodeTimeBeg = dr_timer_now();
    drflac_read_pcm_frames_s32(pFlac, libflac.pcmFrameCount, pTempBuffer);
    decodeTimeEnd = dr_timer_now();

    free(pTempBuffer);
    free(pFileData);

    drflacDecodeTimeInSeconds = decodeTimeEnd - decodeTimeBeg;
    dr_printf_fixed_with_margin(NUMBER_WIDTH, TABLE_MARGIN, "%.2fms", drflacDecodeTimeInSeconds*1000);

    /* Difference. */
    dr_printf_fixed_with_margin(NUMBER_WIDTH, TABLE_MARGIN, "%d%%", (int)(drflacDecodeTimeInSeconds/libflac.decodeTimeInSeconds * 100));

    libflac_uninit(&libflac);
    drflac_close(pFlac);

    return DRFLAC_SUCCESS;
}

drflac_result decode_profiling_directory(const char* pDirectoryPath)
{
    dr_file_iterator iteratorState;
    dr_file_iterator* pFile;
    drflac_bool32 foundError = DRFLAC_FALSE;

    dr_printf_fixed(FILE_NAME_WIDTH, "%s", pDirectoryPath);
    dr_printf_fixed_with_margin(NUMBER_WIDTH, TABLE_MARGIN, "libFLAC");
    dr_printf_fixed_with_margin(NUMBER_WIDTH, TABLE_MARGIN, "dr_flac");
    printf("\n");

    pFile = dr_file_iterator_begin(pDirectoryPath, &iteratorState);
    while (pFile != NULL) {
        drflac_result result;

        /* Skip directories for now, but we may want to look at doing recursive file iteration. */
        if (!pFile->isDirectory) {
            result = decode_profiling_file(pFile->absolutePath);
            if (result != DRFLAC_SUCCESS) {
                foundError = DRFLAC_TRUE;
            }

            printf("\n");
        }

        pFile = dr_file_iterator_next(pFile);
    }

    return (foundError) ? DRFLAC_ERROR : DRFLAC_SUCCESS;
}

drflac_result decode_profiling()
{
    drflac_result result = DRFLAC_SUCCESS;

    /* Directories. */
    {
        result = decode_profiling_directory(DEFAULT_SOURCE_DIR);
    }

    return result;
}


int main(int argc, char** argv)
{
    drflac_result result = DRFLAC_SUCCESS;
    drflac_bool32 doTesting = DRFLAC_TRUE;
    drflac_bool32 doProfiling = DRFLAC_TRUE;

    /* This program has two main parts. The first is just a normal functionality test. The second is a profiling of the different seeking methods. */
    if (dr_argv_is_set(argc, argv, "--onlyprofile")) {
        doTesting = DRFLAC_FALSE;
    }

    print_cpu_caps();

    /* Exhaustive seek test. */
    if (doTesting) {
        printf("=======================================================================\n");
        printf("DECODE TESTING\n");
        printf("=======================================================================\n");
        result = decode_test();
        if (result != DRFLAC_SUCCESS) {
            return (int)result;    /* Don't continue if an error occurs during testing. */
        }
        printf("\n");

        printf("=======================================================================\n");
        printf("OPEN-AND-READ TESTING - drflac_open_*_and_read_pcm_frames_*()\n");
        printf("=======================================================================\n");
        result = open_and_read_test();
        if (result != DRFLAC_SUCCESS) {
            return (int)result;    /* Don't continue if an error occurs during testing. */
        }
        printf("\n");
    } else {
        printf("=======================================================================\n");
        printf("WARNING: Correctness Tests Disabled\n");
        printf("=======================================================================\n");
    }

    /* Profiling. */
    if (doProfiling) {
        printf("=======================================================================\n");
        printf("DECODE PROFILING (LOWER IS BETTER)\n");
        printf("=======================================================================\n");
        result = decode_profiling();
        printf("\n");
    }

    /*getchar();*/
    return (int)result;
}
