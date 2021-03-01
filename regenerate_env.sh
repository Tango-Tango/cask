#!/bin/bash

BUILD_TARGET=${1:-all}

echo "=====> Regnerating Build Environments"


if [ "$BUILD_TARGET" = "host" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_host
    meson setup build_host -Db_sanitize=address,undefined -Db_coverage=true
fi

if [ "$BUILD_TARGET" = "benchmark" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_benchmark
    meson setup build_benchmark -Dbuildtype=release
fi

