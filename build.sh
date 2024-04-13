#!/usr/bin/env bash

set -eu

host_os="$(uname -s)"

echo "Building natively"
pushd build

compile_flags="$(pkg-config --cflags --libs sdl2,sdl2_image,sdl2_ttf)"

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
cc ${compile_flags} -g -O0 -o summoning ../main.c
set +x

if [[ "${host_os}" == "Darwin" ]]; then
	otool -L ./ludum-dare
fi

popd
