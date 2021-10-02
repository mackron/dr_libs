cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

enable_language(C)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)					# don't let OPTION erase non-cache variables
set(CMAKE_CXX_EXTENSIONS OFF CACHE INTERNAL "" FORCE)	# disable compiler extensions
set(CMAKE_POSITION_INDEPENDENT_CODE ON)					# enable PIC
set(CMAKE_DEBUG_POSTFIX "-d")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)			# ensure CMAKE_BUILD_TYPE is not empty
endif()

# project parameters
set(project_name dr_libs)
set(project_version "0.1.0.0")
set(C_STANDARD 99)

project(${project_name} VERSION "${project_version}")

add_library(${project_name})
add_library(${project_name}::${project_name} ALIAS ${project_name})
add_subdirectory(include/dr_libs)

target_include_directories(${project_name} PRIVATE
"${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_include_directories(${project_name} SYSTEM INTERFACE
"${CMAKE_CURRENT_SOURCE_DIR}/include"
)