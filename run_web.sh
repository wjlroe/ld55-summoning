#!/usr/bin/env bash

set -eu

pushd build/web

python3 -m http.server

popd
