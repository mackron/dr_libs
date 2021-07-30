#define DR_WAV_IMPLEMENTATION
#include "../../dr_wav.h"
#include <sndfile.h>
#include "../common/dr_common.c"

dr_handle g_libsndfile = NULL;

typedef SNDFILE*   (* pfn_sf_open_virtual)(SF_VIRTUAL_IO* sfvirtual, int mode, SF_INFO* sfinfo, void* user_data);
typedef int        (* pfn_sf_close)       (SNDFILE* sndfile);
typedef sf_count_t (* pfn_sf_readf_short) (SNDFILE *sndfile, short *ptr, sf_count_t frames);
typedef sf_count_t (* pfn_sf_readf_int)   (SNDFILE *sndfile, int *ptr, sf_count_t frames);
typedef sf_count_t (* pfn_sf_readf_float) (SNDFILE *sndfile, float *ptr, sf_count_t frames);
typedef sf_count_t (* pfn_sf_readf_double)(SNDFILE *sndfile, double *ptr, sf_count_t frames);
typedef sf_count_t (* pfn_sf_seek)        (SNDFILE *sndfile, sf_count_t frames, int whence);

pfn_sf_open_virtual libsndfile__sf_open_virtual;
pfn_sf_close        libsndfile__sf_close;
pfn_sf_readf_short  libsndfile__sf_readf_short;
pfn_sf_readf_int    libsndfile__sf_readf_int;
pfn_sf_readf_float  libsndfile__sf_readf_float;
pfn_sf_readf_double libsndfile__sf_readf_double;
pfn_sf_seek         libsndfile__sf_seek;

drwav_result libsndfile_init_api()
{
    unsigned int i;
    const char* pFileNames[] = {
#if defined(_WIN32)
    #if defined(_WIN64)
        "libsndfile-1-x64.dll",
    #else
        "libsndfile-1-x86.dll",
    #endif
        "libsndfile-1.dll"
#else
        "libsndfile-1.so",
        "libsndfile.so.1"
#endif
    };

    if (g_libsndfile != NULL) {
        return DRWAV_INVALID_OPERATION; /* Already initialized. */
    }
    
    for (i = 0; i < sizeof(pFileNames)/sizeof(pFileNames[0]); i += 1) {
        g_libsndfile = dr_dlopen(pFileNames[i]);
        if (g_libsndfile != NULL) {
            break;
        }
    }

    if (g_libsndfile == NULL) {
        return DRWAV_ERROR; /* Unable to load libsndfile-1.so/dll. */
    }

    libsndfile__sf_open_virtual = (pfn_sf_open_virtual)dr_dlsym(g_libsndfile, "sf_open_virtual");
    libsndfile__sf_close        = (pfn_sf_close)       dr_dlsym(g_libsndfile, "sf_close");
    libsndfile__sf_readf_short  = (pfn_sf_readf_short) dr_dlsym(g_libsndfile, "sf_readf_short");
    libsndfile__sf_readf_int    = (pfn_sf_readf_int)   dr_dlsym(g_libsndfile, "sf_readf_int");
    libsndfile__sf_readf_float  = (pfn_sf_readf_float) dr_dlsym(g_libsndfile, "sf_readf_float");
    libsndfile__sf_readf_double = (pfn_sf_readf_double)dr_dlsym(g_libsndfile, "sf_readf_double");
    libsndfile__sf_seek         = (pfn_sf_seek)        dr_dlsym(g_libsndfile, "sf_seek");
    
    return DRWAV_SUCCESS;
}

void libsndfile_uninit_api()
{
    if (g_libsndfile == NULL) {
        return; /* Invalid operation. Not initialized. */
    }

    dr_dlclose(g_libsndfile);
    g_libsndfile = NULL;
}

typedef struct
{
    SNDFILE* pHandle;
    SF_INFO info;
    drwav_uint8* pFileData;
    size_t fileSizeInBytes;
    size_t fileReadPos;
} libsndfile;

sf_count_t libsndfile__on_filelen(void *user_data)
{
    libsndfile* pSndFile = (libsndfile*)user_data;
    return (sf_count_t)pSndFile->fileSizeInBytes;
}

sf_count_t libsndfile__on_seek(sf_count_t offset, int whence, void *user_data)
{
    libsndfile* pSndFile = (libsndfile*)user_data;

    switch (whence)
    {
        case SF_SEEK_SET:
        {
            pSndFile->fileReadPos = (size_t)offset;
        } break;
        case SF_SEEK_CUR:
        {
            pSndFile->fileReadPos += (size_t)offset;
        } break;
        case SF_SEEK_END:
        {
            pSndFile->fileReadPos = pSndFile->fileSizeInBytes - (size_t)offset;
        } break;
    }

    return (sf_count_t)pSndFile->fileReadPos;
}

sf_count_t libsndfile__on_read(void *ptr, sf_count_t count, void *user_data)
{
    libsndfile* pSndFile = (libsndfile*)user_data;

    DRWAV_COPY_MEMORY(ptr, pSndFile->pFileData + pSndFile->fileReadPos, (size_t)count);
    pSndFile->fileReadPos += (size_t)count;

    return count;
}

sf_count_t libsndfile__on_write(const void *ptr, sf_count_t count, void *user_data)
{
    /* We're not doing anything with writing. */
    (void)ptr;
    (void)count;
    (void)user_data;
    return 0;
}

sf_count_t libsndfile__on_tell(void *user_data)
{
    libsndfile* pSndFile = (libsndfile*)user_data;
    return (sf_count_t)pSndFile->fileReadPos;
}

drwav_result libsndfile_init_file(const char* pFilePath, libsndfile* pSndFile)
{
    SF_VIRTUAL_IO callbacks;

    if (pFilePath == NULL || pSndFile == NULL) {
        return DRWAV_INVALID_ARGS;
    }

    DRWAV_ZERO_MEMORY(pSndFile, sizeof(*pSndFile));

    /* We use libsndfile's virtual IO technique because we want to load from memory to make speed benchmarking fairer. */
    pSndFile->pFileData = (drwav_uint8*)dr_open_and_read_file(pFilePath, &pSndFile->fileSizeInBytes);
    if (pSndFile->pFileData == NULL) {
        return DRWAV_ERROR; /* Failed to open the file. */
    }

    DRWAV_ZERO_MEMORY(&callbacks, sizeof(callbacks));
    callbacks.get_filelen = libsndfile__on_filelen;
    callbacks.seek        = libsndfile__on_seek;
    callbacks.read        = libsndfile__on_read;
    callbacks.write       = libsndfile__on_write;
    callbacks.tell        = libsndfile__on_tell;

    pSndFile->pHandle = libsndfile__sf_open_virtual(&callbacks, SFM_READ, &pSndFile->info, pSndFile);
    if (pSndFile->pHandle == NULL) {
        free(pSndFile->pFileData);
        return DRWAV_ERROR;
    }

    return DRWAV_SUCCESS;
}

void libsndfile_uninit(libsndfile* pSndFile)
{
    if (pSndFile == NULL) {
        return;
    }

    libsndfile__sf_close(pSndFile->pHandle);
    free(pSndFile->pFileData);
}

drwav_uint64 libsndfile_read_pcm_frames_s16(libsndfile* pSndFile, drwav_uint64 framesToRead, drwav_int16* pBufferOut)
{
    if (pSndFile == NULL || pBufferOut == NULL) {
        return 0;
    }

    /* Unfortunately it looks like libsndfile does not return correct integral values when the source file is floating point. */
    if ((pSndFile->info.format & SF_FORMAT_SUBMASK) == SF_FORMAT_FLOAT) {
        /* Read as float and convert. */
        drwav_uint64 totalFramesRead = 0;
        while (totalFramesRead < framesToRead) {
            float temp[4096];
            drwav_uint64 framesRemaining = framesToRead - totalFramesRead;
            drwav_uint64 framesReadThisIteration;
            drwav_uint64 framesToReadThisIteration = sizeof(temp)/sizeof(temp[0]) / pSndFile->info.channels;
            if (framesToReadThisIteration > framesRemaining) {
                framesToReadThisIteration = framesRemaining;
            }

            framesReadThisIteration = libsndfile__sf_readf_float(pSndFile->pHandle, temp, (sf_count_t)framesToReadThisIteration);

            drwav_f32_to_s16(pBufferOut, temp, (size_t)(framesReadThisIteration*pSndFile->info.channels));

            totalFramesRead += framesReadThisIteration;
            pBufferOut      += framesReadThisIteration*pSndFile->info.channels;

            /* If we read less frames than we requested we've reached the end of the file. */
            if (framesReadThisIteration < framesToReadThisIteration) {
                break;
            }
        }

        return totalFramesRead;
    } else if ((pSndFile->info.format & SF_FORMAT_SUBMASK) == SF_FORMAT_DOUBLE) {
        /* Read as double and convert. */
        drwav_uint64 totalFramesRead = 0;
        while (totalFramesRead < framesToRead) {
            double temp[4096];
            drwav_uint64 framesRemaining = framesToRead - totalFramesRead;
            drwav_uint64 framesReadThisIteration;
            drwav_uint64 framesToReadThisIteration = sizeof(temp)/sizeof(temp[0]) / pSndFile->info.channels;
            if (framesToReadThisIteration > framesRemaining) {
                framesToReadThisIteration = framesRemaining;
            }

            framesReadThisIteration = libsndfile__sf_readf_double(pSndFile->pHandle, temp, (sf_count_t)framesToReadThisIteration);

            drwav_f64_to_s16(pBufferOut, temp, (size_t)(framesReadThisIteration*pSndFile->info.channels));

            totalFramesRead += framesReadThisIteration;
            pBufferOut      += framesReadThisIteration*pSndFile->info.channels;

            /* If we read less frames than we requested we've reached the end of the file. */
            if (framesReadThisIteration < framesToReadThisIteration) {
                break;
            }
        }

        return totalFramesRead;
    } else {
        return libsndfile__sf_readf_short(pSndFile->pHandle, pBufferOut, framesToRead);
    }
}

drwav_uint64 libsndfile_read_pcm_frames_f32(libsndfile* pSndFile, drwav_uint64 framesToRead, float* pBufferOut)
{
    if (pSndFile == NULL || pBufferOut == NULL) {
        return 0;
    }

    return libsndfile__sf_readf_float(pSndFile->pHandle, pBufferOut, framesToRead);
}

drwav_uint64 libsndfile_read_pcm_frames_s32(libsndfile* pSndFile, drwav_uint64 framesToRead, drwav_int32* pBufferOut)
{
    if (pSndFile == NULL || pBufferOut == NULL) {
        return 0;
    }

    /* Unfortunately it looks like libsndfile does not return correct integral values when the source file is floating point. */
    if ((pSndFile->info.format & SF_FORMAT_SUBMASK) == SF_FORMAT_FLOAT) {
        /* Read and float and convert. */
        drwav_uint64 totalFramesRead = 0;
        while (totalFramesRead < framesToRead) {
            float temp[4096];
            drwav_uint64 framesRemaining = framesToRead - totalFramesRead;
            drwav_uint64 framesReadThisIteration;
            drwav_uint64 framesToReadThisIteration = sizeof(temp)/sizeof(temp[0]) / pSndFile->info.channels;
            if (framesToReadThisIteration > framesRemaining) {
                framesToReadThisIteration = framesRemaining;
            }

            framesReadThisIteration = libsndfile__sf_readf_float(pSndFile->pHandle, temp, (sf_count_t)framesToReadThisIteration);

            drwav_f32_to_s32(pBufferOut, temp, (size_t)(framesReadThisIteration*pSndFile->info.channels));

            totalFramesRead += framesReadThisIteration;
            pBufferOut      += framesReadThisIteration*pSndFile->info.channels;

            /* If we read less frames than we requested we've reached the end of the file. */
            if (framesReadThisIteration < framesToReadThisIteration) {
                break;
            }
        }

        return totalFramesRead;
    } else if ((pSndFile->info.format & SF_FORMAT_SUBMASK) == SF_FORMAT_DOUBLE) {
        /* Read and double and convert. */
        drwav_uint64 totalFramesRead = 0;
        while (totalFramesRead < framesToRead) {
            double temp[4096];
            drwav_uint64 framesRemaining = framesToRead - totalFramesRead;
            drwav_uint64 framesReadThisIteration;
            drwav_uint64 framesToReadThisIteration = sizeof(temp)/sizeof(temp[0]) / pSndFile->info.channels;
            if (framesToReadThisIteration > framesRemaining) {
                framesToReadThisIteration = framesRemaining;
            }

            framesReadThisIteration = libsndfile__sf_readf_double(pSndFile->pHandle, temp, (sf_count_t)framesToReadThisIteration);

            drwav_f64_to_s32(pBufferOut, temp, (size_t)(framesReadThisIteration*pSndFile->info.channels));

            totalFramesRead += framesReadThisIteration;
            pBufferOut      += framesReadThisIteration*pSndFile->info.channels;

            /* If we read less frames than we requested we've reached the end of the file. */
            if (framesReadThisIteration < framesToReadThisIteration) {
                break;
            }
        }

        return totalFramesRead;
    } else {
        return libsndfile__sf_readf_int(pSndFile->pHandle, pBufferOut, framesToRead);
    }
}

drwav_bool32 libsndfile_seek_to_pcm_frame(libsndfile* pSndFile, drwav_uint64 targetPCMFrameIndex)
{
    if (pSndFile == NULL) {
        return DRWAV_FALSE;
    }

    return libsndfile__sf_seek(pSndFile->pHandle, (sf_count_t)targetPCMFrameIndex, SF_SEEK_SET) == (sf_count_t)targetPCMFrameIndex;
}
