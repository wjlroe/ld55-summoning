#!/usr/bin/env bash

set -eu

host_os="$(uname -s)"
echo "${EMSDK:=$HOME/Code/source/emsdk}"

if ! type "emcc" > /dev/null 2>&1; then
	if [[ "${host_os}" == "Darwin" ]]; then
		export PATH="${PATH}:/opt/homebrew/bin"
	else
		source "${EMSDK}/emsdk_env.sh"
	fi
fi

mkdir -p build/web

echo "Building for the web"
cd build/web
cp -r ../../assets .
set -x
compile_flags="-I../../vendor/SDL2_gfx $(pkg-config --cflags sdl2,SDL2_image,SDL2_ttf)"
# emcc -c ../../main.c ../../vendor/SDL2_gfx/SDL2_gfxPrimitives.c ../../vendor/SDL2_gfx/SDL2_rotozoom.c ${compile_flags}
tree assets
# Do we need '-sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=1gb' here?
emcc --use-port=sdl2 --use-port=sdl2_image:formats=png,jpg --use-port=sdl2_ttf --preload-file ./assets/fonts/im_fell_roman.ttf --bind -o index.html ../../main.c ${compile_flags}
set +x
