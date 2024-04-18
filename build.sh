#!/usr/bin/env bash

set -eu

host_os="$(uname -s)"

xxd -i assets/shaders/vertex_shader.glsl > src/generated_resources.h
xxd -i assets/shaders/fragment_shader.glsl >> src/generated_resources.h

echo "Building natively"
pushd build

compile_flags="-I../vendor/stb -I../vendor/SDL2_gfx $(pkg-config --cflags --libs sdl2,SDL2_image,SDL2_ttf) -DUNIX"

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
cc -std=c11 -g -O0 -o summoning_debug ../src/main.c ${compile_flags} -lm -DDEBUG
cc -std=c11 -O3 -o summoning ../src/main.c ${compile_flags} -lm
set +x

if [[ "${host_os}" == "Darwin" ]]; then
	otool -L ./summoning
else
    ldd ./summoning
fi

popd
