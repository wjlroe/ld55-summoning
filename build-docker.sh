#!/bin/bash

set -eu

docker build . -t summoning

docker run -t -i --rm -v `pwd`:/io summoning
