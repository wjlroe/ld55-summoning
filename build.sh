#!/usr/bin/env sh

set -eu

host_os="$(uname -s)"

echo "Building natively"
cd build

compile_flags="-I. -I../vendor/stb -I/usr/local/include $(pkg-config --cflags --libs sdl2) -DUNIX"

is_mac_rosetta() {
	if [ "${host_os}" = "Darwin" ]; then
		my_pid=$$
		fuser /usr/libexec/rosetta/runtime 2>/dev/null | xargs -n1 | while read -r pid; do
			if [ "${pid}" = "${my_pid}" ]; then
				echo "yes"
				return
			fi
		done
	fi
	echo "no"
}

if [ "$(is_mac_rosetta)" = "yes" ]; then
	>&2 echo "Running under Rosetta, so we are on an M1 Mac"
	compile_flags="${compile_flags} --target=arm64-apple-macos11"
fi

set -x
cc -std=c11 -g -O0 -o generate_resources ../src/generate_resources.c -DDEBUG -DDEBUG_STDOUT
./generate_resources
cc -std=c11 -g -O0 -o summoning_debug ../src/main.c ${compile_flags} -lm -DDEBUG -DDEBUG_STDOUT
cc -std=c11 -g -O3 -o summoning ../src/main.c ${compile_flags} -lm -static
set +x

# Static compiling the release binary means we can't/don't have to check this
# if [[ "${host_os}" == "Darwin" ]]; then
# 	otool -L ./summoning
# else
#   ldd ./summoning
# fi

cd -
