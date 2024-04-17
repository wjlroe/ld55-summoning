#!/usr/bin/env bash

set -eu

host_os="$(uname -s)"

xxd -i vertex_shader.glsl > assets/vertex_shader_glsl.h

echo "Building natively"
pushd build

cc -c ../assets/vertex_shader_glsl.h -o vertex_shader_glsl.o

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
cc -std=c11 -g -O0 -o summoning_debug ../main.c ${compile_flags} vertex_shader_glsl.o -lm -DDEBUG
cc -std=c11 -O3 -o summoning ../main.c ${compile_flags} -lm vertex_shader_glsl.o
set +x

if [[ "${host_os}" == "Darwin" ]]; then
	otool -L ./summoning
else
    ldd ./summoning
fi

popd
