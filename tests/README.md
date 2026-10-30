All tests for all decoders are run through dr_libs_test. You can test files or folders by passing it on the command line:

    dr_libs_test wav
    dr_libs_test wav flac
    dr_libs_test path/to/my/file.flac

In these examples, "wav" and "flac" are folders, and they will be enumerated recursively and will test each file within. The name of the base folder will be used to determine the encoding format being tested. You can also pass in a specific file if you want to test just one file which is useful if you're debugging a single problematic file.

The test program also recognizes a database of test vectors with this format:

    <root>
        vectors/
            wav/
            flac/
            mp3/

By default it will look at "external/audiovectors" for this database. This is not distributed with the dr_libs repository so if you want to use it you'll need to set it up. I have a public database available here:

    https://github.com/mackron/audiovectors

You can clone this into "external/", or you can put it somewhere else and use the `--audiovectors <path>` command line switch. Alternatively, you can build your own vector database, so long as it follows the folder structure described above. Look at the mackron/audiovectors repository for an example of how to structure the database.

When looking for files and folders, the test program will first check the file system like normal, and if it's not found will check in the `<audiovectors>/vectors` folder.

By default, the test program will always assume the file is valid. However, in practice it's useful to also test erroneous files. Decoders will want to fail opening or decoding files in these cases. In order for the test program to know to expect failures it needs to know which files to expect to fail. You can specify this in exceptions.txt. Inspect that file in this repository to see how to use this.

Sometimes it's useful to keep test files private, such as if it's a licensed music file, for example. To make this easy to support, the test program will also recognized an exceptions-private.txt. This file is not included in this repository.

To build the test program you'll need libsndfile, libFLAC and libmpg123. These are used as references for comparing decoded output. The CMake script will look for system-installed development packages. If they're not found it will try pkg-config. Finally it'll look in "external/":

    external/libsndfile
    external/flac
    external/ogg
    external/mpg123

Set `DR_LIBS_USE_BUNDLED_DEPENDENCIES=ON` to skip system-package searches and build these dependencies from `external/`. Missing sources cause configuration to fail. These source folders are gitignored and must not be committed. Windows CI downloads fixed source versions into these folders before building.
