/*#define DR_FLAC_NO_CRC*/
#include "dr_flac_common.c"

#define PROFILING_NAME_WIDTH    40
#define PROFILING_NUMBER_WIDTH  10
#define PROFILING_NUMBER_MARGIN 2

typedef struct
{
    double totalSeconds_BruteForce;
    double totalSeconds_BinarySearch;
    double totalSeconds_SeekTable;
} profiling_state;

void profiling_state_init(profiling_state* pProfiling)
{
    DRFLAC_ZERO_MEMORY(pProfiling, sizeof(*pProfiling));
}

profiling_state profiling_state_sum(const profiling_state* pA, const profiling_state* pB)
{
    profiling_state result;
    result.totalSeconds_BruteForce   = pA->totalSeconds_BruteForce   + pB->totalSeconds_BruteForce;
    result.totalSeconds_BinarySearch = pA->totalSeconds_BinarySearch + pB->totalSeconds_BinarySearch;
    result.totalSeconds_SeekTable    = pA->totalSeconds_SeekTable    + pB->totalSeconds_SeekTable;

    return result;
}


drflac_result seek_test_pcm_frame(libflac* pLibFlac, drflac* pFlac, drflac_uint64 targetPCMFrameIndex)
{
    drflac_bool32 seekResult;
    drflac_uint64 pcmFrameCount_libflac;
    drflac_uint64 pcmFrameCount_drflac;
    drflac_int32* pPCMFrames_libflac;
    drflac_int32* pPCMFrames_drflac;
    drflac_uint64 iPCMFrame;

    if (pFlac->_noSeekTableSeek == DRFLAC_FALSE && pFlac->_noBinarySearchSeek == DRFLAC_TRUE && pFlac->_noBruteForceSeek == DRFLAC_TRUE) {
        if (pFlac->seekpointCount == 0) {
            printf("  No seek table");
            return DRFLAC_ERROR;
        }
    }

    /*
    To test seeking we just seek to the PCM frame, and then decode the rest of the file. If the PCM frames we read
    differs between the two implementations there's something wrong with one of them (probably dr_flac).
    */
    seekResult = libflac_seek_to_pcm_frame(pLibFlac, targetPCMFrameIndex);
    if (seekResult == DRFLAC_FALSE) {
        printf("  [libFLAC] Failed to seek to PCM frame @ %d", (int)targetPCMFrameIndex);
        return DRFLAC_ERROR;
    }

    seekResult = drflac_seek_to_pcm_frame(pFlac, targetPCMFrameIndex);
    if (seekResult == DRFLAC_FALSE) {
        printf("  [dr_flac] Failed to seek to PCM frame @ %d", (int)targetPCMFrameIndex);
        return DRFLAC_ERROR;
    }

    if (pLibFlac->currentPCMFrame != pFlac->currentPCMFrame) {
        printf("  Current PCM frame inconsistent @ %d: libFLAC=%d, dr_flac=%d", (int)targetPCMFrameIndex, (int)pLibFlac->currentPCMFrame, (int)pFlac->currentPCMFrame);
        return DRFLAC_ERROR;
    }

    /*
    Now we decode the rest of the file and compare the samples. Note that we try reading the _entire_ file, not just the leftovers, to ensure we
    haven't seeked too short.
    */
    pPCMFrames_libflac = (drflac_int32*)malloc((size_t)(pLibFlac->pcmFrameCount * pLibFlac->channels * sizeof(drflac_int32)));
    if (pPCMFrames_libflac == NULL) {
        printf("  [libFLAC] Out of memory");
        return DRFLAC_ERROR;
    }
    pcmFrameCount_libflac = libflac_read_pcm_frames_s32(pLibFlac, pLibFlac->pcmFrameCount, pPCMFrames_libflac);

    pPCMFrames_drflac = (drflac_int32*)malloc((size_t)(pLibFlac->pcmFrameCount * pLibFlac->channels * sizeof(drflac_int32)));
    if (pPCMFrames_drflac == NULL) {
        free(pPCMFrames_libflac);
        printf("  [dr_flac] Out of memory");
        return DRFLAC_ERROR;
    }
    pcmFrameCount_drflac = drflac_read_pcm_frames_s32(pFlac, pLibFlac->pcmFrameCount, pPCMFrames_drflac);

    /* The total number of frames we decoded need to match. */
    if (pcmFrameCount_libflac != pcmFrameCount_drflac) {
        free(pPCMFrames_drflac);
        free(pPCMFrames_libflac);
        printf("  Decoded frame counts differ @ %d: libFLAC=%d, dr_flac=%d", (int)targetPCMFrameIndex, (int)pLibFlac->currentPCMFrame, (int)pFlac->currentPCMFrame);
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
                printf("  PCM Frame @ %d[%d] does not match: targetPCMFrameIndex=%d", (int)iPCMFrame, iChannel, (int)targetPCMFrameIndex);
                hasError = DRFLAC_TRUE;
                break;
            }
        }

        if (hasError) {
            free(pPCMFrames_drflac);
            free(pPCMFrames_libflac);
            return DRFLAC_ERROR;    /* Decoded frames do not match. */
        }
    }


    /* Done. */
    free(pPCMFrames_drflac);
    free(pPCMFrames_libflac);

    return DRFLAC_SUCCESS;
}

drflac_result seek_test_file(const char* pFilePath)
{
    /* To test seeking we just seek to our target PCM frame and then decode whatever is remaining and compare it against libFLAC. */
    drflac_result result;
    libflac libflac;
    drflac* pFlac;
    drflac_uint32 iteration;
    drflac_uint32 totalIterationCount = 10;

    dr_printf_fixed_with_margin(PROFILING_NAME_WIDTH, PROFILING_NUMBER_MARGIN, "%s", dr_path_file_name(pFilePath));

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

    /* Use these to use specific seeking methods. Set all to false to use the normal prioritization (seek table, then binary search, then brute force). */
    pFlac->_noSeekTableSeek    = DRFLAC_FALSE;
    pFlac->_noBinarySearchSeek = DRFLAC_FALSE;
    pFlac->_noBruteForceSeek   = DRFLAC_FALSE;

    /* At this point we should have both libFLAC and dr_flac decoders open. We can now perform identical operations on each of them and compare. */

    /* Start with the basics: Seek to the very end, and then the very start. */
    if (result == DRFLAC_SUCCESS) {
        result = seek_test_pcm_frame(&libflac, pFlac, libflac.pcmFrameCount);
    }
    if (result == DRFLAC_SUCCESS) {
        result = seek_test_pcm_frame(&libflac, pFlac, 0);
    }

    /* Now we'll try seeking to random locations. */
    dr_seed(1234);

    iteration = 0;
    while (result == DRFLAC_SUCCESS && iteration < totalIterationCount) {
        dr_uint64 targetPCMFrame = dr_rand_range_u64(0, libflac.pcmFrameCount);
        if (targetPCMFrame > libflac.pcmFrameCount) {
            DRFLAC_ASSERT(DRFLAC_FALSE);    /* Should never hit this, but if we do it means our random number generation routine is wrong. */
        }

        result = seek_test_pcm_frame(&libflac, pFlac, targetPCMFrame);
        iteration += 1;
    }

    /* We're done with our decoders. */
    drflac_close(pFlac);
    libflac_uninit(&libflac);

    if (result == DRFLAC_SUCCESS) {
        printf("  Passed");
    }

    return result;
}

drflac_result seek_test_directory(const char* pDirectoryPath)
{
    dr_file_iterator iteratorState;
    dr_file_iterator* pFile;

    dr_printf_fixed(PROFILING_NAME_WIDTH, "%s", pDirectoryPath);
    dr_printf_fixed_with_margin(PROFILING_NUMBER_WIDTH, PROFILING_NUMBER_MARGIN, "RESULT");
    printf("\n");

    pFile = dr_file_iterator_begin(pDirectoryPath, &iteratorState);
    while (pFile != NULL) {
        drflac_result result;

        /* Skip directories for now, but we may want to look at doing recursive file iteration. */
        if (!pFile->isDirectory) {
            result = seek_test_file(pFile->absolutePath);
            (void)result;

            printf("\n");
        }

        pFile = dr_file_iterator_next(pFile);
    }

    return DRFLAC_SUCCESS;
}

drflac_result seek_test()
{
    drflac_result result = DRFLAC_SUCCESS;

    /* Directories. */
    {
        result = seek_test_directory("testvectors/flac/tests");
        (void)result;
    }

    return result;
}


drflac_result seek_profiling_drflac_and_close(drflac* pFlac, double* pProcessingTime)
{
    drflac_result result = DRFLAC_SUCCESS;
    int i;

    if (pFlac == NULL) {
        result = DRFLAC_INVALID_ARGS;
        goto done;
    }

    if (pFlac->totalPCMFrameCount == 0) {
        result = DRFLAC_INVALID_ARGS;
        goto done;
    }

    if (pProcessingTime != NULL) {
        *pProcessingTime = 0;
    }

    /* Seek back to the start to keep everything normalized. */
    drflac_seek_to_pcm_frame(pFlac, 0);

    /* Random seek points based on a seed. */
    dr_seed(1234);
    /*dr_seed(4321);*/
    for (i = 0; i < 100; ++i) {
        double startTime;
        double endTime;
        dr_uint64 targetPCMFrame = dr_rand_range_u64(0, pFlac->totalPCMFrameCount);

        startTime = dr_timer_now();
        {
            drflac_seek_to_pcm_frame(pFlac, targetPCMFrame);
        }
        endTime = dr_timer_now();

        if (pProcessingTime != NULL) {
            *pProcessingTime += (endTime - startTime);
        }
    }

done:
    drflac_close(pFlac);
    return result;
}

drflac_result seek_profiling_file__seek_table(const char* pFilePath, double* pProcessingTime)
{
    drflac* pFlac;

    if (pFilePath == NULL) {
        return DRFLAC_INVALID_ARGS;
    }

    pFlac = drflac_open_file(pFilePath, NULL);
    if (pFlac == NULL) {
        return DRFLAC_ERROR;
    }

    pFlac->_noSeekTableSeek    = DRFLAC_FALSE;
    pFlac->_noBinarySearchSeek = DRFLAC_TRUE;
    pFlac->_noBruteForceSeek   = DRFLAC_TRUE;

    return seek_profiling_drflac_and_close(pFlac, pProcessingTime);
}

drflac_result seek_profiling_file__binary_search(const char* pFilePath, double* pProcessingTime)
{
    drflac* pFlac;

    if (pFilePath == NULL) {
        return DRFLAC_INVALID_ARGS;
    }

    pFlac = drflac_open_file(pFilePath, NULL);
    if (pFlac == NULL) {
        return DRFLAC_ERROR;
    }

    pFlac->_noSeekTableSeek    = DRFLAC_TRUE;
    pFlac->_noBinarySearchSeek = DRFLAC_FALSE;
    pFlac->_noBruteForceSeek   = DRFLAC_TRUE;

    return seek_profiling_drflac_and_close(pFlac, pProcessingTime);
}

drflac_result seek_profiling_file__brute_force(const char* pFilePath, double* pProcessingTime)
{
    drflac* pFlac;

    if (pFilePath == NULL) {
        return DRFLAC_INVALID_ARGS;
    }

    pFlac = drflac_open_file(pFilePath, NULL);
    if (pFlac == NULL) {
        return DRFLAC_ERROR;
    }

    pFlac->_noSeekTableSeek    = DRFLAC_TRUE;
    pFlac->_noBinarySearchSeek = DRFLAC_TRUE;
    pFlac->_noBruteForceSeek   = DRFLAC_FALSE;

    return seek_profiling_drflac_and_close(pFlac, pProcessingTime);
}

drflac_result seek_profiling_file(const char* pFilePath, profiling_state* pProfiling)
{
    drflac_result result;

    profiling_state_init(pProfiling);

    /*
    There are different seeking modes, and each one is profiled so that we can compare the results:
        - Brute Force
        - Binary Search
        - Seek Table

    In order to keep the total run time fair, we can only include files with a seek table.
    */
    dr_printf_fixed_with_margin(PROFILING_NAME_WIDTH, 2, "%s", dr_path_file_name(pFilePath));
    
    /* Start off with the seek table version. If this fails we don't bother continuing. */
#if 1
    result = seek_profiling_file__seek_table(pFilePath, &pProfiling->totalSeconds_SeekTable);
    if (result != DRFLAC_SUCCESS) {
        return result;
    }
    dr_printf_fixed_with_margin(PROFILING_NUMBER_WIDTH, PROFILING_NUMBER_MARGIN, "%f", pProfiling->totalSeconds_SeekTable);
#else
    dr_printf_fixed_with_margin(PROFILING_NUMBER_WIDTH, PROFILING_NUMBER_MARGIN, "");
#endif

#if 1
    result = seek_profiling_file__binary_search(pFilePath, &pProfiling->totalSeconds_BinarySearch);
    if (result != DRFLAC_SUCCESS) {
        return result;
    }
    dr_printf_fixed_with_margin(PROFILING_NUMBER_WIDTH, PROFILING_NUMBER_MARGIN, "%f", pProfiling->totalSeconds_BinarySearch);
#else
    dr_printf_fixed_with_margin(PROFILING_NUMBER_WIDTH, PROFILING_NUMBER_MARGIN, "");
#endif

#if 1
    result = seek_profiling_file__brute_force(pFilePath, &pProfiling->totalSeconds_BruteForce);
    if (result != DRFLAC_SUCCESS) {
        return result;
    }
    dr_printf_fixed_with_margin(PROFILING_NUMBER_WIDTH, PROFILING_NUMBER_MARGIN, "%f", pProfiling->totalSeconds_BruteForce);
#else
    dr_printf_fixed_with_margin(PROFILING_NUMBER_WIDTH, PROFILING_NUMBER_MARGIN, "");
#endif

    return DRFLAC_SUCCESS;
}

drflac_result seek_profiling_directory(const char* pDirectoryPath, profiling_state* pProfiling)
{
    dr_file_iterator iteratorState;
    dr_file_iterator* pFile;

    profiling_state_init(pProfiling);

    dr_printf_fixed(PROFILING_NAME_WIDTH, "%s", pDirectoryPath);
    dr_printf_fixed_with_margin(PROFILING_NUMBER_WIDTH, PROFILING_NUMBER_MARGIN, "S/Table");
    dr_printf_fixed_with_margin(PROFILING_NUMBER_WIDTH, PROFILING_NUMBER_MARGIN, "Bin Srch");
    dr_printf_fixed_with_margin(PROFILING_NUMBER_WIDTH, PROFILING_NUMBER_MARGIN, "B/Force");
    printf("\n");

    pFile = dr_file_iterator_begin(pDirectoryPath, &iteratorState);
    while (pFile != NULL) {
        drflac_result result;
        profiling_state fileProfiling;

        /* Skip directories for now, but we may want to look at doing recursive file iteration. */
        if (!pFile->isDirectory) {
            result = seek_profiling_file(pFile->absolutePath, &fileProfiling);
            if (result == DRFLAC_SUCCESS) {
                *pProfiling = profiling_state_sum(pProfiling, &fileProfiling);
            }

            printf("\n");
        }

        pFile = dr_file_iterator_next(pFile);
    }

    return DRFLAC_SUCCESS;
}

drflac_result seek_profiling()
{
    drflac_result result = DRFLAC_SUCCESS;
    profiling_state globalProfiling;

    profiling_state_init(&globalProfiling);

    /* Directories. */
    {
        profiling_state directoryProfiling;

        result = seek_profiling_directory("testvectors/flac/tests", &directoryProfiling);
        if (result == DRFLAC_SUCCESS) {
            globalProfiling = profiling_state_sum(&globalProfiling, &directoryProfiling);
        }
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
    
    /* Exhaustive seek test. */
    if (doTesting) {
        printf("=======================================================================\n");
        printf("SEEK TESTING\n");
        printf("=======================================================================\n");
        result = seek_test();
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
        printf("SEEK PROFILING\n");
        printf("=======================================================================\n");
        result = seek_profiling();
        printf("\n");
    }
    
    /*getchar();*/
    return (int)result;
}
