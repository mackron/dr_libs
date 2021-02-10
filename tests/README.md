Building
========
Move into this directory and run the build script for the relevant platform. Run from this directory.

    clear && ./build_opus && ./bin/dr_opus_test_0
    
Alternatively you can compile a specific test manually:

    clear && gcc ./opus/dr_opus_test_0 -o ./bin/dr_opus_test_0 && ./bin/dr_opus_test_0
    
Test vectors will be loaded from the "testvectors" folder, relative to this directory. Therefore, you need to run
each test program from this directory:

    ./bin/dr_opus_test_0
    
    
Building and Running WAV Tests
------------------------------
The WAV tests use libsndfile as a benchmark. The tests programs dynamically link to libsndfile at runtime which
means you don't need to link to it at compile time. However, you will need the headers installed in a standard
location. The batch files for the Windows build will allow you to customize the include path. On the Windows build
you will need to drop two versions of libsndfile-1.dll into the bin directory. For the 32-bit build you need to
name it libsndfile-1-x86.dll and for the 64-bit build you need to name it libsndfile-1-x64.dll.
    

Test Vectors
============
In order to run certain tests you will need to download test vectors for the relevant project and place them into the
"testvectors" folder.

Opus
----
- Download both the original and new test vectors from https://opus-codec.org/testvectors/ and place them into
  the "testvectors/opus" folder.
- Download the Ogg Opus test vectors from https://wiki.xiph.org/OggOpus/testvectors and place them into the
  "testvectors/opus" folder.
- The folder structure should like like the following:
  - testvectors
    - opus
      - opus_testvectors
      - opus_newvectors
      - oggopus
        - failure_cases
        - opus_multichannel_examples

Flac
---
- Download test flacs from https://media.xiph.org/ and place them into the "testvectors/flac/tests" folder.