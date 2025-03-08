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


# Set up an interface for our compile options.
add_library(dr_libs_interface INTERFACE)
target_compile_options(dr_libs_interface INTERFACE ${COMPILE_OPTIONS})


# Common includes for dl, pthread, etc.
set(COMMON_LIBRARIES)
if(UNIX)
    list(APPEND COMMON_LIBRARIES dl pthread m dr_libs_interface)
endif()


# WAV
if(NOT DR_LIBS_NO_WAV)
    if(DR_LIBS_BUILD_TESTS)
        enable_testing()

        # We use libsndfile as a benchmark for dr_wav. We link dynamically at runtime, but we still need the sndfile.h header at compile time.
        find_path(SNDFILE_INCLUDE_DIR sndfile.h HINTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/external/libsndfile/include)
        if(SNDFILE_INCLUDE_DIR)
            message(STATUS "sndfile.h found at ${SNDFILE_INCLUDE_DIR}")

            add_executable(wav_decoding tests/wav/wav_decoding.c)
            target_include_directories(wav_decoding PRIVATE ${SNDFILE_INCLUDE_DIR})
            target_link_libraries(wav_decoding PRIVATE ${COMMON_LIBRARIES})
            add_test(NAME wav_decoding COMMAND wav_decoding)

            add_executable(wav_decoding_cpp tests/wav/wav_decoding.cpp)
            target_include_directories(wav_decoding_cpp PRIVATE ${SNDFILE_INCLUDE_DIR})
            target_link_libraries(wav_decoding_cpp PRIVATE ${COMMON_LIBRARIES})
            add_test(NAME wav_decoding_cpp COMMAND wav_decoding_cpp)

            add_executable(wav_encoding tests/wav/wav_encoding.c)
            target_include_directories(wav_encoding PRIVATE ${SNDFILE_INCLUDE_DIR})
            target_link_libraries(wav_encoding PRIVATE ${COMMON_LIBRARIES})
            add_test(NAME wav_encoding COMMAND wav_encoding)

            add_executable(wav_playback tests/wav/wav_playback.c)
            target_include_directories(wav_playback PRIVATE ${SNDFILE_INCLUDE_DIR})
            target_link_libraries(wav_playback PRIVATE ${COMMON_LIBRARIES})
            add_test(NAME wav_playback COMMAND wav_playback)

            add_executable(wav_playback_cpp tests/wav/wav_playback.cpp)
            target_include_directories(wav_playback_cpp PRIVATE ${SNDFILE_INCLUDE_DIR})
            target_link_libraries(wav_playback_cpp PRIVATE ${COMMON_LIBRARIES})
            add_test(NAME wav_playback_cpp COMMAND wav_playback_cpp)

            # The debugging sandbox is not intended to be run as a test. Just adding it here for IDE integration.
            add_executable(wav_debugging tests/wav/wav_debugging.cpp)
            target_include_directories(wav_debugging PRIVATE ${SNDFILE_INCLUDE_DIR})
            target_link_libraries(wav_debugging PRIVATE ${COMMON_LIBRARIES})
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
                # As a last resort, look in the tests/external/flac folder.
                if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/external/flac/CMakeLists.txt)
                    message(STATUS "libFLAC not found. Building FLAC from source.")

                    if(NOT TARGET ogg)
                        add_subdirectory(tests/external/ogg)
                    endif()

                    set(BUILD_DOCS OFF CACHE BOOL "")
                    set(BUILD_TESTING OFF CACHE BOOL "")
                    set(BUILD_EXAMPLES OFF CACHE BOOL "")
                    set(INSTALL_MANPAGES OFF CACHE BOOL "")
                    add_subdirectory(tests/external/flac)
                    set(HAS_FLAC TRUE)
                else()
                    message(WARNING "libFLAC not found. FLAC tests will not be built.")
                endif()
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

        add_executable(mp3_basic tests/mp3/mp3_basic.c)
        target_link_libraries(mp3_basic PRIVATE ${COMMON_LIBRARIES})
        add_test(NAME mp3_basic COMMAND mp3_basic ${CMAKE_CURRENT_SOURCE_DIR}/tests/testvectors/mp3/tests)

        add_executable(mp3_playback tests/mp3/mp3_playback.c)
        target_link_libraries(mp3_playback PRIVATE ${COMMON_LIBRARIES})
        add_test(NAME mp3_playback COMMAND mp3_playback ${CMAKE_CURRENT_SOURCE_DIR}/tests/testvectors/mp3/tests/test.mp3)

        add_executable(mp3_extract tests/mp3/mp3_extract.c)
        target_link_libraries(mp3_extract PRIVATE ${COMMON_LIBRARIES})
        add_test(NAME mp3_extract COMMAND mp3_extract ${CMAKE_CURRENT_SOURCE_DIR}/tests/testvectors/mp3/tests/test.mp3 -o ${CMAKE_CURRENT_BINARY_DIR}/test.mp3)
    else()
        # Not building tests.
    endif()
endif()
