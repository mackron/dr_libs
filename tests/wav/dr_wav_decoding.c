#define DR_WAV_LIBSNDFILE_COMPAT
#include "dr_wav_common.c"

#define FILE_NAME_WIDTH 40
#define NUMBER_WIDTH    10
#define TABLE_MARGIN    2

#define DEFAULT_SOURCE_DIR  "testvectors/wav/tests"


drwav_result decode_test__read_and_compare_pcm_frames_s32(libsndfile* pSndFile, drwav* pWav, drwav_uint64 pcmFrameCount, drwav_int32* pPCMFrames_libsndfile, drwav_int32* pPCMFrames_drwav)
{
    drwav_uint64 pcmFrameCount_libsndfile;
    drwav_uint64 pcmFrameCount_drwav;
    drwav_uint64 iPCMFrame;

    /* To test decoding we just read a number of PCM frames from each decoder and compare. */
    pcmFrameCount_libsndfile = libsndfile_read_pcm_frames_s32(pSndFile, pcmFrameCount, pPCMFrames_libsndfile);
    pcmFrameCount_drwav = drwav_read_pcm_frames_s32(pWav, pcmFrameCount, pPCMFrames_drwav);

    /* The total number of frames we decoded need to match. */
    if (pcmFrameCount_libsndfile != pcmFrameCount_drwav) {
        printf("  Decoded frame counts differ: pcmFrameCount=%d, libsndfile=%d, dr_wav=%d", (int)pcmFrameCount, (int)pcmFrameCount_libsndfile, (int)pcmFrameCount_drwav);
        return DRWAV_ERROR;
    }

    /* Each of the decoded PCM frames need to match. */
    DRWAV_ASSERT(pcmFrameCount_libsndfile == pcmFrameCount_drwav);

    for (iPCMFrame = 0; iPCMFrame < pcmFrameCount_libsndfile; iPCMFrame += 1) {
        drwav_int32* pPCMFrame_libsndfile = pPCMFrames_libsndfile + (iPCMFrame * pWav->channels);
        drwav_int32* pPCMFrame_drwav  = pPCMFrames_drwav  + (iPCMFrame * pWav->channels);
        drwav_uint32 iChannel;
        drwav_bool32 hasError = DRWAV_FALSE;

        for (iChannel = 0; iChannel < pWav->channels; iChannel += 1) {
            if (pPCMFrame_libsndfile[iChannel] != pPCMFrame_drwav[iChannel]) {
                printf("  PCM Frame @ %d[%d] does not match: pcmFrameCount=%d", (int)iPCMFrame, iChannel, (int)pcmFrameCount);
                hasError = DRWAV_TRUE;
                break;
            }
        }

        if (hasError) {
            return DRWAV_ERROR;    /* Decoded frames do not match. */
        }
    }

    /* Done. */
    return DRWAV_SUCCESS;
}

drwav_result decode_test__read_and_compare_pcm_frame_chunks_s32(libsndfile* pSndFile, drwav* pWav, drwav_uint64 pcmFrameChunkSize)
{
    drwav_result result = DRWAV_SUCCESS;
    drwav_uint64 iPCMFrame;
    drwav_int32* pPCMFrames_libsndfile;
    drwav_int32* pPCMFrames_drwav;

    /* Make sure the decoder's are seeked back to the start first. */
    drwav_seek_to_pcm_frame(pWav, 0);
    libsndfile_seek_to_pcm_frame(pSndFile, 0);

    pPCMFrames_libsndfile = (drwav_int32*)malloc((size_t)(pcmFrameChunkSize * pWav->channels * sizeof(drwav_int32)));
    if (pPCMFrames_libsndfile == NULL) {
        printf("  [libsndfile] Out of memory");
        return DRWAV_ERROR;
    }

    pPCMFrames_drwav = (drwav_int32*)malloc((size_t)(pcmFrameChunkSize * pWav->channels * sizeof(drwav_int32)));
    if (pPCMFrames_drwav == NULL) {
        free(pPCMFrames_libsndfile);
        printf("  [dr_wav] Out of memory");
        return DRWAV_ERROR;
    }

    for (iPCMFrame = 0; iPCMFrame < pWav->totalPCMFrameCount; iPCMFrame += pcmFrameChunkSize) {
        result = decode_test__read_and_compare_pcm_frames_s32(pSndFile, pWav, pcmFrameChunkSize, pPCMFrames_libsndfile, pPCMFrames_drwav);
        if (result != DRWAV_SUCCESS) {
            break;
        }
    }

    free(pPCMFrames_libsndfile);
    free(pPCMFrames_drwav);

    return result;
}


drwav_result decode_test__read_and_compare_pcm_frames_f32(libsndfile* pSndFile, drwav* pWav, drwav_uint64 pcmFrameCount, float* pPCMFrames_libsndfile, float* pPCMFrames_drwav)
{
    drwav_uint64 pcmFrameCount_libsndfile;
    drwav_uint64 pcmFrameCount_drwav;
    drwav_uint64 iPCMFrame;

    /* To test decoding we just read a number of PCM frames from each decoder and compare. */
    pcmFrameCount_libsndfile = libsndfile_read_pcm_frames_f32(pSndFile, pcmFrameCount, pPCMFrames_libsndfile);
    pcmFrameCount_drwav = drwav_read_pcm_frames_f32(pWav, pcmFrameCount, pPCMFrames_drwav);

    /* The total number of frames we decoded need to match. */
    if (pcmFrameCount_libsndfile != pcmFrameCount_drwav) {
        printf("  Decoded frame counts differ: pcmFrameCount=%d, libsndfile=%d, dr_wav=%d", (int)pcmFrameCount, (int)pcmFrameCount_libsndfile, (int)pcmFrameCount_drwav);
        return DRWAV_ERROR;
    }

    /* Each of the decoded PCM frames need to match. */
    DRWAV_ASSERT(pcmFrameCount_libsndfile == pcmFrameCount_drwav);

    for (iPCMFrame = 0; iPCMFrame < pcmFrameCount_libsndfile; iPCMFrame += 1) {
        float* pPCMFrame_libsndfile = pPCMFrames_libsndfile + (iPCMFrame * pWav->channels);
        float* pPCMFrame_drwav  = pPCMFrames_drwav  + (iPCMFrame * pWav->channels);
        drwav_uint32 iChannel;
        drwav_bool32 hasError = DRWAV_FALSE;

        for (iChannel = 0; iChannel < pWav->channels; iChannel += 1) {
            if (pPCMFrame_libsndfile[iChannel] != pPCMFrame_drwav[iChannel]) {
                printf("  PCM Frame @ %d[%d] does not match: pcmFrameCount=%d", (int)iPCMFrame, iChannel, (int)pcmFrameCount);
                hasError = DRWAV_TRUE;
                break;
            }
        }

        if (hasError) {
            return DRWAV_ERROR;    /* Decoded frames do not match. */
        }
    }

    /* Done. */
    return DRWAV_SUCCESS;
}

drwav_result decode_test__read_and_compare_pcm_frame_chunks_f32(libsndfile* pSndFile, drwav* pWav, drwav_uint64 pcmFrameChunkSize)
{
    drwav_result result = DRWAV_SUCCESS;
    drwav_uint64 iPCMFrame;
    float* pPCMFrames_libsndfile;
    float* pPCMFrames_drwav;

    /* Make sure the decoder's are seeked back to the start first. */
    drwav_seek_to_pcm_frame(pWav, 0);
    libsndfile_seek_to_pcm_frame(pSndFile, 0);

    pPCMFrames_libsndfile = (float*)malloc((size_t)(pcmFrameChunkSize * pWav->channels * sizeof(float)));
    if (pPCMFrames_libsndfile == NULL) {
        printf("  [libsndfile] Out of memory");
        return DRWAV_ERROR;
    }

    pPCMFrames_drwav = (float*)malloc((size_t)(pcmFrameChunkSize * pWav->channels * sizeof(float)));
    if (pPCMFrames_drwav == NULL) {
        free(pPCMFrames_libsndfile);
        printf("  [dr_wav] Out of memory");
        return DRWAV_ERROR;
    }

    for (iPCMFrame = 0; iPCMFrame < pWav->totalPCMFrameCount; iPCMFrame += pcmFrameChunkSize) {
        result = decode_test__read_and_compare_pcm_frames_f32(pSndFile, pWav, pcmFrameChunkSize, pPCMFrames_libsndfile, pPCMFrames_drwav);
        if (result != DRWAV_SUCCESS) {
            break;
        }
    }

    free(pPCMFrames_libsndfile);
    free(pPCMFrames_drwav);

    return result;
}


drwav_result decode_test__read_and_compare_pcm_frames_s16(libsndfile* pSndFile, drwav* pWav, drwav_uint64 pcmFrameCount, drwav_int16* pPCMFrames_libsndfile, drwav_int16* pPCMFrames_drwav)
{
    drwav_uint64 pcmFrameCount_libsndfile;
    drwav_uint64 pcmFrameCount_drwav;
    drwav_uint64 iPCMFrame;

    /* To test decoding we just read a number of PCM frames from each decoder and compare. */
    pcmFrameCount_libsndfile = libsndfile_read_pcm_frames_s16(pSndFile, pcmFrameCount, pPCMFrames_libsndfile);
    pcmFrameCount_drwav = drwav_read_pcm_frames_s16(pWav, pcmFrameCount, pPCMFrames_drwav);

    /* The total number of frames we decoded need to match. */
    if (pcmFrameCount_libsndfile != pcmFrameCount_drwav) {
        printf("  Decoded frame counts differ: pcmFrameCount=%d, libsndfile=%d, dr_wav=%d", (int)pcmFrameCount, (int)pcmFrameCount_libsndfile, (int)pcmFrameCount_drwav);
        return DRWAV_ERROR;
    }

    /* Each of the decoded PCM frames need to match. */
    DRWAV_ASSERT(pcmFrameCount_libsndfile == pcmFrameCount_drwav);

    for (iPCMFrame = 0; iPCMFrame < pcmFrameCount_libsndfile; iPCMFrame += 1) {
        drwav_int16* pPCMFrame_libsndfile = pPCMFrames_libsndfile + (iPCMFrame * pWav->channels);
        drwav_int16* pPCMFrame_drwav  = pPCMFrames_drwav  + (iPCMFrame * pWav->channels);
        drwav_uint32 iChannel;
        drwav_bool32 hasError = DRWAV_FALSE;

        for (iChannel = 0; iChannel < pWav->channels; iChannel += 1) {
            if (pPCMFrame_libsndfile[iChannel] != pPCMFrame_drwav[iChannel]) {
                printf("  PCM Frame @ %d[%d] does not match: pcmFrameCount=%d", (int)iPCMFrame, iChannel, (int)pcmFrameCount);
                hasError = DRWAV_TRUE;
                break;
            }
        }

        if (hasError) {
            return DRWAV_ERROR;    /* Decoded frames do not match. */
        }
    }

    /* Done. */
    return DRWAV_SUCCESS;
}

drwav_result decode_test__read_and_compare_pcm_frame_chunks_s16(libsndfile* pSndFile, drwav* pWav, drwav_uint64 pcmFrameChunkSize)
{
    drwav_result result = DRWAV_SUCCESS;
    drwav_uint64 iPCMFrame;
    drwav_int16* pPCMFrames_libsndfile;
    drwav_int16* pPCMFrames_drwav;

    /* Make sure the decoder's are seeked back to the start first. */
    drwav_seek_to_pcm_frame(pWav, 0);
    libsndfile_seek_to_pcm_frame(pSndFile, 0);

    pPCMFrames_libsndfile = (drwav_int16*)malloc((size_t)(pcmFrameChunkSize * pWav->channels * sizeof(drwav_int16)));
    if (pPCMFrames_libsndfile == NULL) {
        printf("  [libsndfile] Out of memory");
        return DRWAV_ERROR;
    }

    pPCMFrames_drwav = (drwav_int16*)malloc((size_t)(pcmFrameChunkSize * pWav->channels * sizeof(drwav_int16)));
    if (pPCMFrames_drwav == NULL) {
        free(pPCMFrames_libsndfile);
        printf("  [dr_wav] Out of memory");
        return DRWAV_ERROR;
    }

    for (iPCMFrame = 0; iPCMFrame < pWav->totalPCMFrameCount; iPCMFrame += pcmFrameChunkSize) {
        result = decode_test__read_and_compare_pcm_frames_s16(pSndFile, pWav, pcmFrameChunkSize, pPCMFrames_libsndfile, pPCMFrames_drwav);
        if (result != DRWAV_SUCCESS) {
            break;
        }
    }

    free(pPCMFrames_libsndfile);
    free(pPCMFrames_drwav);

    return result;
}


drwav_result decode_test_file_s32(libsndfile* pSndFile, drwav* pWav)
{
    drwav_result result = DRWAV_SUCCESS;

    /* Start with reading the entire file in one go. */
    if (result == DRWAV_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_s32(pSndFile, pWav, pWav->totalPCMFrameCount);
    }

    /* Now try with reading one PCM frame at a time.*/
    if (result == DRWAV_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_s32(pSndFile, pWav, 1);
    }

    return result;
}

drwav_result decode_test_file_f32(libsndfile* pSndFile, drwav* pWav)
{
    drwav_result result = DRWAV_SUCCESS;

    /* Start with reading the entire file in one go. */
    if (result == DRWAV_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_f32(pSndFile, pWav, pWav->totalPCMFrameCount);
    }

    /* Now try with reading one PCM frame at a time.*/
    if (result == DRWAV_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_f32(pSndFile, pWav, 1);
    }

    return result;
}

drwav_result decode_test_file_s16(libsndfile* pSndFile, drwav* pWav)
{
    drwav_result result = DRWAV_SUCCESS;

    /* Start with reading the entire file in one go. */
    if (result == DRWAV_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_s16(pSndFile, pWav, pWav->totalPCMFrameCount);
    }

    /* Now try with reading one PCM frame at a time.*/
    if (result == DRWAV_SUCCESS) {
        result = decode_test__read_and_compare_pcm_frame_chunks_s16(pSndFile, pWav, 1);
    }

    return result;
}

drwav_result decode_test_file(const char* pFilePath)
{
    /* To test seeking we just seek to our target PCM frame and then decode whatever is remaining and compare it against libsndfile. */
    drwav_result result;
    libsndfile libsndfile;
    drwav wav;

    dr_printf_fixed_with_margin(FILE_NAME_WIDTH, TABLE_MARGIN, "%s", dr_path_file_name(pFilePath));

    /* First load the decoder from libsndfile. */
    result = libsndfile_init_file(pFilePath, &libsndfile);
    if (result != DRWAV_SUCCESS) {
        printf("  Failed to open via libsndfile.");
        return result;
    }

    /* Now load from dr_wav. */
    if (!drwav_init_file_with_metadata(&wav, pFilePath, 0, NULL)) {
        printf("  Failed to open via dr_wav.");
        libsndfile_uninit(&libsndfile);
        return DRWAV_ERROR; /* Failed to load dr_wav decoder. */
    }

    /* At this point we should have both libsndfile and dr_wav decoders open. We can now perform identical operations on each of them and compare. */
    result = decode_test_file_s32(&libsndfile, &wav);
    if (result != DRWAV_SUCCESS) {
        drwav_uninit(&wav);
        libsndfile_uninit(&libsndfile);
        return result;
    }

    result = decode_test_file_f32(&libsndfile, &wav);
    if (result != DRWAV_SUCCESS) {
        drwav_uninit(&wav);
        libsndfile_uninit(&libsndfile);
        return result;
    }

    result = decode_test_file_s16(&libsndfile, &wav);
    if (result != DRWAV_SUCCESS) {
        drwav_uninit(&wav);
        libsndfile_uninit(&libsndfile);
        return result;
    }


    /* We're done with our decoders. */
    drwav_uninit(&wav);
    libsndfile_uninit(&libsndfile);

    if (result == DRWAV_SUCCESS) {
        printf("  Passed");
    }

    return result;
}

drwav_result decode_test_directory(const char* pDirectoryPath)
{
    dr_file_iterator iteratorState;
    dr_file_iterator* pFile;

    dr_printf_fixed(FILE_NAME_WIDTH, "%s", pDirectoryPath);
    dr_printf_fixed_with_margin(NUMBER_WIDTH, TABLE_MARGIN, "RESULT");
    printf("\n");

    pFile = dr_file_iterator_begin(pDirectoryPath, &iteratorState);
    while (pFile != NULL) {
        drwav_result result;

        /* Skip directories for now, but we may want to look at doing recursive file iteration. */
        if (!pFile->isDirectory) {
            result = decode_test_file(pFile->absolutePath);
            (void)result;

            printf("\n");
        }

        pFile = dr_file_iterator_next(pFile);
    }

    return DRWAV_SUCCESS;
}

drwav_result decode_test()
{
    drwav_result result = DRWAV_SUCCESS;

    /* Directories. */
    {
        result = decode_test_directory(DEFAULT_SOURCE_DIR);
        (void)result;
    }

    return result;
}

drwav_result decode_profiling()
{
    return DRWAV_SUCCESS;
}

int main(int argc, char** argv)
{
    drwav_result result      = DRWAV_SUCCESS;
    drwav_bool32 doTesting   = DRWAV_TRUE;
    drwav_bool32 doProfiling = DRWAV_TRUE;

    if (dr_argv_is_set(argc, argv, "--onlyprofile")) {
        doTesting = DRWAV_FALSE;
    }

    if (libsndfile_init_api() != DRWAV_SUCCESS) {
        printf("Failed to initialize libsndfile API.");
        return -1;
    }

    if (doTesting) {
        printf("=======================================================================\n");
        printf("DECODE TESTING\n");
        printf("=======================================================================\n");
        result = decode_test();
        if (result != DRWAV_SUCCESS) {
            goto done;  /* Don't continue if an error occurs during testing. */
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

done:
    libsndfile_uninit_api();
    return (int)result;
}
