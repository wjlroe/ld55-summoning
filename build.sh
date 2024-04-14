#!/usr/bin/env bash

set -eu

host_os="$(uname -s)"

echo "Building natively"
pushd build

compile_flags="-I../vendor/SDL2_gfx $(pkg-config --cflags --libs sdl2,SDL2_image,SDL2_ttf)"

if [[ "${host_os}" == "Darwin" ]]; then
	rosetta_pids="$(fuser /usr/libexec/rosetta/runtime 2>/dev/null | xargs -n1)"
	my_pid=$$
	while read pid; do
		if [[ "${pid}" == "${my_pid}" ]]; then
			>&2 echo "Running under Rosetta, so we are on an M1 Mac"
			compile_flags="${compile_flags} --target=arm64-apple-macos11"
			break
		fi
	done <<<"${rosetta_pids}"
fi

set -x
cc -std=c11 -g -O0 -o summoning ../main.c ${compile_flags} -lm
set +x

if [[ "${host_os}" == "Darwin" ]]; then
	otool -L ./summoning
else
    ldd ./summoning
fi

popd
