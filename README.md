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


// Meanwhile, in another thread...
void MyOtherThreadEntryProc()
{
	easyfsw_event e;
	while (isMyApplicationStillAlive && easyfsw_nextevent(context, &e))
	{
		switch (e.type)
		{
		case easyfsw_event_type_created: OnFileCreated(e.absolutePath); break;
		case easyfsw_event_type_created: OnFileDeleted(e.absolutePath); break;
		case easyfsw_event_type_created: OnFileRenamed(e.absolutePath, e.absolutePathNew); break;
		case easyfsw_event_type_created: OnFileUpdated(e.absolutePath); break;
		default: break;
		}
	}
}

// Shutdown
easyfsw_context* contextOld = context;
context = NULL;
easyfsw_delete_context(contextOld);
```
Directories are watched recursively, so try to avoid using this on high level directories
like "C:/". Note the use of forward slashes. Internally, easy_fsw will convert paths to
their native formats. So on Windows, slashes are normalized to back slashes.

It's not required to wait for events on a separate thread, however easyfsw_nextevent() is
a blocking call, so it's usually best to do that on another thread. Alternative, you can
use easyfsw_peekevent() which is the same, except non-blocking. Avoid using both
easyfsw_nextevent() and easyfsw_peekevent() at the same time because both will remove
the event from the internal queue so it likely won't work the way you expect.

The shutdown sequence is a bit strange, but since another thread is accessing that pointer,
you should set it to null before calling easyfsw_delete_context(). That way, the next call
to easyfsw_nextevent() will pass in a null pointer which will cause it to immediately return
with zero and thus break the loop and terminate the thread. It is up to the application to
ensure the pointer passed to easyfsw_nextevent() is valid. Deleting a context will cause a
waiting call to easyfsw_nextevent() to return 0.


# Supported platforms
Currently, only Windows is supported which is implemented with the ReadDirectoryChangesW()
API. This has a few limitations which Change Journals should help with, but that's not
implemented yet.

Linux is coming and will use inotify. This will be implemented when I get a chance, but
feel free to contribute if you'd like.

OSX support will come when I finally get myself a Mac.


# Licence
easy_fsw is in the public domain so feel free to use however you'd like. I'd be interested
to know if you're using it, though.

There's no warranty with this. Use it at your own risk.