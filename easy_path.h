// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#ifndef __easy_path_h_
#define __easy_path_h_

#ifdef __cplusplus
extern "C" {
#endif


// Structure representing a section of a path.
typedef struct 
{
    int offset;
    int length;

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
easypath_iterator easypath_begin(const char* path);

/// Goes to the next segment in the path as per the given iterator.
///
/// @param i [in] A pointer to the iterator to increment.
///
/// @return True if the iterator contains a valid value. Use this to determine when to terminate iteration.
int easypath_next(easypath_iterator* i);

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
///     On a path such as "filename.ext1.ext2" the returned string will be "ext1.ext2". You can call this function
///     like "easypath_extension(easypath_extension(path))" to get "ext2".
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


/// Determines whether or not the given path is relative.
///
/// @param path [in] The path to check.
int easypath_isrelative(const char* path);

/// Determines whether or not the given path is absolute.
///
/// @param path [in] The path to check.
int easypath_isabsolute(const char* path);


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