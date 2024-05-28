#!/usr/bin/env sh

set -eu

host_os="$(uname -s)"

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

build_arg="${1:-debug}"
build_debug=yes
build_release=no

if [ "${build_arg}" = "release" ]; then
	build_debug=no
	build_release=yes
fi

if [ "${build_arg}" = "all" ]; then
	build_debug=yes
	build_release=yes
fi

odin="${odin_cmd:-/opt/odin/dev-master/odin}"

odin_dir="$(dirname ${odin})"
cp -r "${odin_dir}/vendor/raylib/linux" ./

echo "Debug build"
if [ "${build_debug}" = "yes" ]; then
	"${odin}" build src \
		-out:build/summoning_debug \
		-build-mode:exe \
		-define:RAYLIB_SHARED=false \
		-debug \
		-show-timings
fi

if [ "${build_release}" = "yes" ]; then
	echo "Release build"
	"${odin}" build src \
		-out:build/summoning \
		-build-mode:exe \
		-define:RAYLIB_SHARED=false \
		-o:speed \
		-disable-assert \
		-show-timings
fi

# We're not building release mode as it doesn't work yet
# if [ "${host_os}" = "Darwin" ]; then
# 	otool -L ./build/summoning
# else
# 	ldd ./build/summoning
# fi

