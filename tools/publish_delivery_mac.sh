#!/bin/bash
set -x
set -e

echo Uploading all Python artifacts...
make pypi_upload UNIX_PYTHON_VER=2.7
make pypi_upload UNIX_PYTHON_VER=3.5
make pypi_upload UNIX_PYTHON_VER=3.6
echo Uploading all Python artifacts...DONE
