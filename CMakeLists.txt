cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

if (CMAKE_CURRENT_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message(FATAL_ERROR "In source-tree build not support")
endif()

# Select the MSVC runtime library for use by compilers targeting the MSVC ABI.
# Use CMAKE_MSVC_CRT_LINKAGE variable internally, option: dynamic and static
#
# Introduced by cmake 3.15
# https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html#variable:CMAKE_MSVC_RUNTIME_LIBRARY
# and https://cmake.org/cmake/help/latest/policy/CMP0091.html
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

# https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?redirectedfrom=MSDN&view=msvc-170
set(CMAKE_MSVC_CRT_LINKAGE "dynamic" CACHE STRING "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<STREQUAL:${CMAKE_MSVC_CRT_LINKAGE},dynamic>:DLL>" CACHE STRING "")

# Defer enabling C and CXX languages.
project(GenshinLoader NONE)

if(MSVC)
  # On Windows, prefer cl over gcc if both are available. By default most of
  # the CMake generators prefer gcc, even on Windows.
  set(CMAKE_GENERATOR_CC cl)

  # new in cmake 3.10
  # https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_ARCHITECTURE_ID.html
  # not used in ABI-detection due to the cmake issue https://gitlab.kitware.com/cmake/cmake/-/issues/17702
  set(CMAKE_C_COMPILER_ARCHITECTURE_ID "${CMAKE_C_COMPILER_TARGET}" CACHE STRING "")
  set(CMAKE_CXX_COMPILER_ARCHITECTURE_ID "${CMAKE_CXX_COMPILER_TARGET}" CACHE STRING "")
endif()

if (APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.10")
endif()

## Export build compilation database if possible
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

enable_language(C)
enable_language(CXX)

if (CMAKE_MSVC_CRT_LINKAGE STREQUAL "static")
  # replace /MD with /MT
  set(CompilerFlags
    CMAKE_CXX_FLAGS
    CMAKE_CXX_FLAGS_DEBUG
    CMAKE_CXX_FLAGS_MINSIZEREL
    CMAKE_CXX_FLAGS_RELEASE
    CMAKE_CXX_FLAGS_RELWITHDEBINFO
    CMAKE_C_FLAGS
    CMAKE_C_FLAGS_DEBUG
    CMAKE_C_FLAGS_RELEASE
    CMAKE_C_FLAGS_MINSIZEREL
    CMAKE_C_FLAGS_RELEASE
    CMAKE_C_FLAGS_RELWITHDEBINFO
    )
  foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
  endforeach()
  foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/MDd" "/MTd" ${CompilerFlag} "${${CompilerFlag}}")
  endforeach()
endif()

# source code contains utf-8 charcters
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

message(STATUS "Build with MSVC ${CMAKE_MSVC_CRT_LINKAGE} CRT library")

## definitions FOR CRT
add_definitions(-D__STD_C)
add_definitions(-D_CRT_RAND_S)
add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
add_definitions(-D_SCL_SECURE_NO_DEPRECATE)

include_directories(imgui)
include_directories(imgui/backends)
include_directories(imgui/examples/libs/glfw/include)

link_directories(imgui/examples/libs/glfw/lib-vc2010-64)

add_executable(GenshinLoader
  WIN32
  main.cpp
  imgui/backends/imgui_impl_glfw.cpp
  imgui/backends/imgui_impl_opengl3.cpp
  imgui/imgui.cpp
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  )

find_library(glfw3 glfw3.lib PATHS imgui/examples/libs/glfw/lib-vc2010-64)
find_library(opengl32 opengl32.lib)
link_libraries(opengl32)
link_libraries(glfw3)
