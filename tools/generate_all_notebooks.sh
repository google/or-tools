#!/usr/bin/env bash
# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# usage: ./tools/generate_all_notebooks.sh
set -eu

DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${DIR}" ]]; then
  DIR="${PWD}";
fi

echo "Checking python3-notebook is installed..."
/usr/bin/env python3 -m pip show notebook

###############
##  Cleanup  ##
###############
echo "Remove previous .ipynb files..."
rm -f examples/notebook/*.ipynb
rm -f examples/notebook/*/*.ipynb
echo "Remove previous .ipynb files...DONE"

################
##  Examples  ##
################
for FILE in examples/python/*.py; do
  # if no files found do nothing
  [[ -e "$FILE" ]] || continue
  mkdir -p examples/notebook/examples
  echo "Generating ${FILE%.py}.ipynb"
  ./tools/export_to_ipynb.py "$FILE";
done

###############
##  Contrib  ##
###############
for FILE in examples/contrib/*.py; do
  # if no files found do nothing
  [[ -e "$FILE" ]] || continue
  if [[ $(basename "$FILE") == "word_square.py" ]]; then continue; fi
  mkdir -p examples/notebook/contrib
  echo "Generating ${FILE%.py}.ipynb"
  ./tools/export_to_ipynb.py "$FILE";
done

###############
##  Samples  ##
###############
for FILE in ortools/*/samples/*.py ; do
  # if no files found do nothing
  [[ -e "$FILE" ]] || continue
  D=$(dirname "$(dirname "${FILE}")")
  mkdir -p "${D/ortools/examples\/notebook}"
  echo "Generating ${FILE%.py}.ipynb"
  ./tools/export_to_ipynb.py "$FILE"
done
# vim: set tw=0 ts=2 sw=2 expandtab:
