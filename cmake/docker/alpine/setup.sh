#!/usr/bin/env sh

set -x
apk add --no-cache git build-base linux-headers cmake swig \
	python3-dev py3-virtualenv
python3 -m pip install wheel
