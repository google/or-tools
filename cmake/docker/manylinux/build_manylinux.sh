#!/bin/bash
set -x
set -e

PATH_BCKP=$PATH
echo "PATH: $PATH_BCKP"
for PYROOT in /opt/python/*
do
  PYTAG=$(basename "${PYROOT}")
  echo "$PYTAG"
  # Clean the build dir
  rm -rf cache/manylinux/build_$PYTAG

  PATH=${PYROOT}/bin:${PATH_BCKP}
  python -m pip install --user virtualenv
  cmake -H. -Bcache/manylinux/build_$PYTAG \
 -DBUILD_PYTHON=ON -DPYTHON_LIBRARY=${PYROOT}/lib/ -DPYTHON_INCLUDE_DIR=${PYROOT}/include/
done



