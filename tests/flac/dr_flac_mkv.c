/*#define DR_FLAC_NO_CRC*/
/*#define DR_FLAC_NO_SIMD*/
/*#define DR_FLAC_BUFFER_SIZE 4096*/
#include "dr_flac_common.c"

/* for waitpid */
#include <sys/types.h>
#include <sys/wait.h>

#define FILE_NAME_WIDTH 40
#define NUMBER_WIDTH    10
#define TABLE_MARGIN    2

#define DEFAULT_SOURCE_DIR  "testvectors/flac/tests"

typedef struct
{
    drflac_uint64 pcmFrameCount;
    drflac_uint32 channels;
    drflac_uint32 sampleRate;
    const char *pFilePath;
} ffmpeg;

drflac_uint64 ffmpeg_read_pcm_frames_s32(ffmpeg *pFFmpeg, drflac_uint64 framesToRead, drflac_int32 *pBufferOut) {
    pid_t pid = 0;
    int pipefd[2];

    FILE* output;
    int status;
    drflac_uint64 framesRead;
    
    /* create a ffmpeg process */
    if(pipe(pipefd) != 0) {
        return 0;
    }
    pid = fork();
    if (pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(STDERR_FILENO); /* keep the test cleans don't print child stderr*/
        if(execl("/usr/bin/ffmpeg", "/usr/bin/ffmpeg", "-i", pFFmpeg->pFilePath, "-vn", "-f", "s32le", "pipe:1", (char*) NULL) == -1) {
            exit(1);
        }
        exit(0);
    }
    close(pipefd[1]);
    output = fdopen(pipefd[0], "r");
    if(output == NULL) {
        close(pipefd[0]);
        waitpid(pid, &status, 0);
        return 0;
    }    
    
    /* read the output and cleanup FILE */
    framesRead = fread(pBufferOut, (sizeof(drflac_int32)*pFFmpeg->channels), framesToRead, output);
    fclose(output);

    /* cleanup the process */   
    waitpid(pid, &status, 0);
    
    return framesRead;
}

drflac_result decode_test_file(const char* pFilePath)
{
    drflac_result result = DRFLAC_SUCCESS;
    drflac_uint64 framesToRead;
    
    drflac *pFlac;
    drflac_int32* pPCMFrames_drflac;
    drflac_uint64 framesRead_drflac; 
    
    ffmpeg ffMPEG;
    drflac_int32* pPCMFrames_ffmpeg;
    drflac_uint64 framesRead_ffmpeg;

    

    int iPCMFrame;


    dr_printf_fixed_with_margin(FILE_NAME_WIDTH, TABLE_MARGIN, "%s", dr_path_file_name(pFilePath));
    
    /* Now load from dr_flac. */
    pFlac = drflac_open_file(pFilePath, NULL);
    if (pFlac == NULL) {
        printf("  Failed to open via dr_flac.");
        return DRFLAC_ERROR;    /* Failed to load dr_flac decoder. */
    }

    framesToRead = pFlac->totalPCMFrameCount;

    pPCMFrames_drflac = (drflac_int32*)malloc((size_t)(framesToRead * pFlac->channels * sizeof(drflac_int32)));
    if (pPCMFrames_drflac == NULL) {
        printf("  [dr_flac] Out of memory");
        return DRFLAC_ERROR;
    }    
    framesRead_drflac = drflac_read_pcm_frames_s32(pFlac, framesToRead, pPCMFrames_drflac);


    ffMPEG.channels = pFlac->channels;
    ffMPEG.pcmFrameCount = pFlac->totalPCMFrameCount;
    ffMPEG.pFilePath = pFilePath;
    pPCMFrames_ffmpeg = (drflac_int32*)malloc( framesToRead * ffMPEG.channels * sizeof(drflac_int32));
    if (pPCMFrames_ffmpeg == NULL) {
        printf("  [ffmpeg] Out of memory");
        return DRFLAC_ERROR;
    }
    framesRead_ffmpeg = ffmpeg_read_pcm_frames_s32(&ffMPEG, framesToRead, pPCMFrames_ffmpeg);   

    
    /* The total number of frames we decoded need to match. */
    if (framesRead_ffmpeg != framesRead_drflac) {
        printf("  Decoded frame counts differ: pcmFrameCount=%d, ffmpeg=%d, dr_flac=%d", (int)framesToRead, (int)framesRead_ffmpeg, (int)framesRead_drflac);
        return DRFLAC_ERROR;
    }

    /* Each of the decoded PCM frames need to match. */
    DRFLAC_ASSERT(framesRead_ffmpeg == framesRead_drflac);

    for (iPCMFrame = 0; iPCMFrame < framesRead_ffmpeg; iPCMFrame += 1) {
        drflac_int32* pPCMFrame_ffmpeg = pPCMFrames_ffmpeg + (iPCMFrame * pFlac->channels);
        drflac_int32* pPCMFrame_drflac  = pPCMFrames_drflac  + (iPCMFrame * pFlac->channels);
        drflac_uint32 iChannel;
        drflac_bool32 hasError = DRFLAC_FALSE;

        for (iChannel = 0; iChannel < pFlac->channels; iChannel += 1) {
            if (pPCMFrame_ffmpeg[iChannel] != pPCMFrame_drflac[iChannel]) {
                printf("  PCM Frame @ %d[%d] does not match: pcmFrameCount=%d\n", (int)iPCMFrame, iChannel, (int)framesToRead);
                hasError = DRFLAC_TRUE;
                break;
            }
        }

        if (hasError) {
            return DRFLAC_ERROR;    /* Decoded frames do not match. */
        }
    }

    
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
            if(strstr(pFile->absolutePath, ".mkv") != NULL) {
                result = decode_test_file(pFile->absolutePath);
                (void)result;
                printf("\n");
            }            
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

int main(int argc, char** argv)
{
    drflac_result result = DRFLAC_SUCCESS;
    printf("=======================================================================\n");
    printf("DECODE TESTING\n");
    printf("=======================================================================\n");
    result = decode_test();
    if (result != DRFLAC_SUCCESS) {
        return (int)result;    /* Don't continue if an error occurs during testing. */
    }
    printf("\n");
    return (int)result;
}