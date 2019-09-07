#define DR_FLAC_IMPLEMENTATION
#include "../../dr_flac.h"

#include "../common/dr_common.c"

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
    drflac_zero_memory(pProfiling, sizeof(*pProfiling));
}

profiling_state profiling_state_sum(const profiling_state* pA, const profiling_state* pB)
{
    profiling_state result;
    result.totalSeconds_BruteForce   = pA->totalSeconds_BruteForce   + pB->totalSeconds_BruteForce;
    result.totalSeconds_BinarySearch = pA->totalSeconds_BinarySearch + pB->totalSeconds_BinarySearch;
    result.totalSeconds_SeekTable    = pA->totalSeconds_SeekTable    + pB->totalSeconds_SeekTable;

    return result;
}


drflac_result seek_test()
{
    /* Not yet implemented. */
    return 0;
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
    /*dr_seed(1234);*/
    dr_seed(4321);
    for (i = 0; i < 100; ++i) {
        double startTime;
        double endTime;
        int targetPCMFrame = dr_rand_range_s32(0, (int)pFlac->totalPCMFrameCount);
        if (targetPCMFrame > pFlac->totalPCMFrameCount) {
            printf("Too Big: %d\n", targetPCMFrame);
        }
        if (targetPCMFrame < 0) {
            printf("Too Small: %d\n", targetPCMFrame);
        }

        

        //printf("targetPCMFrame=%d\n", targetPCMFrame);

        startTime = dr_timer_now();
        {
            drflac_seek_to_pcm_frame(pFlac, targetPCMFrame /*5596944*/);
        }
        endTime = dr_timer_now();

        //if (g_sameFrameTerminationCount > 0) {
        //    int a = 4; (void)a;
        //}

        if (pProcessingTime != NULL) {
            *pProcessingTime += (endTime - startTime);
            /*printf("TIME: %d : %f\n", targetPCMFrame, (endTime - startTime));*/
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

    pFlac->_noSeekTable        = DRFLAC_FALSE;
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

    pFlac->_noSeekTable        = DRFLAC_TRUE;
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

    pFlac->_noSeekTable        = DRFLAC_TRUE;
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
    dr_printf_fixed_with_margin(PROFILING_NAME_WIDTH, 2, "%s", pFilePath);
    
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

#if 0
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

        result = seek_profiling_directory("testvectors/flac/seek_tests", &directoryProfiling);
        if (result == DRFLAC_SUCCESS) {
            globalProfiling = profiling_state_sum(&globalProfiling, &directoryProfiling);
        }
    }

    return result;
}

#include <conio.h>
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
        result = seek_test();
        if (result != DRFLAC_SUCCESS) {
            return (int)result;    /* Don't continue if an error occurs during testing. */
        }
    } else {
        printf("=======================================================================\n");
        printf("WARNING: Correctness Tests Disabled\n");
        printf("=======================================================================\n");
    }

    /* Profiling. */
    if (doProfiling) {
        result = seek_profiling();
    }
    
    _getch();
    return (int)result;
}
