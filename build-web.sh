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
emcc --use-port=sdl2 --use-port=sdl2_image:formats=png,jpg --use-port=sdl2_ttf --preload-file ./assets/spritesheet.png --bind -o index.html ../../main.c
set +x
