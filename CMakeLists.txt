cmake_minimum_required(VERSION 3.16)

project(OpenCVTask)

set(OCVTASK_VERSION "0.1")

message(STATUS "Running CMake for OpenCVTask v${OCVTASK_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(WINDOWS_HOST TRUE)
    set(LIBRARY_DIR "Win64")
endif()

option(BUILD_TEST_CLIENT OFF)


include("Dependencies.cmake")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_ITERATOR_DEBUG_LEVEL 2)
else()
    set(_ITERATOR_DEBUG_LEVEL 0)
endif()
add_definitions(-D_ITERATOR_DEBUG_LEVEL=${_ITERATOR_DEBUG_LEVEL})

if(WINDOWS_HOST)
    add_definitions(-D_WIN32_LEAN_AND_MEAN)
endif()


include_directories(${OPENCV_INCLUDE_DIR})
include_directories(${MOONVDEC_INCLUDE_DIR})
include_directories(${SATLIB_INCLUDE_DIR})
include_directories(${NVCODEC_INCLUDE_DIRS})


add_subdirectory("OpenCVTask")

if(BUILD_TEST_CLIENT)
    add_subdirectory("TestClient")
endif()

include("Deploy.cmake")
