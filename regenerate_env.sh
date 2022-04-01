#!/bin/bash

BUILD_TARGET=${1:-all}

echo "=====> Regnerating Build Environments"


if [ "$BUILD_TARGET" = "debug" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_debug
    CC=gcc CXX=g++ meson setup build_debug \
        -Db_sanitize=address,undefined \
        -Db_coverage=true \
        -Dwarning_level=3 \
        -Dwerror=true \
        -Dref_uses_atomics=false
fi

if [ "$BUILD_TARGET" = "release" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_release
    CC=gcc CXX=g++ meson setup build_release \
        -Dbuildtype=release \
        -Dwarning_level=3 \
        -Dwerror=true \
        -Dref_uses_atomics=true
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

