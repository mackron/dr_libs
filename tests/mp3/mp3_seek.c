/*
Checks that seeking with a seek table returns the same samples as reading from the start.
*/
#include "mp3_common.c"

#define FILE_NAME_WIDTH 40
#define NUMBER_WIDTH    10
#define TABLE_MARGIN    2

#define SEEK_POINT_COUNT    64
#define FRAMES_TO_COMPARE   1024

int seek_and_compare(drmp3* pMP3, const float* pExpected, drmp3_uint64 totalFrameCount, drmp3_uint64 frameIndex)
{
    float pcmFrames[FRAMES_TO_COMPARE * 2];
    drmp3_uint64 framesToRead;

    if (frameIndex >= totalFrameCount) {
        return 0;
    }

    framesToRead = totalFrameCount - frameIndex;
    if (framesToRead > FRAMES_TO_COMPARE) {
        framesToRead = FRAMES_TO_COMPARE;
    }

    if (!drmp3_seek_to_pcm_frame(pMP3, frameIndex)) {
        printf("Failed to seek to %d\n", (int)frameIndex);
        return 1;
    }

    if (drmp3_read_pcm_frames_f32(pMP3, framesToRead, pcmFrames) != framesToRead || memcmp(pcmFrames, pExpected + frameIndex*pMP3->channels, (size_t)(framesToRead * pMP3->channels * sizeof(float))) != 0) {
        printf("Samples differ after seeking to %d\n", (int)frameIndex);
        return 1;
    }

    return 0;
}

int test_file_inner(const char* pFilePath)
{
    int result = 0;
    drmp3 mp3;
    drmp3_uint64 totalFrameCount;
    drmp3_uint64 frameIndex;
    float* pExpected;
    drmp3_seek_point seekPoints[SEEK_POINT_COUNT];
    drmp3_uint32 seekPointCount = SEEK_POINT_COUNT;

    if (!drmp3_init_file(&mp3, pFilePath, NULL)) {
        printf("Failed to open file \"%s\"\n", pFilePath);
        return 1;
    }

    totalFrameCount = drmp3_get_pcm_frame_count(&mp3);
    pExpected = (float*)malloc((size_t)(totalFrameCount * mp3.channels * sizeof(float)));
    if (pExpected == NULL) {
        drmp3_uninit(&mp3);
        return 1;
    }

    drmp3_seek_to_pcm_frame(&mp3, 0);
    totalFrameCount = drmp3_read_pcm_frames_f32(&mp3, totalFrameCount, pExpected);

    if (!drmp3_calculate_seek_points(&mp3, &seekPointCount, seekPoints) || !drmp3_bind_seek_table(&mp3, seekPointCount, seekPoints)) {
        printf("Failed to create the seek table\n");
        result = 1;
        goto done;
    }

    /* Before the first seek point. */
    result = seek_and_compare(&mp3, pExpected, totalFrameCount, seekPoints[0].pcmFrameIndex / 2);

    for (frameIndex = 0; frameIndex < totalFrameCount && result == 0; frameIndex += totalFrameCount / 100 + 1) {
        result = seek_and_compare(&mp3, pExpected, totalFrameCount, frameIndex);
    }

done:
    free(pExpected);
    drmp3_uninit(&mp3);
    return result;
}

int test_file(const char* pFilePath)
{
    int result;

    dr_printf_fixed_with_margin(FILE_NAME_WIDTH, TABLE_MARGIN, "%s", dr_path_file_name(pFilePath));

    result = test_file_inner(pFilePath);
    if (result != 0) {
        printf("  ERROR\n");
    } else {
        printf("  OK\n");
    }

    return result;
}

int test_directory(const char* pDirectoryPath)
{
    dr_file_iterator iteratorState;
    dr_file_iterator* pFile;
    drmp3_bool32 hasError = DRMP3_FALSE;

    dr_printf_fixed(FILE_NAME_WIDTH, "%s", pDirectoryPath);
    dr_printf_fixed_with_margin(NUMBER_WIDTH, TABLE_MARGIN, "RESULT");
    printf("\n");

    pFile = dr_file_iterator_begin(pDirectoryPath, &iteratorState);
    if (pFile == NULL) {
        printf("Failed to open directory \"%s\"\n", pDirectoryPath);
        return 1;
    }

    while (pFile != NULL) {
        if (!pFile->isDirectory) {
            if (test_file(pFile->absolutePath) != 0) {
                hasError = DRMP3_TRUE;
            }
        }

        pFile = dr_file_iterator_next(pFile);
    }

    if (hasError) {
        return 1;
    } else {
        return 0;
    }
}

int main(int argc, char** argv)
{
    const char* pTestsFolder = "tests/testvectors/mp3/tests";

    if (argc >= 2) {
        pTestsFolder = argv[1];
    }

    return test_directory(pTestsFolder);
}
