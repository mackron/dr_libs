# About
easy_fsw is a small library for watching for changes to the file system. It's designed to be
simple, small and easy to integrate into a project.


# How to use it
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


# Supported platforms
Currently, only Windows is supported which is implemented with the ReadDirectoryChangesW()
API. This has a few limitations which Change Journals should help with, but that's not
implemented yet.

Linux support is coming and will use inotify. This will be implemented when I get a chance,
but feel free to contribute if you'd like.

OSX support will come when I finally get myself a Mac.


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