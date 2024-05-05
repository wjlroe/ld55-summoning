#!/usr/bin/env bash

set -eu

host_os="$(uname -s)"

echo "Building natively"
pushd build

compile_flags="-I. -I../vendor/stb -I/usr/local/include $(pkg-config --cflags --libs sdl2) -DUNIX"

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
cc -std=c11 -g -O0 -o generate_resources ../src/generate_resources.c
./generate_resources
cc -std=c11 -g -O0 -o summoning_debug ../src/main.c ${compile_flags} -lm -DDEBUG
cc -std=c11 -g -O3 -o summoning ../src/main.c ${compile_flags} -lm -static
set +x

# Static compiling the release binary means we can't/don't have to check this
# if [[ "${host_os}" == "Darwin" ]]; then
# 	otool -L ./summoning
# else
#   ldd ./summoning
# fi

popd
