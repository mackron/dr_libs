/*#define DR_FLAC_NO_CRC*/
/*#define DR_FLAC_NO_SIMD*/
/*#define DR_FLAC_BUFFER_SIZE 4096*/
#include "dr_flac_common.c"

#define FILE_NAME_WIDTH 40
#define NUMBER_WIDTH    10
#define TABLE_MARGIN    2

#define DEFAULT_SOURCE_DIR  "testvectors/flac/tests"

drflac_result decode_test_file(const char* pFilePath)
{
    drflac_result result = DRFLAC_SUCCESS;
    drflac *pFlac;
    drflac_int32* pPCMFrames_drflac;
    drflac_uint64 framesRead_drflac; 
    
    pid_t pid = 0;
    int pipefd[2];
    FILE* output;
    drflac_uint8 line[256];
    int status;
    drflac_uint64 bytesleft;
    drflac_uint64 bytesread = 0;
    ssize_t nowread;
    drflac_int32* pPCMFrames_ffmpeg;
    drflac_uint64 framesRead_ffmpeg;
    drflac_uint8* pwrite;



    dr_printf_fixed_with_margin(FILE_NAME_WIDTH, TABLE_MARGIN, "%s", dr_path_file_name(pFilePath));

     /* Now load from dr_flac. */
    pFlac = drflac_open_file(pFilePath, NULL);
    if (pFlac == NULL) {
        printf("  Failed to open via dr_flac.");
        return DRFLAC_ERROR;    /* Failed to load dr_flac decoder. */
    }

    pPCMFrames_drflac = (drflac_int32*)malloc((size_t)(pFlac->totalPCMFrameCount * pFlac->channels * sizeof(drflac_int32)));
    if (pPCMFrames_drflac == NULL) {
        printf("  [dr_flac] Out of memory");
        return DRFLAC_ERROR;
    }    
    framesRead_drflac = drflac_read_pcm_frames_s32(pFlac, pFlac->totalPCMFrameCount, pPCMFrames_drflac);


    /* get the pcm data from ffmpeg */
    bytesleft = (size_t)(pFlac->totalPCMFrameCount * pFlac->channels * sizeof(drflac_int32));
    pPCMFrames_ffmpeg = (drflac_int32*)malloc(bytesleft+1);
    if (pPCMFrames_ffmpeg == NULL) {
        printf("  [dr_flac] Out of memory");
        return DRFLAC_ERROR;
    }
    pipe(pipefd); /*create a pipe*/
    pid = fork(); /*span a child process*/
    if (pid == 0)
    {
        /* Child. Let's redirect its standard output to our pipe and replace process with tail*/
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        /*dup2(pipefd[1], STDERR_FILENO);*/
        if(execl("/usr/bin/ffmpeg", "/usr/bin/ffmpeg", "-i", pFilePath, "-vn", "-f", "s32le", "pipe:1", (char*) NULL) == -1) {
            return DRFLAC_ERROR;
        }
    }
    close(pipefd[1]);
    /*output = fdopen(pipefd[0], "r");
    bytesread = fread(pPCMFrames_ffmpeg, bytesleft+1, 1, output);*/

    pwrite = (drflac_uint8*)pPCMFrames_ffmpeg;
    for(;;) {
        nowread = read(pipefd[0], pwrite, bytesleft+1);
        printf("nowread %d\n", nowread);
        if(nowread == -1) {
            break;
        }
        if(nowread == 0) {
            break;
        }
        if(nowread > bytesleft) {
            printf("  [dr_flac] ffmpeg read more bytes.");
            /*fclose(output);*/
            return DRFLAC_ERROR;            
        }
        bytesleft -= nowread;
        pwrite += nowread;
        bytesread += nowread;
    }    
    
       
    /*or wait for the child process to terminate*/
    waitpid(pid, &status, 0);
   /* fclose(output);*/
    
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