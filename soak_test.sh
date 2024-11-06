#!/bin/bash

BUILD_FOLDER=$1
shift

I=1

meson compile -C "${BUILD_FOLDER}"

while true; do
	echo
	echo
	echo "=====> Test Run #${I}"
	if ! ${BUILD_FOLDER}/test/testexe $@; then
		echo
		echo
		echo "!!!! Tests Failed on Run #${I}"
		exit 1
	fi

	((I=I+1))
done

