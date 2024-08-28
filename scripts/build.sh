#!/bin/bash
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

ARCH=$(uname -s)
MACHINE=$(uname -m)

LLVM_MINGW_VER=20240619
WITH_CPU=${WITH_CPU:-${MACHINE}}
BUILD_TYPE=${BUILD_TYPE:-MinSizeRel}

case "$WITH_CPU" in
  x86|i586|i686)
    WITH_CPU="i686"
    ;;
  x86_64)
    WITH_CPU="x86_64"
    ;;
  arch64|arm64)
    WITH_CPU="aarch64"
    ;;
esac

mkdir -p third_party

case "$ARCH" in
  MINGW*|MSYS*)
    if [ ! -d third_party/llvm-mingw-${LLVM_MINGW_VER}-ucrt-${MACHINE} ]; then
      pushd third_party
      curl -C - -L -O https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VER}/llvm-mingw-${LLVM_MINGW_VER}-ucrt-${MACHINE}.zip
      "/c/Program Files/7-Zip/7z.exe" x llvm-mingw-${LLVM_MINGW_VER}-ucrt-${MACHINE}.zip -aoa
      rm -f llvm-mingw-${LLVM_MINGW_VER}-ucrt-${MACHINE}.zip
      popd
    fi
    LLVM_BASE="$PWD/third_party/llvm-mingw-${LLVM_MINGW_VER}-ucrt-${MACHINE}"
    ;;
  Linux)
    if [ ! -d third_party/llvm-mingw-${LLVM_MINGW_VER}-ucrt-ubuntu-20.04-${MACHINE} ]; then
      pushd third_party
      curl -C - -L -O https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VER}/llvm-mingw-${LLVM_MINGW_VER}-ucrt-ubuntu-20.04-${MACHINE}.tar.xz
      tar -xf llvm-mingw-${LLVM_MINGW_VER}-ucrt-ubuntu-20.04-${MACHINE}.tar.xz
      rm -f llvm-mingw-${LLVM_MINGW_VER}-ucrt-ubuntu-20.04-${MACHINE}.tar.xz
      popd
    fi
    LLVM_BASE="$PWD/third_party/llvm-mingw-${LLVM_MINGW_VER}-ucrt-ubuntu-20.04-${MACHINE}"
    ;;
  Darwin)
    if [ ! -d third_party/llvm-mingw-${LLVM_MINGW_VER}-ucrt-macos-universal ]; then
      pushd third_party
      curl -C - -L -O https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VER}/llvm-mingw-${LLVM_MINGW_VER}-ucrt-macos-universal.tar.xz
      tar -xf llvm-mingw-${LLVM_MINGW_VER}-ucrt-macos-universal.tar.xz
      rm -f llvm-mingw-${LLVM_MINGW_VER}-ucrt-macos-universal.tar.xz
      popd
    fi
    LLVM_BASE="$PWD/third_party/llvm-mingw-${LLVM_MINGW_VER}-ucrt-macos-universal"
    ;;
  *)
    echo "Unsupported OS ${ARCH}"
    exit 1
    ;;
esac

rm -rf build
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -G Ninja -DHOST_OS=$HOST_OS \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/../cmake/platforms/MinGW.cmake" \
  -DLLVM_SYSROOT="$LLVM_BASE" -DMINGW_SYSROOT="$LLVM_BASE" \
  -DGCC_SYSTEM_PROCESSOR=$WITH_CPU -DGCC_TARGET=$WITH_CPU-w64-mingw32
rm -f GenshinImpactLoader.exe
ninja GenshinImpactLoader
"$LLVM_BASE/bin/llvm-objcopy" --only-keep-debug GenshinImpactLoader.exe GenshinImpactLoader.exe.dbg
"$LLVM_BASE/bin/llvm-objcopy" --strip-debug GenshinImpactLoader.exe
"$LLVM_BASE/bin/llvm-objcopy" --add-gnu-debuglink=GenshinImpactLoader.exe.dbg GenshinImpactLoader.exe
