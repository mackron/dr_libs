// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#ifndef easy_path
#define easy_path

#ifdef __cplusplus
extern "C" {
#endif


// Set this to 0 to avoid using the standard library. This adds a dependency, however it is likely more efficient for things like
// finding the length of a null-terminated string.
#define EASYPATH_USE_STDLIB     1


// Structure representing a section of a path.
typedef struct 
{
    unsigned int offset;
    unsigned int length;

} easypath_segment;

// Structure used for iterating over a path while at the same time providing useful and easy-to-use information about the iteration.
typedef struct easypath_iterator 
{
    const char* path;
    easypath_segment segment;

} easypath_iterator;



/// Compares a section of two strings for equality.
///
/// @param s0Path [in] The first path.
/// @param s0     [in] The segment of the first path to compare.
/// @param s1Path [in] The second path.
/// @param s1     [in] The segment of the second path to compare.
///
/// @return 1 if the strings are equal; 0 otherwise.
int easypath_segments_equal(const char* s0Path, const easypath_segment s0, const char* s1Path, const easypath_segment s1);


/// Creates an iterator for iterating over each segment in a path.
///
/// @param path [in] The path whose segments are being iterated.
///
/// @return An iterator object that is passed to easypath_next().
easypath_iterator easypath_first(const char* path);

/// Creates an iterator beginning at the last segment.
easypath_iterator easypath_last(const char* path);

/// Goes to the next segment in the path as per the given iterator.
///
/// @param i [in] A pointer to the iterator to increment.
///
/// @return True if the iterator contains a valid value. Use this to determine when to terminate iteration.
int easypath_next(easypath_iterator* i);

/// Goes to the previous segment in the path.
///
/// @param i [in] A pointer to the iterator to decrement.
///
/// @return 1 if the iterator contains a valid value. Use this to determine when to terminate iteration.
int easypath_prev(easypath_iterator* i);

/// Determines if the given iterator is at the end.
///
/// @param i [in] The iterator to check.
int easypath_atend(easypath_iterator i);

/// Determines if the given iterator is at the start.
///
/// @param i [in] The iterator to check.
int easypath_atstart(easypath_iterator i);

/// Compares the string values of two iterators for equality.
///
/// @param i0 [in] The first iterator to compare.
/// @param i1 [in] The second iterator to compare.
///
/// @return 1 if the strings are equal; 0 otherwise.
int easypath_iterators_equal(const easypath_iterator i0, const easypath_iterator i1);


/// Converts the slashes in the given path to forward slashes.
///
/// @param path [in] The path whose slashes are being converted.
void easypath_toforwardslashes(char* path);

/// Converts the slashes in the given path to back slashes.
///
/// @param path [in] The path whose slashes are being converted.
void easypath_tobackslashes(char* path);


/// Determines whether or not the given path is a decendant of another.
///
/// @param descendantAbsolutePath [in] The absolute path of the descendant.
/// @param parentAbsolutePath     [in] The absolute path of the parent.
///
/// @remarks
///     As an example, "C:/My/Folder" is a descendant of "C:/".
///     @par
///     If either path contains "." or "..", clean it with easypath_clean() before calling this.
int easypath_isdescendant(const char* descendantAbsolutePath, const char* parentAbsolutePath);

/// Determines whether or not the given path is a direct child of another.
///
/// @param childAbsolutePath [in] The absolute of the child.
/// @param parentAbsolutePath [in] The absolute path of the parent.
///
/// @remarks
///     As an example, "C:/My/Folder" is NOT a child of "C:/" - it is a descendant. Alternatively, "C:/My" IS a child of "C:/".
///     @par
///     If either path contains "." or "..", clean it with easypath_clean() before calling this.
int easypath_ischild(const char* childAbsolutePath, const char* parentAbsolutePath);


/// Modifies the given path by transforming it into it's base path.
void easypath_basepath(char* path);

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
void easypath_copybasepath(const char* path, char* baseOut, unsigned int baseSizeInBytes);

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
const char* easypath_filename(const char* path);

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
const char* easypath_extension(const char* path);


/// Checks whether or not the two paths are equal.
///
/// @param path1 [in] The first path.
/// @param path2 [in] The second path.
///
/// @return 1 if the paths are equal, 0 otherwise.
///
/// @remarks
///     This is case-sensitive.
///     @par
///     This is more than just a string comparison. Rather, this splits the path and compares each segment. The path "C:/My/Folder" is considered
///     equal to to "C:\\My\\Folder".
int easypath_equal(const char* path1, const char* path2);

/// Checks if the extension of the given path is equal to the given extension.
///
/// @remarks
///     By default this is NOT case-sensitive, however if the standard library is disable, it is case-sensitive.
int easypath_extensionequal(const char* path, const char* extension);


/// Determines whether or not the given path is relative.
///
/// @param path [in] The path to check.
int easypath_isrelative(const char* path);

/// Determines whether or not the given path is absolute.
///
/// @param path [in] The path to check.
int easypath_isabsolute(const char* path);


/// Appends two paths together, ensuring there is not double up on the last slash.
///
/// @param base                  [in, out] The base path that is being appended to.
/// @param baseBufferSizeInBytes [in]      The size of the buffer pointed to by "base", in bytes.
/// @param other                 [in]      The other path segment.
///
/// @remarks
///     This assumes both paths are well formed and "other" is a relative path.
int easypath_append(char* base, unsigned int baseBufferSizeInBytes, const char* other);

/// Appends an iterator object to the given base path.
int easypath_appenditerator(char* base, unsigned int baseBufferSizeInBytes, easypath_iterator i);

/// Appends an extension to the given path.
int easypath_appendextension(char* base, unsigned int baseBufferSizeInBytes, const char* extension);

/// Appends two paths together, and copyies them to a separate buffer.
///
/// @param dst            [out] The destination buffer.
/// @param dstSizeInBytes [in]  The size of the buffer pointed to by "dst", in bytes.
/// @param base           [in]  The base directory.
/// @param other          [in]  The relative path to append to "base".
///
/// @return 1 if the paths were appended successfully; 0 otherwise.
///
/// @remarks
///     This assumes both paths are well formed and "other" is a relative path.
int easypath_copyandappend(char* dst, unsigned int dstSizeInBytes, const char* base, const char* other);

/// Appends a base path and an iterator together, and copyies them to a separate buffer.
///
/// @param dst            [out] The destination buffer.
/// @param dstSizeInBytes [in]  The size of the buffer pointed to by "dst", in bytes.
/// @param base           [in]  The base directory.
/// @param i              [in]  The iterator to append.
///
/// @return 1 if the paths were appended successfully; 0 otherwise.
///
/// @remarks
///     This assumes both paths are well formed and "i" is a valid iterator.
int easypath_copyandappenditerator(char* dst, unsigned int dstSizeInBytes, const char* base, easypath_iterator i);

/// Appends an extension to the given base path and copies them to a separate buffer.
/// @param dst            [out] The destination buffer.
/// @param dstSizeInBytes [in]  The size of the buffer pointed to by "dst", in bytes.
/// @param base           [in]  The base directory.
/// @param extension      [in]  The relative path to append to "base".
///
/// @return 1 if the paths were appended successfully; 0 otherwise.
int easypath_copyandappendextension(char* dst, unsigned int dstSizeInBytes, const char* base, const char* extension);

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
///     The output buffer should be overlap with the input path.
///     @par
///     As an example, the path "my/messy/../path" will result in "my/path"
///     @par
///     The path "my/messy/../../../path" (note how there are too many ".." segments) will return "path" (the extra ".." segments will be dropped.)
///     @par
///     If an error occurs, such as an invalid input path, 0 will be returned.
unsigned int easypath_clean(const char* path, char* pathOut, unsigned int pathOutSizeInBytes);


/// Removes the extension from the given path.
int easypath_removeextension(char* path);


/// strlen()
unsigned int easypath_strlen(const char* str);

/// strcpy_s() implementation.
void easypath_strcpy(char* dst, unsigned int dstSizeInBytes, const char* src);

/// easypath_strcpy2()
void easypath_strcpy2(char* dst, unsigned int dstSizeInBytes, const char* src, unsigned int srcSizeInBytes);


#ifdef __cplusplus
}
#endif

#endif

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
