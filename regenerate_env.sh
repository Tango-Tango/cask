#!/bin/bash

BUILD_TARGET=${1:-all}

echo "=====> Regnerating Build Environments"


if [ "$BUILD_TARGET" = "debug" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_debug
    CC=gcc CXX=g++ meson setup build_debug \
        -Db_sanitize=address,undefined \
        -Db_coverage=true \
        -Dwarning_level=3 \
        -Dwerror=true
fi

if [ "$BUILD_TARGET" = "release" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_release
    CC=gcc CXX=g++ meson setup build_release \
        -Dbuildtype=release \
        -Dwarning_level=3 \
        -Dwerror=true
fi

if [ "$BUILD_TARGET" = "release_no_atomics" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_release_no_atomics
    CC=gcc CXX=g++ meson setup build_release_no_atomics \
        -Dbuildtype=release \
        -Dwarning_level=3 \
        -Dwerror=true \
        -Dref_uses_atomics=false
fi


if [ "$BUILD_TARGET" = "release_always_async" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_release_always_async
    CC=gcc CXX=g++ meson setup build_release_always_async \
        -Dbuildtype=release \
        -Dwarning_level=3 \
        -Dwerror=true \
        -Dbatch_size=1
fi


if [ "$BUILD_TARGET" = "clang" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_clang
    CC=clang CXX=clang++ meson setup build_clang \
        -Doptimization=0 \
        -Ddebug=false \
        -Dwarning_level=3 \
        -Dwerror=true \
        -Dref_uses_atomics=true
fi

if [ "$BUILD_TARGET" = "clang_debug" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_clang_debug
    CC=clang CXX=clang++ meson setup build_clang_debug \
        -Db_sanitize=address,undefined \
        -Db_coverage=true \
        -Dwarning_level=3 \
        -Dwerror=true \
        -Dref_uses_atomics=false
fi

if [ "$BUILD_TARGET" = "mips" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_mips
    meson setup build_mips --cross-file mips.ini \
        -Dbuildtype=release \
        -Dwarning_level=3 \
        -Dwerror=true
fi

if [ "$BUILD_TARGET" = "arm" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_arm
    meson setup build_arm --cross-file arm.ini \
        -Dbuildtype=release \
        -Dwarning_level=3 \
        -Dwerror=true
fi

