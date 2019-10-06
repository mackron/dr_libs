#define DR_FLAC_IMPLEMENTATION
#include "../../dr_flac.h"

/* libFLAC has a warning in their header. *sigh*. */
#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
    #if defined(__clang__)
        #pragma GCC diagnostic ignored "-Wc99-extensions"
    #endif
#endif
#include <FLAC/stream_decoder.h>
#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

#include "../common/dr_common.c"


/*
The libflac object is used to make it easier to compare things against the reference implementation. I don't think
libFLAC's API is all that easy to use or intuitive so I'm wrapping it. This is deliberately simple. It decodes the
entire file into memory upon initialization.
*/
typedef struct
{
    drflac_int32* pPCMFrames;       /* Interleaved. */
    drflac_uint64 pcmFrameCount;
    drflac_uint64 pcmFrameCap;      /* The capacity of the pPCMFrames buffer in PCM frames. */
    drflac_uint32 channels;
    drflac_uint32 sampleRate;
    drflac_uint64 currentPCMFrame;  /* The index of the PCM frame the decoder is currently sitting on. */
    double decodeTimeInSeconds;     /* The total amount of time it took to decode the file. This is used for profiling. */
    drflac_uint8* pFileData;
    size_t fileSizeInBytes;
    size_t fileReadPos;
} libflac;

static FLAC__StreamDecoderReadStatus libflac__read_callback(const FLAC__StreamDecoder *pStreamDecoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
    libflac* pDecoder = (libflac*)client_data;
    size_t bytesToRead = *bytes;
    size_t bytesRemaining = pDecoder->fileSizeInBytes - pDecoder->fileReadPos;

    (void)pStreamDecoder;

    if (bytesToRead > bytesRemaining) {
        bytesToRead = bytesRemaining;
    }

    if (bytesToRead > 0) {
        memcpy(buffer, pDecoder->pFileData + pDecoder->fileReadPos, bytesToRead);
        pDecoder->fileReadPos += bytesToRead;
    }

    *bytes = bytesToRead;
    return (bytesToRead == 0 && bytesRemaining == 0) ? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM : FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderWriteStatus libflac__write_callback(const FLAC__StreamDecoder *pStreamDecoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
    libflac* pDecoder = (libflac*)client_data;
    drflac_uint32 pcmFramesInFLACFrame;
    drflac_int32* pNextSampleInNewFrame;
    drflac_uint32 i, j;

    (void)pStreamDecoder;

    pcmFramesInFLACFrame = frame->header.blocksize;

    /* Make sure there's room in the buffer. */
    if ((pDecoder->pcmFrameCount + pcmFramesInFLACFrame) > pDecoder->pcmFrameCap) {
        drflac_int32* pNewPCMFrames;

        pDecoder->pcmFrameCap *= 2;
        if (pDecoder->pcmFrameCap < (pDecoder->pcmFrameCount + pcmFramesInFLACFrame)) {
            pDecoder->pcmFrameCap = (pDecoder->pcmFrameCount + pcmFramesInFLACFrame);
        }

        pNewPCMFrames = (drflac_int32*)realloc(pDecoder->pPCMFrames, (size_t)(pDecoder->pcmFrameCap * pDecoder->channels * sizeof(drflac_int32)));
        if (pNewPCMFrames == NULL) {
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }

        pDecoder->pPCMFrames = pNewPCMFrames;
    }

    /* Make sure the decoded samples are interleaved. */
    pNextSampleInNewFrame = pDecoder->pPCMFrames + (pDecoder->pcmFrameCount * pDecoder->channels);
    for (i = 0; i < pcmFramesInFLACFrame; i += 1) {
        for (j = 0; j < pDecoder->channels; ++j) {
            *pNextSampleInNewFrame++ = buffer[j][i] << (32 - frame->header.bits_per_sample);
        }
    }

    pDecoder->pcmFrameCount += pcmFramesInFLACFrame;

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static FLAC__StreamDecoderLengthStatus libflac__length_callback(const FLAC__StreamDecoder *pStreamDecoder, FLAC__uint64 *stream_length, void *client_data)
{
    libflac* pDecoder = (libflac*)client_data;

    (void)pStreamDecoder;

    *stream_length = pDecoder->fileSizeInBytes;
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static void libflac__metadata_callback(const FLAC__StreamDecoder *pStreamDecoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
    libflac* pDecoder = (libflac*)client_data;

    (void)pStreamDecoder;

    /* Here is where we initialize the buffer for the decoded data. */
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        pDecoder->pcmFrameCount = 0;    /* Always set this to 0. Don't be tempted to set it to metadata->data.stream_info.total_samples. It's increment in libflac__write_callback. */
        pDecoder->channels      = metadata->data.stream_info.channels;
        pDecoder->sampleRate    = metadata->data.stream_info.sample_rate;

        /* Allocate an initial block of memory if we know the total size. */
        if (metadata->data.stream_info.total_samples > 0) {
            pDecoder->pcmFrameCap = metadata->data.stream_info.total_samples;
            pDecoder->pPCMFrames  = (drflac_int32*)malloc((size_t)(pDecoder->pcmFrameCap * pDecoder->channels * sizeof(drflac_int32)));
            if (pDecoder->pPCMFrames != NULL) {
                DRFLAC_ZERO_MEMORY(pDecoder->pPCMFrames, (size_t)(pDecoder->pcmFrameCap * pDecoder->channels * sizeof(drflac_int32)));
            }
        }
    }
}

static void libflac__error_callback(const FLAC__StreamDecoder *pStreamDecoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
    (void)pStreamDecoder;
    (void)status;
    (void)client_data;
}

/*
Initializes a libflac object.

This will perform a full decode of the file using libFLAC. The time it takes to decode the file will be stored in decodeTimeInSeconds
for the purpose of profiling. The decoding process will load the entire file into memory before calling into libFLAC. This way profiling
will exclude any IO time.
*/
drflac_result libflac_init_file(const char* pFilePath, libflac* pDecoder)
{
    FLAC__StreamDecoder* pStreamDecoder;
    FLAC__bool libflacResult;
    FLAC__StreamDecoderInitStatus libflacStatus;
    double decodeTimeBeg;
    double decodeTimeEnd;

    DRFLAC_ZERO_MEMORY(pDecoder, sizeof(*pDecoder));

    pDecoder->pFileData = (drflac_uint8*)dr_open_and_read_file(pFilePath, &pDecoder->fileSizeInBytes);
    if (pDecoder->pFileData == NULL) {
        return DRFLAC_ERROR;    /* Failed to open the file. */
    }


    /* Initialize the libFLAC decoder. */
    pStreamDecoder = FLAC__stream_decoder_new();
    if (pDecoder == NULL) {
        return DRFLAC_ERROR;    /* Failed to create a new stream decoder. Out of memory. */
    }

    if (dr_extension_equal(pFilePath, "ogg") || dr_extension_equal(pFilePath, "oga") || dr_extension_equal(pFilePath, "ogv")) {
        libflacStatus = FLAC__stream_decoder_init_ogg_stream(pStreamDecoder, libflac__read_callback, NULL, NULL, libflac__length_callback, NULL, libflac__write_callback, libflac__metadata_callback, libflac__error_callback, pDecoder);
    } else {
        libflacStatus = FLAC__stream_decoder_init_stream(pStreamDecoder, libflac__read_callback, NULL, NULL, libflac__length_callback, NULL, libflac__write_callback, libflac__metadata_callback, libflac__error_callback, pDecoder);
    }

    if (libflacStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        FLAC__stream_decoder_delete(pStreamDecoder);
        free(pDecoder->pFileData);
        return DRFLAC_ERROR;    /* Failed to initialize the stream. */
    }

    /* Read past the metadata first. This will fire the libflac__metadata_callback which will do a bit of initialization for us. */
    libflacResult = FLAC__stream_decoder_process_until_end_of_metadata(pStreamDecoder);
    if (libflacResult == DRFLAC_FALSE) {
        FLAC__stream_decoder_delete(pStreamDecoder);
        free(pDecoder->pFileData);
        return DRFLAC_ERROR;    /* Failed to read metadata from the FLAC stream. */
    }

    /* Now we can just decode the entire file. */
    decodeTimeBeg = dr_timer_now();
    libflacResult = FLAC__stream_decoder_process_until_end_of_stream(pStreamDecoder);
    decodeTimeEnd = dr_timer_now();

    pDecoder->decodeTimeInSeconds = decodeTimeEnd - decodeTimeBeg;

    /* We're done with the libFLAC decoder. */
    FLAC__stream_decoder_delete(pStreamDecoder);
    pStreamDecoder = NULL;


    /* Free the file data. */
    free(pDecoder->pFileData);
    pDecoder->pFileData = NULL;
    pDecoder->fileSizeInBytes = 0;
    pDecoder->fileReadPos = 0;

    if (libflacResult == DRFLAC_FALSE) {
        return DRFLAC_ERROR;    /* Some error ocurred while decoding. */
    }
    
    return DRFLAC_SUCCESS;
}

void libflac_uninit(libflac* pDecoder)
{
    if (pDecoder == NULL) {
        return;
    }

    free(pDecoder->pPCMFrames);
}

drflac_uint64 libflac_read_pcm_frames_s32(libflac* pDecoder, drflac_uint64 framesToRead, drflac_int32* pBufferOut)
{
    drflac_uint64 pcmFramesRemaining;

    if (pDecoder == NULL) {
        return 0;
    }

    pcmFramesRemaining = pDecoder->pcmFrameCount - pDecoder->currentPCMFrame;
    if (framesToRead > pcmFramesRemaining) {
        framesToRead = pcmFramesRemaining;
    }

    if (framesToRead == 0) {
        return 0;
    }

    DRFLAC_COPY_MEMORY(pBufferOut, pDecoder->pPCMFrames + (pDecoder->currentPCMFrame * pDecoder->channels), (size_t)(framesToRead * pDecoder->channels * sizeof(drflac_int32)));
    pDecoder->currentPCMFrame += framesToRead;

    return framesToRead;
}

drflac_uint64 libflac_read_pcm_frames_f32(libflac* pDecoder, drflac_uint64 framesToRead, float* pBufferOut)
{
    drflac_uint64 pcmFramesRemaining;

    if (pDecoder == NULL) {
        return 0;
    }

    pcmFramesRemaining = pDecoder->pcmFrameCount - pDecoder->currentPCMFrame;
    if (framesToRead > pcmFramesRemaining) {
        framesToRead = pcmFramesRemaining;
    }

    if (framesToRead == 0) {
        return 0;
    }

    /* s32 to f32 */
    dr_pcm_s32_to_f32(pBufferOut, pDecoder->pPCMFrames + (pDecoder->currentPCMFrame * pDecoder->channels), framesToRead * pDecoder->channels);
    pDecoder->currentPCMFrame += framesToRead;

    return framesToRead;
}

drflac_uint64 libflac_read_pcm_frames_s16(libflac* pDecoder, drflac_uint64 framesToRead, drflac_int16* pBufferOut)
{
    drflac_uint64 pcmFramesRemaining;

    if (pDecoder == NULL) {
        return 0;
    }

    pcmFramesRemaining = pDecoder->pcmFrameCount - pDecoder->currentPCMFrame;
    if (framesToRead > pcmFramesRemaining) {
        framesToRead = pcmFramesRemaining;
    }

    if (framesToRead == 0) {
        return 0;
    }

    /* s32 to f32 */
    dr_pcm_s32_to_s16(pBufferOut, pDecoder->pPCMFrames + (pDecoder->currentPCMFrame * pDecoder->channels), framesToRead * pDecoder->channels);
    pDecoder->currentPCMFrame += framesToRead;

    return framesToRead;
}

drflac_bool32 libflac_seek_to_pcm_frame(libflac* pDecoder, drflac_uint64 targetPCMFrameIndex)
{
    if (pDecoder == NULL) {
        return DRFLAC_FALSE;
    }

    if (targetPCMFrameIndex > pDecoder->pcmFrameCount) {
        return DRFLAC_FALSE; /* Trying to seek too far forward. */
    }

    pDecoder->currentPCMFrame = targetPCMFrameIndex;

    return DRFLAC_TRUE;
}


/* Helper for printing CPU caps from dr_flac. */
void print_cpu_caps()
{
#if defined(DRFLAC_64BIT)
    printf("64 Bit\n");
#else
    printf("32 Bit\n");
#endif

    drflac__init_cpu_caps();

#if defined(DRFLAC_X86) || defined(DRFLAC_X64)
    #if defined(DRFLAC_X64)
        printf("Architecture: x64\n");
    #else
        printf("Architecture: x86\n");
    #endif
    printf("Has SSE2:     %s\n", drflac_has_sse2()  ? "YES" : "NO");
    printf("Has SSE4.1:   %s\n", drflac_has_sse41() ? "YES" : "NO");
    printf("Has LZCNT:    %s\n", drflac__is_lzcnt_supported() ? "YES" : "NO");
#endif
}
