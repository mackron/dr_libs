#include "../external/fs/fs.c"

#include <sndfile.h>
#include <float.h>
#include <math.h>

typedef int dr_bool32;
#define DR_FALSE    0
#define DR_TRUE     1

static fs_file* STDIN  = NULL;
static fs_file* STDOUT = NULL;
static fs_file* STDERR = NULL;

enum
{
    dr_test_log_flag_verbose = 0x1
};

typedef struct
{
    fs_file* pOutput;
    const char* pFilePath;
    unsigned int flags;
} dr_test_log;

static void dr_printf(const char* pFormat, ...)
{
    va_list args;

    va_start(args, pFormat);
    fs_file_writefv(STDOUT, pFormat, args);
    va_end(args);
}

static void dr_errorf(const char* pFormat, ...)
{
    va_list args;

    va_start(args, pFormat);
    fs_file_writefv(STDERR, pFormat, args);
    va_end(args);
}

/* A NULL output disables diagnostics for this test. */
static void dr_test_log_errorf(const dr_test_log* pLog, const char* pFormat, ...)
{
    va_list args;

    if (pLog->pOutput == NULL) {
        return;
    }

    fs_file_writef(pLog->pOutput, "%s: error: ", pLog->pFilePath);

    va_start(args, pFormat);
    fs_file_writefv(pLog->pOutput, pFormat, args);
    va_end(args);
}

/* Warnings are only shown in verbose mode. */
static void dr_test_log_warningf(const dr_test_log* pLog, const char* pFormat, ...)
{
    va_list args;

    if (pLog->pOutput == NULL || (pLog->flags & dr_test_log_flag_verbose) == 0) {
        return;
    }

    fs_file_writef(pLog->pOutput, "%s: warning: ", pLog->pFilePath);

    va_start(args, pFormat);
    fs_file_writefv(pLog->pOutput, pFormat, args);
    va_end(args);
}

static void dr_print_usage(void)
{
    dr_printf("USAGE:\n");
    dr_printf("    dr_libs_test [<options>] [<inputs>]\n");
    dr_printf("\n");
    dr_printf("INPUTS:\n");
    dr_printf("    Files or folders. Folders are searched recursively.\n");
    dr_printf("    Try each path as given, then relative to the database's vectors folder.\n");
    dr_printf("    Repeated input arguments run once. No inputs means the whole database.\n");
    dr_printf("\n");
    dr_printf("OPTIONS:\n");
    dr_printf("    -h, --help\n");
    dr_printf("        Show this help text.\n");
    dr_printf("    --verbose\n");
    dr_printf("        Show warnings.\n");
    dr_printf("    --audiovectors <path>\n");
    dr_printf("        Database root. Defaults to external/audiovectors relative to the working directory.\n");
    dr_printf("    --exceptions <path>\n");
    dr_printf("        Repeat to combine exception lists. With none, load tests/exceptions.txt\n");
    dr_printf("        and tests/exceptions-private.txt if the private file exists.\n");
    dr_printf("    --test <decoding|seeking>\n");
    dr_printf("        Select a test stage. Repeat to select both. Defaults to both.\n");
    dr_printf("    --format <wav|aiff|flac|mp3>\n");
    dr_printf("        Override the decoder for all inputs. Otherwise use file extensions.\n");
    dr_printf("        Database folders keep their existing decoder selection.\n");
}

typedef enum
{
    dr_test_sample_format_s16,
    dr_test_sample_format_s32,
    dr_test_sample_format_f32
} dr_test_sample_format;

static size_t dr_test_sample_size(dr_test_sample_format format)
{
    if (format == dr_test_sample_format_s16) {
        return 2;
    } else if (format == dr_test_sample_format_f32) {
        return 4;
    } else if (format == dr_test_sample_format_s32) {
        return 4;
    } else {
        assert(!"Unknown format.");
        return 0;
    }
}

static const char* dr_test_sample_format_name(dr_test_sample_format format)
{
    if (format == dr_test_sample_format_s16) {
        return "s16";
    }

    if (format == dr_test_sample_format_f32) {
        return "f32";
    }

    return "s32";
}

static dr_bool32 dr_test_f32_equal(float a, float b)
{
    double magnitude;
    double difference;
    double tolerance;

    /* NaNs match only NaNs. Equal values include signed zeros and equal infinities. */
    if (a != a || b != b) {
        return a != a && b != b;
    }

    if (a == b) {
        return DR_TRUE;
    }

    if (a > FLT_MAX || a < -FLT_MAX || b > FLT_MAX || b < -FLT_MAX) {
        return DR_FALSE;
    }

    magnitude = fabs((double)a);
    if (fabs((double)b) > magnitude) {
        magnitude = fabs((double)b);
    }

    /* One absolute epsilon plus one relative epsilon. Use double to avoid overflow. */
    difference = fabs((double)a - (double)b);
    tolerance = FLT_EPSILON + FLT_EPSILON * magnitude;

    return difference <= tolerance;
}

typedef enum
{
    dr_test_stage_decoding,
    dr_test_stage_seeking,
    dr_test_stage_seeking_basic
} dr_test_stage;

enum
{
    dr_test_flag_skip_reference = 0x1
};

typedef int (* dr_test_file_proc)(const dr_test_log*, const char*, dr_test_stage, unsigned int);

typedef enum
{
    dr_test_format_unknown = -1,
    dr_test_format_wav,
    dr_test_format_flac,
    dr_test_format_mp3
} dr_test_format;

#include "dr_libs_test_wav.c"
#include "dr_libs_test_flac.c"
#include "dr_libs_test_mp3.c"

/* Fields are byte ranges, not null-terminated strings. */
typedef struct
{
    const char* pData;
    size_t dataSize;
} dr_test_ssv_field;

typedef struct
{
    const char* pData;
    size_t dataSize;
    size_t cursor;
} dr_test_ssv_parser;

typedef struct
{
    const char* pData;
    size_t dataSize;
    size_t cursor;
    int atEnd;
} dr_test_ssv_row;

static int dr_test_ssv_is_whitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

static int dr_test_ssv_parser_init(const char* pData, size_t dataSize, dr_test_ssv_parser* pParser)
{
    if (pParser == NULL) {
        return -1;
    }

    pParser->pData = NULL;
    pParser->dataSize = 0;
    pParser->cursor = 0;

    if (pData == NULL && dataSize != 0) {
        return -1;
    }

    pParser->pData = pData;
    pParser->dataSize = dataSize;

    return 1;
}

static int dr_test_ssv_parser_next_row(dr_test_ssv_parser* pParser, dr_test_ssv_row* pRow)
{
    size_t cursor;

    if (pRow == NULL) {
        return -1;
    }

    pRow->pData = NULL;
    pRow->dataSize = 0;
    pRow->cursor = 0;
    pRow->atEnd = 1;

    if (pParser == NULL || (pParser->pData == NULL && pParser->dataSize != 0) || pParser->cursor > pParser->dataSize) {
        return -1;
    }

    cursor = pParser->cursor;
    while (cursor < pParser->dataSize) {
        size_t lineStart;
        size_t lineEnd;

        lineStart = cursor;
        while (cursor < pParser->dataSize && pParser->pData[cursor] != '\n') {
            if (pParser->pData[cursor] == '\0') {
                return -1;
            }

            cursor += 1;
        }

        lineEnd = cursor;
        if (cursor < pParser->dataSize) {
            cursor += 1;
        }

        while (lineStart < lineEnd && dr_test_ssv_is_whitespace(pParser->pData[lineStart])) {
            lineStart += 1;
        }

        if (lineStart == lineEnd || pParser->pData[lineStart] == '#') {
            continue;
        }

        pRow->pData = pParser->pData + lineStart;
        pRow->dataSize = lineEnd - lineStart;
        pRow->atEnd = 0;
        pParser->cursor = cursor;

        return 1;
    }

    pParser->cursor = cursor;

    return 0;
}

static int dr_test_ssv_row_next_field(dr_test_ssv_row* pRow, dr_test_ssv_field* pField)
{
    size_t fieldStart;
    size_t fieldEnd;

    if (pField == NULL) {
        return -1;
    }

    pField->pData = NULL;
    pField->dataSize = 0;

    if (pRow == NULL || pRow->cursor > pRow->dataSize) {
        return -1;
    }

    if (pRow->atEnd) {
        return 0;
    }

    if (pRow->pData == NULL) {
        return -1;
    }

    fieldStart = pRow->cursor;
    fieldEnd = fieldStart;
    while (fieldEnd < pRow->dataSize && pRow->pData[fieldEnd] != ';') {
        fieldEnd += 1;
    }

    if (fieldEnd < pRow->dataSize) {
        pRow->cursor = fieldEnd + 1;
    } else {
        pRow->cursor = fieldEnd;
        pRow->atEnd = 1;
    }

    while (fieldStart < fieldEnd && dr_test_ssv_is_whitespace(pRow->pData[fieldStart])) {
        fieldStart += 1;
    }

    while (fieldEnd > fieldStart && dr_test_ssv_is_whitespace(pRow->pData[fieldEnd - 1])) {
        fieldEnd -= 1;
    }

    pField->pData = pRow->pData + fieldStart;
    pField->dataSize = fieldEnd - fieldStart;

    return 1;
}

enum
{
    dr_test_exception_fail_open   = 0x1,
    dr_test_exception_fail_decode = 0x2,
    dr_test_exception_ignore      = 0x4,
    dr_test_exception_skip_reference = 0x8,
    dr_test_exception_fail_seek = 0x10
};

typedef struct
{
    dr_test_ssv_field path;
    unsigned int flags;
} dr_test_exception;

typedef struct
{
    void** ppFileData;
    size_t fileCount;
    dr_test_exception* pItems;
    size_t count;
} dr_test_exceptions;

static dr_bool32 dr_test_ssv_field_equal(const dr_test_ssv_field* pField, const char* pString)
{
    return pField->dataSize == strlen(pString) && memcmp(pField->pData, pString, pField->dataSize) == 0;
}

static int dr_test_exception_parse_flags(const dr_test_ssv_field* pField, unsigned int* pFlags)
{
    size_t cursor = 0;

    *pFlags = 0;
    while (cursor < pField->dataSize) {
        dr_test_ssv_field token;

        while (cursor < pField->dataSize && dr_test_ssv_is_whitespace(pField->pData[cursor])) {
            cursor += 1;
        }

        token.pData = pField->pData + cursor;
        token.dataSize = 0;
        while (cursor < pField->dataSize && !dr_test_ssv_is_whitespace(pField->pData[cursor])) {
            token.dataSize += 1;
            cursor += 1;
        }

        if (token.dataSize == 0) {
            break;
        }

        if (dr_test_ssv_field_equal(&token, "FAIL_OPEN")) {
            *pFlags |= dr_test_exception_fail_open;
        } else if (dr_test_ssv_field_equal(&token, "FAIL_DECODE")) {
            *pFlags |= dr_test_exception_fail_decode;
        } else if (dr_test_ssv_field_equal(&token, "FAIL_SEEK")) {
            *pFlags |= dr_test_exception_fail_seek;
        } else if (dr_test_ssv_field_equal(&token, "SKIP_REFERENCE")) {
            *pFlags |= dr_test_exception_skip_reference;
        } else if (dr_test_ssv_field_equal(&token, "IGNORE") || dr_test_ssv_field_equal(&token, "UNSUPPORTED")) {
            *pFlags |= dr_test_exception_ignore;
        } else {
            dr_errorf("Unknown exception flag: %.*s.\n", (int)token.dataSize, token.pData);
            return 1;
        }
    }

    /* Each exception selects one policy. */
    if (*pFlags != dr_test_exception_fail_open && *pFlags != dr_test_exception_fail_decode && *pFlags != dr_test_exception_fail_seek && *pFlags != dr_test_exception_ignore && *pFlags != dr_test_exception_skip_reference) {
        dr_errorf("Exception flags are empty or conflict.\n");
        return 1;
    }

    return 0;
}

static void dr_test_exceptions_uninit(dr_test_exceptions* pExceptions)
{
    size_t iFile;

    free(pExceptions->pItems);
    for (iFile = 0; iFile < pExceptions->fileCount; iFile += 1) {
        fs_free(pExceptions->ppFileData[iFile], NULL);
    }

    free(pExceptions->ppFileData);
    memset(pExceptions, 0, sizeof(*pExceptions));
}

static int dr_test_exceptions_init(const char* const* ppFilePaths, size_t fileCount, dr_test_exceptions* pExceptions)
{
    size_t capacity = 0;
    size_t iFile;

    memset(pExceptions, 0, sizeof(*pExceptions));

    /* Retain each source buffer because exception paths point into its SSV fields. */
    pExceptions->ppFileData = (void**)calloc(fileCount, sizeof(*pExceptions->ppFileData));
    if (pExceptions->ppFileData == NULL) {
        dr_errorf("Out of memory for exception files.\n");
        return 1;
    }

    pExceptions->fileCount = fileCount;

    for (iFile = 0; iFile < fileCount; iFile += 1) {
        const char* pFilePath;
        dr_test_ssv_parser parser;
        dr_test_ssv_row row;
        size_t fileDataSize;
        size_t entryCount = 0;
        fs_result result;
        int parseResult;

        pFilePath = ppFilePaths[iFile];

        result = fs_file_open_and_read(NULL, pFilePath, FS_FORMAT_TEXT, &pExceptions->ppFileData[iFile], &fileDataSize);
        if (result != FS_SUCCESS) {
            dr_errorf("Failed to read exceptions from %s: %s.\n", pFilePath, fs_result_description(result));
            goto error;
        }

        dr_test_ssv_parser_init((const char*)pExceptions->ppFileData[iFile], fileDataSize, &parser);
        while ((parseResult = dr_test_ssv_parser_next_row(&parser, &row)) == 1) {
            dr_test_exception item;
            dr_test_ssv_field flags;
            dr_test_ssv_field extraField;
            size_t iItem;

            if (dr_test_ssv_row_next_field(&row, &item.path) != 1 || dr_test_ssv_row_next_field(&row, &flags) != 1 || dr_test_ssv_row_next_field(&row, &extraField) != 0 || item.path.dataSize == 0) {
                dr_errorf("Invalid exception entry %lu in %s. Expected path; flags.\n", (unsigned long)(entryCount + 1), pFilePath);
                goto error;
            }

            if (dr_test_exception_parse_flags(&flags, &item.flags) != 0) {
                dr_errorf("Invalid exception entry %lu in %s.\n", (unsigned long)(entryCount + 1), pFilePath);
                goto error;
            }

            for (iItem = 0; iItem < pExceptions->count; iItem += 1) {
                const dr_test_ssv_field* pPath;

                pPath = &pExceptions->pItems[iItem].path;
                if (fs_path_compare(pPath->pData, pPath->dataSize, item.path.pData, item.path.dataSize) == 0) {
                    dr_errorf("Duplicate exception path in %s: %.*s.\n", pFilePath, (int)item.path.dataSize, item.path.pData);
                    goto error;
                }
            }

            if (pExceptions->count == capacity) {
                dr_test_exception* pItems;

                if (capacity == 0) {
                    capacity = 16;
                } else {
                    capacity *= 2;
                }

                pItems = (dr_test_exception*)realloc(pExceptions->pItems, capacity * sizeof(*pItems));
                if (pItems == NULL) {
                    dr_errorf("Out of memory for exceptions.\n");
                    goto error;
                }

                pExceptions->pItems = pItems;
            }

            pExceptions->pItems[pExceptions->count] = item;
            pExceptions->count += 1;
            entryCount += 1;
        }

        if (parseResult != 0) {
            dr_errorf("Failed to parse exceptions in %s.\n", pFilePath);
            goto error;
        }

    }

    return 0;

error:
    dr_test_exceptions_uninit(pExceptions);
    return 1;
}

static unsigned int dr_test_exception_flags(const dr_test_exceptions* pExceptions, const char* pPath)
{
    size_t iItem;

    for (iItem = 0; iItem < pExceptions->count; iItem += 1) {
        const dr_test_ssv_field* pExceptionPath;

        pExceptionPath = &pExceptions->pItems[iItem].path;
        if (fs_path_compare(pExceptionPath->pData, pExceptionPath->dataSize, pPath, FS_NULL_TERMINATED) == 0) {
            return pExceptions->pItems[iItem].flags;
        }
    }

    return 0;
}

static dr_bool32 dr_test_can_open_file(const char* pFilePath, dr_test_format format)
{
    if (format == dr_test_format_wav) {
        drwav wav;

        if (!drwav_init_file(&wav, pFilePath, NULL)) {
            return DR_FALSE;
        }

        drwav_uninit(&wav);
    } else if (format == dr_test_format_flac) {
        drflac* pFlac = NULL;

        pFlac = drflac_open_file(pFilePath, NULL);
        if (pFlac == NULL) {
            return DR_FALSE;
        }

        drflac_close(pFlac);
    } else {
        drmp3 mp3;

        if (!drmp3_init_file(&mp3, pFilePath, NULL)) {
            return DR_FALSE;
        }

        drmp3_uninit(&mp3);
    }

    return DR_TRUE;
}

static int dr_test_expected_failure(const dr_test_log* pLog, const char* pFilePath, dr_test_format format, unsigned int flags, dr_test_file_proc onTestFile)
{
    dr_test_log quietLog;
    dr_test_stage stage;
    const char* pStageName;
    dr_bool32 opened;
    int result;

    opened = dr_test_can_open_file(pFilePath, format);
    
    if (flags == dr_test_exception_fail_open) {
        if (opened) {
            dr_test_log_errorf(pLog, "Expected an open failure, but dr_libs opened the file.\n");
            return 1;
        }

        return 0;
    }

    if (flags == dr_test_exception_fail_seek) {
        stage = dr_test_stage_seeking;
        pStageName = "seeking";
    } else {
        stage = dr_test_stage_decoding;
        pStageName = "decoding";
    }

    if (!opened) {
        dr_test_log_errorf(pLog, "Expected a %s failure, but dr_libs could not open the file.\n", pStageName);
        return 1;
    }

    /* Keep expected failures quiet after the file name is printed. */
    quietLog = *pLog;
    quietLog.pOutput = NULL;
    result = onTestFile(&quietLog, pFilePath, stage, 0);
    if (result == 0) {
        dr_test_log_errorf(pLog, "Expected a %s failure, but %s checks passed.\n", pStageName, pStageName);
        return 1;
    }

    return 0;
}

static dr_bool32 dr_test_is_excluded_file(const char* pName)
{
    static const char* excludedNames[] = {
        "README", "README.md", "README.txt",
        "COPYING", "COPYING.md", "COPYING.txt",
        "LICENSE", "LICENSE.md", "LICENSE.txt",
        "AUTHORS", "AUTHORS.md", "AUTHORS.txt",
        ".DS_Store", "Thumbs.db", "desktop.ini"
    };
    size_t iName;

    for (iName = 0; iName < sizeof(excludedNames) / sizeof(excludedNames[0]); iName += 1) {
        if (strcmp(pName, excludedNames[iName]) == 0) {
            return DR_TRUE;
        }
    }

    return DR_FALSE;
}

static char* dr_test_path_append(const char* pBasePath, const char* pName)
{
    char* pPath;
    int pathLength;

    pathLength = fs_path_append(NULL, 0, pBasePath, FS_NULL_TERMINATED, pName, FS_NULL_TERMINATED);
    if (pathLength < 0) {
        dr_errorf("Failed to build a vector path.\n");
        return NULL;
    }

    pPath = (char*)malloc((size_t)pathLength + 1);
    if (pPath == NULL) {
        dr_errorf("Out of memory.\n");
        return NULL;
    }

    fs_path_append(pPath, (size_t)pathLength + 1, pBasePath, FS_NULL_TERMINATED, pName, FS_NULL_TERMINATED);

    return pPath;
}

typedef struct
{
    dr_test_format format;
    const char* pName;
    dr_test_file_proc onTestFile;
} dr_test_format_info;

/* Decoder entries follow dr_test_format order. Database folder aliases follow them. */
static const dr_test_format_info dr_test_formats[] = {
    {dr_test_format_wav,  "wav",  dr_test_wav_file},
    {dr_test_format_flac, "flac", dr_test_flac_file},
    {dr_test_format_mp3,  "mp3",  dr_test_mp3_file},
    {dr_test_format_wav,  "aiff", dr_test_wav_file}
};

static dr_test_format dr_test_format_from_name(const char* pName)
{
    size_t iFormat;

    for (iFormat = 0; iFormat < sizeof(dr_test_formats) / sizeof(dr_test_formats[0]); iFormat += 1) {
        if (strcmp(pName, dr_test_formats[iFormat].pName) == 0) {
            return dr_test_formats[iFormat].format;
        }
    }

    return dr_test_format_unknown;
}

typedef struct
{
    const char* pExtension;
    dr_test_format format;
} dr_test_extension_info;

static dr_test_format dr_test_format_from_extension(const char* pPath)
{
    static const dr_test_extension_info extensions[] = {
        {"wav",  dr_test_format_wav},
        {"wave", dr_test_format_wav},
        {"w64",  dr_test_format_wav},
        {"rf64", dr_test_format_wav},
        {"aif",  dr_test_format_wav},
        {"aiff", dr_test_format_wav},
        {"aifc", dr_test_format_wav},
        {"flac", dr_test_format_flac},
        {"ogg",  dr_test_format_flac},
        {"oga",  dr_test_format_flac},
        {"mp3",  dr_test_format_mp3}
    };
    size_t iExtension;

    for (iExtension = 0; iExtension < sizeof(extensions) / sizeof(extensions[0]); iExtension += 1) {
        if (fs_path_extension_equal(pPath, FS_NULL_TERMINATED, extensions[iExtension].pExtension, FS_NULL_TERMINATED)) {
            return extensions[iExtension].format;
        }
    }

    return dr_test_format_unknown;
}

static dr_test_format dr_test_database_format(const char* pPath)
{
    fs_path_iterator first;
    size_t iFormat;

    if (fs_path_first(pPath, FS_NULL_TERMINATED, &first) != FS_SUCCESS) {
        return dr_test_format_unknown;
    }

    for (iFormat = 0; iFormat < sizeof(dr_test_formats) / sizeof(dr_test_formats[0]); iFormat += 1) {
        const char* pName;

        pName = dr_test_formats[iFormat].pName;
        if (first.segmentLength == strlen(pName) && memcmp(first.pFullPath + first.segmentOffset, pName, first.segmentLength) == 0) {
            return dr_test_formats[iFormat].format;
        }
    }

    return dr_test_format_unknown;
}

static int dr_test_file(const char* pFilePath, const char* pDisplayPath, dr_bool32 isDatabase, dr_test_format format, dr_test_stage stage, const dr_test_exceptions* pExceptions, unsigned int logFlags)
{
    dr_test_log log;
    const char* pName;
    unsigned int flags = 0;
    unsigned int testFlags = 0;
    dr_test_file_proc onTestFile;
    int result;

    memset(&log, 0, sizeof(log));

    pName = fs_path_file_name(pFilePath, FS_NULL_TERMINATED);
    if (pName != NULL && dr_test_is_excluded_file(pName)) {
        return 0;
    }

    if (isDatabase) {
        flags = dr_test_exception_flags(pExceptions, pDisplayPath);
    }

    if (flags == dr_test_exception_ignore) {
        return 0;
    }

    if (flags == dr_test_exception_skip_reference) {
        testFlags |= dr_test_flag_skip_reference;
    }

    dr_printf("%s\n", pDisplayPath);
    log.pOutput = STDERR;
    log.pFilePath = pDisplayPath;
    log.flags = logFlags;

    if (format == dr_test_format_unknown) {
        format = dr_test_format_from_extension(pFilePath);
    }

    if (format == dr_test_format_unknown) {
        dr_test_log_errorf(&log, "Cannot determine the format. Specify --format wav, aiff, flac, or mp3.\n");
        return 1;
    }

    onTestFile = dr_test_formats[format].onTestFile;
    if (stage == dr_test_stage_seeking && flags != dr_test_exception_fail_open) {
        result = onTestFile(&log, pFilePath, dr_test_stage_seeking_basic, testFlags);
        if (result != 0) {
            return result;
        }

        /* Malformed files still exercise seeking, but cannot supply a decoded reference. */
        if (flags == dr_test_exception_fail_decode) {
            return 0;
        }
    }

    if (flags == 0 || flags == dr_test_exception_skip_reference || (flags == dr_test_exception_fail_seek && stage == dr_test_stage_decoding)) {
        result = onTestFile(&log, pFilePath, stage, testFlags);
    } else {
        result = dr_test_expected_failure(&log, pFilePath, format, flags, onTestFile);
    }

    return result;
}

static int dr_test_directory(const char* pDirectoryPath, const char* pDisplayPath, dr_bool32 isDatabase, dr_test_format format, dr_test_stage stage, const dr_test_exceptions* pExceptions, unsigned int logFlags)
{
    fs_iterator* pIterator;
    int exitCode = 0;

    /* A database folder selects its decoder, including for vectors without an extension. */
    if (isDatabase && format == dr_test_format_unknown) {
        format = dr_test_database_format(pDisplayPath);
    }

    for (pIterator = fs_first(NULL, pDirectoryPath, FS_READ | FS_OPAQUE); pIterator != NULL; pIterator = fs_next(pIterator)) {
        char* pFilePath;
        char* pFileDisplayPath;
        int testResult;

        if (!pIterator->info.directory && dr_test_is_excluded_file(pIterator->pName)) {
            continue;
        }

        pFilePath = dr_test_path_append(pDirectoryPath, pIterator->pName);
        pFileDisplayPath = dr_test_path_append(pDisplayPath, pIterator->pName);
        if (pFilePath == NULL || pFileDisplayPath == NULL) {
            free(pFileDisplayPath);
            free(pFilePath);
            fs_free_iterator(pIterator);
            return 1;
        }

        if (pIterator->info.directory) {
            testResult = dr_test_directory(pFilePath, pFileDisplayPath, isDatabase, format, stage, pExceptions, logFlags);
        } else {
            testResult = dr_test_file(pFilePath, pFileDisplayPath, isDatabase, format, stage, pExceptions, logFlags);
        }

        free(pFileDisplayPath);
        free(pFilePath);

        if (testResult != 0) {
            exitCode = 1;
        }
    }

    return exitCode;
}

typedef struct
{
    const char* pArgument;
    char* pPath;
    const char* pDisplayPath;
    dr_bool32 isDatabase;
    dr_bool32 isDirectory;
} dr_test_input;

static int dr_test_input_init(dr_test_input* pInput, const char* pVectorsPath, dr_bool32 databaseOnly)
{
    fs_file_info info;
    fs_result result;

    if (databaseOnly) {
        pInput->pPath = dr_test_path_append(pVectorsPath, pInput->pArgument);
    } else {
        pInput->pPath = dr_test_path_append("", pInput->pArgument);
    }

    if (pInput->pPath == NULL) {
        return 1;
    }

    result = fs_info(NULL, pInput->pPath, FS_READ | FS_OPAQUE, &info);
    if ((result == FS_DOES_NOT_EXIST || result == FS_NOT_DIRECTORY) && !databaseOnly) {
        free(pInput->pPath);
        pInput->pPath = dr_test_path_append(pVectorsPath, pInput->pArgument);
        if (pInput->pPath == NULL) {
            return 1;
        }

        result = fs_info(NULL, pInput->pPath, FS_READ | FS_OPAQUE, &info);
    }

    if (result != FS_SUCCESS) {
        /* Default database groups are optional. Explicit inputs must exist. */
        if (databaseOnly && result == FS_DOES_NOT_EXIST) {
            free(pInput->pPath);
            pInput->pPath = NULL;
            return 0;
        }

        dr_printf("%s\n", pInput->pArgument);
        dr_errorf("%s: error: Cannot open input: %s.\n", pInput->pArgument, fs_result_description(result));
        return 1;
    }

    pInput->pDisplayPath = fs_path_trim_base(pInput->pPath, FS_NULL_TERMINATED, pVectorsPath, FS_NULL_TERMINATED);
    pInput->isDatabase = pInput->pDisplayPath != NULL;
    if (!pInput->isDatabase) {
        pInput->pDisplayPath = pInput->pPath;
    }

    pInput->isDirectory = info.directory;

    return 0;
}

static int dr_main(int argc, char** argv)
{
    static const dr_test_stage stages[] = {
        dr_test_stage_decoding,
        dr_test_stage_seeking
    };

    const char* pAudiovectorsPath = "external/audiovectors";
    const char** ppExceptionsPaths = NULL;
    size_t exceptionsFileCount = 0;
    dr_test_exceptions exceptions;
    dr_test_input* pInputs = NULL;
    size_t inputCount = 0;
    char* pVectorsPath = NULL;
    dr_test_format format = dr_test_format_unknown;
    dr_bool32 selectedStages[2] = {0};
    dr_bool32 hasTests = DR_FALSE;
    dr_bool32 databaseOnly = DR_FALSE;
    dr_bool32 endOptions = DR_FALSE;
    unsigned int logFlags = 0;
    size_t iInput;
    size_t iStage;
    int iarg;
    int exitCode = 1;

    memset(&exceptions, 0, sizeof(exceptions));

    pInputs = (dr_test_input*)calloc((size_t)argc + sizeof(dr_test_formats) / sizeof(dr_test_formats[0]), sizeof(*pInputs));
    if (pInputs == NULL) {
        dr_errorf("Out of memory for test inputs.\n");
        goto done;
    }

    ppExceptionsPaths = (const char**)calloc((size_t)argc + 2, sizeof(*ppExceptionsPaths));
    if (ppExceptionsPaths == NULL) {
        dr_errorf("Out of memory for exception paths.\n");
        goto done;
    }

    for (iarg = 1; iarg < argc; iarg += 1) {
        const char* pArgument;

        pArgument = argv[iarg];
        if (!endOptions && pArgument[0] == '-') {
            if (strcmp(pArgument, "-h") == 0 || strcmp(pArgument, "--help") == 0) {
                dr_print_usage();
                exitCode = 0;
                goto done;
            } else if (strcmp(pArgument, "--") == 0) {
                endOptions = DR_TRUE;
            } else if (strcmp(pArgument, "--verbose") == 0) {
                logFlags |= dr_test_log_flag_verbose;
            } else if (strcmp(pArgument, "--audiovectors") == 0) {
                if (iarg + 1 == argc || argv[iarg + 1][0] == '\0' || argv[iarg + 1][0] == '-') {
                    dr_errorf("%s requires a value.\n", pArgument);
                    goto done;
                }

                iarg += 1;
                pAudiovectorsPath = argv[iarg];
            } else if (strcmp(pArgument, "--exceptions") == 0) {
                if (iarg + 1 == argc || argv[iarg + 1][0] == '\0' || argv[iarg + 1][0] == '-') {
                    dr_errorf("%s requires a value.\n", pArgument);
                    goto done;
                }

                iarg += 1;
                ppExceptionsPaths[exceptionsFileCount] = argv[iarg];
                exceptionsFileCount += 1;
            } else if (strcmp(pArgument, "--format") == 0) {
                if (iarg + 1 == argc || argv[iarg + 1][0] == '\0' || argv[iarg + 1][0] == '-') {
                    dr_errorf("%s requires a value.\n", pArgument);
                    goto done;
                }

                iarg += 1;
                format = dr_test_format_from_name(argv[iarg]);
                if (format == dr_test_format_unknown) {
                    dr_errorf("Unknown format: %s. Use wav, aiff, flac, or mp3.\n", argv[iarg]);
                    goto done;
                }
            } else if (strcmp(pArgument, "--test") == 0) {
                if (iarg + 1 == argc || argv[iarg + 1][0] == '\0' || argv[iarg + 1][0] == '-') {
                    dr_errorf("%s requires a value.\n", pArgument);
                    goto done;
                }

                iarg += 1;
                if (strcmp(argv[iarg], "decoding") == 0) {
                    selectedStages[dr_test_stage_decoding] = DR_TRUE;
                } else if (strcmp(argv[iarg], "seeking") == 0) {
                    selectedStages[dr_test_stage_seeking] = DR_TRUE;
                } else {
                    dr_errorf("Unknown test: %s. Use decoding or seeking.\n", argv[iarg]);
                    goto done;
                }

                hasTests = DR_TRUE;
            } else {
                dr_errorf("Unknown option: %s.\n", pArgument);
                goto done;
            }

            continue;
        }

        /* Only identical command-line inputs are deduplicated. Folder overlaps are intentional. */
        for (iInput = 0; iInput < inputCount; iInput += 1) {
            if (strcmp(pInputs[iInput].pArgument, pArgument) == 0) {
                break;
            }
        }

        if (iInput == inputCount) {
            pInputs[inputCount].pArgument = pArgument;
            inputCount += 1;
        }
    }

    if (!hasTests) {
        selectedStages[dr_test_stage_decoding] = DR_TRUE;
        selectedStages[dr_test_stage_seeking]  = DR_TRUE;
    }

    pVectorsPath = dr_test_path_append(pAudiovectorsPath, "vectors");
    if (pVectorsPath == NULL) {
        goto done;
    }

    if (inputCount == 0) {
        fs_file_info info;
        fs_result result;

        result = fs_info(NULL, pVectorsPath, FS_READ | FS_OPAQUE, &info);
        if (result != FS_SUCCESS || !info.directory) {
            dr_errorf("Cannot open vector directory: %s.\n", pVectorsPath);
            goto done;
        }

        databaseOnly = DR_TRUE;
        for (iInput = 0; iInput < sizeof(dr_test_formats) / sizeof(dr_test_formats[0]); iInput += 1) {
            pInputs[inputCount].pArgument = dr_test_formats[iInput].pName;
            inputCount += 1;
        }
    }

    for (iInput = 0; iInput < inputCount; iInput += 1) {
        if (dr_test_input_init(&pInputs[iInput], pVectorsPath, databaseOnly) != 0) {
            goto done;
        }
    }

    if (exceptionsFileCount == 0) {
        fs_file_info info;
        fs_result result;

        ppExceptionsPaths[exceptionsFileCount] = "tests/exceptions.txt";
        exceptionsFileCount += 1;

        result = fs_info(NULL, "tests/exceptions-private.txt", FS_READ | FS_OPAQUE, &info);
        if (result == FS_SUCCESS) {
            ppExceptionsPaths[exceptionsFileCount] = "tests/exceptions-private.txt";
            exceptionsFileCount += 1;
        } else if (result != FS_DOES_NOT_EXIST) {
            dr_errorf("Cannot access tests/exceptions-private.txt: %s.\n", fs_result_description(result));
            goto done;
        }
    }

    if (dr_test_exceptions_init(ppExceptionsPaths, exceptionsFileCount, &exceptions) != 0) {
        goto done;
    }

    exitCode = 0;
    for (iStage = 0; iStage < sizeof(stages) / sizeof(stages[0]); iStage += 1) {
        if (!selectedStages[stages[iStage]]) {
            continue;
        }

        if (stages[iStage] == dr_test_stage_decoding) {
            dr_printf("DECODING\n");
            dr_printf("========\n");
        } else {
            dr_printf("SEEKING\n");
            dr_printf("=======\n");
        }

        for (iInput = 0; iInput < inputCount; iInput += 1) {
            const dr_test_input* pInput;
            int result;

            pInput = &pInputs[iInput];
            if (pInput->pPath == NULL) {
                continue;
            }

            if (pInput->isDirectory) {
                result = dr_test_directory(pInput->pPath, pInput->pDisplayPath, pInput->isDatabase, format, stages[iStage], &exceptions, logFlags);
            } else {
                result = dr_test_file(pInput->pPath, pInput->pDisplayPath, pInput->isDatabase, format, stages[iStage], &exceptions, logFlags);
            }

            if (result != 0) {
                exitCode = 1;
            }
        }

        dr_printf("\n");
    }

done:
    dr_test_exceptions_uninit(&exceptions);
    for (iInput = 0; iInput < inputCount; iInput += 1) {
        free(pInputs[iInput].pPath);
    }

    free(pInputs);
    free(ppExceptionsPaths);
    free(pVectorsPath);

    return exitCode;
}

int main(int argc, char** argv)
{
    int exitCode;

    fs_file_open(NULL, FS_STDIN,  FS_READ,  &STDIN);
    fs_file_open(NULL, FS_STDOUT, FS_WRITE, &STDOUT);
    fs_file_open(NULL, FS_STDERR, FS_WRITE, &STDERR);

    exitCode = dr_main(argc, argv);

    fs_file_close(STDERR);
    fs_file_close(STDOUT);
    fs_file_close(STDIN);

    return exitCode;
}
