# About
This repository is a container for a few basic, public domain libraries written in C
which are intended to make working with the file system a bit easier. Libraries contained
within this repository include:
 - easy_vfs: A virtual file system
 - easy_fsw: A library for watching for changes to the file system
 
Each library is independant from the others.


# easy_vfs
easy_vfs is an implementation of a virtual file system which allows you to load files
from archives/packages (such as zip files) using a common API.

Some noteworthy features:
 - Supports verbose absolute paths to avoid ambiguity. For example you can specify a path
   such as "my/package.zip/file.txt"
 - Supports shortened, transparent paths by automatically scanning for supported archives. The
   path "my/package.zip/file.txt" can be shortened to "my/file.txt", for example.
 - Fully recursive. A path such as "pack1.zip/pack2.zip/file.txt" should work just fine.
 - Easily supports custom package formats. Look at the implementations of Zip archives for an
   example.
 - No compulsory dependencies except for the C standard library.
    - Optionally depends on miniz.c which is included in easy_vfs' source tree and released
	  under the same licence. This is required for Zip archive support.
	- Optionally depends on easy_path which is used to strip away some duplicate code if
	  your project happens to already use it.

Limitations:
 - When a file contained within a Zip file is opened, the entire uncompressed data is loaded
   onto the heap. Keep this in mind when working with large files.
 - Zip, PAK and Wavefront MTL archives are read-only at the moment.

 
## How to use it
There's just a few files. Just add these to your project's source tree and you should be
good to go. The "extras" folder contains implementations of supported archive formats -
just include the ones you want, and leave out the ones you don't.

Below is an example:
```c
// Create a context.
easyvfs_context* pVFS = easyvfs_create_context();
if (pVFS == NULL)
{
	// There was an error creating the context.
}

// Add your base directories for loading from relative paths. If you do not specify at
// least one base directory you will need to load from absolute paths.
easyvfs_add_base_directory(pVFS, "C:/Users/Admin");
easyvfs_add_base_directory(pVFS, "C:/My/Folder");

...

// Open a file. A relative path was specified which means it will first check it against
// "C:/Users/Admin". If it can't be found it will then check against "C:/My/Folder".
easyvfs_file* pFile = easyvfs_open(pVFS, "my/file.txt", EASYVFS_READ);
if (pFile == NULL)
{
	// There was an error loading the file. It probably doesn't exist.
}

easyvfs_read(pFile, buffer, bufferSize);
easyvfs_close(pFile);

...

// Shutdown.
easyvfs_delete_context(pVFS);
```


## Platforms
Currently, only Windows is supported. The platform-specific section is quite small so it
should be quite easy to add support for other platforms.

Support for Linux is coming soon.


## TODO
 - Add support for Linux
 - Improve efficiency.
 - Improve ZIP files.

 
---
# easy_fsw
easy_fsw is a small library for watching for changes to the file system. It's designed to be
simple, small and easy to integrate into a project.


## How to use it
The library is made up of just two files - one header file and one source file. Just add the
files to your project's source tree and you should be good. There are no dependencies except
for the standard library for malloc(), etc.

Below is a usage example:
```c
// Startup.
easyfsw_context* context = easyfsw_create_context();
if (context == NULL)
{
	// There was an error creating the context...
}

easyfsw_add_directory(context, "C:/My/Folder");
easyfsw_add_directory(context, "C:/My/Other/Folder");

...

// Meanwhile, in another thread...
void MyOtherThreadEntryProc()
{
	easyfsw_event e;
	while (isMyApplicationStillAlive && easyfsw_nextevent(context, &e))
	{
		switch (e.type)
		{
		case easyfsw_event_type_created: OnFileCreated(e.absolutePath); break;
		case easyfsw_event_type_deleted: OnFileDeleted(e.absolutePath); break;
		case easyfsw_event_type_renamed: OnFileRenamed(e.absolutePath, e.absolutePathNew); break;
		case easyfsw_event_type_updated: OnFileUpdated(e.absolutePath); break;
		default: break;
		}
	}
}

...

// Shutdown
easyfsw_context* contextOld = context;
context = NULL;
easyfsw_delete_context(contextOld);
```
Directories are watched recursively, so try to avoid using this on high level directories
like "C:\". Also avoid watching a directory that is a descendant of another directory that's
already being watched.

It does not matter whether or not paths are specified with forward or back slashes; the
library will normalize all of that internally depending on the platform. Note, however,
that events always report their paths with forward slashes.

You don't need to wait for events on a separate thread, however easyfsw_nextevent() is
a blocking call, so it's usually best to do so. Alternatively, you can use
easyfsw_peekevent() which is the same, except non-blocking. Avoid using both
easyfsw_nextevent() and easyfsw_peekevent() at the same time because both will remove
the event from the internal queue so it likely won't work the way you expect.

The shutdown sequence is a bit strange, but since another thread is accessing that pointer,
you should set any shared pointers to null before calling easyfsw_delete_context(). That way,
the next call to easyfsw_nextevent() will pass in a null pointer which will cause it to
immediately return with zero and thus break the loop and terminate the thread. It is up to
the application to ensure the pointer passed to easyfsw_nextevent() is valid. Deleting a context
will cause a waiting call to easyfsw_nextevent() to return 0, however there is a chance that the
context is deleted before easyfsw_nextevent() has entered into it's wait state. It's up to the
application to make sure the context remains valid.


## Supported platforms
Currently, only Windows is supported which is implemented with the ReadDirectoryChangesW()
API. This has a few limitations which Change Journals should help with, but that's not
implemented yet.

Linux support is coming and will use inotify. This will be implemented when I get a chance,
but feel free to contribute if you'd like.

OSX support will come when I finally get myself a Mac.

 
---
# License
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