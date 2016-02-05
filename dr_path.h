// Public Domain. See "unlicense" statement at the end of this file.

// ABOUT
//
// dr_path is a simple path manipulation library. At it's core this is just a string manipulation library - it
// doesn't do anything with the actual file system such as checking whether or not a path points to an actual file
// or whatnot.
//
// Features:
// - It's made up of just one file with no dependencies except for the standard library.
// - Never uses the heap
// - Public domain
//
//
//
// USAGE
//
// This is a single-file library. To use it, do something like the following in one .c file.
//   #define DR_PATH_IMPLEMENTATION
//   #include "dr_path.h"
//
// You can then #include dr_path.h in other parts of the program as you would with any other header file.

#ifndef dr_path_h
#define dr_path_h

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


// Structure representing a section of a path.
typedef struct
{
    size_t offset;
    size_t length;

} drpath_segment;

// Structure used for iterating over a path while at the same time providing useful and easy-to-use information about the iteration.
typedef struct drpath_iterator
{
    const char* path;
    drpath_segment segment;

} drpath_iterator;



/// Compares a section of two strings for equality.
///
/// @param s0Path [in] The first path.
/// @param s0     [in] The segment of the first path to compare.
/// @param s1Path [in] The second path.
/// @param s1     [in] The segment of the second path to compare.
///
/// @return true if the strings are equal; false otherwise.
bool drpath_segments_equal(const char* s0Path, const drpath_segment s0, const char* s1Path, const drpath_segment s1);


/// Creates an iterator for iterating over each segment in a path.
///
/// @param path [in] The path whose segments are being iterated.
///
/// @return True if at least one segment is found; false if it's an empty path.
bool drpath_first(const char* path, drpath_iterator* i);

/// Creates an iterator beginning at the last segment.
bool drpath_last(const char* path, drpath_iterator* i);

/// Goes to the next segment in the path as per the given iterator.
///
/// @param i [in] A pointer to the iterator to increment.
///
/// @return True if the iterator contains a valid value. Use this to determine when to terminate iteration.
bool drpath_next(drpath_iterator* i);

/// Goes to the previous segment in the path.
///
/// @param i [in] A pointer to the iterator to decrement.
///
/// @return true if the iterator contains a valid value. Use this to determine when to terminate iteration.
bool drpath_prev(drpath_iterator* i);

/// Determines if the given iterator is at the end.
///
/// @param i [in] The iterator to check.
bool drpath_at_end(drpath_iterator i);

/// Determines if the given iterator is at the start.
///
/// @param i [in] The iterator to check.
bool drpath_at_start(drpath_iterator i);

/// Compares the string values of two iterators for equality.
///
/// @param i0 [in] The first iterator to compare.
/// @param i1 [in] The second iterator to compare.
///
/// @return true if the strings are equal; false otherwise.
bool drpath_iterators_equal(const drpath_iterator i0, const drpath_iterator i1);


/// Determines whether or not the given iterator refers to the root segment of a path.
bool drpath_is_root_segment(const drpath_iterator i);

/// Determines whether or not the given iterator refers to a Linux style root directory ("/")
bool drpath_is_linux_style_root_segment(const drpath_iterator i);

/// Determines whether or not the given iterator refers to a Windows style root directory.
bool drpath_is_win32_style_root_segment(const drpath_iterator i);


/// Converts the slashes in the given path to forward slashes.
///
/// @param path [in] The path whose slashes are being converted.
void drpath_to_forward_slashes(char* path);

/// Converts the slashes in the given path to back slashes.
///
/// @param path [in] The path whose slashes are being converted.
void drpath_to_backslashes(char* path);


/// Determines whether or not the given path is a decendant of another.
///
/// @param descendantAbsolutePath [in] The absolute path of the descendant.
/// @param parentAbsolutePath     [in] The absolute path of the parent.
///
/// @remarks
///     As an example, "C:/My/Folder" is a descendant of "C:/".
///     @par
///     If either path contains "." or "..", clean it with drpath_clean() before calling this.
bool drpath_is_descendant(const char* descendantAbsolutePath, const char* parentAbsolutePath);

/// Determines whether or not the given path is a direct child of another.
///
/// @param childAbsolutePath [in] The absolute of the child.
/// @param parentAbsolutePath [in] The absolute path of the parent.
///
/// @remarks
///     As an example, "C:/My/Folder" is NOT a child of "C:/" - it is a descendant. Alternatively, "C:/My" IS a child of "C:/".
///     @par
///     If either path contains "." or "..", clean it with drpath_clean() before calling this.
bool drpath_is_child(const char* childAbsolutePath, const char* parentAbsolutePath);


/// Modifies the given path by transforming it into it's base path.
void drpath_base_path(char* path);

/// Retrieves the base path from the given path, not including the trailing slash.
///
/// @param path            [in]  The full path.
/// @param baseOut         [out] A pointer to the buffer that will receive the base path.
/// @param baseSizeInBytes [in]  The size in bytes of the buffer that will receive the base directory.
///
/// @remarks
///     As an example, when "path" is "C:/MyFolder/MyFile", the output will be "C:/MyFolder". Note that there is no trailing slash.
///     @par
///     If "path" is something like "/MyFolder", the return value will be an empty string.
void drpath_copy_base_path(const char* path, char* baseOut, size_t baseSizeInBytes);

/// Finds the file name portion of the path.
///
/// @param path [in] The path to search.
///
/// @return A pointer to the beginning of the string containing the file name. If this is non-null, it will be an offset of "path".
///
/// @remarks
///     A path with a trailing slash will return an empty string.
///     @par
///     The return value is just an offset of "path".
const char* drpath_file_name(const char* path);

/// Copies the file name into the given buffer.
const char* drpath_copy_file_name(const char* path, char* fileNameOut, size_t fileNameSizeInBytes);

/// Finds the file extension of the given file path.
///
/// @param path [in] The path to search.
///
/// @return A pointer to the beginning of the string containing the file's extension.
///
/// @remarks
///     A path with a trailing slash will return an empty string.
///     @par
///     The return value is just an offset of "path".
///     @par
///     On a path such as "filename.ext1.ext2" the returned string will be "ext2".
const char* drpath_extension(const char* path);


/// Checks whether or not the two paths are equal.
///
/// @param path1 [in] The first path.
/// @param path2 [in] The second path.
///
/// @return true if the paths are equal, false otherwise.
///
/// @remarks
///     This is case-sensitive.
///     @par
///     This is more than just a string comparison. Rather, this splits the path and compares each segment. The path "C:/My/Folder" is considered
///     equal to to "C:\\My\\Folder".
bool drpath_equal(const char* path1, const char* path2);

/// Checks if the extension of the given path is equal to the given extension.
///
/// @remarks
///     By default this is NOT case-sensitive, however if the standard library is disable, it is case-sensitive.
bool drpath_extension_equal(const char* path, const char* extension);


/// Determines whether or not the given path is relative.
///
/// @param path [in] The path to check.
bool drpath_is_relative(const char* path);

/// Determines whether or not the given path is absolute.
///
/// @param path [in] The path to check.
bool drpath_is_absolute(const char* path);


/// Appends two paths together, ensuring there is not double up on the last slash.
///
/// @param base                  [in, out] The base path that is being appended to.
/// @param baseBufferSizeInBytes [in]      The size of the buffer pointed to by "base", in bytes.
/// @param other                 [in]      The other path segment.
///
/// @remarks
///     This assumes both paths are well formed and "other" is a relative path.
bool drpath_append(char* base, size_t baseBufferSizeInBytes, const char* other);

/// Appends an iterator object to the given base path.
bool drpath_append_iterator(char* base, size_t baseBufferSizeInBytes, drpath_iterator i);

/// Appends an extension to the given path.
bool drpath_append_extension(char* base, size_t baseBufferSizeInBytes, const char* extension);

/// Appends two paths together, and copyies them to a separate buffer.
///
/// @param dst            [out] The destination buffer.
/// @param dstSizeInBytes [in]  The size of the buffer pointed to by "dst", in bytes.
/// @param base           [in]  The base directory.
/// @param other          [in]  The relative path to append to "base".
///
/// @return true if the paths were appended successfully; false otherwise.
///
/// @remarks
///     This assumes both paths are well formed and "other" is a relative path.
bool drpath_copy_and_append(char* dst, size_t dstSizeInBytes, const char* base, const char* other);

/// Appends a base path and an iterator together, and copyies them to a separate buffer.
///
/// @param dst            [out] The destination buffer.
/// @param dstSizeInBytes [in]  The size of the buffer pointed to by "dst", in bytes.
/// @param base           [in]  The base directory.
/// @param i              [in]  The iterator to append.
///
/// @return true if the paths were appended successfully; false otherwise.
///
/// @remarks
///     This assumes both paths are well formed and "i" is a valid iterator.
bool drpath_copy_and_append_iterator(char* dst, size_t dstSizeInBytes, const char* base, drpath_iterator i);

/// Appends an extension to the given base path and copies them to a separate buffer.
/// @param dst            [out] The destination buffer.
/// @param dstSizeInBytes [in]  The size of the buffer pointed to by "dst", in bytes.
/// @param base           [in]  The base directory.
/// @param extension      [in]  The relative path to append to "base".
///
/// @return true if the paths were appended successfully; false otherwise.
bool drpath_copy_and_append_extension(char* dst, size_t dstSizeInBytes, const char* base, const char* extension);


/// Cleans the path and resolves the ".." and "." segments.
///
/// @param path               [in]  The path to clean.
/// @param pathOut            [out] A pointer to the buffer that will receive the path.
/// @param pathOutSizeInBytes [in]  The size of the buffer pointed to by pathOut, in bytes.
///
/// @return The number of bytes written to the output buffer, including the null terminator.
///
/// @remarks
///     The output path will never be longer than the input path.
///     @par
///     The output buffer should never overlap with the input path.
///     @par
///     As an example, the path "my/messy/../path" will result in "my/path"
///     @par
///     The path "my/messy/../../../path" (note how there are too many ".." segments) will return "path" (the extra ".." segments will be dropped.)
///     @par
///     If an error occurs, such as an invalid input path, 0 will be returned.
size_t drpath_clean(const char* path, char* pathOut, size_t pathOutSizeInBytes);

/// Appends one path to the other and then cleans it.
size_t drpath_append_and_clean(char* dst, size_t dstSizeInBytes, const char* base, const char* other);


/// Removes the extension from the given path.
///
/// @remarks
///     If the given path does not have an extension, 1 will be returned, but the string will be left unmodified.
bool drpath_remove_extension(char* path);

/// Creates a copy of the given string and removes the extension.
bool drpath_copy_and_remove_extension(char* dst, size_t dstSizeInBytes, const char* path);


/// Removes the last segment from the given path.
bool drpath_remove_file_name(char* path);

/// Creates a copy of the given string and removes the extension.
bool drpath_copy_and_remove_file_name(char* dst, size_t dstSizeInBytes, const char* path);


/// Converts an absolute path to a relative path.
///
/// @return true if the conversion was successful; false if there was an error.
///
/// @remarks
///     This will normalize every slash to forward slashes.
bool drpath_to_relative(const char* absolutePathToMakeRelative, const char* absolutePathToMakeRelativeTo, char* relativePathOut, size_t relativePathOutSizeInBytes);

/// Converts a relative path to an absolute path based on a base path.
///
/// @return true if the conversion was successful; false if there was an error.
///
/// @remarks
///     This is equivalent to an append followed by a clean. Slashes will be normalized to forward slashes.
bool drpath_to_absolute(const char* relativePathToMakeAbsolute, const char* basePath, char* absolutePathOut, size_t absolutePathOutSizeInBytes);


#ifdef __cplusplus
}
#endif

#endif  //dr_path_h



///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////
#ifdef DR_PATH_IMPLEMENTATION
#include <string.h>
#include <ctype.h>
#include <errno.h>

static int drpath_strcpy(char* dst, size_t dstSizeInBytes, const char* src)
{
#if defined(_MSC_VER)
    return strcpy_s(dst, dstSizeInBytes, src);
#else
    if (dst == 0) {
        return EINVAL;
    }
    if (dstSizeInBytes == 0) {
        return ERANGE;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }

    size_t i;
    for (i = 0; i < dstSizeInBytes && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (i < dstSizeInBytes) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return ERANGE;
#endif
}

static int drpath_strncpy(char* dst, size_t dstSizeInBytes, const char* src, size_t count)
{
#if defined(_MSC_VER)
    return strncpy_s(dst, dstSizeInBytes, src, count);
#else
    if (dst == 0) {
        return EINVAL;
    }
    if (dstSizeInBytes == 0) {
        return EINVAL;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }

    size_t maxcount = count;
    if (count == ((size_t)-1) || count >= dstSizeInBytes) {        // -1 = _TRUNCATE
        maxcount = dstSizeInBytes - 1;
    }

    size_t i;
    for (i = 0; i < maxcount && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (src[i] == '\0' || i == count || count == ((size_t)-1)) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return ERANGE;
#endif
}


bool drpath_first(const char* path, drpath_iterator* i)
{
    if (path == 0 || path[0] == '\0' || i == 0) {
        return false;
    }

    i->path = path;
    i->segment.offset = 0;
    i->segment.length = 0;

    while (i->path[i->segment.length] != '\0' && (i->path[i->segment.length] != '/' && i->path[i->segment.length] != '\\')) {
        i->segment.length += 1;
    }

    return true;
}

bool drpath_last(const char* path, drpath_iterator* i)
{
    if (path == 0 || path[0] == '\0' || i == 0) {
        return false;
    }

    i->path = path;
    i->segment.offset = strlen(path);
    i->segment.length = 0;

    return drpath_prev(i);
}

bool drpath_next(drpath_iterator* i)
{
    if (i == NULL || i->path == NULL) {
        return false;
    }

    i->segment.offset = i->segment.offset + i->segment.length;
    i->segment.length = 0;

    while (i->path[i->segment.offset] != '\0' && (i->path[i->segment.offset] == '/' || i->path[i->segment.offset] == '\\')) {
        i->segment.offset += 1;
    }

    if (i->path[i->segment.offset] == '\0') {
        return false;
    }


    while (i->path[i->segment.offset + i->segment.length] != '\0' && (i->path[i->segment.offset + i->segment.length] != '/' && i->path[i->segment.offset + i->segment.length] != '\\')) {
        i->segment.length += 1;
    }

    return true;
}

bool drpath_prev(drpath_iterator* i)
{
    if (i == NULL || i->path == NULL || i->segment.offset == 0) {
        return false;
    }

    i->segment.length = 0;

    do
    {
        i->segment.offset -= 1;
    } while (i->segment.offset > 0 && (i->path[i->segment.offset] == '/' || i->path[i->segment.offset] == '\\'));

    if (i->segment.offset == 0) {
        if (i->path[i->segment.offset] == '/' || i->path[i->segment.offset] == '\\') {
            i->segment.length = 0;
            return true;
        }

        return false;
    }


    size_t offsetEnd = i->segment.offset + 1;
    while (i->segment.offset > 0 && (i->path[i->segment.offset] != '/' && i->path[i->segment.offset] != '\\')) {
        i->segment.offset -= 1;
    }

    if (i->path[i->segment.offset] == '/' || i->path[i->segment.offset] == '\\') {
        i->segment.offset += 1;
    }


    i->segment.length = offsetEnd - i->segment.offset;

    return true;
}

bool drpath_at_end(drpath_iterator i)
{
    return i.path == 0 || i.path[i.segment.offset] == '\0';
}

bool drpath_at_start(drpath_iterator i)
{
    return i.path != 0 && i.segment.offset == 0;
}

bool drpath_iterators_equal(const drpath_iterator i0, const drpath_iterator i1)
{
    return drpath_segments_equal(i0.path, i0.segment, i1.path, i1.segment);
}

bool drpath_segments_equal(const char* s0Path, const drpath_segment s0, const char* s1Path, const drpath_segment s1)
{
    if (s0Path == NULL || s1Path == NULL) {
        return false;
    }

    if (s0.length != s1.length) {
        return false;
    }

    return strncmp(s0Path + s0.offset, s1Path + s1.offset, s0.length) == 0;
}


bool drpath_is_root_segment(const drpath_iterator i)
{
    return drpath_is_linux_style_root_segment(i) || drpath_is_win32_style_root_segment(i);
}

bool drpath_is_linux_style_root_segment(const drpath_iterator i)
{
    if (i.path == NULL) {
        return false;
    }

    if (i.segment.offset == 0 && i.segment.length == 0) {
        return true;    // "/" style root.
    }

    return false;
}

bool drpath_is_win32_style_root_segment(const drpath_iterator i)
{
    if (i.path == NULL) {
        return false;
    }

    if (i.segment.offset == 0 && i.segment.length == 2) {
        if (((i.path[0] >= 'a' && i.path[0] <= 'z') || (i.path[0] >= 'A' && i.path[0] <= 'Z')) && i.path[1] == ':') {
            return true;
        }
    }

    return false;
}


void drpath_to_forward_slashes(char* path)
{
    if (path == NULL) {
        return;
    }

    while (path[0] != '\0') {
        if (path[0] == '\\') {
            path[0] = '/';
        }

        path += 1;
    }
}

void drpath_to_backslashes(char* path)
{
    if (path == NULL) {
        return;
    }

    while (path[0] != '\0') {
        if (path[0] == '/') {
            path[0] = '\\';
        }

        path += 1;
    }
}


bool drpath_is_descendant(const char* descendantAbsolutePath, const char* parentAbsolutePath)
{
    drpath_iterator iChild;
    if (!drpath_first(descendantAbsolutePath, &iChild)) {
        return false;   // The descendant is an empty string which makes it impossible for it to be a descendant.
    }

    drpath_iterator iParent;
    if (drpath_first(parentAbsolutePath, &iParent))
    {
        do
        {
            // If the segment is different, the paths are different and thus it is not a descendant.
            if (!drpath_iterators_equal(iParent, iChild)) {
                return false;
            }

            if (!drpath_next(&iChild)) {
                return false;   // The descendant is shorter which means it's impossible for it to be a descendant.
            }

        } while (drpath_next(&iParent));
    }

    return true;
}

bool drpath_is_child(const char* childAbsolutePath, const char* parentAbsolutePath)
{
    drpath_iterator iChild;
    if (!drpath_first(childAbsolutePath, &iChild)) {
        return false;   // The descendant is an empty string which makes it impossible for it to be a descendant.
    }

    drpath_iterator iParent;
    if (drpath_first(parentAbsolutePath, &iParent))
    {
        do
        {
            // If the segment is different, the paths are different and thus it is not a descendant.
            if (!drpath_iterators_equal(iParent, iChild)) {
                return false;
            }

            if (!drpath_next(&iChild)) {
                return false;   // The descendant is shorter which means it's impossible for it to be a descendant.
            }

        } while (drpath_next(&iParent));
    }

    // At this point we have finished iteration of the parent, which should be shorter one. We now do one more iterations of
    // the child to ensure it is indeed a direct child and not a deeper descendant.
    return !drpath_next(&iChild);
}

void drpath_base_path(char* path)
{
    if (path == NULL) {
        return;
    }

    char* baseend = path;

    // We just loop through the path until we find the last slash.
    while (path[0] != '\0') {
        if (path[0] == '/' || path[0] == '\\') {
            baseend = path;
        }

        path += 1;
    }


    // Now we just loop backwards until we hit the first non-slash.
    while (baseend > path) {
        if (baseend[0] != '/' && baseend[0] != '\\') {
            break;
        }

        baseend -= 1;
    }


    // We just put a null terminator on the end.
    baseend[0] = '\0';
}

void drpath_copy_base_path(const char* path, char* baseOut, size_t baseSizeInBytes)
{
    if (path == NULL || baseOut == NULL || baseSizeInBytes == 0) {
        return;
    }

    drpath_strcpy(baseOut, baseSizeInBytes, path);
    drpath_base_path(baseOut);
}

const char* drpath_file_name(const char* path)
{
    if (path == NULL) {
        return NULL;
    }

    const char* fileName = path;

    // We just loop through the path until we find the last slash.
    while (path[0] != '\0') {
        if (path[0] == '/' || path[0] == '\\') {
            fileName = path;
        }

        path += 1;
    }

    // At this point the file name is sitting on a slash, so just move forward.
    while (fileName[0] != '\0' && (fileName[0] == '/' || fileName[0] == '\\')) {
        fileName += 1;
    }

    return fileName;
}

const char* drpath_copy_file_name(const char* path, char* fileNameOut, size_t fileNameSizeInBytes)
{
    const char* fileName = drpath_file_name(path);
    if (fileName != NULL) {
        drpath_strcpy(fileNameOut, fileNameSizeInBytes, fileName);
    }

    return fileName;
}

const char* drpath_extension(const char* path)
{
    if (path == NULL) {
        return NULL;
    }

    const char* extension     = drpath_file_name(path);
    const char* lastoccurance = 0;

    // Just find the last '.' and return.
    while (extension[0] != '\0')
    {
        extension += 1;

        if (extension[0] == '.') {
            extension    += 1;
            lastoccurance = extension;
        }
    }

    return (lastoccurance != 0) ? lastoccurance : extension;
}


bool drpath_equal(const char* path1, const char* path2)
{
    if (path1 == NULL || path2 == NULL) {
        return false;
    }

    if (path1 == path2 || (path1[0] == '\0' && path2[0] == '\0')) {
        return true;    // Two empty paths are treated as the same.
    }

    drpath_iterator iPath1;
    drpath_iterator iPath2;
    if (drpath_first(path1, &iPath1) && drpath_first(path2, &iPath2))
    {
        bool isPath1Valid;
        bool isPath2Valid;

        do
        {
            if (!drpath_iterators_equal(iPath1, iPath2)) {
                return false;
            }

            isPath1Valid = drpath_next(&iPath1);
            isPath2Valid = drpath_next(&iPath2);

        } while (isPath1Valid && isPath2Valid);

        // At this point either iPath1 and/or iPath2 have finished iterating. If both of them are at the end, the two paths are equal.
        return isPath1Valid == isPath2Valid && iPath1.path[iPath1.segment.offset] == '\0' && iPath2.path[iPath2.segment.offset] == '\0';
    }

    return false;
}

bool drpath_extension_equal(const char* path, const char* extension)
{
    if (path == NULL || extension == NULL) {
        return false;
    }

    const char* ext1 = extension;
    const char* ext2 = drpath_extension(path);

#ifdef _MSC_VER
    return _stricmp(ext1, ext2) == 0;
#else
    return strcasecmp(ext1, ext2) == 0;
#endif
}



bool drpath_is_relative(const char* path)
{
    if (path == NULL) {
        return false;
    }

    drpath_iterator seg;
    if (drpath_first(path, &seg)) {
        return !drpath_is_root_segment(seg);
    }

    // We'll get here if the path is empty. We consider this to be a relative path.
    return true;
}

bool drpath_is_absolute(const char* path)
{
    return !drpath_is_relative(path);
}


bool drpath_append(char* base, size_t baseBufferSizeInBytes, const char* other)
{
    if (base == NULL || other == NULL) {
        return false;
    }

    size_t path1Length = strlen(base);
    size_t path2Length = strlen(other);

    if (path1Length >= baseBufferSizeInBytes) {
        return false;
    }


    // Slash.
    if (path1Length > 0 && base[path1Length - 1] != '/' && base[path1Length - 1] != '\\') {
        base[path1Length] = '/';
        path1Length += 1;
    }

    // Other part.
    if (path1Length + path2Length >= baseBufferSizeInBytes) {
        path2Length = baseBufferSizeInBytes - path1Length - 1;      // -1 for the null terminator.
    }

    drpath_strncpy(base + path1Length, baseBufferSizeInBytes - path1Length, other, path2Length);

    return true;
}

bool drpath_append_iterator(char* base, size_t baseBufferSizeInBytes, drpath_iterator i)
{
    if (base == NULL) {
        return false;
    }

    size_t path1Length = strlen(base);
    size_t path2Length = i.segment.length;

    if (path1Length >= baseBufferSizeInBytes) {
        return false;
    }

    // Slash.
    if (path1Length > 0 && base[path1Length - 1] != '/' && base[path1Length - 1] != '\\') {
        base[path1Length] = '/';
        path1Length += 1;
    }

    // Other part.
    if (path1Length + path2Length >= baseBufferSizeInBytes) {
        path2Length = baseBufferSizeInBytes - path1Length - 1;      // -1 for the null terminator.
    }

    drpath_strncpy(base + path1Length, baseBufferSizeInBytes - path1Length, i.path + i.segment.offset, path2Length);

    return true;
}

bool drpath_append_extension(char* base, size_t baseBufferSizeInBytes, const char* extension)
{
    if (base == NULL || extension == NULL) {
        return false;
    }

    size_t baseLength = strlen(base);
    size_t extLength  = strlen(extension);

    if (baseLength >= baseBufferSizeInBytes) {
        return false;
    }

    base[baseLength] = '.';
    baseLength += 1;

    if (baseLength + extLength >= baseBufferSizeInBytes) {
        extLength = baseBufferSizeInBytes - baseLength - 1;      // -1 for the null terminator.
    }

    drpath_strncpy(base + baseLength, baseBufferSizeInBytes - baseLength, extension, extLength);

    return true;
}

bool drpath_copy_and_append(char* dst, size_t dstSizeInBytes, const char* base, const char* other)
{
    if (dst == NULL || dstSizeInBytes == 0) {
        return false;
    }

    drpath_strcpy(dst, dstSizeInBytes, base);
    return drpath_append(dst, dstSizeInBytes, other);
}

bool drpath_copy_and_append_iterator(char* dst, size_t dstSizeInBytes, const char* base, drpath_iterator i)
{
    if (dst == NULL || dstSizeInBytes == 0) {
        return false;
    }

    drpath_strcpy(dst, dstSizeInBytes, base);
    return drpath_append_iterator(dst, dstSizeInBytes, i);
}

bool drpath_copy_and_append_extension(char* dst, size_t dstSizeInBytes, const char* base, const char* extension)
{
    if (dst == NULL || dstSizeInBytes == 0) {
        return false;
    }

    drpath_strcpy(dst, dstSizeInBytes, base);
    return drpath_append_extension(dst, dstSizeInBytes, extension);
}



// This function recursively cleans a path which is defined as a chain of iterators. This function works backwards, which means at the time of calling this
// function, each iterator in the chain should be placed at the end of the path.
//
// This does not write the null terminator, nor a leading slash for absolute paths.
size_t _drpath_clean_trywrite(drpath_iterator* iterators, unsigned int iteratorCount, char* pathOut, size_t pathOutSizeInBytes, unsigned int ignoreCounter)
{
    if (iteratorCount == 0) {
        return 0;
    }

    drpath_iterator isegment = iterators[iteratorCount - 1];


    // If this segment is a ".", we ignore it. If it is a ".." we ignore it and increment "ignoreCount".
    int ignoreThisSegment = ignoreCounter > 0 && isegment.segment.length > 0;

    if (isegment.segment.length == 1 && isegment.path[isegment.segment.offset] == '.')
    {
        // "."
        ignoreThisSegment = 1;
    }
    else
    {
        if (isegment.segment.length == 2 && isegment.path[isegment.segment.offset] == '.' && isegment.path[isegment.segment.offset + 1] == '.')
        {
            // ".."
            ignoreThisSegment = 1;
            ignoreCounter += 1;
        }
        else
        {
            // It's a regular segment, so decrement the ignore counter.
            if (ignoreCounter > 0) {
                ignoreCounter -= 1;
            }
        }
    }


    // The previous segment needs to be written before we can write this one.
    size_t bytesWritten = 0;

    drpath_iterator prev = isegment;
    if (!drpath_prev(&prev))
    {
        if (iteratorCount > 1)
        {
            iteratorCount -= 1;
            prev = iterators[iteratorCount - 1];
        }
        else
        {
            prev.path           = NULL;
            prev.segment.offset = 0;
            prev.segment.length = 0;
        }
    }

    if (prev.segment.length > 0)
    {
        iterators[iteratorCount - 1] = prev;
        bytesWritten = _drpath_clean_trywrite(iterators, iteratorCount, pathOut, pathOutSizeInBytes, ignoreCounter);
    }


    if (!ignoreThisSegment)
    {
        if (pathOutSizeInBytes > 0)
        {
            pathOut            += bytesWritten;
            pathOutSizeInBytes -= bytesWritten;

            if (bytesWritten > 0)
            {
                pathOut[0] = '/';
                bytesWritten += 1;

                pathOut            += 1;
                pathOutSizeInBytes -= 1;
            }

            if (pathOutSizeInBytes >= isegment.segment.length)
            {
                drpath_strncpy(pathOut, pathOutSizeInBytes, isegment.path + isegment.segment.offset, isegment.segment.length);
                bytesWritten += isegment.segment.length;
            }
            else
            {
                drpath_strncpy(pathOut, pathOutSizeInBytes, isegment.path + isegment.segment.offset, pathOutSizeInBytes);
                bytesWritten += pathOutSizeInBytes;
            }
        }
    }

    return bytesWritten;
}

size_t drpath_clean(const char* path, char* pathOut, size_t pathOutSizeInBytes)
{
    if (path == NULL) {
        return 0;
    }

    drpath_iterator last;
    if (drpath_last(path, &last))
    {
        size_t bytesWritten = 0;
        if (path[0] == '/') {
            if (pathOut != NULL && pathOutSizeInBytes > 1) {
                pathOut[0] = '/';
                bytesWritten = 1;
            }
        }

        bytesWritten += _drpath_clean_trywrite(&last, 1, pathOut + bytesWritten, pathOutSizeInBytes - bytesWritten - 1, 0);  // -1 to ensure there is enough room for a null terminator later on.
        if (pathOutSizeInBytes > bytesWritten) {
            pathOut[bytesWritten] = '\0';
        }

        return bytesWritten + 1;
    }

    return 0;
}

size_t drpath_append_and_clean(char* dst, size_t dstSizeInBytes, const char* base, const char* other)
{
    if (base == NULL || other == NULL) {
        return 0;
    }

    drpath_iterator last[2];
    bool isPathEmpty0 = !drpath_last(base,  last + 0);
    bool isPathEmpty1 = !drpath_last(other, last + 1);

    if (isPathEmpty0 && isPathEmpty1) {
        return 0;   // Both input strings are empty.
    }

    size_t bytesWritten = 0;
    if (base[0] == '/') {
        if (dst != NULL && dstSizeInBytes > 1) {
            dst[0] = '/';
            bytesWritten = 1;
        }
    }

    bytesWritten += _drpath_clean_trywrite(last, 2, dst + bytesWritten, dstSizeInBytes - bytesWritten - 1, 0);  // -1 to ensure there is enough room for a null terminator later on.
    if (dstSizeInBytes > bytesWritten) {
        dst[bytesWritten] = '\0';
    }

    return bytesWritten + 1;
}


bool drpath_remove_extension(char* path)
{
    if (path == NULL) {
        return false;
    }

    const char* extension = drpath_extension(path);
    if (extension != NULL) {
        extension -= 1;
        path[(extension - path)] = '\0';
    }

    return true;
}

bool drpath_copy_and_remove_extension(char* dst, size_t dstSizeInBytes, const char* path)
{
    if (dst == NULL || dstSizeInBytes == 0 || path == NULL) {
        return false;
    }

    const char* extension = drpath_extension(path);
    if (extension != NULL) {
        extension -= 1;
    }

    drpath_strncpy(dst, dstSizeInBytes, path, (size_t)(extension - path));
    return true;
}


bool drpath_remove_file_name(char* path)
{
    if (path == NULL) {
        return false;
    }


    // We just create an iterator that starts at the last segment. We then move back one and place a null terminator at the end of
    // that segment. That will ensure the resulting path is not left with a slash.
    drpath_iterator iLast;
    if (!drpath_last(path, &iLast)) {
        return false;   // The path is empty.
    }

    if (drpath_is_root_segment(iLast)) {
        return false;
    }


    drpath_iterator iSecondLast = iLast;
    if (drpath_prev(&iSecondLast))
    {
        // There is a segment before it so we just place a null terminator at the end, but only if it's not the root of a Linux-style absolute path.
        if (drpath_is_linux_style_root_segment(iSecondLast)) {
            path[iLast.segment.offset] = '\0';
        } else {
            path[iSecondLast.segment.offset + iSecondLast.segment.length] = '\0';
        }
    }
    else
    {
        // This is already the last segment, so just place a null terminator at the beginning of the string.
        path[0] = '\0';
    }

    return true;
}

bool drpath_copy_and_remove_file_name(char* dst, size_t dstSizeInBytes, const char* path)
{
    if (dst == NULL) {
        return false;
    }
    if (dstSizeInBytes == 0) {
        return false;
    }
    if (path == NULL) {
        dst[0] = '\0';
        return false;
    }


    // We just create an iterator that starts at the last segment. We then move back one and place a null terminator at the end of
    // that segment. That will ensure the resulting path is not left with a slash.
    drpath_iterator iLast;
    if (!drpath_last(path, &iLast)) {
        dst[0] = '\0';
        return false;   // The path is empty.
    }

    if (drpath_is_linux_style_root_segment(iLast)) {
        if (dstSizeInBytes > 1) {
            dst[0] = '/'; dst[1] = '\0';
            return false;
        } else {
            dst[0] = '\0';
            return false;
        }
    }

    if (drpath_is_win32_style_root_segment(iLast)) {
        return drpath_strncpy(dst, dstSizeInBytes, path + iLast.segment.offset, iLast.segment.length) == 0;
    }


    drpath_iterator iSecondLast = iLast;
    if (drpath_prev(&iSecondLast))
    {
        // There is a segment before it so we just place a null terminator at the end, but only if it's not the root of a Linux-style absolute path.
        if (drpath_is_linux_style_root_segment(iSecondLast)) {
            return drpath_strcpy(dst, dstSizeInBytes, "/");
        } else {
            return drpath_strncpy(dst, dstSizeInBytes, path, iSecondLast.segment.offset + iSecondLast.segment.length) == 0;
        }
    }
    else
    {
        // This is already the last segment, so just place a null terminator at the beginning of the string.
        dst[0] = '\0';
        return true;
    }
}


bool drpath_to_relative(const char* absolutePathToMakeRelative, const char* absolutePathToMakeRelativeTo, char* relativePathOut, size_t relativePathOutSizeInBytes)
{
    // We do this in to phases. The first phase just iterates past each segment of both the path to convert and the
    // base path until we find two that are not equal. The second phase just adds the appropriate ".." segments.

    if (relativePathOut == 0) {
        return false;
    }
    if (relativePathOutSizeInBytes == 0) {
        return false;
    }


    drpath_iterator iPath;
    drpath_iterator iBase;
    bool isPathEmpty = !drpath_first(absolutePathToMakeRelative, &iPath);
    bool isBaseEmpty = !drpath_first(absolutePathToMakeRelativeTo, &iBase);

    if (isPathEmpty && isBaseEmpty)
    {
        // Looks like both paths are empty.
        relativePathOut[0] = '\0';
        return false;
    }


    // Phase 1: Get past the common section.
    int isPathAtEnd = 0;
    int isBaseAtEnd = 0;
    while (!isPathAtEnd && !isBaseAtEnd && drpath_iterators_equal(iPath, iBase))
    {
        isPathAtEnd = !drpath_next(&iPath);
        isBaseAtEnd = !drpath_next(&iBase);
    }

    if (iPath.segment.offset == 0)
    {
        // The path is not relative to the base path.
        relativePathOut[0] = '\0';
        return false;
    }


    // Phase 2: Append ".." segments - one for each remaining segment in the base path.
    char* pDst = relativePathOut;
    size_t bytesAvailable = relativePathOutSizeInBytes;

    if (!drpath_at_end(iBase))
    {
        do
        {
            if (pDst != relativePathOut)
            {
                if (bytesAvailable <= 3)
                {
                    // Ran out of room.
                    relativePathOut[0] = '\0';
                    return false;
                }

                pDst[0] = '/';
                pDst[1] = '.';
                pDst[2] = '.';

                pDst += 3;
                bytesAvailable -= 3;
            }
            else
            {
                // It's the first segment, so we need to ensure we don't lead with a slash.
                if (bytesAvailable <= 2)
                {
                    // Ran out of room.
                    relativePathOut[0] = '\0';
                    return false;
                }

                pDst[0] = '.';
                pDst[1] = '.';

                pDst += 2;
                bytesAvailable -= 2;
            }
        } while (drpath_next(&iBase));
    }


    // Now we just append whatever is left of the main path. We want the path to be clean, so we append segment-by-segment.
    if (!drpath_at_end(iPath))
    {
        do
        {
            // Leading slash, if required.
            if (pDst != relativePathOut)
            {
                if (bytesAvailable <= 1)
                {
                    // Ran out of room.
                    relativePathOut[0] = '\0';
                    return false;
                }

                pDst[0] = '/';

                pDst += 1;
                bytesAvailable -= 1;
            }


            if (bytesAvailable <= iPath.segment.length)
            {
                // Ran out of room.
                relativePathOut[0] = '\0';
                return false;
            }

            if (drpath_strncpy(pDst, bytesAvailable, iPath.path + iPath.segment.offset, iPath.segment.length) != 0)
            {
                // Error copying the string. Probably ran out of room in the output buffer.
                relativePathOut[0] = '\0';
                return false;
            }

            pDst += iPath.segment.length;
            bytesAvailable -= iPath.segment.length;


        } while (drpath_next(&iPath));
    }


    // Always null terminate.
    //assert(bytesAvailable > 0);
    pDst[0] = '\0';

    return true;
}

bool drpath_to_absolute(const char* relativePathToMakeAbsolute, const char* basePath, char* absolutePathOut, size_t absolutePathOutSizeInBytes)
{
    return drpath_append_and_clean(absolutePathOut, absolutePathOutSizeInBytes, basePath, relativePathToMakeAbsolute) != 0;
}
#endif  //DR_PATH_IMPLEMENTATION

/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/
