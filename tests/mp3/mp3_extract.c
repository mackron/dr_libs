#include "mp3_common.c"

/* Define OPEN_MEMORY to use drmp3_init_memory() instead of drmp3_init_file(). */
#define OPEN_MEMORY

/* Define WITH_METADATA to open with a metadata callback. */
#define WITH_METADATA

#define FORMAT_S16 0
#define FORMAT_F32 1

#if defined(WITH_METADATA)
void on_meta(void* pUserData, const drmp3_metadata* pMetadata)
{
    const char* pMetaName = "Unknown";
    if (pMetadata->type == DRMP3_METADATA_TYPE_ID3V1) {
        pMetaName = "ID3v1";
    } else if (pMetadata->type == DRMP3_METADATA_TYPE_ID3V2) {
        pMetaName = "ID3v2";
    } else if (pMetadata->type == DRMP3_METADATA_TYPE_APE) {
        pMetaName = "APE";
    } else if (pMetadata->type == DRMP3_METADATA_TYPE_XING) {
        pMetaName = "Xing";
    } else if (pMetadata->type == DRMP3_METADATA_TYPE_VBRI) {
        pMetaName = "Info";
    }

    printf("Metadata: %s (%d bytes)\n", pMetaName, (int)pMetadata->rawDataSize);

    (void)pUserData;
}
#endif

int main(int argc, char** argv)
{
    drmp3 mp3;
    const char* pOutputFilePath = NULL;
    int format = FORMAT_S16;
    int iarg;
    FILE* pFileOut;
    drmp3_uint64 framesRead;
    drmp3_uint64 totalFramesRead = 0;
    drmp3_uint64 queriedFrameCount = 0;
    drmp3_bool32 hasError = DRMP3_FALSE;
#if defined(OPEN_MEMORY)
    size_t dataSize;
    void* pData;
#endif
#if defined(WITH_METADATA)
    drmp3_meta_proc onMeta = on_meta;
#else
    drmp3_meta_proc onMeta = NULL;
#endif

    if (argc < 2) {
        printf("Usage: mp3_extract <input filename> -o <output filename> -f [s16|f32]\n");
        return 1;
    }

    /* Parse parameters. */
    for (iarg = 2; iarg < argc; iarg += 1) {
        if (strcmp(argv[iarg], "-o") == 0) {
            if (iarg+1 < argc) {
                iarg += 1;
                pOutputFilePath = argv[iarg];
            }
        } else if (strcmp(argv[iarg], "-f") == 0) {
            if (iarg+1 < argc) {
                iarg += 1;
                if (strcmp(argv[iarg], "s16") == 0) {
                    format = FORMAT_S16;
                } else if (strcmp(argv[iarg], "f32") == 0) {
                    format = FORMAT_F32;
                }
            }
        }
    }

    if (pOutputFilePath == NULL) {
        printf("No output file specified.\n");
        return 1;
    }

    #if defined(OPEN_MEMORY)
    {
        pData = dr_open_and_read_file(argv[1], &dataSize);
        if (pData == NULL) {
            printf("Failed to open file: %s\n", argv[1]);
            return 1;
        }

        if (drmp3_init_memory_with_metadata(&mp3, pData, dataSize, onMeta, NULL, NULL) == DRMP3_FALSE) {
            free(pData);
            printf("Failed to init MP3 decoder: %s\n", argv[1]);
            return 1;
        }
    }
    #else
    {
        if (drmp3_init_file_with_metadata(&mp3, argv[1], onMeta, NULL, NULL) == DRMP3_FALSE) {
            printf("Failed to open file: %s\n", argv[1]);
            return 1;
        }
    }
    #endif

    /*
    There was a bug once where seeking would result in the decoder not properly skipping the Xing/Info
    header if present. We'll do a see here to ensure that code path is hit.
    */
    {
        drmp3_uint64 totalFrameCount = drmp3_get_pcm_frame_count(&mp3);
        drmp3_seek_to_pcm_frame(&mp3, totalFrameCount / 2);
        drmp3_seek_to_pcm_frame(&mp3, 0);
    }

    if (drmp3_fopen(&pFileOut, pOutputFilePath, "wb") != DRMP3_SUCCESS) {
        printf("Failed to open output file: %s\n", pOutputFilePath);
        return 1;
    }

    /* This will be compared against the total frames read below. */
    queriedFrameCount = drmp3_get_pcm_frame_count(&mp3);
    totalFramesRead   = 0;

    if (format == FORMAT_S16) {
        drmp3_int16 pcm[4096];

        for (;;) {
            framesRead = drmp3_read_pcm_frames_s16(&mp3, sizeof(pcm)/sizeof(pcm[0]) / mp3.channels, pcm);
            if (framesRead == 0) {
                break;
            }

            fwrite(pcm, 1, framesRead * mp3.channels * sizeof(pcm[0]), pFileOut);
            totalFramesRead += framesRead;
        }
    } else {
        float pcm[4096];

        for (;;) {
            framesRead = drmp3_read_pcm_frames_f32(&mp3, sizeof(pcm)/sizeof(pcm[0]) / mp3.channels, pcm);
            if (framesRead == 0) {
                break;
            }

            fwrite(pcm, 1, framesRead * mp3.channels * sizeof(pcm[0]), pFileOut);
            totalFramesRead += framesRead;
        }
    }

    if (totalFramesRead != queriedFrameCount) {
        printf("Frame count mismatch: %d (queried) != %d (read)\n", (int)queriedFrameCount, (int)totalFramesRead);
        hasError = DRMP3_TRUE;
    }

    fclose(pFileOut);
    drmp3_uninit(&mp3);

    #if defined(OPEN_MEMORY)
    {
        free(pData);
    }
    #endif

    if (hasError) {
        return 1;
    } else {
        return 0;
    }
}
