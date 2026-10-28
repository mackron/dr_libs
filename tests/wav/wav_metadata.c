/*
Tests the metadata objects handed back by drwav_init_memory_with_metadata(). Every file is built in memory.

Each object handed back must be fully parsed: any pointer it holds is valid for the size it reports and
every string is null terminated. Allocations are filled with garbage so anything left uninitialized
stands out.
*/
#define DR_WAV_IMPLEMENTATION
#include "../../dr_wav.h"

#include <stdio.h>
#include <string.h>

static void* test_malloc(size_t sz, void* pUserData)
{
    void* p = malloc(sz);
    (void)pUserData;

    if (p != NULL) {
        memset(p, 0xAB, sz);
    }

    return p;
}

static void* test_realloc(void* p, size_t sz, void* pUserData)
{
    (void)pUserData;
    return realloc(p, sz);
}

static void test_free(void* p, void* pUserData)
{
    (void)pUserData;
    free(p);
}


typedef struct
{
    drwav_uint8 data[1024];
    size_t size;
} test_file;

static void test_file_write(test_file* pFile, const void* pData, size_t size)
{
    DRWAV_ASSERT(pFile->size + size <= sizeof(pFile->data));

    memcpy(pFile->data + pFile->size, pData, size);
    pFile->size += size;
}

static void test_file_write_u32(test_file* pFile, drwav_uint32 value)
{
    drwav_uint8 bytes[4];
    bytes[0] = (drwav_uint8)((value >>  0) & 0xFF);
    bytes[1] = (drwav_uint8)((value >>  8) & 0xFF);
    bytes[2] = (drwav_uint8)((value >> 16) & 0xFF);
    bytes[3] = (drwav_uint8)((value >> 24) & 0xFF);
    test_file_write(pFile, bytes, sizeof(bytes));
}

static void test_file_patch_u32(test_file* pFile, size_t offset, drwav_uint32 value)
{
    size_t size = pFile->size;
    pFile->size = offset;
    test_file_write_u32(pFile, value);
    pFile->size = size;
}

/* Starts a chunk and returns the offset of its size so it can be patched with test_file_end_chunk(). */
static size_t test_file_begin_chunk(test_file* pFile, const char* pID)
{
    size_t sizeOffset;

    test_file_write(pFile, pID, 4);
    sizeOffset = pFile->size;
    test_file_write_u32(pFile, 0);

    return sizeOffset;
}

static void test_file_end_chunk(test_file* pFile, size_t sizeOffset)
{
    drwav_uint32 chunkSize = (drwav_uint32)(pFile->size - (sizeOffset + 4));
    test_file_patch_u32(pFile, sizeOffset, chunkSize);

    if ((chunkSize % 2) != 0) {
        test_file_write(pFile, "", 1);
    }
}

/* A 16-bit mono PCM file with two frames. Metadata chunks are written after the data chunk. */
static void test_file_begin(test_file* pFile)
{
    size_t sizeOffset;

    pFile->size = 0;

    test_file_write(pFile, "RIFF", 4);
    test_file_write_u32(pFile, 0);
    test_file_write(pFile, "WAVE", 4);

    sizeOffset = test_file_begin_chunk(pFile, "fmt ");
    {
        drwav_uint8 fmt[16] = {1, 0, 1, 0, 0x40, 0x1F, 0, 0, 0x80, 0x3E, 0, 0, 2, 0, 16, 0};  /* PCM, 1 channel, 8000 Hz, 16 bits. */
        test_file_write(pFile, fmt, sizeof(fmt));
    }
    test_file_end_chunk(pFile, sizeOffset);

    sizeOffset = test_file_begin_chunk(pFile, "data");
    test_file_write_u32(pFile, 0);
    test_file_end_chunk(pFile, sizeOffset);
}

static void test_file_end(test_file* pFile)
{
    test_file_patch_u32(pFile, 4, (drwav_uint32)(pFile->size - 8));
}

static void test_file_write_smpl(test_file* pFile, drwav_uint32 sampleLoopCount, drwav_uint32 loopsToWrite)
{
    size_t sizeOffset = test_file_begin_chunk(pFile, "smpl");
    drwav_uint32 i;

    for (i = 0; i < 7; i += 1) {
        test_file_write_u32(pFile, 0);      /* Manufacturer through to SMPTE offset. */
    }
    test_file_write_u32(pFile, sampleLoopCount);
    test_file_write_u32(pFile, 0);          /* Sampler specific data size. */

    for (i = 0; i < loopsToWrite; i += 1) {
        test_file_write_u32(pFile, i + 1);  /* Cue point ID. */
        test_file_write_u32(pFile, 0);      /* Type. */
        test_file_write_u32(pFile, 0);      /* First sample offset. */
        test_file_write_u32(pFile, 1);      /* Last sample offset. */
        test_file_write_u32(pFile, 0);      /* Fraction. */
        test_file_write_u32(pFile, 0);      /* Play count. */
    }

    test_file_end_chunk(pFile, sizeOffset);
}

static void test_file_write_cue(test_file* pFile, drwav_uint32 cuePointCount, drwav_uint32 cuePointsToWrite)
{
    size_t sizeOffset = test_file_begin_chunk(pFile, "cue ");
    drwav_uint32 i;

    test_file_write_u32(pFile, cuePointCount);

    for (i = 0; i < cuePointsToWrite; i += 1) {
        test_file_write_u32(pFile, i + 1);  /* ID. */
        test_file_write_u32(pFile, 0);      /* Play order position. */
        test_file_write(pFile, "data", 4);
        test_file_write_u32(pFile, 0);      /* Chunk start. */
        test_file_write_u32(pFile, 0);      /* Block start. */
        test_file_write_u32(pFile, i);      /* Sample offset. */
    }

    test_file_end_chunk(pFile, sizeOffset);
}

/* A "labl" or "note" subchunk. The string is written as is, so it can be left without a null terminator. */
static void test_file_write_label(test_file* pFile, const char* pID, drwav_uint32 cuePointId, const char* pString, size_t stringSize)
{
    size_t sizeOffset = test_file_begin_chunk(pFile, pID);
    test_file_write_u32(pFile, cuePointId);
    test_file_write(pFile, pString, stringSize);
    test_file_end_chunk(pFile, sizeOffset);
}


static drwav_bool32 test_open(drwav* pWav, const test_file* pFile)
{
    drwav_allocation_callbacks allocationCallbacks;
    allocationCallbacks.pUserData = NULL;
    allocationCallbacks.onMalloc  = test_malloc;
    allocationCallbacks.onRealloc = test_realloc;
    allocationCallbacks.onFree    = test_free;

    if (!drwav_init_memory_with_metadata(pWav, pFile->data, pFile->size, 0, &allocationCallbacks)) {
        printf("  Failed to open file.\n");
        return DRWAV_FALSE;
    }

    return DRWAV_TRUE;
}

static drwav_bool32 test_check(drwav_bool32 condition, const char* pDescription)
{
    if (!condition) {
        printf("  Check failed: %s\n", pDescription);
    }

    return condition;
}

/* The cue chunk declares far more cue points than it has room for. It must not be handed back without its list of cue points. */
static drwav_bool32 test_init_with_metadata__cue_point_count_exceeds_chunk(void)
{
    test_file file;
    drwav wav;
    drwav_bool32 passed = DRWAV_TRUE;

    test_file_begin(&file);
    test_file_write_cue(&file, 0x7C000001, 1);
    test_file_end(&file);

    if (!test_open(&wav, &file)) {
        return DRWAV_FALSE;
    }

    passed = test_check(wav.metadataCount == 0, "metadataCount == 0") && passed;

    drwav_uninit(&wav);
    return passed;
}

/* The cue chunk has room for no cue points but declares one. The chunk is fully consumed, but must still be rejected. */
static drwav_bool32 test_init_with_metadata__cue_chunk_without_room_for_cue_points(void)
{
    test_file file;
    drwav wav;
    drwav_bool32 passed = DRWAV_TRUE;

    test_file_begin(&file);
    test_file_write_cue(&file, 1, 0);
    test_file_end(&file);

    if (!test_open(&wav, &file)) {
        return DRWAV_FALSE;
    }

    passed = test_check(wav.metadataCount == 0, "metadataCount == 0") && passed;

    drwav_uninit(&wav);
    return passed;
}

/*
The smpl chunk declares a loop it has no room for, so it's rejected. It must not take up a metadata object that
was set aside for the labels that follow it, nor push the labels past the end of the metadata.
*/
static drwav_bool32 test_init_with_metadata__rejected_smpl_chunk_before_labels(void)
{
    test_file file;
    drwav wav;
    size_t listSizeOffset;
    drwav_bool32 passed = DRWAV_TRUE;

    test_file_begin(&file);
    test_file_write_smpl(&file, 1, 0);
    listSizeOffset = test_file_begin_chunk(&file, "LIST");
    test_file_write(&file, "adtl", 4);
    test_file_write_label(&file, "labl", 1, "", 0);
    test_file_write_label(&file, "labl", 2, "", 0);
    test_file_end_chunk(&file, listSizeOffset);
    test_file_end(&file);

    if (!test_open(&wav, &file)) {
        return DRWAV_FALSE;
    }

    if (test_check(wav.metadataCount == 2, "metadataCount == 2")) {
        passed = test_check(wav.pMetadata[0].type == drwav_metadata_type_list_label && wav.pMetadata[0].data.labelOrNote.cuePointId == 1, "first object is the label of cue point 1") && passed;
        passed = test_check(wav.pMetadata[1].type == drwav_metadata_type_list_label && wav.pMetadata[1].data.labelOrNote.cuePointId == 2, "second object is the label of cue point 2") && passed;
    } else {
        passed = DRWAV_FALSE;
    }

    drwav_uninit(&wav);
    return passed;
}

/* The strings in the file have no null terminator. The documentation says they are null terminated for convenience. */
static drwav_bool32 test_init_with_metadata__strings_without_null_terminator(void)
{
    test_file file;
    drwav wav;
    size_t listSizeOffset;
    size_t subchunkSizeOffset;
    drwav_uint32 iMetadata;
    drwav_bool32 passed = DRWAV_TRUE;

    test_file_begin(&file);

    listSizeOffset = test_file_begin_chunk(&file, "LIST");
    test_file_write(&file, "adtl", 4);
    test_file_write_label(&file, "labl", 1, "abcd", 4);
    test_file_write_label(&file, "note", 1, "abcd", 4);
    subchunkSizeOffset = test_file_begin_chunk(&file, "ltxt");
    {
        drwav_uint8 ltxt[16] = {0};     /* Sample length through to code page. */
        test_file_write_u32(&file, 1);  /* Cue point ID. */
        test_file_write(&file, ltxt, sizeof(ltxt));
        test_file_write(&file, "abcd", 4);
    }
    test_file_end_chunk(&file, subchunkSizeOffset);
    test_file_end_chunk(&file, listSizeOffset);

    listSizeOffset = test_file_begin_chunk(&file, "LIST");
    test_file_write(&file, "INFO", 4);
    subchunkSizeOffset = test_file_begin_chunk(&file, "INAM");
    test_file_write(&file, "abcd", 4);
    test_file_end_chunk(&file, subchunkSizeOffset);
    test_file_end_chunk(&file, listSizeOffset);

    test_file_end(&file);

    if (!test_open(&wav, &file)) {
        return DRWAV_FALSE;
    }

    passed = test_check(wav.metadataCount == 4, "metadataCount == 4") && passed;

    for (iMetadata = 0; iMetadata < wav.metadataCount; iMetadata += 1) {
        const drwav_metadata* pMetadata = &wav.pMetadata[iMetadata];
        const char* pString = NULL;
        drwav_uint32 stringLength = 0;

        if (pMetadata->type == drwav_metadata_type_list_label || pMetadata->type == drwav_metadata_type_list_note) {
            pString      = pMetadata->data.labelOrNote.pString;
            stringLength = pMetadata->data.labelOrNote.stringLength;
        } else if (pMetadata->type == drwav_metadata_type_list_labelled_cue_region) {
            pString      = pMetadata->data.labelledCueRegion.pString;
            stringLength = pMetadata->data.labelledCueRegion.stringLength;
        } else if (pMetadata->type == drwav_metadata_type_list_info_title) {
            pString      = pMetadata->data.infoText.pString;
            stringLength = pMetadata->data.infoText.stringLength;
        }

        passed = test_check(pString != NULL && stringLength == 3 && memcmp(pString, "abc", 4) == 0, "string is \"abc\" with a null terminator") && passed;
    }

    drwav_uninit(&wav);
    return passed;
}

/* Well formed chunks are all handed back as they are in the file, with no pointer left uninitialized. */
static drwav_bool32 test_init_with_metadata__valid_chunks(void)
{
    test_file file;
    drwav wav;
    size_t listSizeOffset;
    drwav_bool32 passed = DRWAV_TRUE;

    test_file_begin(&file);
    test_file_write_cue(&file, 2, 2);
    test_file_write_smpl(&file, 1, 1);
    listSizeOffset = test_file_begin_chunk(&file, "LIST");
    test_file_write(&file, "adtl", 4);
    test_file_write_label(&file, "labl", 2, "loop", 5);
    test_file_end_chunk(&file, listSizeOffset);
    test_file_end(&file);

    if (!test_open(&wav, &file)) {
        return DRWAV_FALSE;
    }

    if (test_check(wav.metadataCount == 3, "metadataCount == 3")) {
        const drwav_metadata* pCue   = &wav.pMetadata[0];
        const drwav_metadata* pSmpl  = &wav.pMetadata[1];
        const drwav_metadata* pLabel = &wav.pMetadata[2];

        passed = test_check(pCue->type == drwav_metadata_type_cue && pCue->data.cue.cuePointCount == 2 && pCue->data.cue.pCuePoints[1].id == 2 && pCue->data.cue.pCuePoints[1].sampleOffset == 1, "cue chunk") && passed;
        passed = test_check(pSmpl->type == drwav_metadata_type_smpl && pSmpl->data.smpl.sampleLoopCount == 1 && pSmpl->data.smpl.pLoops[0].cuePointId == 1 && pSmpl->data.smpl.pSamplerSpecificData == NULL, "smpl chunk") && passed;
        passed = test_check(pLabel->type == drwav_metadata_type_list_label && pLabel->data.labelOrNote.cuePointId == 2 && pLabel->data.labelOrNote.stringLength == 4 && strcmp(pLabel->data.labelOrNote.pString, "loop") == 0, "label") && passed;
    } else {
        passed = DRWAV_FALSE;
    }

    drwav_uninit(&wav);
    return passed;
}


static int run_test(drwav_bool32 (* pTest)(void), const char* pName)
{
    drwav_bool32 passed;

    printf("%s\n", pName);
    passed = pTest();
    printf("  %s\n", passed ? "PASSED" : "FAILED");

    return passed ? 0 : 1;
}

int main(int argc, char** argv)
{
    int failures = 0;

    (void)argc;
    (void)argv;

    failures += run_test(test_init_with_metadata__cue_point_count_exceeds_chunk,         "cue point count exceeds chunk");
    failures += run_test(test_init_with_metadata__cue_chunk_without_room_for_cue_points, "cue chunk without room for cue points");
    failures += run_test(test_init_with_metadata__rejected_smpl_chunk_before_labels,     "rejected smpl chunk before labels");
    failures += run_test(test_init_with_metadata__strings_without_null_terminator,       "strings without null terminator");
    failures += run_test(test_init_with_metadata__valid_chunks,                          "valid chunks");

    return failures == 0 ? 0 : 1;
}
