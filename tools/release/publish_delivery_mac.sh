#!/usr/bin/env bash
set -x
set -e

# Print version
make print-OR_TOOLS_VERSION | tee publish.log

command -v twine
command -v twine | xargs echo "twine: " | tee -a publish.log

echo Uploading all Python artifacts... | tee -a publish.log
twine upload "*.whl"
echo Uploading all Python artifacts...DONE | tee -a publish.log
