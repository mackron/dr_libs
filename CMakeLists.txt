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
        # We use libsndfile as a benchmark for dr_wav. We link dynamically at runtime, but we still need the sndfile.h header at compile time.
        find_path(SNDFILE_INCLUDE_DIR sndfile.h)
        if(SNDFILE_INCLUDE_DIR)
            message(STATUS "sndfile.h found at ${SNDFILE_INCLUDE_DIR}")

            add_executable(wav_decoding tests/wav/wav_decoding.c)
            target_include_directories(wav_decoding PRIVATE ${SNDFILE_INCLUDE_DIR})
            target_link_libraries(wav_decoding PRIVATE dl)
            add_test(NAME wav_decoding COMMAND wav_decoding)
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
        
    else()
        # Not building tests.
    endif()
endif()


# MP3
if(NOT DR_LIBS_NO_MP3)
    if(DR_LIBS_BUILD_TESTS)
        
    else()
        # Not building tests.
    endif()
endif()
