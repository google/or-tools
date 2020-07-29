#!/usr/bin/env bash
# usage: ./tools/generate_all_notebooks.sh
set -e

DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${DIR}" ]]; then
  DIR="${PWD}";
fi

echo "Intalling python3 notebook..."
/usr/bin/env python3 -m pip install --user notebook

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
