#define DR_WAV_IMPLEMENTATION
#include "../../dr_wav.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
    /* The sample data is virtual so large seeks do not require large files. */
    unsigned char header[44];
    drwav_uint64 size;
    drwav_uint64 cursor;
    unsigned int seekCount;
    unsigned int failSeek;
} test_stream;

#define CHECK(expression) do { if (!(expression)) { printf("Line %d: %s\n", __LINE__, #expression); return -1; } } while (0)

static void write_u16(unsigned char* p, drwav_uint16 value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void write_u32(unsigned char* p, drwav_uint32 value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static size_t on_read(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    test_stream* pStream = (test_stream*)pUserData;
    size_t i;
    if (bytesToRead > pStream->size - pStream->cursor) {
        bytesToRead = (size_t)(pStream->size - pStream->cursor);
    }
    for (i = 0; i < bytesToRead; i += 1) {
        drwav_uint64 position = pStream->cursor + i;
        ((unsigned char*)pBufferOut)[i] = position < sizeof(pStream->header) ? pStream->header[(size_t)position] : 0;
    }
    pStream->cursor += bytesToRead;
    return bytesToRead;
}

static drwav_bool32 on_seek(void* pUserData, int offset, drwav_seek_origin origin)
{
    test_stream* pStream = (test_stream*)pUserData;
    drwav_int64 position;
    pStream->seekCount += 1;
    if (pStream->failSeek != 0 && pStream->seekCount == pStream->failSeek) {
        return DRWAV_FALSE;
    }
    position = (origin == DRWAV_SEEK_SET ? 0 : (drwav_int64)pStream->cursor) + offset;
    if (position < 0 || (drwav_uint64)position > pStream->size) {
        return DRWAV_FALSE;
    }
    pStream->cursor = (drwav_uint64)position;
    return DRWAV_TRUE;
}

static int init_stream(test_stream* pStream, drwav* pWav, drwav_uint16 channels, drwav_uint16 bitsPerSample)
{
    drwav_uint16 bytesPerFrame = (drwav_uint16)(channels * (bitsPerSample / 8));
    drwav_uint32 dataSize = 0xFFFFF000U / (bytesPerFrame * 2U) * (bytesPerFrame * 2U);
    memset(pStream, 0, sizeof(*pStream));
    memcpy(pStream->header, "RIFF", 4);
    write_u32(pStream->header + 4, dataSize + 36);
    memcpy(pStream->header + 8, "WAVEfmt ", 8);
    write_u32(pStream->header + 16, 16);
    write_u16(pStream->header + 20, DR_WAVE_FORMAT_PCM);
    write_u16(pStream->header + 22, channels);
    write_u32(pStream->header + 24, 48000);
    write_u32(pStream->header + 28, 48000U * bytesPerFrame);
    write_u16(pStream->header + 32, bytesPerFrame);
    write_u16(pStream->header + 34, bitsPerSample);
    memcpy(pStream->header + 36, "data", 4);
    write_u32(pStream->header + 40, dataSize);
    pStream->size = (drwav_uint64)dataSize + sizeof(pStream->header);
    CHECK(drwav_init(pWav, on_read, on_seek, NULL, pStream, NULL));
    return 0;
}

static int check_cursor(drwav* pWav, test_stream* pStream, drwav_uint64 expected)
{
    drwav_uint64 cursor;
    drwav_uint32 bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);
    CHECK(drwav_get_cursor_in_pcm_frames(pWav, &cursor) == DRWAV_SUCCESS);
    CHECK(cursor == expected);
    CHECK((pStream->cursor - sizeof(pStream->header)) / bytesPerFrame == expected);
    CHECK(pWav->bytesRemaining == pStream->size - pStream->cursor);
    return 0;
}

static int test_seeking(drwav_uint16 channels, drwav_uint16 bitsPerSample)
{
    test_stream stream;
    drwav wav;
    drwav_uint64 positions[7];
    drwav_uint64 boundary;
    drwav_uint64 expected;
    drwav_uint32 bytesPerFrame;
    unsigned char frame[12];
    size_t i;

    CHECK(init_stream(&stream, &wav, channels, bitsPerSample) == 0);
    bytesPerFrame = drwav_get_bytes_per_pcm_frame(&wav);
    boundary = (drwav_uint64)INT_MAX / bytesPerFrame;
    positions[0] = boundary;
    positions[1] = boundary + 1;
    positions[2] = 0;
    positions[3] = boundary + 2;
    positions[4] = wav.totalPCMFrameCount - 1;
    positions[5] = wav.totalPCMFrameCount;
    positions[6] = 1;

    for (i = 0; i < sizeof(positions) / sizeof(positions[0]); i += 1) {
        CHECK(drwav_seek_to_pcm_frame(&wav, positions[i]));
        CHECK(stream.cursor == sizeof(stream.header) + positions[i] * bytesPerFrame);
        CHECK(check_cursor(&wav, &stream, positions[i]) == 0);
        if (positions[i] < wav.totalPCMFrameCount) {
            CHECK(drwav_read_pcm_frames(&wav, 1, frame) == 1);
            CHECK(check_cursor(&wav, &stream, positions[i] + 1) == 0);
        } else {
            CHECK(drwav_read_pcm_frames(&wav, 1, frame) == 0);
            CHECK(check_cursor(&wav, &stream, positions[i]) == 0);
        }
    }

    /* A failed second seek must retain the position reached by the first. */
    CHECK(drwav_seek_to_pcm_frame(&wav, 0));
    stream.failSeek = stream.seekCount + 2;
    CHECK(!drwav_seek_to_pcm_frame(&wav, boundary + 2));
    expected = (stream.cursor - sizeof(stream.header)) / bytesPerFrame;
    CHECK(check_cursor(&wav, &stream, expected) == 0);
    stream.failSeek = 0;
    CHECK(drwav_seek_to_pcm_frame(&wav, expected + 3));
    CHECK(stream.cursor == sizeof(stream.header) + (expected + 3) * bytesPerFrame);
    CHECK(check_cursor(&wav, &stream, expected + 3) == 0);

    /* A raw read need not end on a PCM frame boundary. */
    CHECK(drwav_seek_to_pcm_frame(&wav, 0));
    CHECK(drwav_read_raw(&wav, 1, frame) == 1);
    CHECK(drwav_seek_to_pcm_frame(&wav, 2));
    CHECK(stream.cursor == sizeof(stream.header) + 2 * bytesPerFrame);
    CHECK(check_cursor(&wav, &stream, 2) == 0);

    CHECK(drwav_uninit(&wav) == DRWAV_SUCCESS);
    return 0;
}

static int test_aiff_block_padding(void)
{
    static const unsigned char data[] = {
        'F','O','R','M', 0,0,0,72, 'A','I','F','F',
        'C','O','M','M', 0,0,0,18, 0,1, 0,0,0,4, 0,16,
        0x40,0x0e,0xac,0x44,0,0,0,0,0,0,
        'S','S','N','D', 0,0,0,34, 0,0,0,10, 0,0,0,16,
        0,0,0,0,0,0,0,0,0,0,
        0,1, 0,2, 0,3, 0,4, 0,0,0,0,0,0,0,0
    };
    drwav wav;
    drwav_uint64 cursor;
    drwav_int16 sample;
    drwav_uint64 i;

    CHECK(drwav_init_memory(&wav, data, sizeof(data), NULL));
    for (i = 0; i < 4; i += 1) {
        CHECK(drwav_seek_to_pcm_frame(&wav, i));
        CHECK(drwav_get_cursor_in_pcm_frames(&wav, &cursor) == DRWAV_SUCCESS);
        CHECK(cursor == i);
        CHECK(drwav_read_pcm_frames_s16(&wav, 1, &sample) == 1);
        CHECK(sample == (drwav_int16)(i + 1));
    }
    CHECK(drwav_seek_to_pcm_frame(&wav, 1));
    CHECK(drwav_get_cursor_in_pcm_frames(&wav, &cursor) == DRWAV_SUCCESS);
    CHECK(cursor == 1);
    CHECK(drwav_read_pcm_frames_s16(&wav, 1, &sample) == 1);
    CHECK(sample == 2);
    CHECK(drwav_uninit(&wav) == DRWAV_SUCCESS);
    return 0;
}

int main(void)
{
    static const drwav_uint16 formats[][2] = {
        {1, 8}, {1, 16}, {1, 24}, {1, 32}, {2, 16}, {2, 24}, {2, 32}, {3, 24}, {3, 32}
    };
    size_t i;
    for (i = 0; i < sizeof(formats) / sizeof(formats[0]); i += 1) {
        CHECK(test_seeking(formats[i][0], formats[i][1]) == 0);
    }
    CHECK(test_aiff_block_padding() == 0);
    printf("WAV seek cursor tests passed.\n");
    return 0;
}
