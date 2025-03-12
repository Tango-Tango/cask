#!/bin/bash

BUILD_TARGET=${1:-all}

echo "=====> Regnerating Build Environments"


if [ "$BUILD_TARGET" = "debug" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_debug
    CC=gcc CXX=g++ meson setup build_debug \
        -Dinitial_blocks_per_pool=64 \
        -Ddebug=true \
        -Doptimization=0 \
        -Db_coverage=true \
        -Dwarning_level=3 \
        -Dwerror=true
fi

if [ "$BUILD_TARGET" = "release" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_release
    CC=gcc CXX=g++ meson setup build_release \
        -Dinitial_blocks_per_pool=64 \
        -Ddebug=true \
        -Doptimization=s \
        -Dwarning_level=3 \
        -Dwerror=true
fi

if [ "$BUILD_TARGET" = "release_no_atomics" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_release_no_atomics
    CC=gcc CXX=g++ meson setup build_release_no_atomics \
        -Dinitial_blocks_per_pool=64 \
        -Dbuildtype=release \
        -Dwarning_level=3 \
        -Dwerror=true \
        -Dref_uses_atomics=false
fi


if [ "$BUILD_TARGET" = "release_always_async" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_release_always_async
    CC=gcc CXX=g++ meson setup build_release_always_async \
        -Dinitial_blocks_per_pool=64 \
        -Dbuildtype=release \
        -Dwarning_level=3 \
        -Dwerror=true \
        -Dcede_iterations=1
fi

if [ "$BUILD_TARGET" = "release_forced_cede_disabled" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf release_forced_cede_disabled
    CC=gcc CXX=g++ meson setup build_release_forced_cede_disabled \
        -Dinitial_blocks_per_pool=64 \
        -Dbuildtype=release \
        -Dwarning_level=3 \
        -Dwerror=true \
        -Dcede_iterations=0
fi


if [ "$BUILD_TARGET" = "clang" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_clang
    CC=clang CXX=clang++ meson setup build_clang \
        -Dinitial_blocks_per_pool=64 \
        -Doptimization=0 \
        -Ddebug=false \
        -Dwarning_level=3 \
        -Dwerror=true \
        -Dref_uses_atomics=true
fi

if [ "$BUILD_TARGET" = "clang_debug" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_clang_debug
    CC=clang CXX=clang++ meson setup build_clang_debug \
        -Dinitial_blocks_per_pool=64 \
        -Db_sanitize=address,undefined \
        -Db_coverage=true \
        -Dwarning_level=3 \
        -Dwerror=true \
        -Dref_uses_atomics=false
fi

if [ "$BUILD_TARGET" = "mips" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_mips
    meson setup build_mips --cross-file mips.ini \
        -Dcache_line_size=32 \
        -Dinitial_blocks_per_pool=64 \
        -Dbuildtype=release \
        -Dwarning_level=3 \
        -Dwerror=true
fi

if [ "$BUILD_TARGET" = "arm" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_arm
    meson setup build_arm --cross-file arm.ini \
        -Dinitial_blocks_per_pool=64 \
        -Dbuildtype=release \
        -Dwarning_level=3 \
        -Dwerror=true
fi

