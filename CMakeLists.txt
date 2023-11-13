cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

if (CMAKE_CURRENT_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message(FATAL_ERROR "In source-tree build not support")
endif()

# Select the MSVC runtime library for use by compilers targeting the MSVC ABI.
# Use MSVC_CRT_LINKAGE variable internally, option: dynamic and static
#
# Introduced by cmake 3.15
# https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html#variable:CMAKE_MSVC_RUNTIME_LIBRARY
# and https://cmake.org/cmake/help/latest/policy/CMP0091.html
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

# https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?redirectedfrom=MSDN&view=msvc-170
set(MSVC_CRT_LINKAGE "static" CACHE STRING "")
if (NOT MSVC_CRT_LINKAGE STREQUAL "static" AND NOT MSVC_CRT_LINKAGE STREQUAL "dynamic")
  message(FATAL_ERROR "MSVC_CRT_LINKAGE only accepts: dynamic or static")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<STREQUAL:${MSVC_CRT_LINKAGE},dynamic>:DLL>")

# Defer enabling C and CXX languages.
set(APP_NAME "GenshinImpactLoader")
project(${APP_NAME} NONE)

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

## Export build compilation database if possible
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Set to a modern C++ standard
set(CMAKE_CXX_STANDARD 17)

enable_language(C)
enable_language(CXX)
set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  set(COMPILER_CLANG TRUE)
else()
  set(COMPILER_CLANG FALSE)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(COMPILER_GCC TRUE)
else()
  set(COMPILER_GCC FALSE)
endif()

if (MSVC AND NOT COMPILER_CLANG)
  set(COMPILER_MSVC TRUE)
else()
  set(COMPILER_MSVC FALSE)
endif()

if (MSVC)
  link_libraries(bcrypt dwmapi d3d11 d3dcompiler AdvAPI32 User32 kernel32)
else()
  link_libraries(bcrypt dwmapi d3d11 d3dcompiler advapi32 user32 kernel32)
endif()

if (MSVC AND MSVC_CRT_LINKAGE STREQUAL "static")
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
if (MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif()

message(STATUS "Build with MSVC ${MSVC_CRT_LINKAGE} CRT library")

## definitions FOR CRT
add_definitions(-D__STD_C)
add_definitions(-D_CRT_RAND_S)
add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
add_definitions(-D_SCL_SECURE_NO_DEPRECATE)

## imply unicode versions
add_definitions(-D_WIN32)
add_definitions(-DWIN32)
add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)
add_definitions(-DNOMINMAX)
add_definitions(-DWIN32_LEAN_AND_MEAN)

option(BUILD_SHARED_LIBS "Build with shared libraries." OFF)

# symbol hiddens
if (NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden -fvisibility=hidden")
endif()

if (NOT MSVC)
  ## definitions
  add_definitions(-D__STDC_CONSTANT_MACROS)
  add_definitions(-D__STDC_FORMAT_MACROS)

  ## disable rtti
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

  ## disable exceptions
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
else()
  ## Disable RTTI
  set(CompilerFlags
      CMAKE_CXX_FLAGS
      CMAKE_CXX_FLAGS_DEBUG
      CMAKE_CXX_FLAGS_MINSIZEREL
      CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_RELWITHDEBINFO
      CMAKE_C_FLAGS
      CMAKE_C_FLAGS_DEBUG
      CMAKE_C_FLAGS_MINSIZEREL
      CMAKE_C_FLAGS_RELEASE
      CMAKE_C_FLAGS_RELWITHDEBINFO
      )
  foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/GR" "/GR-" ${CompilerFlag} "${${CompilerFlag}}")
  endforeach()

  ## Disable RTTI
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")

  # Disables exceptions in the STL.
  add_definitions(-D_HAS_EXCEPTIONS=0)

  ## C4530, The code uses C++ exception handling,
  ## but /EHsc wasn't included in the compiler options
  ## https://docs.microsoft.com/en-us/cpp/build/reference/eh-exception-handling-model?view=msvc-170
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

if (MSVC)
  ## Remove unreferenced data
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /INCREMENTAL:NO /FIXED:NO")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /INCREMENTAL:NO /FIXED:NO")

  # Tell linker to include FIXUP information symbol data
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /PROFILE")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /PROFILE")
endif()

if (COMPILER_GCC OR COMPILER_CLANG)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--icf=all")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--icf=all")
endif()

# add windows 7 support
add_definitions(-DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP)
add_definitions(-D_WIN32_WINNT=0x0601)
add_definitions(-DWINVER=0x0601)

# *****************************************************************************************
#           imgui Library
# *****************************************************************************************

include_directories(imgui)
include_directories(imgui/backends)
add_library(imgui STATIC
  imgui/backends/imgui_impl_win32.cpp
  imgui/backends/imgui_impl_dx11.cpp
  imgui/imgui.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
)
link_libraries(imgui)

# *****************************************************************************************
#           leveldb Library
# *****************************************************************************************

set(LEVELDB_BUILD_UTILS "OFF" CACHE STRING "")
set(LEVELDB_BUILD_TESTS "OFF" CACHE STRING "")
set(LEVELDB_BUILD_BENCHMARKS "OFF" CACHE STRING "")
set(LEVELDB_INSTALL "OFF" CACHE STRING "")
set(HAVE_CRC32C 0)
set(HAVE_SNAPPY 0)
set(HAVE_ZSTD 0)
set(HAVE_TCMALLOC 0)
add_subdirectory(leveldb)
include_directories(leveldb/include SYSTEM)
link_libraries(leveldb)

# *****************************************************************************************
#           json Library
# *****************************************************************************************
include_directories(njson/include SYSTEM)

add_executable(${APP_NAME}
  WIN32
  main.cpp
  account.cpp
  helper.cpp
  resource.rc
  )
