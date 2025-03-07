/*
This test is mainly just to check basic functionality is working at a basic level without crashing. One thing
in particular it checks is consistency in the output when opening with different modes.

When a file is opened from a memory buffer, dr_mp3 will take a different path for decoding which is optimized
to reduce data movement. This test will ensure that the output between callback based decoding and memory
buffer decoding is consistent.

Another thing this will check is that opening with the `_with_metadata()` variants results in consistent
output. The reason this is necessary is because tags are skipped in slightly different ways depending on
whether or not a metadata callback is provided.
*/
#include "mp3_common.c"

#define FILE_NAME_WIDTH 40
#define NUMBER_WIDTH    10
#define TABLE_MARGIN    2

void on_meta(void* pUserData, const drmp3_metadata* pMetadata)
{
    (void)pUserData;
    (void)pMetadata;
}

void* open_decoders(drmp3* pDecoderMemory, drmp3* pDecoderMemoryMD, drmp3* pDecoderFile, drmp3* pDecoderFileMD, const char* pFilePath, size_t* pFileSize)
{
    size_t dataSize;
    void* pData;

    /* Initialize the memory decoder. */
    pData = dr_open_and_read_file(pFilePath, &dataSize);
    if (pData == NULL) {
        printf("Failed to open file \"%s\"\n", pFilePath);
        return NULL;
    }

    if (!drmp3_init_memory_with_metadata(pDecoderMemory, pData, dataSize, NULL, NULL, NULL)) {
        free(pData);
        printf("Failed to init MP3 decoder \"%s\"\n", pFilePath);
        return NULL;
    }

    if (!drmp3_init_memory_with_metadata(pDecoderMemoryMD, pData, dataSize, on_meta, NULL, NULL)) {
        drmp3_uninit(pDecoderMemory);
        free(pData);
        printf("Failed to init MP3 decoder \"%s\"\n", pFilePath);
        return NULL;
    }

    /* Initialize the file decoder. */
    if (!drmp3_init_file_with_metadata(pDecoderFile, pFilePath, NULL, NULL, NULL)) {
        drmp3_uninit(pDecoderMemory);
        free(pData);
        printf("Failed to open file \"%s\"\n", pFilePath);
        return NULL;
    }

    if (!drmp3_init_file_with_metadata(pDecoderFileMD, pFilePath, on_meta, NULL, NULL)) {
        drmp3_uninit(pDecoderMemory);
        drmp3_uninit(pDecoderFile);
        free(pData);
        printf("Failed to open file \"%s\"\n", pFilePath);
        return NULL;
    }

    *pFileSize = dataSize;

    return pData;
}

int validate_basic_properties(drmp3* pMP3Memory, drmp3* pMP3MemoryMD, drmp3* pMP3File, drmp3* pMP3FileMD)
{
    if (pMP3Memory->channels != pMP3File->channels || pMP3Memory->channels != pMP3MemoryMD->channels || pMP3Memory->channels != pMP3FileMD->channels) {
        printf("Channel counts differ\n");
        return 1;
    }

    if (pMP3Memory->sampleRate != pMP3File->sampleRate || pMP3Memory->sampleRate != pMP3MemoryMD->sampleRate || pMP3Memory->sampleRate != pMP3FileMD->sampleRate) {
        printf("Sample rates differ\n");
        return 1;
    }

    return 0;
}

int validate_decoding(drmp3* pMP3Memory, drmp3* pMP3MemoryMD, drmp3* pMP3File, drmp3* pMP3FileMD)
{
    int result = 0;

    for (;;) {
        drmp3_uint64 iSample;
        drmp3_uint64 pcmFrameCountMemory;
        drmp3_uint64 pcmFrameCountMemoryMD;
        drmp3_uint64 pcmFrameCountFile;
        drmp3_uint64 pcmFrameCountFileMD;
        drmp3_int16 pcmFramesMemory[4096];
        drmp3_int16 pcmFramesMemoryMD[4096];
        drmp3_int16 pcmFramesFile[4096];
        drmp3_int16 pcmFramesFileMD[4096];

        pcmFrameCountMemory   = drmp3_read_pcm_frames_s16(pMP3Memory,   DRMP3_COUNTOF(pcmFramesMemory)   / pMP3Memory->channels,   pcmFramesMemory);
        pcmFrameCountMemoryMD = drmp3_read_pcm_frames_s16(pMP3MemoryMD, DRMP3_COUNTOF(pcmFramesMemoryMD) / pMP3MemoryMD->channels, pcmFramesMemoryMD);
        pcmFrameCountFile     = drmp3_read_pcm_frames_s16(pMP3File,     DRMP3_COUNTOF(pcmFramesFile)     / pMP3File->channels,     pcmFramesFile);
        pcmFrameCountFileMD   = drmp3_read_pcm_frames_s16(pMP3FileMD,   DRMP3_COUNTOF(pcmFramesFileMD)   / pMP3FileMD->channels,   pcmFramesFileMD);

        /* Check the frame count first. */
        if (pcmFrameCountMemory != pcmFrameCountFile) {
            printf("Frame counts differ between memory and file: memory = %d; file = %d\n", (int)pcmFrameCountMemory, (int)pcmFrameCountFile);
            result = 1;
            break;
        }

        if (pcmFrameCountMemory != pcmFrameCountMemoryMD) {
            printf("Frame counts differ when loading from memory without metadata: memory = %d; memory with metadata = %d\n", (int)pcmFrameCountMemory, (int)pcmFrameCountMemoryMD);
            result = 1;
            break;
        }

        if (pcmFrameCountFile != pcmFrameCountFileMD) {
            printf("Frame counts differ when loading from file without metadata: file = %d; file with metadata = %d\n", (int)pcmFrameCountFile, (int)pcmFrameCountFileMD);
            result = 1;
            break;
        }

        /* Check individual frames. */
        DRMP3_ASSERT(pcmFrameCountMemory == pcmFrameCountFile);
        for (iSample = 0; iSample < pcmFrameCountMemory * pMP3Memory->channels; iSample += 1) {
            if (pcmFramesMemory[iSample] != pcmFramesFile[iSample]) {
                printf("Samples differ between memory and file: memory = %d; file = %d\n", (int)pcmFramesMemory[iSample], (int)pcmFramesFile[iSample]);
                result = 1;
                break;
            }
        }

        /* We've reached the end if we didn't return any PCM frames. */
        if (pcmFrameCountMemory == 0 || pcmFrameCountMemoryMD || pcmFrameCountFile == 0 || pcmFrameCountFileMD) {
            break;
        }
    }

    return result;
}

int test_file_inner(const char* pFilePath)
{
    int result = 0;
    drmp3 mp3Memory;
    drmp3 mp3MemoryMD;
    drmp3 mp3File;
    drmp3 mp3FileMD;
    size_t dataSize;
    void* pData;

    /* Open the decoders. This will print the relevant error message. */
    pData = open_decoders(&mp3Memory, &mp3MemoryMD, &mp3File, &mp3FileMD, pFilePath, &dataSize);
    if (pData == NULL) {
        return 1;
    }

    result = validate_basic_properties(&mp3Memory, &mp3MemoryMD, &mp3File, &mp3FileMD);
    if (result != 0) {
        goto done;
    }

    result = validate_decoding(&mp3Memory, &mp3MemoryMD, &mp3File, &mp3FileMD);
    if (result != 0) {
        goto done;
    }

done:
    drmp3_uninit(&mp3File);
    drmp3_uninit(&mp3Memory);
    free(pData);
    return result;
}

int test_file(const char* pFilePath)
{
    int result = 0;
    drmp3_bool32 hasError = DRMP3_FALSE;

    /*
    When opening from a memory buffer, dr_mp3 will take a different path for decoding which is optimized to reduce data movement. Since it's
    running on a separate path, we need to ensure it's returning consistent results with the other code path which will be used when decoding
    from a file.
    */
    dr_printf_fixed_with_margin(FILE_NAME_WIDTH, TABLE_MARGIN, "%s", dr_path_file_name(pFilePath));

    result = test_file_inner(pFilePath);
    if (result != 0) {
        hasError = DRMP3_TRUE;
    }

    if (hasError) {
        printf("  ERROR\n");
    } else {
        printf("  OK\n");
    }

    if (hasError) {
        return 1;
    } else {
        return 0;
    }
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
        int result;

        /* Skip directories for now, but we may want to look at doing recursive file iteration. */
        if (!pFile->isDirectory) {
            result = test_file(pFile->absolutePath);
            if (result != 0) {
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
