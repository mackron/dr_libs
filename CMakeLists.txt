cmake_minimum_required(VERSION 3.10)
project(dr_libs)

# Options
option(DR_LIBS_BUILD_TESTS "Build tests"              OFF)
option(DR_LIBS_FORCE_CXX   "Force compilation as C++" OFF)
option(DR_LIBS_FORCE_C89   "Force compilation as C89" OFF)
option(DR_LIBS_NO_WAV      "Disable WAV"              OFF)
option(DR_LIBS_NO_FLAC     "Disable FLAC"             OFF)
option(DR_LIBS_NO_MP3      "Disable MP3"              OFF)

# Construct compiler flags.
set(COMPILE_OPTIONS)

if(DR_LIBS_FORCE_CXX AND DR_LIBS_FORCE_C89)
    message(FATAL_ERROR "DR_LIBS_FORCE_CXX and DR_LIBS_FORCE_C89 cannot be enabled at the same time.")
endif()

if(DR_LIBS_FORCE_CXX)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Compiling as C++ (GNU/Clang)")
        list(APPEND COMPILE_OPTIONS -x c++)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(STATUS "Compiling as C++ (MSVC)")
        list(APPEND COMPILE_OPTIONS /TP)
    else()
        message(WARNING "DR_LIBS_FORCE_CXX is enabled but the compiler does not support it. Ignoring.")
    endif()
endif()

if(DR_LIBS_FORCE_C89)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Compiling as C89")
        list(APPEND COMPILE_OPTIONS -std=c89)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(WARNING "MSVC does not support forcing C89. DR_LIBS_FORCE_C89 ignored.")
    else()
        message(WARNING "DR_LIBS_FORCE_C89 is enabled but the compiler does not support it. Ingoring.")
    endif()
endif()

# Warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND COMPILE_OPTIONS -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    #list(APPEND COMPILE_OPTIONS /W4)
endif()



# WAV
if(NOT DR_LIBS_NO_WAV)
    if(DR_LIBS_BUILD_TESTS)
        enable_testing()

        # We use libsndfile as a benchmark for dr_wav. We link dynamically at runtime, but we still need the sndfile.h header at compile time.
        find_path(SNDFILE_INCLUDE_DIR sndfile.h)
        if(SNDFILE_INCLUDE_DIR)
            message(STATUS "sndfile.h found at ${SNDFILE_INCLUDE_DIR}")

            add_executable(wav_decoding tests/wav/wav_decoding.c)
            target_include_directories(wav_decoding PRIVATE ${SNDFILE_INCLUDE_DIR})
            target_link_libraries(wav_decoding PRIVATE dl)
            add_test(NAME wav_decoding COMMAND wav_decoding)

            add_executable(wav_decoding_cpp tests/wav/wav_decoding.cpp)
            target_include_directories(wav_decoding_cpp PRIVATE ${SNDFILE_INCLUDE_DIR})
            target_link_libraries(wav_decoding_cpp PRIVATE dl)
            add_test(NAME wav_decoding_cpp COMMAND wav_decoding_cpp)

            add_executable(wav_encoding tests/wav/wav_encoding.c)
            target_include_directories(wav_encoding PRIVATE ${SNDFILE_INCLUDE_DIR})
            target_link_libraries(wav_encoding PRIVATE dl m)
            add_test(NAME wav_encoding COMMAND wav_encoding)

            add_executable(wav_playback tests/wav/wav_playback.c)
            target_include_directories(wav_playback PRIVATE ${SNDFILE_INCLUDE_DIR})
            target_link_libraries(wav_playback PRIVATE dl m pthread)
            add_test(NAME wav_playback COMMAND wav_playback)

            add_executable(wav_playback_cpp tests/wav/wav_playback.cpp)
            target_include_directories(wav_playback_cpp PRIVATE ${SNDFILE_INCLUDE_DIR})
            target_link_libraries(wav_playback_cpp PRIVATE dl m pthread)
            add_test(NAME wav_playback_cpp COMMAND wav_playback_cpp)
        else()
            message(WARNING "Could not find sndfile.h. dr_wav tests will not be built.")
        endif()
    else()
        # Not building tests.
    endif()
endif()


# FLAC
if(NOT DR_LIBS_NO_FLAC)
    if(DR_LIBS_BUILD_TESTS)
        enable_testing()

        function(add_flac_test name source)
            add_executable(${name} tests/flac/${source})
            target_link_libraries(${name} PRIVATE FLAC)
            add_test(NAME ${name} COMMAND ${name})
        endfunction()

        # We test against libFLAC.
        if(TARGET FLAC)
            message(STATUS "libFLAC found. Building FLAC tests.")
            set(HAS_FLAC TRUE)
        else()
            find_library(FLAC FLAC)
            if(FLAC)
                message(STATUS "libFLAC found. Building FLAC tests.")
                set(HAS_FLAC TRUE)
            else()
                message(WARNING "libFLAC not found. FLAC tests will not be built.")
            endif()
        endif()

        if(HAS_FLAC)
            add_flac_test(flac_decoding flac_decoding.c)
            add_flac_test(flac_decoding_cpp flac_decoding.cpp)
            add_flac_test(flac_seeking flac_seeking.c)
        endif()
    else()
        # Not building tests.
    endif()
endif()


# MP3
if(NOT DR_LIBS_NO_MP3)
    if(DR_LIBS_BUILD_TESTS)
        enable_testing()

        add_executable(mp3_playback tests/mp3/mp3_playback.c)
        target_include_directories(mp3_playback PRIVATE ${SNDFILE_INCLUDE_DIR})
        target_link_libraries(mp3_playback PRIVATE dl m pthread)
        add_test(NAME mp3_playback COMMAND mp3_playback ${CMAKE_CURRENT_SOURCE_DIR}/tests/testvectors/mp3/tests/test.mp3)

        add_executable(mp3_extract tests/mp3/mp3_extract.c)
        target_include_directories(mp3_extract PRIVATE ${SNDFILE_INCLUDE_DIR})
        target_link_libraries(mp3_extract PRIVATE dl m pthread)
        add_test(NAME mp3_extract COMMAND mp3_extract ${CMAKE_CURRENT_SOURCE_DIR}/tests/testvectors/mp3/tests/test.mp3 -o ${CMAKE_CURRENT_BINARY_DIR}/test.mp3)
    else()
        # Not building tests.
    endif()
endif()
