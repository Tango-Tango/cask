#!/bin/bash

BUILD_TARGET=${1:-all}

echo "=====> Regnerating Build Environments"


if [ "$BUILD_TARGET" = "debug" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_debug
    meson setup build_debug -Db_sanitize=address,undefined -Db_coverage=true
fi

if [ "$BUILD_TARGET" = "release" ] || [ "$BUILD_TARGET" = "all" ]; then
    rm -rf build_release
    meson setup build_release -Dbuildtype=release
fi

