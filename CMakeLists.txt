cmake_minimum_required(VERSION 3.10)
project(dr_libs)

# Options
option(DR_LIBS_BUILD_TESTS      "Build tests"               OFF)
option(DR_LIBS_BUILD_FUZZERS    "Build libFuzzer harnesses" OFF)
option(DR_LIBS_FORCE_CXX        "Force compilation as C++"  OFF)
option(DR_LIBS_FORCE_C89        "Force compilation as C89"  OFF)
option(DR_LIBS_SANITIZE_ADDRESS "Enable AddressSanitizer"   OFF)
option(DR_LIBS_NO_SIMD          "Disable SIMD"              OFF)
option(DR_LIBS_NO_WAV           "Disable WAV"               OFF)
option(DR_LIBS_NO_FLAC          "Disable FLAC"              OFF)
option(DR_LIBS_NO_MP3           "Disable MP3"               OFF)
option(DR_LIBS_USE_BUNDLED_DEPENDENCIES "Build test dependencies from external/ without searching for system packages" OFF)

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

# AddressSanitizer
set(LINK_OPTIONS)
if(DR_LIBS_SANITIZE_ADDRESS)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Enabling AddressSanitizer")
        list(APPEND COMPILE_OPTIONS -fsanitize=address)
        list(APPEND LINK_OPTIONS -fsanitize=address)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(STATUS "Enabling AddressSanitizer (MSVC)")
        list(APPEND COMPILE_OPTIONS /fsanitize=address)
    else()
        message(WARNING "DR_LIBS_SANITIZE_ADDRESS is enabled but the compiler does not support it. Ignoring.")
    endif()
endif()


# Set up an interface for our compile options.
add_library(dr_libs_interface INTERFACE)
target_compile_options(dr_libs_interface INTERFACE ${COMPILE_OPTIONS})
target_link_options   (dr_libs_interface INTERFACE ${LINK_OPTIONS})

if(DR_LIBS_NO_SIMD)
    target_compile_definitions(dr_libs_interface INTERFACE DR_FLAC_NO_SIMD DR_MP3_NO_SIMD)
endif()


# Common includes for dl, pthread, etc.
set(COMMON_LIBRARIES)
if(UNIX)
    list(APPEND COMMON_LIBRARIES dl pthread m dr_libs_interface)
endif()


# Prefer package targets, but also support development packages that only provide pkg-config files.
function(dr_libs_find_system_test_library package target pkg_config_name)
    if(DR_LIBS_USE_BUNDLED_DEPENDENCIES)
        return()
    endif()

    if(TARGET ${target} OR CMAKE_DISABLE_FIND_PACKAGE_${package})
        return()
    endif()

    find_package(${package} CONFIG QUIET)
    if(TARGET ${target})
        message(STATUS "Using system ${package}.")
        return()
    endif()

    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(DR_LIBS_${package} QUIET IMPORTED_TARGET ${pkg_config_name})
        if(TARGET PkgConfig::DR_LIBS_${package})
            add_library(${target} INTERFACE IMPORTED)
            set_target_properties(${target} PROPERTIES INTERFACE_LINK_LIBRARIES PkgConfig::DR_LIBS_${package})
            message(STATUS "Using system ${package} through pkg-config.")
        endif()
    endif()
endfunction()


# Suppress compiler warnings only for targets built within a bundled dependency.
function(dr_libs_disable_dependency_warnings source_dir)
    get_property(targets DIRECTORY "${source_dir}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(target_type ${target} TYPE)
        if(target_type MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
            if(MSVC)
                target_compile_options(${target} PRIVATE "$<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:/w>")
            elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                target_compile_options(${target} PRIVATE "$<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-w>")
            endif()
        endif()
    endforeach()

    get_property(subdirectories DIRECTORY "${source_dir}" PROPERTY SUBDIRECTORIES)
    foreach(subdirectory IN LISTS subdirectories)
        dr_libs_disable_dependency_warnings("${subdirectory}")
    endforeach()
endfunction()


# Keep dependency options local so they do not change the parent project's build settings.
function(dr_libs_setup_test_dependencies)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

    # Allow the bundled libogg's older CMake file to run with CMake 4.
    if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM OR CMAKE_POLICY_VERSION_MINIMUM VERSION_LESS 3.5)
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    endif()

    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    set(BUILD_REGTEST OFF)
    set(BUILD_PROGRAMS OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_DOCS OFF)
    set(BUILD_CXXLIBS OFF)
    set(BUILD_LIBOUT123 OFF)
    set(INSTALL_DOCS OFF)
    set(INSTALL_MANPAGES OFF)
    set(ENABLE_CPACK OFF)
    set(ENABLE_BOW_DOCS OFF)
    set(ENABLE_EXTERNAL_LIBS OFF)
    set(ENABLE_MPEG OFF)
    set(ENABLE_EXPERIMENTAL OFF)
    set(WITH_OGG ON)

    find_package(Threads REQUIRED)

    dr_libs_find_system_test_library(Ogg Ogg::ogg ogg)
    if(NOT TARGET Ogg::ogg)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/ogg/CMakeLists.txt")
            message(FATAL_ERROR "libogg was not found. Install its development package or add sources to external/ogg.")
        endif()

        message(STATUS "Building bundled libogg.")
        add_subdirectory(external/ogg EXCLUDE_FROM_ALL)
        dr_libs_disable_dependency_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/ogg")
    endif()

    dr_libs_find_system_test_library(FLAC FLAC::FLAC flac)
    if(NOT TARGET FLAC::FLAC)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/flac/CMakeLists.txt")
            message(FATAL_ERROR "libFLAC was not found. Install its development package or add sources to external/flac.")
        endif()

        message(STATUS "Building bundled libFLAC.")
        add_subdirectory(external/flac EXCLUDE_FROM_ALL)
        dr_libs_disable_dependency_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/flac")
    endif()

    dr_libs_find_system_test_library(SndFile SndFile::sndfile sndfile)
    if(NOT TARGET SndFile::sndfile)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/libsndfile/CMakeLists.txt")
            message(FATAL_ERROR "libsndfile was not found. Install its development package or add sources to external/libsndfile.")
        endif()

        message(STATUS "Building bundled libsndfile.")
        add_subdirectory(external/libsndfile EXCLUDE_FROM_ALL)
        dr_libs_disable_dependency_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/libsndfile")
    endif()

    dr_libs_find_system_test_library(MPG123 MPG123::libmpg123 libmpg123)
    if(NOT TARGET MPG123::libmpg123)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/mpg123/ports/cmake/CMakeLists.txt")
            message(FATAL_ERROR "libmpg123 was not found. Install its development package or add sources to external/mpg123.")
        endif()

        message(STATUS "Building bundled libmpg123.")
        add_subdirectory(external/mpg123/ports/cmake external/mpg123 EXCLUDE_FROM_ALL)
        dr_libs_disable_dependency_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/mpg123/ports/cmake")
        target_include_directories(libmpg123 INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/mpg123/src/include>")
        if(NOT TARGET MPG123::libmpg123)
            add_library(MPG123::libmpg123 ALIAS libmpg123)
        endif()
    endif()
endfunction()


if(DR_LIBS_BUILD_TESTS)
    dr_libs_setup_test_dependencies()

    add_executable(dr_libs_test tests/dr_libs_test.c)
    target_link_libraries(dr_libs_test PRIVATE dr_libs_interface ${COMMON_LIBRARIES} SndFile::sndfile FLAC::FLAC MPG123::libmpg123)
endif()


if(DR_LIBS_BUILD_FUZZERS)
    if(DEFINED ENV{LIB_FUZZING_ENGINE})
        set(DR_LIBS_FUZZING_ENGINE $ENV{LIB_FUZZING_ENGINE})
    else()
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(WARNING "DR_LIBS_BUILD_FUZZERS: LIB_FUZZING_ENGINE is not set and the compiler is not Clang; defaulting to -fsanitize=fuzzer, which requires Clang.")
        endif()
        set(DR_LIBS_FUZZING_ENGINE "-fsanitize=fuzzer,address")
    endif()

    function(add_dr_libs_fuzzer name source)
        add_executable(${name} ${source})
        if(NOT DEFINED ENV{LIB_FUZZING_ENGINE})
            target_compile_options(${name} PRIVATE -fsanitize=fuzzer,address)
        endif()
        target_link_libraries(${name} PRIVATE ${COMMON_LIBRARIES} ${DR_LIBS_FUZZING_ENGINE})
    endfunction()
endif()


# WAV
if(NOT DR_LIBS_NO_WAV)
    if(DR_LIBS_BUILD_TESTS)
        add_library(dr_wav_compilation STATIC tests/compilation/dr_wav.c)
        target_link_libraries(dr_wav_compilation PRIVATE dr_libs_interface)
    endif()

    if(DR_LIBS_BUILD_FUZZERS)
        add_dr_libs_fuzzer(dr_wav_fuzzer tests/fuzzing/dr_wav_fuzzer.cc)
    endif()
endif()


# FLAC
if(NOT DR_LIBS_NO_FLAC)
    if(DR_LIBS_BUILD_TESTS)
        add_library(dr_flac_compilation STATIC tests/compilation/dr_flac.c)
        target_link_libraries(dr_flac_compilation PRIVATE dr_libs_interface)
    endif()

    if(DR_LIBS_BUILD_FUZZERS)
        add_dr_libs_fuzzer(dr_flac_fuzzer tests/fuzzing/dr_flac_fuzzer.cc)
    endif()
endif()


# MP3
if(NOT DR_LIBS_NO_MP3)
    if(DR_LIBS_BUILD_TESTS)
        add_library(dr_mp3_compilation STATIC tests/compilation/dr_mp3.c)
        target_link_libraries(dr_mp3_compilation PRIVATE dr_libs_interface)
    endif()

    if(DR_LIBS_BUILD_FUZZERS)
        add_dr_libs_fuzzer(dr_mp3_fuzzer tests/fuzzing/dr_mp3_fuzzer.cc)
    endif()
endif()
